#pragma once

#include "ProcessParams.h"

class CJPEGImage;

// Allows to copy an image to the clipboard
class CClipboard {
public:
	// Copies the image pixels of the currently visible rectangle of the image as a 24 bpp DIB to the clipboard
	static void CopyImageToClipboard(HWND hWnd, CJPEGImage * pImage);

	// Copy processed full size image to clipboard
	static void CopyFullImageToClipboard(HWND hWnd, CJPEGImage * pImage, const CImageProcessingParams& procParams,
		EProcessingFlags eFlags);

	// Copy section of full size image to clipboard
	static void CopyFullImageToClipboard(HWND hWnd, CJPEGImage * pImage, const CImageProcessingParams& procParams,
		EProcessingFlags eFlags, CRect clipRect);

	// Paste image from clipboard
	static CJPEGImage* PasteImageFromClipboard(HWND hWnd, const CImageProcessingParams& procParams, EProcessingFlags eFlags);

private:
	CClipboard(void);

	static void DoCopy(HWND hWnd, int nWidth, int nHeight, bool bFlip, const void* pSourceImageDIB32);
};
