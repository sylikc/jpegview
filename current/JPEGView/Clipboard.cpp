#include "StdAfx.h"
#include "Clipboard.h"
#include "JPEGImage.h"
#include "Helpers.h"
#include "BasicProcessing.h"
#include <gdiplus.h>

void CClipboard::CopyImageToClipboard(HWND hWnd, CJPEGImage * pImage) {
	if (pImage == NULL || pImage->DIBPixelsLastProcessed(true) == NULL) {
		return;
	}

	pImage->EnableDimming(false);
	DoCopy(hWnd, pImage->DIBWidth(), pImage->DIBHeight(), pImage->DIBPixelsLastProcessed(true));
	pImage->EnableDimming(true);
}

void CClipboard::CopyFullImageToClipboard(HWND hWnd, CJPEGImage * pImage, const CImageProcessingParams& procParams,
										  EProcessingFlags eFlags) {
	CopyFullImageToClipboard(hWnd, pImage, procParams, eFlags, CRect(0, 0, pImage->OrigWidth(), pImage->OrigHeight()));
}

void CClipboard::CopyFullImageToClipboard(HWND hWnd, CJPEGImage * pImage, const CImageProcessingParams& procParams,
										  EProcessingFlags eFlags, CRect clipRect) {
	if (pImage == NULL) {
		return;
	}

	clipRect.left = max(0, clipRect.left);
	clipRect.top = max(0, clipRect.top);
	clipRect.right = min(pImage->OrigWidth(), clipRect.right);
	clipRect.bottom = min(pImage->OrigHeight(), clipRect.bottom);

	pImage->EnableDimming(false);
	void* pDIB = pImage->GetDIB(pImage->OrigSize(), clipRect.Size(), clipRect.TopLeft(), procParams, eFlags);
	DoCopy(hWnd, clipRect.Width(), clipRect.Height(), pDIB);
	pImage->EnableDimming(true);
}

CJPEGImage* CClipboard::PasteImageFromClipboard(HWND hWnd, const CImageProcessingParams& procParams, 
												EProcessingFlags eFlags) {
	if (!::OpenClipboard(hWnd)) {
        return NULL;
	}

	CJPEGImage* pImage = NULL;
	HANDLE handle = ::GetClipboardData(CF_DIB);
	if (handle != NULL) {
		BITMAPINFO* pbmInfo = (BITMAPINFO*)::GlobalLock(handle);
		if (pbmInfo != NULL) {
			int nNumColors = pbmInfo->bmiHeader.biClrUsed;
			if (nNumColors == 0 && pbmInfo->bmiHeader.biBitCount <= 8) {
				nNumColors = 1 << pbmInfo->bmiHeader.biBitCount;
			}
			if (pbmInfo->bmiHeader.biCompression == BI_BITFIELDS) {
				nNumColors = 3;
			}
			char* pDIBBits = (char*)pbmInfo + pbmInfo->bmiHeader.biSize + nNumColors*sizeof(RGBQUAD);

			Gdiplus::Bitmap* pBitmap = new Gdiplus::Bitmap(pbmInfo, pDIBBits);
			if (pBitmap->GetLastStatus() == Gdiplus::Ok) {
				Gdiplus::Rect bmRect(0, 0, pBitmap->GetWidth(), pBitmap->GetHeight());
				Gdiplus::BitmapData bmData;
				if (pBitmap->LockBits(&bmRect, Gdiplus::ImageLockModeRead, PixelFormat32bppRGB, &bmData) == Gdiplus::Ok) {
					assert(bmData.PixelFormat == PixelFormat32bppRGB);
					void* pDIB = CBasicProcessing::ConvertGdiplus32bppRGB(bmRect.Width, bmRect.Height, bmData.Stride, bmData.Scan0);
					pImage = (pDIB == NULL) ? NULL : new CJPEGImage(bmRect.Width, bmRect.Height, pDIB, NULL, 4, 0, IF_CLIPBOARD, false, 0, 1, 0);
					pBitmap->UnlockBits(&bmData);
				}
			}
			delete pBitmap;

			::GlobalUnlock(handle);
		}
	}

	::CloseClipboard();

	return pImage;
}


void CClipboard::DoCopy(HWND hWnd, int nWidth, int nHeight, const void* pSourceImageDIB32) {
	if (!::OpenClipboard(hWnd)) {
        return;
	}
	::EmptyClipboard();

	// get needed size of memory block
	uint32 nSizeLinePadded = Helpers::DoPadding(nWidth*3, 4);
	uint32 nSizeBytes = sizeof(BITMAPINFO) + nSizeLinePadded * nHeight;
	
	// Allocate memory
	HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, nSizeBytes);
	if (hMem == NULL) {
		::CloseClipboard();
		return;
	}
	void* pMemory = ::GlobalLock(hMem); 

	BITMAPINFO* pBMInfo = (BITMAPINFO*) pMemory;
	memset(pBMInfo, 0, sizeof(BITMAPINFO));

	pBMInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pBMInfo->bmiHeader.biWidth = nWidth;
	pBMInfo->bmiHeader.biHeight = nHeight;
	pBMInfo->bmiHeader.biPlanes = 1;
	pBMInfo->bmiHeader.biBitCount = 24;
	pBMInfo->bmiHeader.biCompression = BI_RGB;
	pBMInfo->bmiHeader.biXPelsPerMeter = 10000;
	pBMInfo->bmiHeader.biYPelsPerMeter = 10000;
	pBMInfo->bmiHeader.biClrUsed = 0;

	uint8* pDIBPixelsTarget = (uint8*)pMemory + sizeof(BITMAPINFO) - sizeof(RGBQUAD);
	CBasicProcessing::Convert32bppTo24bppDIB(nWidth, nHeight, pDIBPixelsTarget, pSourceImageDIB32, true);

	::GlobalUnlock(hMem); 
	::SetClipboardData(CF_DIB, hMem); 

	::CloseClipboard();
}