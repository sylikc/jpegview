#pragma once

#include "Helpers.h"

enum EProcessingFlags;
class CImageProcessingParams;

namespace HelpersGUI {

	// Scaling factor from screen DPI, 96 dpi -> 1.0, 120 dpi -> 1.2
	extern float ScreenScaling;

	// Creates a bold version of the font that is currently selected in the given DC
	// The caller is responsible for deleting the returned font when no longer used
	HFONT CreateBoldFontOfSelectedFont(CDC & dc);

	// Translates all menu strings of the given menu, including the sub-menus
	void TranslateMenuStrings(HMENU hMenu);

	// Draws a text with Arial 10 font with a black outline to improve readability
	void DrawTextBordered(CPaintDC& dc, LPCTSTR sText, const CRect& rect, UINT nFormat);

	// Draws a 32 bit DIB centered in the given target area, filling the remaining area with the given brush
	// The bmInfo struct will be initialized by this method and does not need to be preinitialized.
	// Return value is the top, left coordinate of the painted DIB in the target area
	CPoint DrawDIB32bppWithBlackBorders(CPaintDC& dc, BITMAPINFO& bmInfo, void* pDIBData, HBRUSH backBrush, const CRect& targetArea, CSize dibSize);

	// Gets the text for confirmation of saving of settings to INI file
	CString GetINIFileSaveConfirmationText(const CImageProcessingParams& procParams, 
									 EProcessingFlags eProcFlags, Helpers::ESorting eFileSorting,
									 Helpers::EAutoZoomMode eAutoZoomMode,
									 bool bShowNavPanel);
}