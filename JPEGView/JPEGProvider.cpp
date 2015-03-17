#include "StdAfx.h"
#include "JPEGProvider.h"
#include "JPEGImage.h"
#include "ImageLoadThread.h"
#include "MessageDef.h"
#include "FileList.h"
#include "ProcessParams.h"
#include "BasicProcessing.h"

CJPEGProvider::CJPEGProvider(HWND handlerWnd, int nNumThreads, int nNumBuffers) {
	m_hHandlerWnd = handlerWnd;
	m_nNumThread = nNumThreads;
	m_nNumBuffers = nNumBuffers;
	m_nCurrentTimeStamp = 0;
	m_eOldDirection = FORWARD;
	m_pWorkThreads = new CImageLoadThread*[nNumThreads];
	for (int i = 0; i < nNumThreads; i++) {
		m_pWorkThreads[i] = new CImageLoadThread();
	}
}

CJPEGProvider::~CJPEGProvider(void) {
	for (int i = 0; i < m_nNumThread; i++) {
		delete m_pWorkThreads[i];
	}
	delete[] m_pWorkThreads;
	std::list<CImageRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		delete (*iter)->Image;
		delete *iter;
	}
}

CJPEGImage* CJPEGProvider::RequestJPEG(CFileList* pFileList, EReadAheadDirection eDirection, 
									   LPCTSTR strFileName, int nFrameIndex, const CProcessParams & processParams, bool& bOutOfMemory) {
	if (strFileName == NULL) {
		bOutOfMemory = false;
		return NULL;
	}

	// Search if we have the requested image already present or in progress
	CImageRequest* pRequest = FindRequest(strFileName, nFrameIndex);
	bool bDirectionChanged = eDirection != m_eOldDirection || eDirection == TOGGLE;
	bool bRemoveAlsoReadAhead = bDirectionChanged;
	bool bWasOutOfMemory = false;
	m_eOldDirection = eDirection;

	if (pRequest == NULL) {
		// no, request never sent for this file, add to request queue and start async
		pRequest = StartNewRequest(strFileName, nFrameIndex, processParams);
		// wait with read ahead when direction changed - maybe user just wants to resee last image
		if (!bDirectionChanged && eDirection != NONE) {
			// start parallel if more than one thread
			StartNewRequestBundle(pFileList, eDirection, processParams, m_nNumThread - 1, NULL);
		}
	}

	// wait for request if not yet ready
	if (!pRequest->Ready) {
		::OutputDebugString(_T("Waiting for request: ")); ::OutputDebugString(pRequest->FileName); ::OutputDebugString(_T("\n"));
		::WaitForSingleObject(pRequest->EventFinished, INFINITE);
		GetLoadedImageFromWorkThread(pRequest);
	} else {
		CJPEGImage* pImage = pRequest->Image;
		if (pImage != NULL) {
			// make sure the initial parameters are reset as when keep params was on before they are wrong
			EProcessingFlags procFlags = processParams.ProcFlags;
			pImage->RestoreInitialParameters(strFileName, processParams.ImageProcParams, procFlags, 
				processParams.RotationParams.Rotation, processParams.Zoom, processParams.Offsets, 
				CSize(processParams.TargetWidth, processParams.TargetHeight), processParams.MonitorSize);
		}
		::OutputDebugString(_T("Found in cache: ")); ::OutputDebugString(pRequest->FileName); ::OutputDebugString(_T("\n"));
	}

	// set before removing unused images!
	pRequest->InUse = true;
	pRequest->AccessTimeStamp = m_nCurrentTimeStamp++;

	if (pRequest->OutOfMemory) {
		// The request could not be satisfied because the system is out of memory.
		// Clear all memory and try again - maybe some readahead requests can be deleted
		::OutputDebugString(_T("Retrying request because out of memory: ")); ::OutputDebugString(pRequest->FileName); ::OutputDebugString(_T("\n"));
		bWasOutOfMemory = true;
		if (FreeAllPossibleMemory()) {
			DeleteElement(pRequest);
			pRequest = StartRequestAndWaitUntilReady(strFileName, nFrameIndex, processParams);
		}
	}

	// cleanup stuff no longer used
	RemoveUnusedImages(bRemoveAlsoReadAhead);
	ClearOldestReadAhead();

	// check if we shall start new requests (don't start another request if we are short of memory!)
	if (m_requestList.size() < (unsigned int)m_nNumBuffers && !bDirectionChanged && !bWasOutOfMemory && eDirection != NONE) {
		StartNewRequestBundle(pFileList, eDirection, processParams, m_nNumThread, pRequest);
	}

	bOutOfMemory = pRequest->OutOfMemory;
	return pRequest->Image;
}

void CJPEGProvider::NotifyNotUsed(CJPEGImage* pImage) {
	// mark image as unused but do not remove yet from request queue
	std::list<CImageRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		if ((*iter)->Image == pImage) {
			(*iter)->InUse = false;
			(*iter)->ReadAhead = false;
			return;
		}
	}
	// image not found in request queue - delete it as it is no longer used and will not be cached
	if (pImage != NULL) delete pImage;
}

void CJPEGProvider::ClearAllRequests() {
	std::list<CImageRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		if (ClearRequest((*iter)->Image)) {
			// removed from iteration, restart iteration to remove the rest
			ClearAllRequests();
			break;
		}
	}
}

bool CJPEGProvider::FreeAllPossibleMemory() {
	bool bCouldFreeMemory = false;
	std::list<CImageRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		CImageRequest* pRequest = *iter;
		if (!pRequest->InUse && pRequest->Ready) {
			DeleteElementAt(iter);
			FreeAllPossibleMemory();
			bCouldFreeMemory = true;
			break;
		}
	}
	return bCouldFreeMemory;
}

void CJPEGProvider::FileHasRenamed(LPCTSTR sOldFileName, LPCTSTR sNewFileName) {
	std::list<CImageRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		if (_tcsicmp(sOldFileName, (*iter)->FileName) == 0) {
			(*iter)->FileName = sNewFileName;
		}
	}
}

bool CJPEGProvider::ClearRequest(CJPEGImage* pImage, bool releaseLockedFile) {
	if (pImage == NULL) {
		return false;
	}
	bool bErased = false;
	std::list<CImageRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		if ((*iter)->Image == pImage) {
            if (releaseLockedFile) m_pWorkThreads[0]->ReleaseFile((*iter)->FileName);
			// images that are not ready cannot be removed yet
			if ((*iter)->Ready) {
				DeleteElementAt(iter);
				bErased = true;
			} else {
				(*iter)->Deleted = true;
			}

			break;
		}
	}
	return bErased;
}

void CJPEGProvider::OnJPEGLoaded(int nHandle) {
	std::list<CImageRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		if ((*iter)->Handle == nHandle) {
			GetLoadedImageFromWorkThread(*iter);
			if ((*iter)->Deleted) {
				// this request was deleted, delete image now
				ClearRequest((*iter)->Image);
			}
			break;
		}
	}
}

CJPEGProvider::CImageRequest* CJPEGProvider::FindRequest(LPCTSTR strFileName, int nFrameIndex) {
	std::list<CImageRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		if (_tcsicmp((*iter)->FileName, strFileName) == 0 && (*iter)->FrameIndex == nFrameIndex && !(*iter)->Deleted) {
			return *iter;
		}
	}
	return NULL;
}

CJPEGProvider::CImageRequest* CJPEGProvider::StartRequestAndWaitUntilReady(LPCTSTR sFileName, int nFrameIndex, const CProcessParams & processParams) {
	CImageRequest* pRequest = StartNewRequest(sFileName, nFrameIndex, processParams);
	::WaitForSingleObject(pRequest->EventFinished, INFINITE);
	GetLoadedImageFromWorkThread(pRequest);
	return pRequest;
}

void CJPEGProvider::StartNewRequestBundle(CFileList* pFileList, EReadAheadDirection eDirection, 
										  const CProcessParams & processParams, int nNumRequests, CImageRequest* pLastReadyRequest) {
	if (nNumRequests == 0 || pFileList == NULL) {
		return;
	}
	for (int i = 0; i < nNumRequests; i++) {
        bool bSwitchImage = true;
        int nFrameIndex = (pLastReadyRequest != NULL) ? Helpers::GetFrameIndex(pLastReadyRequest->Image, eDirection == FORWARD, true, bSwitchImage) : 0;
		LPCTSTR sFileName = bSwitchImage ? pFileList->PeekNextPrev(i + 1, eDirection == FORWARD, eDirection == TOGGLE) : pFileList->Current();
		if (sFileName != NULL && FindRequest(sFileName, nFrameIndex) == NULL) {
			if (GetProcessingFlag(PFLAG_NoProcessingAfterLoad, processParams.ProcFlags)) {
				// The read ahead threads need this flag to be deleted - we can speculatively process the image with good hit rate
				CProcessParams paramsCopied = processParams;
				paramsCopied.ProcFlags = SetProcessingFlag(paramsCopied.ProcFlags, PFLAG_NoProcessingAfterLoad, false);
				StartNewRequest(sFileName, nFrameIndex, paramsCopied);
			} else {
				StartNewRequest(sFileName, nFrameIndex, processParams);
			}
		}
	}
}

CJPEGProvider::CImageRequest* CJPEGProvider::StartNewRequest(LPCTSTR sFileName, int nFrameIndex, const CProcessParams & processParams) {
	::OutputDebugString(_T("Start new request: ")); ::OutputDebugString(sFileName); ::OutputDebugString(_T("\n"));
	CImageRequest* pRequest = new CImageRequest(sFileName, nFrameIndex);
	m_requestList.push_back(pRequest);
	pRequest->HandlingThread = SearchThread();
	pRequest->Handle = pRequest->HandlingThread->AsyncLoad(pRequest->FileName, nFrameIndex,
		processParams, m_hHandlerWnd, pRequest->EventFinished);
	pRequest->ReadAhead = true;
	return pRequest;
}

void CJPEGProvider::GetLoadedImageFromWorkThread(CImageRequest* pRequest) {
	if (pRequest->HandlingThread != NULL) {
		::OutputDebugString(_T("Finished request: ")); ::OutputDebugString(pRequest->FileName); ::OutputDebugString(_T("\n"));
        CImageData imageData = pRequest->HandlingThread->GetLoadedImage(pRequest->Handle);
		pRequest->Image = imageData.Image;
		pRequest->OutOfMemory = imageData.IsRequestFailedOutOfMemory;
		pRequest->Ready = true;
		pRequest->HandlingThread = NULL;
	}
}

CImageLoadThread* CJPEGProvider::SearchThread(void) {
	int nSmallestHandle = INT_MAX;
	CImageLoadThread* pBestOccupiedThread = NULL;
	for (int i = 0; i < m_nNumThread; i++) {
		bool bFree = true;
		CImageLoadThread* pThisThread = m_pWorkThreads[i];
		std::list<CImageRequest*>::iterator iter;
		for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
			if ((*iter)->Handle < nSmallestHandle && (*iter)->HandlingThread != NULL) {
				nSmallestHandle = (*iter)->Handle;
				pBestOccupiedThread = (*iter)->HandlingThread;
			}
			if ((*iter)->HandlingThread == pThisThread) {
				bFree = false;
				break;
			}
		}
		if (bFree) {
			return pThisThread;
		}
	}
	// all threads are occupied
	return (pBestOccupiedThread == NULL) ? m_pWorkThreads[0] : pBestOccupiedThread;
}

void CJPEGProvider::RemoveUnusedImages(bool bRemoveAlsoReadAhead) {
	bool bRemoved = false;
	int nTimeStampToRemove = -2;
	do {
		bRemoved = false;
		int nSmallestTimeStamp = INT_MAX;
		std::list<CImageRequest*>::iterator iter;
		for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
			if ((*iter)->InUse == false && (*iter)->Ready && ((*iter)->ReadAhead == false || bRemoveAlsoReadAhead || IsDestructivelyProcessed((*iter)->Image))) {
				// search element with smallest timestamp
				if ((*iter)->AccessTimeStamp < nSmallestTimeStamp) {
					nSmallestTimeStamp = (*iter)->AccessTimeStamp;
				}
				// remove the readahead images - if we get here with read ahead, the strategy was wrong and
				// the read ahead image is not used.
				if ((*iter)->AccessTimeStamp == nTimeStampToRemove || IsDestructivelyProcessed((*iter)->Image)) {
					::OutputDebugString(_T("Delete request: ")); ::OutputDebugString((*iter)->FileName); ::OutputDebugString(_T("\n"));
					DeleteElementAt(iter);
					bRemoved = true;
					break;
				}
			}
		}
		nTimeStampToRemove = -2;
		// Make one buffer free for next readahead (except when bRemoveAlsoReadAhead)
		int nMaxListSize = bRemoveAlsoReadAhead ? (unsigned int)m_nNumBuffers : (unsigned int)m_nNumBuffers - 1;
		if (m_requestList.size() > (unsigned int)nMaxListSize) {
			// remove element with smallest timestamp
			if (nSmallestTimeStamp < INT_MAX) {
				bRemoved = true;
				nTimeStampToRemove = nSmallestTimeStamp;
			}
		}
	} while (bRemoved); // repeat until no element could be removed anymore
}

void CJPEGProvider::ClearOldestReadAhead() {
	if (m_requestList.size() >= (unsigned int)m_nNumBuffers) {
		int nFirstHandle = INT_MAX;
		CImageRequest* pFirstRequest = NULL;
		std::list<CImageRequest*>::iterator iter;
		for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
			if ((*iter)->ReadAhead) {
				// mark very old requests for removal
				if (CImageLoadThread::GetCurHandleValue() - (*iter)->Handle > m_nNumBuffers) {
					(*iter)->ReadAhead = false;
				}
				if ((*iter)->Handle < nFirstHandle) {
					nFirstHandle = (*iter)->Handle;
					pFirstRequest = *iter;
				}
			}
		}
		if (pFirstRequest != NULL) {
			pFirstRequest->ReadAhead = false;
			ClearOldestReadAhead();
		}
	}
}

void CJPEGProvider::DeleteElementAt(std::list<CImageRequest*>::iterator iteratorAt) {
	delete (*iteratorAt)->Image;
	delete *iteratorAt;
	m_requestList.erase(iteratorAt);
}

void CJPEGProvider::DeleteElement(CImageRequest* pRequest) {
	delete pRequest->Image;
	delete pRequest;
	m_requestList.remove(pRequest);
}

bool CJPEGProvider::IsDestructivelyProcessed(CJPEGImage* pImage) {
	return pImage != NULL && pImage->IsDestructivlyProcessed();
}