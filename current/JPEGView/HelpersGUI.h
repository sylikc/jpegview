#pragma once

#include "Helpers.h"
#include "JPEGLosslessTransform.h"

enum EProcessingFlags;
class CImageProcessingParams;
class CKeyMap;
class CUserCommand;

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

	// Scaling factor to scale from 96 dpi to actual screen DPI, 96 dpi -> 1.0, 120 dpi -> 1.2
	extern float ScreenScaling;

	// Scales a pixel value given in 96 DPI to screen pixels, taking the screen DPI into account
	int ScaleToScreen(int value);

	// Creates a bold version of the font that is currently selected in the given DC.
	// The caller is responsible for deleting the returned font when no longer used.
	HFONT CreateBoldFontOfSelectedFont(CDC & dc);

	// Selects the default GUI font into the given DC.
	void SelectDefaultGUIFont(HDC dc);

	// Selectes the default system font into the given DC.
	void SelectDefaultSystemFont(HDC dc);

	// Selects the default file name font into the given DC.
	void SelectDefaultFileNameFont(HDC dc);

	// Translates all menu strings of the given menu, including the sub-menus
	void TranslateMenuStrings(HMENU hMenu, CKeyMap* pKeyMap = NULL);

	// Draws a text with a black outline to improve readability. nFormat is according to format in GDI DrawText method.
	void DrawTextBordered(CDC& dc, LPCTSTR sText, const CRect& rect, UINT nFormat);

	// Draws a rectangle with line width 1 pixel, respectively 2 pixels for high resolution DPI (> 196 dpi)
	void DrawRectangle(CDC & dc, const CRect& rect);

	// Draws a 32 bit DIB centered in the given target area, filling the remaining area with the given brush
	// The bmInfo struct will be initialized by this method and does not need to be preinitialized.
	// Return value is the top, left coordinate of the painted DIB in the target area
	CPoint DrawDIB32bppWithBlackBorders(CDC& dc, BITMAPINFO& bmInfo, void* pDIBData, HBRUSH backBrush, const CRect& targetArea, CSize dibSize, CPoint offset);

	// Gets the text for confirmation of saving of settings to INI file
	CString GetINIFileSaveConfirmationText(const CImageProcessingParams& procParams,
		EProcessingFlags eProcFlags, Helpers::ENavigationMode eNavigationMode, Helpers::ESorting eFileSorting, bool isSortedUpcounting,
		Helpers::EAutoZoomMode eAutoZoomMode,
		bool bShowNavPanel, bool bShowFileName, bool bShowFileInfo,
		Helpers::ETransitionEffect eSlideShowTransitionEffect);

	// Draws an error text for the given file loading error (combination of EFileLoadError codes)
	void DrawImageLoadErrorText(CDC& dc, const CRect& clientRect, LPCTSTR sFailedFileName, int nFileLoadError);

	// Convert a menu item command ID to a transformation enumeration for lossless JPEG transformation
	CJPEGLosslessTransform::ETransformation CommandIdToLosslessTransformation(int nCommandId);

	// Convert the error result code from lossless JPEG transformation to a string
	LPCTSTR LosslessTransformationResultToString(CJPEGLosslessTransform::EResult eResult);

	// Creates the submenu containing the user commands in hMenu, returns if there are any menu items
	bool CreateUserCommandsMenu(HMENU hMenu);
	// Creates the submenu containing the open with.. commands in hMenu, returns if there are any menu items
	bool CreateOpenWithCommandsMenu(HMENU hMenu);

	// Finds a user command given the menu index in the menu as created above
	CUserCommand* FindUserCommand(int index);
	// Finds a open with command given the menu index in the menu as created above
	CUserCommand* FindOpenWithCommand(int index);
	
}