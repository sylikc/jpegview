#include "StdAfx.h"
#include "Clipboard.h"
#include "JPEGImage.h"
#include "Helpers.h"
#include "BasicProcessing.h"

void CClipboard::CopyImageToClipboard(HWND hWnd, CJPEGImage * pImage) {
	if (pImage == NULL || pImage->DIBPixelsLastProcessed() == NULL) {
		return;
	}

	int nOldRegion = pImage->GetDimBitmapRegion();
	pImage->SetDimBitmapRegion(0);
	DoCopy(hWnd, pImage->DIBWidth(), pImage->DIBHeight(), !pImage->GetFlagFlipped(), pImage->DIBPixelsLastProcessed());
	pImage->SetDimBitmapRegion(nOldRegion);
}

void CClipboard::CopyFullImageToClipboard(HWND hWnd, CJPEGImage * pImage, const CImageProcessingParams& procParams,
										  EProcessingFlags eFlags) {
	if (pImage == NULL) {
		return;
	}

	int nOldRegion = pImage->GetDimBitmapRegion();
	pImage->SetDimBitmapRegion(0);
	CSize fullImageSize = CSize(pImage->OrigWidth(), pImage->OrigHeight());
	void* pDIB = pImage->GetDIB(fullImageSize, fullImageSize, CPoint(0, 0), procParams, eFlags);
	DoCopy(hWnd, pImage->OrigWidth(), pImage->OrigHeight(), !pImage->GetFlagFlipped(), pDIB);
	pImage->SetDimBitmapRegion(nOldRegion);
}

void CClipboard::DoCopy(HWND hWnd, int nWidth, int nHeight, bool bFlip, const void* pSourceImageDIB32) {
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
	CBasicProcessing::Convert32bppTo24bppDIB(nWidth, nHeight, pDIBPixelsTarget, pSourceImageDIB32, bFlip);

	::GlobalUnlock(hMem); 
	::SetClipboardData(CF_DIB, hMem); 

	::CloseClipboard();
}