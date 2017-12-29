#include "StdAfx.h"
#include "GUIControls.h"
#include "Panel.h"
#include "HelpersGUI.h"
#include "SettingsProvider.h"
#include <math.h>

#define WINPROP_THIS _T("JV_THIS")

/////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////////////////////

static CSize GetTextRect(HWND hWnd, LPCTSTR sText) {
	CPaintDC dc(hWnd);
	HelpersGUI::SelectDefaultGUIFont(dc);
	CSize textlen;
	dc.GetTextExtent(sText, (int)_tcslen(sText), &textlen);
	return textlen;
}

static CSize GetTextRect(CDC & dc, LPCTSTR sText) {
	HelpersGUI::SelectDefaultGUIFont(dc);
	CSize textlen;
	dc.GetTextExtent(sText, (int)_tcslen(sText), &textlen);
	return textlen;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CUICtrl
/////////////////////////////////////////////////////////////////////////////////////////////

CUICtrl::CUICtrl(CPanel* pPanel) { 
	m_pPanel = pPanel; 
	m_bShow = true; 
	m_bHighlight = false; 
	m_position = CRect(0, 0, 0, 0);
	m_pos2Screen = CPoint(0, 0);
}

void CUICtrl::OnPaint(CDC & dc, const CPoint& offset) {
	HelpersGUI::SelectDefaultGUIFont(dc);
	dc.SetBkMode(TRANSPARENT);

	m_pos2Screen = CPoint(-offset.x, -offset.y);

	CRect pos(m_position);
	pos.OffsetRect(offset);
	PreDraw(dc, pos);
	pos.OffsetRect(-1, 0); Draw(dc, pos, true);
	pos.OffsetRect(2, 0); Draw(dc, pos, true);
	pos.OffsetRect(-1, -1); Draw(dc, pos, true);
	pos.OffsetRect(0, 2); Draw(dc, pos, true);
	pos.OffsetRect(0, -1); Draw(dc, pos, false);
}

void CUICtrl::SetShow(bool bShow, bool bInvalidate) { 
	if (bShow != m_bShow) {
		m_bShow = bShow;
		if (m_pPanel != NULL && bInvalidate) {
			CRect pos(m_position);
			pos.InflateRect(1, 1);
			::InvalidateRect(m_pPanel->GetHWND(), &pos, FALSE); // trigger redraw of area
		}
	}
}

void CUICtrl::SetTooltip(LPCTSTR sTooltipText) {
	m_pPanel->GetTooltipMgr().AddTooltip(this, sTooltipText);
}

void CUICtrl::SetTooltipHandler(TooltipHandler ttHandler, void* pContext) {
	m_pPanel->GetTooltipMgr().AddTooltipHandler(this, ttHandler, pContext);
}

void CUICtrl::PreDraw(CDC & dc, CRect position) {
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CTextCtrl
/////////////////////////////////////////////////////////////////////////////////////////////

CTextCtrl::CTextCtrl(CPanel* pPanel, LPCTSTR sTextInit, bool bEditable, 
					 TextChangedHandler* textChangedHandler, void* pContext) : CUICtrl(pPanel) {
	 m_bEditable = bEditable;
	 m_bRightAligned = false;
	 m_bBold = false;
	 m_bAllowMultiline = false;
	 m_textChangedHandler = textChangedHandler;
	 m_pContext = pContext;
	 m_textSize = CSize(0, 0);
	 m_nMaxTextWidth = HelpersGUI::ScaleToScreen(200);
	 m_pEdit = NULL;
	 m_hBoldFont = NULL;
	 m_highlightOnMouseOver = false;
	 SetText((sTextInit == NULL) ? _T("") : sTextInit);
}

CTextCtrl::~CTextCtrl() {
	if (m_hBoldFont != NULL) {
		::DeleteObject(m_hBoldFont);
	}
}

void CTextCtrl::SetText(LPCTSTR sText) {
	m_sText = sText;
	m_textSize = GetTextRectangle(m_pPanel->GetHWND(), m_sText);
	m_pPanel->RequestRepositioning();
	TerminateEditMode(false);
}

void CTextCtrl::SetEditable(bool bEditable) {
	m_bEditable = bEditable;
	TerminateEditMode(false);
}

void CTextCtrl::SetBold(bool bValue) {
	m_bBold = bValue;
	m_textSize = GetTextRectangle(m_pPanel->GetHWND(), m_sText);
	m_pPanel->RequestRepositioning();
}

bool CTextCtrl::EnterEditMode() {
	if (!IsShown() || !m_bEditable) return false;
	if (m_pEdit != NULL) return true; // already in edit mode

	CRect rectEdit = m_position;
	int nDiff = m_position.Height() / 6;
	rectEdit.left -= 2;
	rectEdit.right = rectEdit.left + m_nMaxTextWidth;
	rectEdit.top += nDiff + 1;
	rectEdit.bottom -= nDiff - 1;
	m_pEdit = new CEdit();
	m_pEdit->Create(m_pPanel->GetHWND(), _U_RECT(&rectEdit), 0, WS_BORDER | WS_CHILD | ES_AUTOHSCROLL, 0);

	// Subclass edit control to capture the ESC and ENTER keys
	// ms-help://MS.VSExpressCC.v80/MS.VSIPCC.v80/MS.PSDKSVR2003R2.1033/winui/winui/windowsuserinterface/windowing/windowprocedures/usingwindowprocedures.htm
	m_OrigEditProc = (WNDPROC) ::SetWindowLongPtr(m_pEdit->m_hWnd, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
	SetProp(m_pEdit->m_hWnd, WINPROP_THIS, (HANDLE) this);

	m_pEdit->SetFont((HFONT)::GetStockObject(DEFAULT_GUI_FONT));
	m_pEdit->SetWindowText(m_sText);
	m_pEdit->LimitText(256);
	m_pEdit->ShowWindow(SW_SHOW);
	m_pEdit->SetFocus();
	m_pEdit->SetSelAll();
	return true;
}

void CTextCtrl::TerminateEditMode(bool bAcceptNewText) {
	if (m_pEdit) {
		TCHAR newName[MAX_PATH];
		m_pEdit->GetWindowText(newName, MAX_PATH);
		::RemoveProp(m_pEdit->m_hWnd, WINPROP_THIS);
		::SetWindowLongPtr(m_pEdit->m_hWnd, GWLP_WNDPROC, (LONG_PTR)m_OrigEditProc);
		m_pEdit->DestroyWindow();
		delete m_pEdit;
		::SetFocus(m_pPanel->GetHWND());
		m_pEdit = NULL;
		if (bAcceptNewText) {
			if (_tcscmp(newName, m_sText) != 0) {
				bool bSetNewName = true;
				if (m_textChangedHandler != NULL) {
					bSetNewName = m_textChangedHandler(m_pContext, *this, newName);
				}
				if (bSetNewName) SetText(newName);
			}
		}
		CRect rect(m_position);
		rect.right = rect.left + m_textSize.cx + HelpersGUI::ScaleToScreen(20);
		::InvalidateRect(m_pPanel->GetHWND(), &rect, FALSE);
	}
}

bool CTextCtrl::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	if (m_bEditable) {
		CPoint mousePos(nX, nY);
		if (m_position.PtInRect(mousePos)) {
			if (eMouseEvent == MouseEvent_BtnDown) {
				EnterEditMode();
			}
			return true;
		} else {
			TerminateEditMode(true);
		}
	}
	return false;
}

bool CTextCtrl::OnMouseMove(int nX, int nY) {
	if (m_highlightOnMouseOver || (m_bEditable && m_pEdit == NULL)) {
		CPoint mousePos(nX, nY);
		if (m_position.PtInRect(mousePos)) {
			if (!m_bHighlight) {
				m_bHighlight = true;
				::InvalidateRect(m_pPanel->GetHWND(), &m_position, FALSE);
			}
		} else if (m_bHighlight) {
			m_bHighlight = false;
			::InvalidateRect(m_pPanel->GetHWND(), &m_position, FALSE);
		}
	}

	return m_pEdit != NULL;
}

void CTextCtrl::CreateBoldFont(CDC & dc) {
	if (m_hBoldFont == NULL) {
		HelpersGUI::SelectDefaultGUIFont(dc);
		m_hBoldFont = HelpersGUI::CreateBoldFontOfSelectedFont(dc);
	}
}

int CTextCtrl::GetTextRectangleHeight(HWND hWnd, int nWidth) {
	if (m_bAllowMultiline) {
		CPaintDC dc = CPaintDC(hWnd);
		if (m_bBold) {
			CreateBoldFont(dc);
			dc.SelectFont(m_hBoldFont);
		} else {
			HelpersGUI::SelectDefaultGUIFont(dc);
		}
		CRect textRect(0, 0, nWidth, 1);
		dc.DrawText(m_sText, m_sText.GetLength(), &textRect, DT_WORDBREAK | DT_CALCRECT | DT_VCENTER | DT_LEFT);
		dc.SelectStockFont(DEFAULT_GUI_FONT);
		return textRect.Height();
	} else {
		return GetTextRectangle(hWnd, m_sText).cy;
	}
}

CSize CTextCtrl::GetTextRectangle(HWND hWnd, LPCTSTR sText) {
	if (m_bBold) {
		CPaintDC dc = CPaintDC(hWnd);
		CreateBoldFont(dc);
		dc.SelectFont(m_hBoldFont);
		CSize textlen;
		dc.GetTextExtent(sText, (int)_tcslen(sText), &textlen);
		dc.SelectStockFont(DEFAULT_GUI_FONT);
		return CSize(textlen.cx + 1, textlen.cy);
	} else {
		return GetTextRect(hWnd, sText);
	}
}

void CTextCtrl::Draw(CDC & dc, CRect position, bool bBlack) {
	if (m_bBold) {
		CreateBoldFont(dc);
		dc.SelectFont(m_hBoldFont);
	} else {
		HelpersGUI::SelectDefaultGUIFont(dc);
	}
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(bBlack ? 0 : m_bHighlight ? CSettingsProvider::This().ColorHighlight() : CSettingsProvider::This().ColorGUI());
	unsigned int nAlignment = m_bRightAligned ? DT_RIGHT : DT_LEFT;
	unsigned int nMultiline = m_bAllowMultiline ? DT_WORDBREAK : DT_SINGLELINE | DT_WORD_ELLIPSIS;
	unsigned int nFlags = nAlignment | DT_VCENTER | nMultiline | DT_NOPREFIX;
	if (m_bAllowMultiline) {
		CRect pos(position);
		int nHeight = dc.DrawText(m_sText, m_sText.GetLength(), &pos, nFlags | DT_CALCRECT);
		position.OffsetRect(0, (position.Height() - nHeight) / 2);
	}
	dc.DrawText(m_sText, m_sText.GetLength(), &position, nFlags);
	dc.SelectStockFont(DEFAULT_GUI_FONT);
}

// Subclass procedure 
LRESULT APIENTRY CTextCtrl::EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	CTextCtrl* pThis = (CTextCtrl*)::GetProp(hwnd, WINPROP_THIS);
	if (uMsg == WM_GETDLGCODE) {
		return DLGC_WANTALLKEYS; // else we never get the ESC key...
	} else if (uMsg == WM_KEYDOWN) {
		if (wParam == VK_ESCAPE) {
			pThis->TerminateEditMode(false);
			return TRUE;
		} else if (wParam == VK_RETURN) {
			pThis->TerminateEditMode(true);
			return TRUE;
		}
	}

	return ::CallWindowProc(pThis->m_OrigEditProc, hwnd, uMsg, 
		wParam, lParam); 
} 

/////////////////////////////////////////////////////////////////////////////////////////////
// CURLCtrl
/////////////////////////////////////////////////////////////////////////////////////////////

CURLCtrl::CURLCtrl(CPanel* pPanel, LPCTSTR sText, LPCTSTR sURL, bool outlineText, void* pContext) : CTextCtrl(pPanel, sText, false, NULL, pContext)
{
	m_sURL = sURL;
	m_outlineText = outlineText;
	m_highlightOnMouseOver = true;
	m_handCursorSet = false;
}

void CURLCtrl::SetURL(LPCTSTR sURL) {
	m_sURL = sURL;
}

bool CURLCtrl::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	CPoint mousePos(nX, nY);
	if (m_position.PtInRect(mousePos)) {
		if (eMouseEvent == MouseEvent_BtnDown) {
			NavigateToURL();
		}
		return true;
	}
	return false;
}

bool CURLCtrl::OnMouseMove(int nX, int nY) {
	bool result = CTextCtrl::OnMouseMove(nX, nY);
	CPoint mousePos(nX, nY);
	if (m_position.PtInRect(mousePos)) {
		if (!m_handCursorSet) {
			::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_HAND)));
			m_handCursorSet = true;
		}
	}
	else if (m_handCursorSet) {
		m_handCursorSet = false;
	}
	return result;
}

bool CURLCtrl::MouseCursorCaptured() {
	return m_handCursorSet;
}

void CURLCtrl::Draw(CDC & dc, CRect position, bool bBlack) {
	if (m_outlineText || !bBlack) {
		CTextCtrl::Draw(dc, position, bBlack);
	}
}

void CURLCtrl::NavigateToURL() {
	if (!m_sURL.IsEmpty())
		::ShellExecute(m_pPanel->GetHWND(), _T("open"), m_sURL, NULL, NULL, SW_SHOW);
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CButtonCtrl
/////////////////////////////////////////////////////////////////////////////////////////////

CButtonCtrl::CButtonCtrl(CPanel* pPanel, LPCTSTR sButtonText, 
						 ButtonPressedHandler* buttonPressedHandler, void* pContext, int nParameter) : CUICtrl(pPanel) {
	m_sText = sButtonText;
	m_paintHandler = NULL;
	m_pPaintContext = NULL;
	m_buttonPressedHandler = buttonPressedHandler;
	m_pContext = pContext;
	m_nParameter = nParameter;
	m_textSize = GetTextRect(m_pPanel->GetHWND(), m_sText);
	m_bDragging = false;
	m_bActive = false;
	m_extendedArea = CRect(0, 0, 0, 0);
	m_dimmingFactor = 0.0f;
}

CButtonCtrl::CButtonCtrl(CPanel* pPanel, PaintHandler* paintHandler, ButtonPressedHandler* buttonPressedHandler,
						 void* pPaintContext, void* pContext, int nParameter) : CUICtrl(pPanel) {
	m_paintHandler = paintHandler;
	m_pPaintContext = pPaintContext;
	m_buttonPressedHandler = buttonPressedHandler;
	m_pContext = pContext;
	m_nParameter = nParameter;
	m_textSize = CSize(0, 0);
	m_bDragging = false;
	m_bActive = false;
	m_extendedArea = CRect(0, 0, 0, 0);
	m_dimmingFactor = 0.0f;
}

void CButtonCtrl::SetActive(bool bActive) {
	if (bActive != m_bActive) {
		m_bActive = bActive;
		if (this->m_bShow) {
			::InvalidateRect(m_pPanel->GetHWND(), &m_position, FALSE);
		}
	}
}

CSize CButtonCtrl::GetMinSize() {
	return CSize(m_textSize.cx + HelpersGUI::ScaleToScreen(26), m_textSize.cy + HelpersGUI::ScaleToScreen(6));
}

bool CButtonCtrl::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	bool bRetVal = false;
	CPoint mousePos(nX, nY);
	CRect extendedPosition(m_position.left + m_extendedArea.left, m_position.top + m_extendedArea.top,
		m_position.right + m_extendedArea.right, m_position.bottom + m_extendedArea.bottom);
	if (extendedPosition.PtInRect(mousePos)) {
		if (eMouseEvent == MouseEvent_BtnDown) {
			m_pPanel->CaptureMouse(this);
			m_bDragging = true;
			::InvalidateRect(m_pPanel->GetHWND(), &m_position, FALSE);
			return true;
		}
		if (eMouseEvent == MouseEvent_BtnUp && m_buttonPressedHandler != NULL) {
			m_buttonPressedHandler(m_pContext, m_nParameter, *this);
			bRetVal = true;
		}
	}
	
	if (m_bDragging) {
		m_pPanel->ReleaseMouse(this);
		m_bDragging = false;
		::InvalidateRect(m_pPanel->GetHWND(), &m_position, FALSE);
		bRetVal = true; // eat this event when the mouse was captured
	}
	return bRetVal;
}

bool CButtonCtrl::OnMouseMove(int nX, int nY) {
	CPoint mousePos(nX, nY);
	CRect extendedPosition(m_position.left + m_extendedArea.left, m_position.top + m_extendedArea.top,
		m_position.right + m_extendedArea.right, m_position.bottom + m_extendedArea.bottom);
	if (extendedPosition.PtInRect(mousePos)) {
		if (!m_bHighlight) {
			m_bHighlight = true;
			::InvalidateRect(m_pPanel->GetHWND(), &m_position, FALSE);
		}
	} else if (m_bHighlight) {
		m_bHighlight = false;
		::InvalidateRect(m_pPanel->GetHWND(), &m_position, FALSE);
	}

	return false;
}

void CButtonCtrl::PreDraw(CDC & dc, CRect position) {
	// dim the button background if requested
	if (m_dimmingFactor > 1e-6f) {
		CDC memDCblack;
		memDCblack.CreateCompatibleDC(dc);
		CBitmap bitmapBlack;
		bitmapBlack.CreateCompatibleBitmap(dc, position.Width(), position.Height());
		memDCblack.SelectBitmap(bitmapBlack);
		memDCblack.FillRect(CRect(0, 0, position.Width(), position.Height()), (HBRUSH)::GetStockObject(BLACK_BRUSH));
	
		BLENDFUNCTION blendFunc;
		memset(&blendFunc, 0, sizeof(blendFunc));
		blendFunc.BlendOp = AC_SRC_OVER;
		blendFunc.SourceConstantAlpha = (unsigned char)(m_dimmingFactor*255 + 0.5f);
		blendFunc.AlphaFormat = 0;
		dc.AlphaBlend(position.left, position.top, position.Width(), position.Height(), memDCblack, 0, 0, position.Width(), position.Height(), blendFunc);
	}
}

void CButtonCtrl::Draw(CDC & dc, CRect position, bool bBlack) {
	dc.SelectStockBrush(HOLLOW_BRUSH);
	HPEN hPen, hOldPen;
	HPEN hPen2 = NULL;
	if (bBlack) {
		dc.SelectStockPen(BLACK_PEN);
	} else {
		hPen = ::CreatePen(PS_SOLID, 1, (m_bDragging && m_bHighlight) ? CSettingsProvider::This().ColorHighlight() : m_bActive ? CSettingsProvider::This().ColorSelected() : CSettingsProvider::This().ColorGUI());
		hOldPen = dc.SelectPen(hPen);
	}
	HelpersGUI::DrawRectangle(dc, position);
	if (m_sText.GetLength() > 0) {
		HelpersGUI::SelectDefaultGUIFont(dc);
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(bBlack ? 0 : m_bHighlight ? CSettingsProvider::This().ColorHighlight() : m_bActive ? CSettingsProvider::This().ColorSelected() : CSettingsProvider::This().ColorGUI());
		dc.DrawText(m_sText, m_sText.GetLength(), &position, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_NOPREFIX);
	} else if (m_paintHandler != NULL) {
		if (m_bHighlight && !bBlack) {
			hPen2 = ::CreatePen(PS_SOLID, 1, CSettingsProvider::This().ColorHighlight());
			dc.SelectPen(hPen2);
		}
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(bBlack ? 0 : m_bHighlight ? CSettingsProvider::This().ColorHighlight() : m_bActive ? CSettingsProvider::This().ColorSelected() : CSettingsProvider::This().ColorGUI());
		m_paintHandler(m_pPaintContext, position, dc);
	}
	if (!bBlack) {
		dc.SelectPen(hOldPen);
		::DeleteObject(hPen);
		if (hPen2 != NULL) {
			::DeleteObject(hPen2);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CSliderDouble
/////////////////////////////////////////////////////////////////////////////////////////////

CSliderDouble::CSliderDouble(CPanel* pPanel, LPCTSTR sName, int nSliderLen, double* pdValue, bool* pbEnable,
	double dMin, double dMax, double dDefaultValue, bool bAllowPreviewAndReset, bool bLogarithmic, bool bInvert) : CUICtrl(pPanel) {
	m_sName = sName;
	m_nSliderLen = nSliderLen;
	m_pValue = pdValue;
	m_pEnable = pbEnable;
	m_dMin = bLogarithmic ? log10(dMin) : dMin;;
	m_dMax = bLogarithmic ? log10(dMax) : dMax;
	m_dDefaultValue = dDefaultValue;
	m_sliderRect = CRect(0, 0, 0, 0);
	m_checkRect = CRect(0, 0, 0, 0);
	m_checkRectFull = CRect(0, 0, 0, 0);
	m_bLogarithmic = bLogarithmic;
	m_nSign = bInvert ? -1 : +1;
	m_bDragging = false;
	m_bHighlightCheck = false;
	m_bHighlight = false;
	m_bHighlightNumber = false;
	m_bNumberClicked = false;
	m_bAllowPreviewAndReset = bAllowPreviewAndReset;

	CPaintDC dc(m_pPanel->GetHWND());
	m_nNumberWidth = GetTextRect(dc, _T("--X.XX")).cx;
	m_textSize = GetTextRect(dc, sName);
	m_nSliderHeight = m_nNumberWidth/6;
	m_nCheckHeight = pbEnable ? m_nNumberWidth/3 : 0;
	m_nNameWidth = pbEnable ? m_textSize.cx + m_nCheckHeight + m_nCheckHeight/2 : m_textSize.cx;
}

CSize CSliderDouble::GetMinSize() {
	return CSize(m_nNumberWidth + m_nNameWidth + m_nSliderLen + m_nNumberWidth / 4, m_nSliderHeight + m_textSize.cy);
}

bool CSliderDouble::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	CPoint mousePos(nX, nY);

	m_bNumberClicked = false;
	bool bEatEvent = false;

	if (eMouseEvent == MouseEvent_BtnDown) {
		if (m_position.PtInRect(mousePos) || m_sliderRect.PtInRect(mousePos)) m_pPanel->CaptureMouse(this);
	} else if (eMouseEvent == MouseEvent_BtnUp) {
		m_pPanel->ReleaseMouse(this);
		bEatEvent = true;
	}

	if (eMouseEvent == MouseEvent_BtnUp && m_bDragging) {
		m_bDragging = false;
		return true;
	} else if (eMouseEvent == MouseEvent_BtnDown && m_sliderRect.PtInRect(mousePos) && !(m_pEnable && !*m_pEnable)) {
		m_bDragging = true;
		OnMouseMove(nX, nY);
		return true;
	} else if (m_pEnable && eMouseEvent == MouseEvent_BtnUp && m_checkRectFull.PtInRect(mousePos)) {
		*m_pEnable = !(*m_pEnable);
		::InvalidateRect(m_pPanel->GetHWND(), NULL, FALSE); // trigger redraw of window
	} else if (eMouseEvent == MouseEvent_BtnDblClk) {
		CRect resetRect(m_sliderRect.left, m_position.top, m_sliderRect.right, m_sliderRect.top);
		if ((!m_pEnable || *m_pEnable) && resetRect.PtInRect(mousePos)) {
			m_dSavedValue = m_dDefaultValue;
			SetValue(m_dDefaultValue);
		}
	} else if (m_bHighlightNumber) {
		OnMouseMove(nX, nY);
		m_bNumberClicked = eMouseEvent == MouseEvent_BtnDown && m_bHighlightNumber;
		if (m_bNumberClicked) m_dSavedValue = *m_pValue;
		SetValue(m_bNumberClicked ? m_dDefaultValue : m_dSavedValue);
		return true;
	}

	return bEatEvent;
}

bool CSliderDouble::OnMouseMove(int nX, int nY) {
	CPoint mousePos(nX, nY);

	if (m_pEnable && !m_bDragging) {
		if (m_checkRectFull.PtInRect(mousePos)) {
			if (!m_bHighlightCheck) {
				m_bHighlightCheck = true;
				::InvalidateRect(m_pPanel->GetHWND(), &m_checkRectFull, FALSE);
			}
		} else if (m_bHighlightCheck) {
			m_bHighlightCheck = false;
			::InvalidateRect(m_pPanel->GetHWND(), &m_checkRectFull, FALSE);
		}
	}

	if (m_pEnable && !*m_pEnable) { return false; }

	if (m_bDragging) {
		SetValue(SliderPosToValue(nX - m_pos2Screen.x, m_nSliderStart, m_nSliderStart + m_nSliderLen));
		return true;
	} else {
		if (m_sliderRect.PtInRect(mousePos) && !m_bNumberClicked) {
			if (!m_bHighlight) {
				m_bHighlight = true;
				CRect invRect(&m_sliderRect);
				invRect.OffsetRect(0, -HelpersGUI::ScaleToScreen(8));
				::InvalidateRect(m_pPanel->GetHWND(), &invRect, FALSE);
			}
		} else if (m_bHighlight) {
			m_bHighlight = false;
			CRect invRect(&m_sliderRect);
			invRect.OffsetRect(0, -HelpersGUI::ScaleToScreen(8));
			::InvalidateRect(m_pPanel->GetHWND(), &invRect, FALSE);
		}
		CRect numberRect(m_sliderRect.right + HelpersGUI::ScaleToScreen(2), m_position.top, m_position.right, m_position.bottom);
		if (numberRect.PtInRect(mousePos)) {
			if (!m_bHighlightNumber && abs(*m_pValue - m_dDefaultValue) > 5e-3) {
				m_bHighlightNumber = true;
				::InvalidateRect(m_pPanel->GetHWND(), &numberRect, FALSE);
			}
		} else if (m_bHighlightNumber && !m_bNumberClicked) {
			m_bHighlightNumber = false;
			::InvalidateRect(m_pPanel->GetHWND(), &numberRect, FALSE);
		}
	}

	return m_bNumberClicked;
}

void CSliderDouble::OnPaint(CDC & dc, const CPoint& offset)  {
	if (m_pEnable && !*m_pEnable) { m_bHighlight = false; m_bHighlightNumber = false; }
	CUICtrl::OnPaint(dc, offset);
}

void CSliderDouble::Draw(CDC & dc, CRect position, bool bBlack) {
	dc.SetTextColor(bBlack ? 0 : m_bHighlightNumber ? CSettingsProvider::This().ColorHighlight() : CSettingsProvider::This().ColorGUI());
	TCHAR buff[16]; _stprintf_s(buff, 16, _T("%.2f"), *m_pValue);
	dc.DrawText(buff, (int)_tcslen(buff), &position, DT_VCENTER | DT_SINGLELINE | DT_RIGHT | DT_NOPREFIX);

	if (m_pEnable != NULL) {
		m_checkRect = CRect(CPoint(position.left, position.top + ((position.Height() - m_nCheckHeight) >> 1)),
			CSize(m_nCheckHeight, m_nCheckHeight));
		m_checkRectFull = CRect(m_checkRect.left, m_checkRect.top, 
			m_checkRect.right + m_nCheckHeight/2 + m_nNameWidth, m_checkRect.bottom);
		m_checkRectFull.OffsetRect(m_pos2Screen.x, m_pos2Screen.y);
		DrawCheck(dc, m_checkRect, bBlack, m_bHighlightCheck);
		position.left += m_nCheckHeight + m_nCheckHeight/2;
	}
	dc.SetTextColor(bBlack ? 0 : CSettingsProvider::This().ColorGUI());
	dc.DrawText(m_sName, (int)_tcslen(m_sName), &position, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	
	int nYMid = position.top + ((position.bottom - position.top) >> 1);
	int nXPos = position.right - m_nNumberWidth;
	m_nSliderStart = nXPos - m_nSliderLen;
	DrawRuler(dc, m_nSliderStart, nXPos, nYMid, bBlack, m_bHighlight);
	m_sliderRect = CRect(m_nSliderStart - m_nSliderHeight, nYMid - HelpersGUI::ScaleToScreen(2), nXPos + m_nSliderHeight, nYMid + m_nSliderHeight + HelpersGUI::ScaleToScreen(8));
	m_sliderRect.OffsetRect(m_pos2Screen.x, m_pos2Screen.y);
}

void CSliderDouble::DrawRuler(CDC & dc, int nXStart, int nXEnd, int nY, bool bBlack, bool bHighlight) {
	HPEN hPen = 0, hPenRed = 0;
	if (bBlack) {
		dc.SelectStockPen(BLACK_PEN);
	} else {
		hPen = ::CreatePen(PS_SOLID, 1, bHighlight ? CSettingsProvider::This().ColorHighlight() : CSettingsProvider::This().ColorGUI());
		hPenRed = ::CreatePen(PS_SOLID, 1, CSettingsProvider::This().ColorSlider());
		dc.SelectPen(hPen);
	}
	
	dc.MoveTo(nXStart, nY);
	dc.LineTo(nXEnd, nY);
	float fStep = (nXEnd - nXStart)/10.0f;
	float fX = (float)nXStart;
	for (int i = 0; i < 11; i++) {
		int nSub;
		if (i == 0 || i == 10) nSub = 8;
		else if (i == 5) nSub = 5;
		else nSub = 3;

		dc.MoveTo((int)(fX + 0.5f), nY);
		dc.LineTo((int)(fX + 0.5f), nY  - HelpersGUI::ScaleToScreen(nSub));
		fX += fStep;
	}

	if (m_pEnable == NULL || *m_pEnable) {
		int nSliderPos = ValueToSliderPos(*m_pValue, nXStart, nXEnd);
		if (!bBlack) dc.SelectPen(hPenRed);
		int nYP = nY + HelpersGUI::ScaleToScreen(3);
		dc.MoveTo(nSliderPos, nYP);
		dc.LineTo(nSliderPos - m_nSliderHeight, nYP + m_nSliderHeight);
		dc.LineTo(nSliderPos + m_nSliderHeight, nYP + m_nSliderHeight);
		dc.LineTo(nSliderPos, nYP);
	}

	if (!bBlack) {
		dc.SelectStockPen(BLACK_PEN);
		::DeleteObject(hPen);
		::DeleteObject(hPenRed);
	}
}

static void DrawCheckOnePass(CDC & dc, const CRect& position) {
	dc.MoveTo(position.left, position.top + position.Height()/2 - 1);
	dc.LineTo(position.left + position.Width()/3, position.bottom - 2);
	dc.LineTo(position.right - 1, position.top - 1);
}

void CSliderDouble::DrawCheck(CDC & dc, CRect position, bool bBlack, bool bHighlight) {
	HPEN hPen = 0;
	if (bBlack) {
		dc.SelectStockPen(BLACK_PEN);
	} else {
		hPen = ::CreatePen(PS_SOLID, 1, bHighlight ? CSettingsProvider::This().ColorHighlight() : CSettingsProvider::This().ColorGUI());
		dc.SelectPen(hPen);
	}

	dc.SelectStockBrush(HOLLOW_BRUSH);
	HelpersGUI::DrawRectangle(dc, position);
	if (*m_pEnable) {
		DrawCheckOnePass(dc, position);
		if (HelpersGUI::ScreenScaling >= 2) {
			position.OffsetRect(1, 0);
			DrawCheckOnePass(dc, position);
		}
	}

	if (!bBlack) {
		dc.SelectStockPen(BLACK_PEN);
		::DeleteObject(hPen);
	}
}

void CSliderDouble::SetValue(double dValue) {
	double dOldValue = *m_pValue;
	*m_pValue = dValue;
	if (dOldValue != *m_pValue) {
		::InvalidateRect(m_pPanel->GetHWND(), NULL, FALSE);
	}
}

int CSliderDouble::ValueToSliderPos(double dValue, int nSliderStart, int nSliderEnd) {
	double dVal = m_bLogarithmic ? m_nSign*log10(dValue) : m_nSign*dValue;
	int nSliderPos = nSliderStart + (int)((nSliderEnd - nSliderStart)*(dVal - m_dMin)/(m_dMax - m_dMin) + 0.5);
	return nSliderPos;
}

double CSliderDouble::SliderPosToValue(int nSliderPos, int nSliderStart, int nSliderEnd) {
	nSliderPos = max(nSliderStart, min(nSliderEnd, nSliderPos));
	double dValLog = m_dMin + (nSliderPos - nSliderStart)*(m_dMax - m_dMin)/(nSliderEnd - nSliderStart);
	return m_bLogarithmic ? pow(10, m_nSign*dValLog) : m_nSign*dValLog;
}
