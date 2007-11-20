#include "StdAfx.h"
#include "GUIControls.h"
#include "Helpers.h"
#include <math.h>

#define SLIDER_WIDTH 130
#define SLIDER_HEIGHT 30
#define WIDTH_ADD_PIXELS 63
#define SLIDER_GAP 23
#define SCREEN_Y_OFFSET 5
#define WINPROP_THIS _T("JV_THIS")

/////////////////////////////////////////////////////////////////////////////////////////////
// CSliderMgr
/////////////////////////////////////////////////////////////////////////////////////////////

static CSize GetTextRect(HWND hWnd, LPCTSTR sText) {
	CPaintDC dc(hWnd);
	dc.SelectStockFont(DEFAULT_GUI_FONT);
	CSize textlen;
	dc.GetTextExtent(sText, _tcslen(sText), &textlen);
	return textlen;
}

static CSize GetTextRect(CDC & dc, LPCTSTR sText) {
	dc.SelectStockFont(DEFAULT_GUI_FONT);
	CSize textlen;
	dc.GetTextExtent(sText, _tcslen(sText), &textlen);
	return textlen;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CSliderMgr
/////////////////////////////////////////////////////////////////////////////////////////////

// Gets the screen size of the screen containing the point with given virtual desktop coordinates
static CSize GetScreenSize(int nX, int nY) {
	HMONITOR hMonitor = ::MonitorFromPoint(CPoint(nX, nY), MONITOR_DEFAULTTONEAREST);
	MONITORINFO monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFO);
	if (::GetMonitorInfo(hMonitor, &monitorInfo)) {
		return CSize(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top);
	} else {
		return CSize(::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
	}
}

CSliderMgr::CSliderMgr(HWND hWnd) {
	m_hWnd = hWnd;
	m_fDPIScale = Helpers::ScreenScaling;
	m_nSliderWidth = (int)(SLIDER_WIDTH*m_fDPIScale);
	m_nNoLabelWidth = m_nSliderWidth + (int)(WIDTH_ADD_PIXELS*m_fDPIScale);
	m_nSliderHeight = (int)(SLIDER_HEIGHT*m_fDPIScale);
	m_nSliderGap = (int) (SLIDER_GAP*m_fDPIScale);
	m_nTotalAreaHeight = m_nSliderHeight*3 + SCREEN_Y_OFFSET;
	m_clientRect = CRect(0, 0, 0, 0);
	m_sliderAreaRect = CRect(0, 0, 0, 0);

	m_pCtrlCaptureMouse = NULL;
	m_nTotalSliders = 0;
	m_nYStart = 0xFFFF;
}

CSliderMgr::~CSliderMgr(void) {
	std::list<CUICtrl*>::iterator iter;
	for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
		delete *iter;
	}
	m_ctrlList.clear();
}

void CSliderMgr::AddSlider(LPCTSTR sName, double* pdValue, bool* pbEnable, 
						   double dMin, double dMax, bool bLogarithmic, bool bInvert) {
	CSliderDouble* pSlider = new CSliderDouble(this, sName, m_nSliderWidth, pdValue, pbEnable, dMin, dMax, 
		bLogarithmic, bInvert);
	m_ctrlList.push_back(pSlider);
	m_nTotalSliders++;
}

CTextCtrl* CSliderMgr::AddText(LPCTSTR sTextInit, bool bEditable, TextChangedHandler textChangedHandler) {
	CTextCtrl* pTextCtrl = new CTextCtrl(this, sTextInit, bEditable, textChangedHandler);
	m_ctrlList.push_back(pTextCtrl);
	return pTextCtrl;
}

CButtonCtrl* CSliderMgr::AddButton(LPCTSTR sButtonText, ButtonPressedHandler buttonPressedHandler) {
	CButtonCtrl* pButtonCtrl = new CButtonCtrl(this, sButtonText, buttonPressedHandler);
	m_ctrlList.push_back(pButtonCtrl);
	return pButtonCtrl;
}

void CSliderMgr::ShowHideSlider(bool bShow, double* pdValue) {
	std::list<CUICtrl*>::iterator iter;
	for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
		CSliderDouble* pSlider = dynamic_cast<CSliderDouble*>(*iter);
		if (pSlider != NULL) {
			if (pSlider->GetValuePtr() == pdValue) {
				pSlider->SetShow(bShow);
			}
		}
	}
}

bool CSliderMgr::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	bool bHandled = false;
	CUICtrl* ctrlCapturedMouse = m_pCtrlCaptureMouse;
	std::list<CUICtrl*>::iterator iter;
	for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
		if ((*iter)->IsShown() && (ctrlCapturedMouse == NULL || *iter == ctrlCapturedMouse)) {
			if ((*iter)->OnMouseLButton(eMouseEvent, nX, nY)) {
				bHandled = true;
			}
		}
	}
	if (nY > m_nYStart) {
		return true;
	}
	return bHandled;
}

bool CSliderMgr::OnMouseMove(int nX, int nY) {
	bool bHandled = false;
	CUICtrl* ctrlCapturedMouse = m_pCtrlCaptureMouse;
	std::list<CUICtrl*>::iterator iter;
	for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
		if ((*iter)->IsShown() && (ctrlCapturedMouse == NULL || *iter == ctrlCapturedMouse)) {
			if ((*iter)->OnMouseMove(nX, nY)) {
				bHandled = true;
			}
		}
	}
	return bHandled;
}

void CSliderMgr::OnPaint(CPaintDC & dc) {
	RepositionAll();
	std::list<CUICtrl*>::iterator iter;
	for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
		if ((*iter)->IsShown()) {
			(*iter)->OnPaint(dc);
		}
	}
}

CRect CSliderMgr::SliderAreaRect() {
	CRect clientRect;
	::GetClientRect(m_hWnd, &clientRect);

	// Check if client rectangle unchanged - no repositioning needed
	if (m_clientRect == clientRect) {
		return m_sliderAreaRect;
	}
	m_clientRect = clientRect;

	CRect wndRect;
	::GetWindowRect(m_hWnd, &wndRect);
	int nYStart = clientRect.Height() - m_nTotalAreaHeight;
	int nXStart = 0;

	// Place on second monitor if its height is larger
	CSize sizeScreen = GetScreenSize(wndRect.left + 10, wndRect.top + nYStart);
	if (sizeScreen.cy < clientRect.Height() - 5) {
		CSize sizeScreen2 = GetScreenSize(wndRect.left + sizeScreen.cx + 10, wndRect.top + nYStart);
		if (sizeScreen2.cy > sizeScreen.cy) {
			nXStart += sizeScreen.cx;
			sizeScreen = sizeScreen2;
		}
	}

	m_sliderAreaRect = CRect(nXStart, nYStart, sizeScreen.cx + nXStart, nYStart + m_nTotalAreaHeight);
	return m_sliderAreaRect;
}

void CSliderMgr::RepositionAll() {
	const int X_OFFSET = 5;

	CRect sliderAreaRect = SliderAreaRect();
	int nXStart = sliderAreaRect.left + X_OFFSET;
	int nYStart = sliderAreaRect.top;

	// Place at bottom of window, grouping sliders in three rows and n-columns,
	// add remaining controls afterwards
	
	m_nYStart = nYStart;
	int nXLimitScreen = sliderAreaRect.right;
	int nX = nXStart, nY = 0;
	int nSliderIdx = 0;
	int nTotalSliderIdx = 0;
	int nSliderEndX = 0;
	int nNumRightAligned = 0;
	CSliderDouble* pSliderColumn[3];
	CTextCtrl* pLastTextCtrl = NULL;
	std::list<CUICtrl*>::iterator iter;
	for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
		CSliderDouble* pSlider = dynamic_cast<CSliderDouble*>(*iter);
		if (pSlider != NULL) {
			nTotalSliderIdx++;
			if (pSlider->IsShown() || nTotalSliderIdx == m_nTotalSliders) {
				pSliderColumn[nSliderIdx] = pSlider;
				if (pSlider->IsShown()) nSliderIdx++;
				if (nSliderIdx == 3 || nTotalSliderIdx == m_nTotalSliders) {
					int nTotalWidth = 0;
					for (int i = 0; i < nSliderIdx; i++) {
						nTotalWidth = max(nTotalWidth, pSliderColumn[i]->GetNameLabelWidth());
					}
					nTotalWidth += m_nNoLabelWidth;
					for (int i = 0; i < nSliderIdx; i++) {
						pSliderColumn[i]->SetPosition(CRect(nX, nYStart + i*m_nSliderHeight, nX + nTotalWidth - m_nSliderGap, nYStart + (i+1)*m_nSliderHeight));
					}
					nSliderEndX = nX + nTotalWidth;
					if (nSliderIdx == 3) {
						nX += nTotalWidth;
						nSliderIdx = 0;
					} else {
						nY = nYStart + nSliderIdx*m_nSliderHeight;
					}
				}
			}
		} else {
			CTextCtrl* pTextCtrl = dynamic_cast<CTextCtrl*>(*iter);
			if (pTextCtrl != NULL) {
				int nTextWidth = pTextCtrl->GetTextLabelWidth() + 16;

				if (pTextCtrl->IsRightAligned()) {
					nX = nSliderEndX;
					nY = nYStart + nNumRightAligned*m_nSliderHeight;
					nNumRightAligned++;
				}

				// Special behaviour if not enough room - remove labels for editable texts
				if (pTextCtrl->IsEditable() && pLastTextCtrl != NULL && !pLastTextCtrl->IsEditable()) {
					if (nX + nTextWidth > nXLimitScreen) {
						pLastTextCtrl->SetShow(false);
						nX -= pLastTextCtrl->GetPosition().Width();
					} else {
						pLastTextCtrl->SetShow(true);
					}
				}
				if (pTextCtrl->IsRightAligned()) {
					nTextWidth = nXLimitScreen - nX - 10;
				} else {
					// limit text on screen boundary
					if (nX + nTextWidth >= nXLimitScreen) {
						nTextWidth = nXLimitScreen - nX - 1;
					}
				}
				// only show editable control if large enough
				if (pTextCtrl->IsEditable()) {
					pTextCtrl->SetShow(nTextWidth > 40);
				} else {
					pTextCtrl->SetShow(nTextWidth > 0);
				}
				pTextCtrl->SetPosition(CRect(nX, nY, nX + nTextWidth, nY + m_nSliderHeight));
				pTextCtrl->SetMaxTextWidth(nXLimitScreen - nX);
				nX += nTextWidth;
				
			} else {
				CButtonCtrl* pButtonCtrl = dynamic_cast<CButtonCtrl*>(*iter);
				if (pButtonCtrl != NULL) {
					int nButtonWidth = pButtonCtrl->GetTextLabelWidth() + 26;
					int nButtonHeigth = m_nSliderHeight*6/10;
					int nStartY = nY + ((m_nSliderHeight - nButtonHeigth) >> 1);
					pButtonCtrl->SetPosition(CRect(nX, nStartY, nX + nButtonWidth, nStartY + nButtonHeigth));
					nX += nButtonWidth + 16;
				}
			}
			pLastTextCtrl = pTextCtrl;
		}
	}
}

void CSliderMgr::CaptureMouse(CUICtrl* pCtrl) {
	if (m_pCtrlCaptureMouse == NULL) m_pCtrlCaptureMouse = pCtrl;
}

void CSliderMgr::ReleaseMouse(CUICtrl* pCtrl) {
	if (pCtrl == m_pCtrlCaptureMouse) m_pCtrlCaptureMouse = NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CUICtrl
/////////////////////////////////////////////////////////////////////////////////////////////

CUICtrl::CUICtrl(CSliderMgr* pSliderMgr) { 
	m_pMgr = pSliderMgr; 
	m_bShow = true; 
	m_bHighlight = false; 
	m_position = CRect(0, 0, 0, 0); 
}

void CUICtrl::OnPaint(CDC & dc) {
	dc.SelectStockFont(DEFAULT_GUI_FONT);
	dc.SetBkMode(TRANSPARENT);

	CRect pos(m_position);
	pos.OffsetRect(-1, 0); Draw(dc, pos, true);
	pos.OffsetRect(2, 0); Draw(dc, pos, true);
	pos.OffsetRect(-1, -1); Draw(dc, pos, true);
	pos.OffsetRect(0, 2); Draw(dc, pos, true);
	Draw(dc, m_position, false);
}

void CUICtrl::DrawGetDC(bool bBlack) {
	HDC hDC = ::GetDC(m_pMgr->GetHWND());
	CDC dc(hDC);
	Draw(dc, m_position, bBlack);
	dc.Detach();
	::ReleaseDC(m_pMgr->GetHWND(), hDC);
}

void CUICtrl::SetShow(bool bShow) { 
	if (bShow != m_bShow) {
		m_bShow = bShow;
		::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE); // trigger redraw of area
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CTextCtrl
/////////////////////////////////////////////////////////////////////////////////////////////

CTextCtrl::CTextCtrl(CSliderMgr* pSliderMgr, LPCTSTR sTextInit, bool bEditable, 
					 TextChangedHandler textChangedHandler) : CUICtrl(pSliderMgr) {
	 m_bEditable = bEditable;
	 m_bRightAligned = false;
	 m_textChangedHandler = textChangedHandler;
	 m_nTextWidth = 0;
	 m_nMaxTextWidth = 200;
	 m_pEdit = NULL;
	 SetText((sTextInit == NULL) ? _T("") : sTextInit);
}

void CTextCtrl::SetText(LPCTSTR sText) {
	m_sText = sText;
	m_nTextWidth = GetTextRect(m_pMgr->GetHWND(), m_sText).cx;
	m_pMgr->RequestRepositioning();
	TerminateEditMode(false);
}

void CTextCtrl::SetEditable(bool bEditable) {
	m_bEditable = bEditable;
	TerminateEditMode(false);
}

void CTextCtrl::TerminateEditMode(bool bAcceptNewName) {
	if (m_pEdit) {
		TCHAR newName[MAX_PATH];
		m_pEdit->GetWindowText(newName, MAX_PATH);
		::RemoveProp(m_pEdit->m_hWnd, WINPROP_THIS);
		::SetWindowLong(m_pEdit->m_hWnd, GWL_WNDPROC, (LONG) m_OrigEditProc); 
		m_pEdit->DestroyWindow();
		delete m_pEdit;
		::SetFocus(m_pMgr->GetHWND());
		m_pEdit = NULL;
		if (bAcceptNewName) {
			if (_tcscmp(newName, m_sText) != 0) {
				bool bSetNewName = true;
				if (m_textChangedHandler != NULL) {
					bSetNewName = m_textChangedHandler(*this, newName);
				}
				if (bSetNewName) SetText(newName);
			}
		}
		CRect rect(m_position);
		rect.right = rect.left + m_nTextWidth + 20;
		::InvalidateRect(m_pMgr->GetHWND(), &rect, FALSE);
	}
}

bool CTextCtrl::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	if (m_bEditable) {
		CPoint mousePos(nX, nY);
		if (m_position.PtInRect(mousePos)) {
			if (m_pEdit == NULL && eMouseEvent == MouseEvent_BtnDown) {
				CRect rectEdit = m_position;
				int nDiff = m_position.Height()/6;
				rectEdit.left -= 2;
				rectEdit.right = rectEdit.left + m_nMaxTextWidth;
				rectEdit.top += nDiff + 1;
				rectEdit.bottom -= nDiff - 1;
				m_pEdit = new CEdit();
				m_pEdit->Create(m_pMgr->GetHWND(), _U_RECT(&rectEdit), 0, WS_BORDER | WS_CHILD | ES_AUTOHSCROLL, 0);
				
				// Subclass edit control to capture the ESC and ENTER keys
				// ms-help://MS.VSExpressCC.v80/MS.VSIPCC.v80/MS.PSDKSVR2003R2.1033/winui/winui/windowsuserinterface/windowing/windowprocedures/usingwindowprocedures.htm
				m_OrigEditProc = (WNDPROC) ::SetWindowLong(m_pEdit->m_hWnd, GWL_WNDPROC, (LONG) EditSubclassProc); 
				SetProp(m_pEdit->m_hWnd, WINPROP_THIS, (HANDLE) this);

				m_pEdit->SetFont((HFONT)::GetStockObject(DEFAULT_GUI_FONT));
				m_pEdit->SetWindowText(m_sText);
				m_pEdit->LimitText(256);
				m_pEdit->ShowWindow(SW_SHOW);
				m_pEdit->SetFocus();
				m_pEdit->SetSelAll();
			}
		} else {
			TerminateEditMode(true);
		}
	}
	return false;
}

bool CTextCtrl::OnMouseMove(int nX, int nY) {
	if (m_bEditable && m_pEdit == NULL) {
		CPoint mousePos(nX, nY);
		if (m_position.PtInRect(mousePos)) {
			if (!m_bHighlight) {
				m_bHighlight = true;
				::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE);
				//DrawGetDC(false);
			}
		} else if (m_bHighlight) {
			m_bHighlight = false;
			::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE);
			//DrawGetDC(false);
		}
	}

	return m_pEdit != NULL;
}

void CTextCtrl::Draw(CDC & dc, CRect position, bool bBlack) {
	dc.SelectStockFont(DEFAULT_GUI_FONT);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(bBlack ? 0 : m_bHighlight ? RGB(255, 255, 255) : RGB(0, 255, 0));
	unsigned int nAlignment = m_bRightAligned ? DT_RIGHT : DT_LEFT;
	dc.DrawText(m_sText, m_sText.GetLength(), &position, nAlignment | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS);
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
// CButtonCtrl
/////////////////////////////////////////////////////////////////////////////////////////////

CButtonCtrl::CButtonCtrl(CSliderMgr* pSliderMgr, LPCTSTR sButtonText, 
						 ButtonPressedHandler buttonPressedHandler) : CUICtrl(pSliderMgr) {
	 m_sText = sButtonText;
	 m_buttonPressedHandler = buttonPressedHandler;
	 m_nTextWidth = GetTextRect(m_pMgr->GetHWND(), m_sText).cx;
	 m_bDragging = false;
}

bool CButtonCtrl::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	bool bRetVal = false;
	CPoint mousePos(nX, nY);
	if (m_position.PtInRect(mousePos)) {
		if (eMouseEvent == MouseEvent_BtnDown) {
			m_pMgr->CaptureMouse(this);
			m_bDragging = true;
			::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE);
			//DrawGetDC(false);
			return true;
		}
		if (eMouseEvent == MouseEvent_BtnUp && m_buttonPressedHandler != NULL) {
			m_buttonPressedHandler(*this);
			bRetVal = true;
		}
	}
	
	if (m_bDragging) {
		m_pMgr->ReleaseMouse(this);
		m_bDragging = false;
		::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE);
		//DrawGetDC(false);
	}
	return bRetVal;
}

bool CButtonCtrl::OnMouseMove(int nX, int nY) {
	CPoint mousePos(nX, nY);
	if (m_position.PtInRect(mousePos)) {
		if (!m_bHighlight) {
			m_bHighlight = true;
			::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE);
			//DrawGetDC(false);
		}
	} else if (m_bHighlight) {
		m_bHighlight = false;
		::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE);
		//DrawGetDC(false);
	}

	return false;
}

void CButtonCtrl::Draw(CDC & dc, CRect position, bool bBlack) {
	dc.SelectStockBrush(HOLLOW_BRUSH);
	HPEN hPen, hOldPen;
	if (bBlack) {
		dc.SelectStockPen(BLACK_PEN);
	} else {
		hPen = ::CreatePen(PS_SOLID, 1, (m_bDragging && m_bHighlight) ? RGB(255, 255, 255) : RGB(0, 255, 0));
		hOldPen = dc.SelectPen(hPen);
	}
	dc.Rectangle(position);
	dc.SelectStockFont(DEFAULT_GUI_FONT);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(bBlack ? 0 : m_bHighlight ? RGB(255, 255, 255) : RGB(0, 255, 0));
	dc.DrawText(m_sText, m_sText.GetLength(), &position, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS);
	if (!bBlack) {
		dc.SelectPen(hOldPen);
		::DeleteObject(hPen);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CSliderDouble
/////////////////////////////////////////////////////////////////////////////////////////////

CSliderDouble::CSliderDouble(CSliderMgr* pMgr, LPCTSTR sName, int nSliderLen, double* pdValue, bool* pbEnable,
							 double dMin, double dMax, bool bLogarithmic, bool bInvert) : CUICtrl(pMgr) {
	m_sName = sName;
	m_nSliderLen = nSliderLen;
	m_pValue = pdValue;
	m_pEnable = pbEnable;
	m_dMin = bLogarithmic ? log10(dMin) : dMin;;
	m_dMax = bLogarithmic ? log10(dMax) : dMax;
	m_sliderRect = CRect(0, 0, 0, 0);
	m_checkRect = CRect(0, 0, 0, 0);
	m_checkRectFull = CRect(0, 0, 0, 0);
	m_bLogarithmic = bLogarithmic;
	m_nSign = bInvert ? -1 : +1;
	m_bDragging = false;
	m_bHighlightCheck = false;

	CPaintDC dc(m_pMgr->GetHWND());
	m_nNumberWidth = GetTextRect(dc, _T("--X.XX")).cx;
	int nNameWidth = GetTextRect(dc, sName).cx;
	m_nSliderHeight = m_nNumberWidth/6;
	m_nCheckHeight = pbEnable ? m_nNumberWidth/3 : 0;
	m_nNameWidth = pbEnable ? nNameWidth + m_nCheckHeight + m_nCheckHeight/2 : nNameWidth;
}

bool CSliderDouble::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	CPoint mousePos(nX, nY);

	if (eMouseEvent == MouseEvent_BtnDown) {
		if (m_position.PtInRect(mousePos)) m_pMgr->CaptureMouse(this);
	} else {
		m_pMgr->ReleaseMouse(this);
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
		::InvalidateRect(m_pMgr->GetHWND(), NULL, FALSE); // trigger redraw of window
	}
	return false;
}

bool CSliderDouble::OnMouseMove(int nX, int nY) {
	CPoint mousePos(nX, nY);

	if (m_pEnable && !m_bDragging) {
		if (m_checkRectFull.PtInRect(mousePos)) {
			if (!m_bHighlightCheck) {
				m_bHighlightCheck = true;
				DrawCheckGetDC(true);
			}
		} else if (m_bHighlightCheck) {
			m_bHighlightCheck = false;
			DrawCheckGetDC(false);
		}
	}

	if (m_pEnable && !*m_pEnable) return false;

	if (m_bDragging) {
		double dOldValue = *m_pValue;
		*m_pValue = SliderPosToValue(nX, m_nSliderStart, m_nSliderStart + m_nSliderLen);
		if (dOldValue != *m_pValue) {
			::InvalidateRect(m_pMgr->GetHWND(), NULL, FALSE);
		}
		return true;
	} else if (m_sliderRect.PtInRect(mousePos)) {
		if (!m_bHighlight) {
			m_bHighlight = true;
			DrawRulerGetDC(true);
		}
	} else if (m_bHighlight) {
		m_bHighlight = false;
		DrawRulerGetDC(false);
	}

	return false;
}

void CSliderDouble::OnPaint(CDC & dc)  {
	if (m_pEnable && !*m_pEnable) m_bHighlight = false;
	CUICtrl::OnPaint(dc);
}

void CSliderDouble::Draw(CDC & dc, CRect position, bool bBlack) {
	dc.SetTextColor(bBlack ? 0 : RGB(0, 255, 0));
	TCHAR buff[16]; _stprintf_s(buff, 16, _T("%.2f"), *m_pValue);
	dc.DrawText(buff, _tcslen(buff), &position, DT_VCENTER | DT_SINGLELINE | DT_RIGHT);

	if (m_pEnable != NULL) {
		m_checkRect = CRect(CPoint(position.left, position.top + ((position.Height() - m_nCheckHeight) >> 1)),
			CSize(m_nCheckHeight, m_nCheckHeight));
		m_checkRectFull = CRect(m_checkRect.left, m_checkRect.top, 
			m_checkRect.right + m_nCheckHeight/2 + m_nNameWidth, m_checkRect.bottom);
		DrawCheck(dc, m_checkRect, bBlack, m_bHighlightCheck);
		position.left += m_nCheckHeight + m_nCheckHeight/2;
	}
	dc.DrawText(m_sName, _tcslen(m_sName), &position, DT_VCENTER | DT_SINGLELINE);
	
	int nYMid = position.top + ((position.bottom - position.top) >> 1);
	int nXPos = position.right - m_nNumberWidth;
	m_nSliderStart = nXPos - m_nSliderLen;
	DrawRuler(dc, m_nSliderStart, nXPos, nYMid, bBlack, m_bHighlight);
	m_sliderRect = CRect(m_nSliderStart - m_nSliderHeight, nYMid - 2, nXPos + m_nSliderHeight, nYMid + m_nSliderHeight + 8);
}

void CSliderDouble::DrawRulerGetDC(bool bHighlight) {
	HDC hDC = ::GetDC(m_pMgr->GetHWND());
	CDC dc(hDC);
	int nYMid = m_position.top + ((m_position.bottom - m_position.top) >> 1);
	DrawRuler(dc, m_nSliderStart, m_nSliderStart + m_nSliderLen, nYMid, false, bHighlight);
	dc.Detach();
	::ReleaseDC(m_pMgr->GetHWND(), hDC);
}

void CSliderDouble::DrawRuler(CDC & dc, int nXStart, int nXEnd, int nY, bool bBlack, bool bHighlight) {
	HPEN hPen = 0, hPenRed = 0;
	if (bBlack) {
		dc.SelectStockPen(BLACK_PEN);
	} else {
		hPen = ::CreatePen(PS_SOLID, 1, bHighlight ? RGB(255, 255, 255) : RGB(0, 255, 0));
		hPenRed = ::CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
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
		dc.LineTo((int)(fX + 0.5f), nY  - nSub);
		fX += fStep;
	}

	if (m_pEnable == NULL || *m_pEnable) {
		int nSliderPos = ValueToSliderPos(*m_pValue, nXStart, nXEnd);
		if (!bBlack) dc.SelectPen(hPenRed);
		dc.MoveTo(nSliderPos, nY+3);
		dc.LineTo(nSliderPos - m_nSliderHeight, nY + 3 + m_nSliderHeight);
		dc.LineTo(nSliderPos + m_nSliderHeight, nY + 3 + m_nSliderHeight);
		dc.LineTo(nSliderPos, nY + 3);
	}

	if (!bBlack) {
		dc.SelectStockPen(BLACK_PEN);
		::DeleteObject(hPen);
		::DeleteObject(hPenRed);
	}
}

void CSliderDouble::DrawCheckGetDC(bool bHighlight) {
	HDC hDC = ::GetDC(m_pMgr->GetHWND());
	CDC dc(hDC);
	DrawCheck(dc, m_checkRect, false, bHighlight);
	dc.Detach();
	::ReleaseDC(m_pMgr->GetHWND(), hDC);
}

void CSliderDouble::DrawCheck(CDC & dc, CRect position, bool bBlack, bool bHighlight) {
	HPEN hPen = 0;
	if (bBlack) {
		dc.SelectStockPen(BLACK_PEN);
	} else {
		hPen = ::CreatePen(PS_SOLID, 1, bHighlight ? RGB(255, 255, 255) : RGB(0, 255, 0));
		dc.SelectPen(hPen);
	}

	dc.SelectStockBrush(HOLLOW_BRUSH);
	dc.Rectangle(position);
	if (*m_pEnable) {
		dc.MoveTo(position.left, position.top + position.Height()/2 - 1);
		dc.LineTo(position.left + position.Width()/3, position.bottom - 2);
		dc.LineTo(position.right - 1, position.top - 1);
	}

	if (!bBlack) {
		dc.SelectStockPen(BLACK_PEN);
		::DeleteObject(hPen);
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