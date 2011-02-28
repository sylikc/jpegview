#include "StdAfx.h"
#include "HelpersGUI.h"
#include "NLS.h"
#include "ProcessParams.h"


static void AddFlagText(CString& sText, LPCTSTR sFlagText, bool bFlag) {
	sText += sFlagText;
	sText += CNLS::GetString(bFlag ? _T("on") : _T("off"));
	sText += _T('\n');
}


namespace HelpersGUI {

float ScreenScaling = -1.0f;

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

void TranslateMenuStrings(HMENU hMenu) {
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
		CString sNewMenuText = CNLS::GetString(menuText);
		if (sNewMenuText != menuText) {
			if (pTab != NULL) {
				sNewMenuText += _T('\t');
				sNewMenuText += pTab;
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
			TranslateMenuStrings(hSubMenu);
		}
	}
}

void DrawTextBordered(CPaintDC& dc, LPCTSTR sText, const CRect& rect, UINT nFormat) {
	static HFONT hFont = 0;
	if (hFont == 0) {
		// Use cleartype here, this improves the readability
		hFont = ::CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Arial"));
	}

	HFONT hOldFont = dc.SelectFont(hFont);
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
	dc.SelectFont(hOldFont);
}

CPoint DrawDIB32bppWithBlackBorders(CPaintDC& dc, BITMAPINFO& bmInfo, void* pDIBData, HBRUSH backBrush, const CRect& targetArea, CSize dibSize) {
	memset(&bmInfo, 0, sizeof(BITMAPINFO));
	bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmInfo.bmiHeader.biWidth = dibSize.cx;
	bmInfo.bmiHeader.biHeight = -dibSize.cy;
	bmInfo.bmiHeader.biPlanes = 1;
	bmInfo.bmiHeader.biBitCount = 32;
	bmInfo.bmiHeader.biCompression = BI_RGB;
	int xDest = (targetArea.Width() - dibSize.cx) / 2;
	int yDest = (targetArea.Height() - dibSize.cy) / 2;
	dc.SetDIBitsToDevice(xDest, yDest, dibSize.cx, dibSize.cy, 0, 0, 0, dibSize.cy, pDIBData, 
		&bmInfo, DIB_RGB_COLORS);

	// remaining client area is painted black
	if (dibSize.cx < targetArea.Width()) {
		CRect r(0, 0, xDest, targetArea.Height());
		dc.FillRect(&r, backBrush);
		CRect rr(xDest + dibSize.cx, 0, targetArea.Width(), targetArea.Height());
		dc.FillRect(&rr, backBrush);
	}
	if (dibSize.cy < targetArea.Height()) {
		CRect r(0, 0, targetArea.Width(), yDest);
		dc.FillRect(&r, backBrush);
		CRect rr(0, yDest + dibSize.cy, targetArea.Width(), targetArea.Height());
		dc.FillRect(&rr, backBrush);
	}

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

void DrawImageLoadErrorText(CPaintDC& dc, const CRect& clientRect, LPCTSTR sFailedFileName, int nFileLoadError) {
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

}