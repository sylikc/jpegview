
#include "StdAfx.h"
#include "ImageLoadThread.h"
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
volatile LONG CImageLoadThread::m_curHandle = 0;

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

CImageLoadThread::CImageLoadThread(void) {
}

CImageLoadThread::~CImageLoadThread(void) {
}

int CImageLoadThread::AsyncLoad(LPCTSTR strFileName, const CProcessParams & processParams, HWND targetWnd, HANDLE eventFinished) {
	CRequest* pRequest = new CRequest(strFileName, targetWnd, processParams, eventFinished);

	ProcessAsync(pRequest);

	return pRequest->RequestHandle;
}

CJPEGImage* CImageLoadThread::GetLoadedImage(int nHandle) {
	// do not delete anything here, only mark as deleted
	::EnterCriticalSection(&m_csList);
	CJPEGImage* imageFound = NULL;
	std::list<CRequestBase*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		CRequest* pRequest = (CRequest*)(*iter);
		if (pRequest->Processed && pRequest->Deleted == false && pRequest->RequestHandle == nHandle) {
			imageFound = pRequest->Image;
			pRequest->Deleted = true;
			break;
		}
	}
	::LeaveCriticalSection(&m_csList);
	return imageFound;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Protected
/////////////////////////////////////////////////////////////////////////////////////////////

void CImageLoadThread::ProcessRequest(CRequestBase& request) {
	CRequest& rq = (CRequest&)request;
	double dStartTime = Helpers::GetExactTickCount(); 
	// Get image format and read the image
	switch (GetImageFormat(rq.FileName)) {
		case CJPEGImage::IF_JPEG :
			ProcessReadJPEGRequest(&rq);
			break;
		case CJPEGImage::IF_WindowsBMP :
			ProcessReadBMPRequest(&rq);
			break;
		default:
			// try with GDI+
			ProcessReadGDIPlusRequest(&rq);
			break;
	}
	// then process the image if read was successful
	if (rq.Image != NULL) {
		rq.Image->SetLoadTickCount(Helpers::GetExactTickCount() - dStartTime); 
		ProcessImageAfterLoad(&rq);
	}
}

void CImageLoadThread::AfterFinishProcess(CRequestBase& request) {
	CRequest& rq = (CRequest&)request;
	if (rq.TargetWnd != NULL) {
		// post message to window that request has been processed
		::PostMessage(rq.TargetWnd, WM_JPEG_LOAD_COMPLETED, 0, rq.RequestHandle);
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

void CImageLoadThread::ProcessReadJPEGRequest(CRequest * request) {

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
				uint8* pEXIFBlock = (uint8*)Helpers::FindJPEGMarker(pBuffer, nFileSize, 0xE1);
				if (pEXIFBlock != NULL && strncmp((const char*)(pEXIFBlock + 4), "Exif", 4) != 0) {
					pEXIFBlock = NULL;
				}

				/*
				double dStartTime = Helpers::GetExactTickCount();
				int16* pGray = CBasicProcessing::Create1Channel16bppGrayscaleImage(nWidth, nHeight, pPixelData, nBPP);
				int16* pSmoothed = CBasicProcessing::GaussFilter16bpp1Channel(CSize(nWidth, nHeight), CPoint(0, 0), CSize(nWidth, nHeight), 2.1, pGray);
				//pPixelData = (char*)CBasicProcessing::Convert16bppGrayTo32bppDIB(cropRect.cx, cropRect.cy, pSmoothed);
				CBasicProcessing::UnsharpMask(CSize(nWidth, nHeight), CPoint(0,0), CSize(nWidth, nHeight), 3.0, 4, pGray, pSmoothed, pPixelData, pPixelData, nBPP);
				delete[] pGray;
				delete[] pSmoothed;

				double dTime = Helpers::GetExactTickCount() - dStartTime;

				request->Image = new CJPEGImage(nWidth, nHeight, pPixelData, 
					pEXIFBlock, nBPP, 
					Helpers::CalculateJPEGFileHash(pBuffer, nFileSize), CJPEGImage::IF_JPEG);

				request->Image->SetUnsharpMaskTickCount(dTime);
				*/
				
				request->Image = new CJPEGImage(nWidth, nHeight, pPixelData, 
					pEXIFBlock, nBPP, 
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

void CImageLoadThread::ProcessReadBMPRequest(CRequest * request) {
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

void CImageLoadThread::ProcessReadGDIPlusRequest(CRequest * request) {
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
		// If there is an alpha channel in the original file we must blit the image onto a background color offscreen
		// bitmap first to archieve proper rendering.
		Gdiplus::PixelFormat pixelFormat = pBitmap->GetPixelFormat();
		bool bHasAlphaChannel = (pixelFormat & (PixelFormatAlpha | PixelFormatPAlpha));
		Gdiplus::Bitmap* pBmTarget = NULL;
		Gdiplus::Graphics* pBmGraphics = NULL;
		Gdiplus::Bitmap* pBitmapToUse;
		if (bHasAlphaChannel) {
			pBmTarget = new Gdiplus::Bitmap(pBitmap->GetWidth(), pBitmap->GetHeight(), PixelFormat32bppRGB);
			pBmGraphics = new Gdiplus::Graphics(pBmTarget);
			COLORREF bkColor = CSettingsProvider::This().ColorBackground();
			Gdiplus::SolidBrush bkBrush(Gdiplus::Color(GetRValue(bkColor), GetGValue(bkColor), GetBValue(bkColor)));
			pBmGraphics->FillRectangle(&bkBrush, 0, 0, pBmTarget->GetWidth(), pBmTarget->GetHeight());
			pBmGraphics->DrawImage(pBitmap, 0, 0, pBmTarget->GetWidth(), pBmTarget->GetHeight());
			pBitmapToUse = pBmTarget;
		} else {
			pBitmapToUse = pBitmap;
		}

		Gdiplus::Rect bmRect(0, 0, pBitmap->GetWidth(), pBitmap->GetHeight());
		Gdiplus::BitmapData bmData;
		if (pBitmapToUse->LockBits(&bmRect, Gdiplus::ImageLockModeRead, PixelFormat32bppRGB, &bmData) == Gdiplus::Ok) {
			assert(bmData.PixelFormat == PixelFormat32bppRGB);
			request->Image = new CJPEGImage(bmRect.Width, bmRect.Height, 
				CBasicProcessing::ConvertGdiplus32bppRGB(bmRect.Width, bmRect.Height, bmData.Stride, bmData.Scan0), 
				NULL, 4, 0, GetBitmapFormat(pBitmap));
			pBitmapToUse->UnlockBits(&bmData);
		}

		if (pBmGraphics != NULL && pBmTarget != NULL) {
			delete pBmGraphics;
			delete pBmTarget;
		}
	}
	delete pBitmap;
	
}

void CImageLoadThread::ProcessImageAfterLoad(CRequest * request) {
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
			request->ProcessParams.TargetWidth, request->ProcessParams.TargetHeight, request->ProcessParams.AutoZoomMode, dZoom);
	} else {
		newSize = CSize((int)(nWidth*dZoom + 0.5), (int)(nHeight*dZoom + 0.5));
	}

	newSize.cx = max(1, min(65535, newSize.cx));
	newSize.cy = max(1, min(65535, newSize.cy)); // max size must not be bigger than this after zoom

	// clip to target rectangle
	CSize clippedSize(min(request->ProcessParams.TargetWidth, newSize.cx), 
		min(request->ProcessParams.TargetHeight, newSize.cy));

	LimitOffsets(request->ProcessParams.Offsets, CSize(request->ProcessParams.TargetWidth, request->ProcessParams.TargetHeight), newSize);

	// this will process the image
	CPoint offsetInImage = request->Image->ConvertOffset(newSize, clippedSize, request->ProcessParams.Offsets);
	request->Image->GetDIB(newSize, clippedSize, offsetInImage,
		request->ProcessParams.ImageProcParams, request->ProcessParams.ProcFlags);
}
