#pragma once

#include "Helpers.h"
#include "JPEGLosslessTransform.h"

enum EProcessingFlags;
class CImageProcessingParams;
class CKeyMap;

namespace HelpersGUI {

	// Errors that occur during loading a file or pasting from clipboard
	enum EFileLoadError {
		FileLoad_Ok = 0,
		FileLoad_PasteFromClipboardFailed = 1,
		FileLoad_LoadError = 2,
		FileLoad_SlideShowListInvalid = 3,
		FileLoad_NoFilesInDirectory = 4,
		FileLoad_OutOfMemory = 65536 // can be combined with other error codes
	};

	// Scaling factor from screen DPI, 96 dpi -> 1.0, 120 dpi -> 1.2
	extern float ScreenScaling;

	// Creates a bold version of the font that is currently selected in the given DC
	// The caller is responsible for deleting the returned font when no longer used
	HFONT CreateBoldFontOfSelectedFont(CDC & dc);

	// Translates all menu strings of the given menu, including the sub-menus
	void TranslateMenuStrings(HMENU hMenu, CKeyMap* pKeyMap = NULL);

	// Draws a text with Arial 10 font with a black outline to improve readability
	void DrawTextBordered(CDC& dc, LPCTSTR sText, const CRect& rect, UINT nFormat);

	// Draws a 32 bit DIB centered in the given target area, filling the remaining area with the given brush
	// The bmInfo struct will be initialized by this method and does not need to be preinitialized.
	// Return value is the top, left coordinate of the painted DIB in the target area
	CPoint DrawDIB32bppWithBlackBorders(CDC& dc, BITMAPINFO& bmInfo, void* pDIBData, HBRUSH backBrush, const CRect& targetArea, CSize dibSize, CPoint offset);

	// Gets the text for confirmation of saving of settings to INI file
	CString GetINIFileSaveConfirmationText(const CImageProcessingParams& procParams, 
									 EProcessingFlags eProcFlags, Helpers::ESorting eFileSorting,
									 Helpers::EAutoZoomMode eAutoZoomMode,
									 bool bShowNavPanel);

	// Draws an error text for the given file loading error (combination of EFileLoadError codes)
	void DrawImageLoadErrorText(CDC& dc, const CRect& clientRect, LPCTSTR sFailedFileName, int nFileLoadError);

    // Convert a menu item command ID to a transformation enumeration for lossless JPEG transformation
    CJPEGLosslessTransform::ETransformation CommandIdToLosslessTransformation(int nCommandId);

    // Convert the error result code from lossless JPEG transformation to a string
    LPCTSTR LosslessTransformationResultToString(CJPEGLosslessTransform::EResult eResult);
}