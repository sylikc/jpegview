
#include "StdAfx.h"
#include "WorkThread.h"
#include <process.h>
#include <gdiplus.h>
#include "JPEGImage.h"
#include "IJLWrapper.h"
#include "MessageDef.h"
#include "Helpers.h"
#include "SettingsProvider.h"
#include "ReaderBMP.h"
#include "BasicProcessing.h"

using namespace Gdiplus;

// static intializers
volatile LONG CWorkThread::m_curHandle = 0;

/////////////////////////////////////////////////////////////////////////////////////////////
// static helpers
/////////////////////////////////////////////////////////////////////////////////////////////

// find image format of this image by reading some header bytes
static CJPEGImage::EImageFormat GetImageFormat(LPCTSTR sFileName) {
	FILE *fptr;
	if ((fptr = _tfopen(sFileName, _T("rb"))) == NULL) {
		return CJPEGImage::IF_Unknown;
	}
	unsigned char header[8];
	int nSize = fread((void*)header, 1, 8, fptr);
	fclose(fptr);
	if (nSize < 2) {
		return CJPEGImage::IF_Unknown;
	}
	if (header[0] == 0x42 && header[1] == 0x4d) {
		return CJPEGImage::IF_WindowsBMP;
	} else if (header[0] == 0xff && header[1] == 0xd8) {
		return CJPEGImage::IF_JPEG;
	} else {
		return CJPEGImage::IF_Unknown;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Public
/////////////////////////////////////////////////////////////////////////////////////////////

CWorkThread::CWorkThread(void) {
	m_bTerminate = false;
	memset(&m_csList, 0, sizeof(CRITICAL_SECTION));
	::InitializeCriticalSection(&m_csList);
	m_wakeUp = ::CreateEvent(0, TRUE, FALSE, NULL);

	m_hThread = (HANDLE)_beginthread(ThreadFunc, 0, this);
}

CWorkThread::~CWorkThread(void) {
	if (!m_bTerminate) {
		Terminate();
	}
	::DeleteCriticalSection(&m_csList);
	::CloseHandle(m_wakeUp);
}

int CWorkThread::AsyncLoad(LPCTSTR strFileName, const CProcessParams & processParams, HWND targetWnd, HANDLE eventFinished) {
	CRequest* pRequest = new CRequest(strFileName, targetWnd, processParams, eventFinished);

	::EnterCriticalSection(&m_csList);
	m_requestList.push_back(pRequest);
	::LeaveCriticalSection(&m_csList);

	::SetEvent(m_wakeUp);

	return pRequest->RequestHandle;
}

CJPEGImage* CWorkThread::GetLoadedImage(int nHandle) {
	// do not delete anything here, only mark as deleted
	::EnterCriticalSection(&m_csList);
	CJPEGImage* imageFound = NULL;
	std::list<CRequest*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		if ((*iter)->Processed && (*iter)->Deleted == false && (*iter)->RequestHandle == nHandle) {
			imageFound = (*iter)->Image;
			(*iter)->Deleted = true;
			break;
		}
	}
	::LeaveCriticalSection(&m_csList);
	return imageFound;
}

void CWorkThread::Terminate() { 
	m_bTerminate = true;
	if (m_hThread != NULL) {
		::SetEvent(m_wakeUp);
		::WaitForSingleObject(m_hThread, 10000);
		m_hThread = NULL;
	}
}

void CWorkThread::Abort() {
	try {
		if (m_hThread != NULL) {
			::SetEvent(m_wakeUp);
			if (WAIT_TIMEOUT == ::WaitForSingleObject(m_hThread, 100)) {
				::TerminateThread(m_hThread, 1);
				::WaitForSingleObject(m_hThread, 100);
			}
			m_hThread = NULL;
		}
	} catch (...) {
		// do not crash
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Private
/////////////////////////////////////////////////////////////////////////////////////////////

static void LimitOffsets(CPoint& offsets, CSize clippingSize, const CSize & imageSize) {
	int nMaxOffsetX = (imageSize.cx - clippingSize.cx)/2;
	nMaxOffsetX = max(0, nMaxOffsetX);
	int nMaxOffsetY = (imageSize.cy - clippingSize.cy)/2;
	nMaxOffsetY = max(0, nMaxOffsetY);
	offsets.x = max(-nMaxOffsetX, min(+nMaxOffsetX, offsets.x));
	offsets.y = max(-nMaxOffsetY, min(+nMaxOffsetY, offsets.y));
}

void CWorkThread::ThreadFunc(void* arg) {

	CWorkThread* thisPtr = (CWorkThread*) arg;
	do {
		::EnterCriticalSection(&thisPtr->m_csList);

		// Delete the requests marked as deleted from request queue
		DeleteMarkedRequests(thisPtr);

		// search a request that is not yet processed
		CRequest* requestHandled = NULL;
		int nNumRequests = 0;
		std::list<CRequest*>::iterator iter;
		for (iter = thisPtr->m_requestList.begin( ); iter != thisPtr->m_requestList.end( ); iter++ ) {
			if ((*iter)->Processed == false) {
				requestHandled = *iter;
				nNumRequests++;
			}
		}
		::LeaveCriticalSection(&thisPtr->m_csList);

		// process this request
		if (requestHandled != NULL) {
			// Get image format and read the image
			switch (GetImageFormat(requestHandled->FileName)) {
				case CJPEGImage::IF_JPEG :
					ProcessReadJPEGRequest(requestHandled);
					break;
				case CJPEGImage::IF_WindowsBMP :
					ProcessReadBMPRequest(requestHandled);
					break;
				default:
					// try with GDI+
					ProcessReadGDIPlusRequest(requestHandled);
					break;
			}
			// then process the image if read was successful
			if (requestHandled->Image != NULL) {
				ProcessImageAfterLoad(requestHandled);
			}
			requestHandled->Processed = true;

			// signal end of processing
			if (requestHandled->EventFinished != NULL) {
				::SetEvent(requestHandled->EventFinished);
			}
			if (!thisPtr->m_bTerminate) {
				::PostMessage(requestHandled->TargetWnd, WM_JPEG_LOAD_COMPLETED, 0, requestHandled->RequestHandle);
			}
			nNumRequests--;
		}

		// if there are no more requests, sleep until woke up
		if (nNumRequests == 0 && !thisPtr->m_bTerminate) {
			::WaitForSingleObject(thisPtr->m_wakeUp, INFINITE);
			::ResetEvent(thisPtr->m_wakeUp);
		}
	} while (!thisPtr->m_bTerminate);
	_endthread();
}

void CWorkThread::ProcessReadJPEGRequest(CRequest * request) {

	const unsigned int MAX_JPEG_FILE_SIZE = 1024*1024*50; // 50 MB

	HANDLE hFile = ::CreateFile(request->FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return;
	}

	char* pBuffer = NULL;
	try {
		// Don't read too huge files...
		unsigned int nFileSize = ::GetFileSize(hFile, NULL);
		if (nFileSize > MAX_JPEG_FILE_SIZE) {
			::CloseHandle(hFile);
			return;
		}
		unsigned int nNumBytesRead;
		pBuffer = new char[nFileSize];
		if (::ReadFile(hFile, pBuffer, nFileSize, (LPDWORD) &nNumBytesRead, NULL) && nNumBytesRead == nFileSize) {
			int nWidth, nHeight, nBPP;
			void* pPixelData = Jpeg::ReadImage(nWidth, nHeight, nBPP, pBuffer, nFileSize);
			// Color and b/w JPEG is supported
			if (pPixelData != NULL && (nBPP == 3 || nBPP == 1)) {
				request->Image = new CJPEGImage(nWidth, nHeight, pPixelData, 
					Helpers::FindJPEGMarker(pBuffer, nFileSize, 0xE1), nBPP, 
					Helpers::CalculateJPEGFileHash(pBuffer, nFileSize), CJPEGImage::IF_JPEG);
			} else {
				// failed, try GDI+
				delete[] pPixelData;
				ProcessReadGDIPlusRequest(request);
			}
		}
	} catch (...) {
		delete request->Image;
		request->Image = NULL;
	}
	::CloseHandle(hFile);
	delete[] pBuffer;
}

void CWorkThread::ProcessReadBMPRequest(CRequest * request) {
	request->Image = CReaderBMP::ReadBmpImage(request->FileName);
	if (request->Image == NULL) {
		// probabely one of the bitmap formats that can not be read directly, try with GDI+
		ProcessReadGDIPlusRequest(request);
	}
}

static CJPEGImage::EImageFormat GetBitmapFormat(Gdiplus::Bitmap * pBitmap) {
	GUID guid;
	memset(&guid, 0, sizeof(GUID));
	pBitmap->GetRawFormat(&guid);
	if (guid == Gdiplus::ImageFormatBMP) {
		return CJPEGImage::IF_WindowsBMP;
	} else if (guid == Gdiplus::ImageFormatPNG) {
		return CJPEGImage::IF_PNG;
	} else if (guid == Gdiplus::ImageFormatGIF) {
		return CJPEGImage::IF_GIF;
	} else if (guid == Gdiplus::ImageFormatTIFF) {
		return CJPEGImage::IF_TIFF;
	} else if (guid == Gdiplus::ImageFormatJPEG || guid == Gdiplus::ImageFormatEXIF) {
		return CJPEGImage::IF_JPEG;
	} else {
		return CJPEGImage::IF_Unknown;
	}
}

void CWorkThread::ProcessReadGDIPlusRequest(CRequest * request) {
	const wchar_t* sFileName;
#ifdef _UNICODE
	sFileName = (const wchar_t*)request->FileName;
#else
	wchar_t buff[MAX_PATH];
	size_t nDummy;
	mbstowcs_s(&nDummy, buff, MAX_PATH, (const char*)request->FileName, request->FileName.GetLength());
	sFileName = (const wchar_t*)buff;
#endif

	Gdiplus::Bitmap* pBitmap = new Gdiplus::Bitmap(sFileName);
	if (pBitmap->GetLastStatus() == Gdiplus::Ok) {
		Gdiplus::Rect bmRect(0, 0, pBitmap->GetWidth(), pBitmap->GetHeight());
		Gdiplus::BitmapData bmData;
		if (pBitmap->LockBits(&bmRect, Gdiplus::ImageLockModeRead, PixelFormat32bppRGB, &bmData) == Gdiplus::Ok) {
			assert(bmData.PixelFormat == PixelFormat32bppRGB);
			request->Image = new CJPEGImage(bmRect.Width, bmRect.Height, 
				CBasicProcessing::ConvertGdiplus32bppRGB(bmRect.Width, bmRect.Height, bmData.Stride, bmData.Scan0), 
				NULL, 4, 0, GetBitmapFormat(pBitmap));
			pBitmap->UnlockBits(&bmData);
		}
	}
	delete pBitmap;
}

void CWorkThread::DeleteMarkedRequests(CWorkThread* thisPtr) {
	bool bDeleted = false;
	std::list<CRequest*>::iterator iter;
	for (iter = thisPtr->m_requestList.begin( ); iter != thisPtr->m_requestList.end( ); iter++ ) {
		if ((*iter)->Deleted) {
			delete *iter;
			thisPtr->m_requestList.erase(iter);
			bDeleted = true;
			break;
		}
	}
	if (bDeleted) {
		DeleteMarkedRequests(thisPtr);
	}
}

void CWorkThread::ProcessImageAfterLoad(CRequest * request) {
	// set process parameters depending on filename
	request->Image->SetFileDependentProcessParams(request->FileName, &(request->ProcessParams));

	// First do rotation, this maybe modifies the width and height
	request->Image->VerifyRotation(request->ProcessParams.Rotation);
	int nWidth = request->Image->OrigWidth();
	int nHeight = request->Image->OrigHeight();

	double dZoom = request->ProcessParams.Zoom;
	CSize newSize;
	if (dZoom < 0.0) {
		newSize = Helpers::GetImageRect(nWidth, nHeight, 
			request->ProcessParams.TargetWidth, request->ProcessParams.TargetHeight, request->ProcessParams.AutoZoomMode);
	} else {
		newSize = CSize((int)(nWidth*dZoom + 0.5), (int)(nHeight*dZoom + 0.5));
	}

	newSize.cx = min(65535, newSize.cx);
	newSize.cy = min(65535, newSize.cy); // max size must not be bigger than this after zoom

	// clip to target rectangle
	CSize clippedSize(min(request->ProcessParams.TargetWidth, newSize.cx), 
		min(request->ProcessParams.TargetHeight, newSize.cy));

	LimitOffsets(request->ProcessParams.Offsets, CSize(request->ProcessParams.TargetWidth, request->ProcessParams.TargetHeight), newSize);

	// this will process the image
	CPoint offsetInImage = request->Image->ConvertOffset(newSize, clippedSize, request->ProcessParams.Offsets);
	request->Image->GetDIB(newSize, clippedSize, offsetInImage,
		request->ProcessParams.ImageProcParams, request->ProcessParams.ProcFlags);
}
