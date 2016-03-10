#pragma once

#include "ProcessParams.h"

class CJPEGImage;

// Allows to copy an image to the clipboard and to paste an image from the clipboard
class CClipboard {
public:
	// Copy the pixels of the currently visible part of the image as a 24 bpp DIB to the clipboard
	// Zoom is as on screen.
	static void CopyImageToClipboard(HWND hWnd, CJPEGImage * pImage, LPCTSTR fileName);

	// Copy processed full size image as a 24 bpp DIB to the clipboard, i.e. in original resolution.
	static void CopyFullImageToClipboard(HWND hWnd, CJPEGImage * pImage, const CImageProcessingParams& procParams,
		EProcessingFlags eFlags, LPCTSTR fileName);

	// Copy section of full size image as a 24 bpp DIB to the clipboard, i.e. in original resolution.
	static void CopyFullImageToClipboard(HWND hWnd, CJPEGImage * pImage, const CImageProcessingParams& procParams,
		EProcessingFlags eFlags, CRect clipRect, LPCTSTR fileName);

	// Paste image from clipboard. Returns NULL if paste failed. The caller gets ownership of the returned image.
	static CJPEGImage* PasteImageFromClipboard(HWND hWnd, const CImageProcessingParams& procParams, EProcessingFlags eFlags);

private:
	CClipboard(void);

	static void DoCopy(HWND hWnd, int nWidth, int nHeight, const void* pSourceImageDIB32, LPCTSTR fileName);
};
