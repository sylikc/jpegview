#include "StdAfx.h"
#include "JPEGProvider.h"
#include "JPEGImage.h"
#include "WorkThread.h"
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
	m_pWorkThreads = new CWorkThread*[nNumThreads];
	for (int i = 0; i < nNumThreads; i++) {
		m_pWorkThreads[i] = new CWorkThread();
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
									   LPCTSTR strFileName, const CProcessParams & processParams) {
	if (strFileName == NULL) {
		return NULL;
	}

	// Search if we have the requested image already present or in progress
	CImageRequest* pRequest = FindRequest(strFileName);
	bool bDirectionChanged = eDirection != m_eOldDirection;
	bool bRemoveAlsoReadAhead = bDirectionChanged;
	m_eOldDirection = eDirection;

	if (pRequest == NULL) {
		// no, request never sent for this file, add to request queue and start async
		pRequest = StartNewRequest(strFileName, processParams);
		// wait with read ahead when direction changed - maybe user just wants to resee last image
		if (!bDirectionChanged) {
			// start parallel if more than one thread
			StartNewRequestBundle(pFileList, eDirection, processParams, m_nNumThread - 1);
		}
	}

	// wait for request if not yet ready
	if (!pRequest->Ready) {
		::OutputDebugStr(_T("Waiting for request: ")); ::OutputDebugStr(pRequest->FileName); ::OutputDebugStr(_T("\n"));
		::WaitForSingleObject(pRequest->EventFinished, INFINITE);
		GetLoadedImageFromWorkThread(pRequest);
	} else {
		CJPEGImage* pImage = pRequest->Image;
		if (pImage != NULL) {
			// make sure the intitial parameters are reset as when keep params was on before they are wrong
			EProcessingFlags procFlags = processParams.ProcFlags;
			pImage->RestoreInitialParameters(strFileName, processParams.ImageProcParams, procFlags, 
				processParams.Rotation, processParams.Zoom, processParams.Offsets, 
				CSize(processParams.TargetWidth, processParams.TargetHeight));
		}
		::OutputDebugStr(_T("Found in cache: ")); ::OutputDebugStr(pRequest->FileName); ::OutputDebugStr(_T("\n"));
	}

	// set before removing unused images!
	pRequest->InUse = true;
	pRequest->AccessTimeStamp = m_nCurrentTimeStamp++;

	// cleanup stuff no longer used
	RemoveUnusedImages(bRemoveAlsoReadAhead);
	ClearOldestReadAhead();

	// check if we shall start new requests
	if (m_requestList.size() < (unsigned int)m_nNumBuffers && !bDirectionChanged) {
		StartNewRequestBundle(pFileList, eDirection, processParams, m_nNumThread);
	}

	return pRequest->Image;
}

void CJPEGProvider::NotifyNotUsed(CJPEGImage* pImage) {
	if (pImage == NULL) {
		return;
	}
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
	delete pImage;
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

void CJPEGProvider::FileHasRenamed(LPCTSTR sOldFileName, LPCTSTR sNewFileName) {
	std::list<CImageRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		if (_tcsicmp(sOldFileName, (*iter)->FileName) == 0) {
			(*iter)->FileName = sNewFileName;
		}
	}
}

bool CJPEGProvider::ClearRequest(CJPEGImage* pImage) {
	if (pImage == NULL) {
		return false;
	}
	bool bErased = false;
	std::list<CImageRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		if ((*iter)->Image == pImage) {
			// images that are not ready cannot be removed yet
			if ((*iter)->Ready) {
				delete (*iter)->Image;
				delete *iter;
				m_requestList.erase(iter);
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

CJPEGProvider::CImageRequest* CJPEGProvider::FindRequest(LPCTSTR strFileName) {
	std::list<CImageRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		if (_tcsicmp((*iter)->FileName, strFileName) == 0 && !(*iter)->Deleted) {
			return *iter;
		}
	}
	return NULL;
}

void CJPEGProvider::StartNewRequestBundle(CFileList* pFileList, EReadAheadDirection eDirection, 
										  const CProcessParams & processParams, int nNumRequests) {
	if (nNumRequests == 0 || pFileList == NULL) {
		return;
	}
	for (int i = 0; i < nNumRequests; i++) {
		LPCTSTR sFileName = pFileList->PeekNextPrev(i + 1, eDirection == FORWARD);
		if (sFileName != NULL && FindRequest(sFileName) == NULL) {
			StartNewRequest(sFileName, processParams);
		}
	}
}

CJPEGProvider::CImageRequest* CJPEGProvider::StartNewRequest(LPCTSTR sFileName, const CProcessParams & processParams) {
	::OutputDebugStr(_T("Start new request: ")); ::OutputDebugStr(sFileName); ::OutputDebugStr(_T("\n"));
	CImageRequest* pRequest = new CImageRequest(sFileName);
	m_requestList.push_back(pRequest);
	pRequest->HandlingThread = SearchThread();
	pRequest->Handle = pRequest->HandlingThread->AsyncLoad(pRequest->FileName, 
		processParams, m_hHandlerWnd, pRequest->EventFinished);
	pRequest->ReadAhead = true;
	return pRequest;
}

void CJPEGProvider::GetLoadedImageFromWorkThread(CImageRequest* pRequest) {
	if (pRequest->HandlingThread != NULL) {
		::OutputDebugStr(_T("Finished request: ")); ::OutputDebugStr(pRequest->FileName); ::OutputDebugStr(_T("\n"));
		pRequest->Image = pRequest->HandlingThread->GetLoadedImage(pRequest->Handle);
		pRequest->Ready = true;
		pRequest->HandlingThread = NULL;
	}
}

CWorkThread* CJPEGProvider::SearchThread(void) {
	int nSmallestHandle = INT_MAX;
	CWorkThread* pBestOccupiedThread = NULL;
	for (int i = 0; i < m_nNumThread; i++) {
		bool bFree = true;
		CWorkThread* pThisThread = m_pWorkThreads[i];
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
			if ((*iter)->InUse == false && (*iter)->Ready && ((*iter)->ReadAhead == false || bRemoveAlsoReadAhead)) {
				// search element with smallest timestamp
				if ((*iter)->AccessTimeStamp < nSmallestTimeStamp) {
					nSmallestTimeStamp = (*iter)->AccessTimeStamp;
				}
				// remove the readahead images - if we get here with read ahead, the strategy was wrong and
				// the read ahead image is not used.
				if ((*iter)->AccessTimeStamp == nTimeStampToRemove) {
					::OutputDebugStr(_T("Delete request: ")); ::OutputDebugStr((*iter)->FileName); ::OutputDebugStr(_T("\n"));
					delete (*iter)->Image;
					delete *iter;
					m_requestList.erase(iter);
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
				if (CWorkThread::GetCurHandleValue() - (*iter)->Handle > m_nNumBuffers) {
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
