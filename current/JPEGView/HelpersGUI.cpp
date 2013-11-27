#include "StdAfx.h"
#include "resource.h"
#include "HelpersGUI.h"
#include "NLS.h"
#include "ProcessParams.h"
#include "KeyMap.h"
#include "SettingsProvider.h"

static void AddFlagText(CString& sText, LPCTSTR sFlagText, bool bFlag) {
	sText += sFlagText;
	sText += CNLS::GetString(bFlag ? _T("on") : _T("off"));
	sText += _T('\n');
}

// Creates a font from the INI file description:
// "Font face" fontSizePoints [bold]
// Returns NULL if default font shall be used
static HFONT CreateFontFromINIDescription(HDC dc, LPCTSTR sIniFileDescription)
{
	CString sIniDesc(sIniFileDescription);
	sIniDesc.TrimLeft(); sIniDesc.TrimRight();

	CString sFontName;
	float fFontSize = 9.0f;
	bool isBold = false;
	if (sIniDesc.CompareNoCase(_T("default")) != 0) {
		if (sIniDesc.Find(_T(' ')) == -1) {
			sIniDesc.TrimLeft(_T('"')); sIniDesc.TrimRight(_T('"'));
			sFontName = sIniDesc;
		} else {
			TCHAR delimiter = (sIniDesc.GetAt(0) == _T('"')) ? _T('"') : _T(' ');
			sIniDesc.TrimLeft(_T('"'));
			if (delimiter != _T('"') && sIniDesc.FindOneOf(_T("0123456789")) == -1) {
				sFontName = sIniDesc;
			} else {
				int indexEndOfFilename = sIniDesc.Find(delimiter);
				if (indexEndOfFilename < 0) {
					sFontName = sIniDesc;
				} else {
					sFontName =  sIniDesc.Left(indexEndOfFilename);
					sIniDesc = sIniDesc.Mid(indexEndOfFilename + 1);
					fFontSize = (float)_ttof(sIniDesc);
					if (fFontSize <= 0.0)
						fFontSize = 9.0;
					fFontSize = max(7.0f, min(30.0f, fFontSize));
					isBold = sIniDesc.Find(_T("bold")) != -1;
				}
			}
		}
	}

	if (sFontName.IsEmpty())
		return (HFONT)NULL;

	int nHeight = - (int)(0.5f + fFontSize * ::GetDeviceCaps(dc, LOGPIXELSY) / 72);
	return ::CreateFont(nHeight, 0, 0, 0, isBold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, sFontName);
}

namespace HelpersGUI {

float ScreenScaling = -1.0f;

HFONT DefaultGUIFont = NULL;
HFONT DefaultFileNameFont = NULL;

HFONT CreateBoldFontOfSelectedFont(CDC & dc) {
	TCHAR buff[LF_FACESIZE];
	if (::GetTextFace(dc, LF_FACESIZE, buff) != 0) {
		TEXTMETRIC textMetrics;
		::GetTextMetrics(dc, &textMetrics);
		LOGFONT logFont;
		memset(&logFont, 0, sizeof(LOGFONT));
		logFont.lfHeight = textMetrics.tmHeight;
		logFont.lfWeight = FW_BOLD;
		_tcsncpy_s(logFont.lfFaceName, LF_FACESIZE, buff, LF_FACESIZE);  
		return ::CreateFontIndirect(&logFont);
	}
	return NULL;
}

void SelectDefaultGUIFont(HDC dc) {
	if (DefaultGUIFont == NULL) {
		DefaultGUIFont = CreateFontFromINIDescription(dc, CSettingsProvider::This().DefaultGUIFont());
		if (DefaultGUIFont == NULL)
			DefaultGUIFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	}
	::SelectObject(dc, DefaultGUIFont);
}

void SelectDefaultFileNameFont(HDC dc) {
	if (DefaultFileNameFont == NULL) {
		DefaultFileNameFont = CreateFontFromINIDescription(dc, CSettingsProvider::This().FileNameFont());
		if (DefaultFileNameFont == NULL)
			DefaultFileNameFont = ::CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Arial"));
	}
	::SelectObject(dc, DefaultFileNameFont);
}

void TranslateMenuStrings(HMENU hMenu, CKeyMap* pKeyMap) {
	int nMenuItemCount = ::GetMenuItemCount(hMenu);
	for (int i = 0; i < nMenuItemCount; i++) {
		const int TEXT_BUFF_LEN = 128;
		TCHAR menuText[TEXT_BUFF_LEN];
		::GetMenuString(hMenu, i, (LPTSTR)&menuText, TEXT_BUFF_LEN, MF_BYPOSITION);
		menuText[TEXT_BUFF_LEN-1] = 0;
		TCHAR* pTab = _tcschr(menuText, _T('\t'));
		if (pTab != NULL) {
			*pTab = 0;
			pTab++;
		}
		CString sKeyDesc;
		if (pKeyMap != NULL) {
			int nMenuItemId = ::GetMenuItemID(hMenu, i);
			sKeyDesc = pKeyMap->GetKeyStringForCommand(nMenuItemId);
		}
		CString sNewMenuText = CNLS::GetString(menuText);
		if (sNewMenuText != menuText || !sKeyDesc.IsEmpty() || pTab != NULL) {
			if (pTab != NULL && pTab[0] != 0) {
				sNewMenuText += _T('\t');
				if (pTab[0] == _T('0') && pTab[1] == _T('x')) {
					// These hexadecimal mappings force to use the shortcut for the given command id
					// instead of the shortcut of the menu item command id.
					int nCommand;
					_stscanf(pTab, _T("%x"), &nCommand);
					if (nCommand == 0xFFFF) {
						// Exit JPEGView is special
						CString shortCutExit = pKeyMap->GetKeyStringForCommand(0xFF);
						if (shortCutExit.IsEmpty()) {
							shortCutExit = pKeyMap->GetKeyStringForCommand(IDM_EXIT);
						}
						sNewMenuText += shortCutExit;
					} else {
						sNewMenuText += pKeyMap->GetKeyStringForCommand(nCommand);
					}
				} else {
					sNewMenuText += pTab;
				}
			} else if (!sKeyDesc.IsEmpty()) {
				sNewMenuText += _T('\t');
				sNewMenuText += sKeyDesc;
			}
			MENUITEMINFO menuInfo;
			memset(&menuInfo , 0, sizeof(MENUITEMINFO));
			menuInfo.cbSize = sizeof(MENUITEMINFO);
			menuInfo.fMask = MIIM_STRING;
			menuInfo.dwTypeData = (LPTSTR)(LPCTSTR)sNewMenuText;
			::SetMenuItemInfo(hMenu, i, TRUE, &menuInfo);
		}

		HMENU hSubMenu = ::GetSubMenu(hMenu, i);
		if (hSubMenu != NULL) {
			TranslateMenuStrings(hSubMenu, pKeyMap);
		}
	}
}

void DrawTextBordered(CDC& dc, LPCTSTR sText, const CRect& rect, UINT nFormat) {
	COLORREF oldColor = dc.SetTextColor(RGB(0, 0, 0));
	CRect textRect = rect;
	textRect.InflateRect(-1, -1);
	textRect.OffsetRect(-1, 0); dc.DrawText(sText, -1, &textRect, nFormat | DT_NOPREFIX);
	textRect.OffsetRect(2, 0); dc.DrawText(sText, -1, &textRect, nFormat | DT_NOPREFIX);
	textRect.OffsetRect(-1, -1); dc.DrawText(sText, -1, &textRect, nFormat | DT_NOPREFIX);
	textRect.OffsetRect(0, 2); dc.DrawText(sText, -1, &textRect, nFormat | DT_NOPREFIX);
	textRect.OffsetRect(0, -1);
	dc.SetTextColor(oldColor);
	dc.DrawText(sText, -1, &textRect, nFormat | DT_NOPREFIX);
}

CPoint DrawDIB32bppWithBlackBorders(CDC& dc, BITMAPINFO& bmInfo, void* pDIBData, HBRUSH backBrush, const CRect& targetArea, CSize dibSize, CPoint offset) {
	memset(&bmInfo, 0, sizeof(BITMAPINFO));
	bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmInfo.bmiHeader.biWidth = dibSize.cx;
	bmInfo.bmiHeader.biHeight = -dibSize.cy;
	bmInfo.bmiHeader.biPlanes = 1;
	bmInfo.bmiHeader.biBitCount = 32;
	bmInfo.bmiHeader.biCompression = BI_RGB;
	int xDest = (targetArea.Width() - dibSize.cx) / 2 + offset.x;
	int yDest = (targetArea.Height() - dibSize.cy) / 2 + offset.y;

	// remaining client area is painted black
	if (xDest > 0) {
		CRect r(0, 0, xDest, targetArea.Height());
		dc.FillRect(&r, backBrush);
	}
	if (xDest + dibSize.cx < targetArea.Width()) {
		CRect r(xDest + dibSize.cx, 0, targetArea.Width(), targetArea.Height());
		dc.FillRect(&r, backBrush);
	}
	if (yDest > 0) {
		CRect r(xDest, 0, xDest + dibSize.cx, yDest);
		dc.FillRect(&r, backBrush);
	}
	if (yDest + dibSize.cy < targetArea.Height()) {
		CRect r(xDest, yDest + dibSize.cy, xDest + dibSize.cx, targetArea.Height());
		dc.FillRect(&r, backBrush);
	}

	dc.SetDIBitsToDevice(xDest, yDest, dibSize.cx, dibSize.cy, 0, 0, 0, dibSize.cy, pDIBData, 
		&bmInfo, DIB_RGB_COLORS);

	return CPoint(xDest, yDest);
}

CString GetINIFileSaveConfirmationText(const CImageProcessingParams& procParams, 
									 EProcessingFlags eProcFlags, Helpers::ESorting eFileSorting,
									 Helpers::EAutoZoomMode eAutoZoomMode,
									 bool bShowNavPanel) {

	const int BUFF_SIZE = 256;
	TCHAR buff[BUFF_SIZE];
	CString sText = CString(CNLS::GetString(_T("Do you really want to save the following parameters as default to the INI file"))) + _T('\n') +
					Helpers::JPEGViewAppDataPath() + _T("JPEGView.ini ?\n\n");
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Contrast"))) + _T(": %.2f\n"), procParams.Contrast);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Brightness"))) + _T(": %.2f\n"), procParams.Gamma);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Saturation"))) + _T(": %.2f\n"), procParams.Saturation);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Sharpen"))) + _T(": %.2f\n"), procParams.Sharpen);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Cyan-Red"))) + _T(": %.2f\n"), procParams.CyanRed);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Magenta-Green"))) + _T(": %.2f\n"), procParams.MagentaGreen);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Yellow-Blue"))) + _T(": %.2f\n"), procParams.YellowBlue);
	sText += buff;
	if (GetProcessingFlag(eProcFlags, PFLAG_LDC)) {
		_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Lighten Shadows"))) + _T(": %.2f\n"), procParams.LightenShadows);
		sText += buff;
		_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Darken Highlights"))) + _T(": %.2f\n"), procParams.DarkenHighlights);
		sText += buff;
		_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Deep Shadows"))) + _T(": %.2f\n"), procParams.LightenShadowSteepness);
		sText += buff;
	}
	AddFlagText(sText, CNLS::GetString(_T("Auto contrast and color correction: ")), GetProcessingFlag(eProcFlags, PFLAG_AutoContrast));
	AddFlagText(sText, CNLS::GetString(_T("Local density correction: ")), GetProcessingFlag(eProcFlags, PFLAG_LDC));
	sText += CNLS::GetString(_T("Order files by: "));
	if (eFileSorting == Helpers::FS_CreationTime) {
		sText += CNLS::GetString(_T("Creation date/time"));
	} else if (eFileSorting == Helpers::FS_LastModTime) {
		sText += CNLS::GetString(_T("Last modification date/time"));
	} else if (eFileSorting == Helpers::FS_Random) {
		sText += CNLS::GetString(_T("Random"));
	} else {
		sText += CNLS::GetString(_T("File name"));
	}
	sText += _T("\n");
	sText += CNLS::GetString(_T("Auto zoom mode")); sText += _T(": ");
	if (eAutoZoomMode == Helpers::ZM_FillScreen) {
		sText += CNLS::GetString(_T("Fill with crop"));
	} else if (eAutoZoomMode == Helpers::ZM_FillScreenNoZoom) {
		sText += CNLS::GetString(_T("Fill with crop (no zoom)"));
	} else if (eAutoZoomMode == Helpers::ZM_FitToScreen) {
		sText += CNLS::GetString(_T("Fit to screen"));
	} else {
		sText += CNLS::GetString(_T("Fit to screen (no zoom)"));
	}
	sText += _T("\n");
	sText += CNLS::GetString(_T("Show navigation panel")); sText += _T(": ");
	sText += bShowNavPanel ? CNLS::GetString(_T("yes")) : CNLS::GetString(_T("no"));
	sText += _T("\n\n");
	sText += CNLS::GetString(_T("These values will override the values from the INI file located in the program folder of JPEGView!"));

	return sText;
}

void DrawImageLoadErrorText(CDC& dc, const CRect& clientRect, LPCTSTR sFailedFileName, int nFileLoadError) {
	bool bOutOfMemory = nFileLoadError & FileLoad_OutOfMemory;
	nFileLoadError &= ~FileLoad_OutOfMemory;

	const int BUF_LEN = 512;
	TCHAR buff[BUF_LEN];
	buff[0] = 0;
	CRect rectText(0, clientRect.Height()/2 - (int)(ScreenScaling*40), clientRect.Width(), clientRect.Height());
	switch (nFileLoadError) {
		case FileLoad_PasteFromClipboardFailed:
			_tcsncpy_s(buff, BUF_LEN, CNLS::GetString(_T("Pasting image from clipboard failed!")), BUF_LEN);
			break;
		case FileLoad_LoadError:
			_stprintf_s(buff, BUF_LEN, CNLS::GetString(_T("The file '%s' could not be read!")), sFailedFileName);
			break;
		case FileLoad_SlideShowListInvalid:
			_stprintf_s(buff, BUF_LEN, CNLS::GetString(_T("The file '%s' does not contain a list of file names!")), sFailedFileName);
			break;
		case FileLoad_NoFilesInDirectory:
			_stprintf_s(buff, BUF_LEN, CString(CNLS::GetString(_T("The directory '%s' does not contain any image files!")))  + _T('\n') +
				CNLS::GetString(_T("Press any key to search the subdirectories for image files.")), sFailedFileName);
			break;
		default:
			return;
	}
	if (bOutOfMemory) {
		_tcscat_s(buff, BUF_LEN, _T("\n"));
		_tcscat_s(buff, BUF_LEN, CNLS::GetString(_T("Reason: Not enough memory available")));
	}
	dc.DrawText(buff, -1, &rectText, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
}

CJPEGLosslessTransform::ETransformation CommandIdToLosslessTransformation(int nCommandId) {
    switch (nCommandId) {
        case IDM_ROTATE_90_LOSSLESS:
        case IDM_ROTATE_90_LOSSLESS_CONFIRM:
            return CJPEGLosslessTransform::Rotate90;
        case IDM_ROTATE_270_LOSSLESS:
        case IDM_ROTATE_270_LOSSLESS_CONFIRM:
            return CJPEGLosslessTransform::Rotate270;
        case IDM_ROTATE_180_LOSSLESS:
            return CJPEGLosslessTransform::Rotate180;
        case IDM_MIRROR_H_LOSSLESS:
            return CJPEGLosslessTransform::MirrorH;
        case IDM_MIRROR_V_LOSSLESS:
            return CJPEGLosslessTransform::MirrorV;
        default:
            return (CJPEGLosslessTransform::ETransformation)(-1);
    }
}

LPCTSTR LosslessTransformationResultToString(CJPEGLosslessTransform::EResult eResult) {
    switch (eResult) {
    case CJPEGLosslessTransform::ReadFileFailed:
        return CNLS::GetString(_T("Reading the JPEG file from disk failed"));
    case CJPEGLosslessTransform::WriteFileFailed:
        return CNLS::GetString(_T("Writing the resulting JPEG file to disk failed"));
    case CJPEGLosslessTransform::TransformationFailed:
        return CNLS::GetString(_T("Performing the transformation failed"));
    default:
        return _T("");
    }
}

bool CreateUserCommandsMenu(HMENU hMenu) {
	bool hasItems = false;
	int count = 0;
	std::list<CUserCommand*>::iterator iter;
	std::list<CUserCommand*> & userCmdList = CSettingsProvider::This().UserCommandList();
	::DeleteMenu(hMenu, 0, MF_BYPOSITION);
	for (iter = userCmdList.begin( ); iter != userCmdList.end( ); iter++ ) {
		LPCTSTR menuItemText = (*iter)->MenuItemText();
		if (menuItemText != NULL) {
			hasItems = true;
			CString itemText(menuItemText);
			CString shortcutKey = CKeyMap::GetShortcutKey((*iter)->GetKeyCode());
			if (!shortcutKey.IsEmpty()) {
				itemText += _T('\t');
				itemText += shortcutKey;
			}
			::AppendMenu(hMenu, MF_ENABLED | MF_STRING, (UINT_PTR)(IDM_FIRST_USER_CMD + count), itemText);
			count++;
		}
	}
	return hasItems;
}

CUserCommand* FindUserCommand(int index) {
	int count = 0;
	std::list<CUserCommand*>::iterator iter;
	std::list<CUserCommand*> & userCmdList = CSettingsProvider::This().UserCommandList();
	for (iter = userCmdList.begin( ); iter != userCmdList.end( ); iter++ ) {
		LPCTSTR menuItemText = (*iter)->MenuItemText();
		if (menuItemText != NULL) {
			if (count == index) {
				return *iter;
			}
			count++;
		}
	}
	return NULL;
}

}