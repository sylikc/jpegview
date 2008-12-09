#include "StdAfx.h"
#include "GUIControls.h"
#include "Helpers.h"
#include <math.h>

#define SLIDER_WIDTH 130
#define MIN_SLIDER_WIDTH 40
#define SLIDER_HEIGHT 30
#define WIDTH_ADD_PIXELS 63
#define SLIDER_GAP 23
#define SCREEN_Y_OFFSET 5
#define NAV_PANEL_HEIGHT 32
#define NAV_PANEL_BORDER 6
#define NAV_PANEL_GAP 5
#define WNDBUTTON_PANEL_HEIGHT 24
#define WNDBUTTON_BORDER 1
#define WINPROP_THIS _T("JV_THIS")

/////////////////////////////////////////////////////////////////////////////////////////////
// static helper functions
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

/////////////////////////////////////////////////////////////////////////////////////////////
// Tooltip support
/////////////////////////////////////////////////////////////////////////////////////////////

CTooltip::CTooltip(HWND hWnd, const CUICtrl* pBoundCtrl, LPCTSTR sTooltip) : 
	m_hWnd(hWnd), m_pBoundCtrl(pBoundCtrl), m_sTooltip(sTooltip), m_TooltipRect(0, 0, 0, 0), m_oldRectBoundCtrl(0, 0, 0, 0) {
	m_ttHandler = NULL;
}

CTooltip::CTooltip(HWND hWnd, const CUICtrl* pBoundCtrl, TooltipHandler ttHandler) :
	m_hWnd(hWnd), m_pBoundCtrl(pBoundCtrl), m_TooltipRect(0, 0, 0, 0) {
	m_ttHandler = ttHandler;
}

const CString& CTooltip::GetTooltip() const {
	if (m_ttHandler != NULL) {
		LPCTSTR sNewTooltip = m_ttHandler();
		if (m_sTooltip != sNewTooltip) {
			const_cast<CTooltip*>(this)->m_sTooltip = sNewTooltip;
			const_cast<CTooltip*>(this)->m_TooltipRect = CRect(0, 0, 0, 0);
		}
	}
	return m_sTooltip;
}

void CTooltip::Paint(CDC & dc) const {
	const CString& sTooltip = GetTooltip();
	CRect tooltipRect = GetTooltipRect();
	CBrush brush;
	brush.CreateSolidBrush(RGB(255, 255, 200));
	dc.SelectBrush(brush);
	dc.SelectStockPen(BLACK_PEN);
	dc.Rectangle(tooltipRect.left, tooltipRect.top, tooltipRect.right, tooltipRect.bottom);
	dc.SetTextColor(RGB(0, 0, 0));
	dc.SetBkColor(RGB(255, 255, 200));
	dc.SelectStockFont(DEFAULT_GUI_FONT);
	tooltipRect.OffsetRect(0, 2);
	dc.DrawText(sTooltip, sTooltip.GetLength(), tooltipRect, DT_CENTER | DT_VCENTER | DT_NOPREFIX);
	dc.SelectStockBrush(BLACK_BRUSH);
}

CRect CTooltip::GetTooltipRect() const {
	GetTooltip();
	if (m_pBoundCtrl->GetPosition() != m_oldRectBoundCtrl || m_TooltipRect.IsRectEmpty()) {
		const_cast<CTooltip*>(this)->m_oldRectBoundCtrl = m_pBoundCtrl->GetPosition();
		const_cast<CTooltip*>(this)->m_TooltipRect = CalculateTooltipRect();
	}
	return m_TooltipRect;
}

CRect CTooltip::CalculateTooltipRect() const {
	HDC dc = ::GetDC(m_hWnd);
	::SelectObject(dc, (HGDIOBJ)::GetStockObject(DEFAULT_GUI_FONT));
	CRect rectText(0, 0, 400, 0);
	const CString& sTooltip = GetTooltip();
	::DrawText(dc, sTooltip, sTooltip.GetLength(), &rectText, DT_CENTER | DT_VCENTER | DT_CALCRECT | DT_NOPREFIX);
	::ReleaseDC(m_hWnd, dc);

	CRect boundRect = m_pBoundCtrl->GetPosition();
	int nLeft = boundRect.CenterPoint().x - rectText.Width()/2;
	int nTop = boundRect.bottom + 6;
	return CRect(nLeft, nTop, nLeft + rectText.Width() + 8, nTop + rectText.Height() + 5);
}

CTooltipMgr::CTooltipMgr(HWND hWnd) {
	m_hWnd = hWnd;
	m_pActiveTooltip = NULL;
	m_pMouseOnTooltip = NULL;
	m_pBlockedTooltip = NULL;
	m_bEnableTooltips = true;
}

void CTooltipMgr::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	if (eMouseEvent == MouseEvent_BtnDown) {
		m_pBlockedTooltip = m_pMouseOnTooltip;
		RemoveActiveTooltip();
	}
}

void CTooltipMgr::OnMouseMove(int nX, int nY) {
	// Find the control the mouse is currently on
	CTooltip* pMouseOnTooltip = NULL;
	std::list<CTooltip*>::const_iterator iter;
	for (iter = m_TooltipList.begin( ); iter != m_TooltipList.end( ); iter++ ) {
		const CUICtrl* pCtrl = (*iter)->GetBoundCtrl();
		if (pCtrl->IsShown() && pCtrl->GetPosition().PtInRect(CPoint(nX, nY))) {
			pMouseOnTooltip = *iter;
			break;
		}
	}
	if (pMouseOnTooltip != m_pMouseOnTooltip) {
		m_pBlockedTooltip = NULL; // unblock showing tooltips when leaving current control
		RemoveActiveTooltip();
		m_pMouseOnTooltip = pMouseOnTooltip;
		m_pActiveTooltip = pMouseOnTooltip;
		if (m_pActiveTooltip != NULL && m_bEnableTooltips) {
			CRect rect(m_pActiveTooltip->GetTooltipRect());
			::InvalidateRect(m_hWnd, &rect, FALSE);
		}
	}
}

void CTooltipMgr::OnPaint(CDC & dc) {
	if (m_pActiveTooltip != NULL && m_bEnableTooltips) {
		m_pActiveTooltip->Paint(dc);
	}
}

void CTooltipMgr::AddTooltip(CUICtrl* pBoundCtrl, CTooltip* tooltip) {
	std::list<CTooltip*>::const_iterator iter;
	for (iter = m_TooltipList.begin( ); iter != m_TooltipList.end( ); iter++ ) {
		if ((*iter)->GetBoundCtrl() == pBoundCtrl) {
			m_TooltipList.remove(*iter);
			delete *iter;
			break;
		}
	}
	if (tooltip != NULL) {
		m_TooltipList.push_back(tooltip);
	}
}

void CTooltipMgr::AddTooltip(CUICtrl* pBoundCtrl, LPCTSTR sTooltip) {
	if (sTooltip != NULL && sTooltip[0] != 0) {
		AddTooltip(pBoundCtrl, new CTooltip(m_hWnd, pBoundCtrl, sTooltip));
	} else {
		AddTooltip(pBoundCtrl, (CTooltip*)NULL);
	}
}

void CTooltipMgr::AddTooltipHandler(CUICtrl* pBoundCtrl, TooltipHandler ttHandler) {
	AddTooltip(pBoundCtrl, new CTooltip(m_hWnd, pBoundCtrl, ttHandler));
}

void CTooltipMgr::RemoveActiveTooltip() {
	if (m_pActiveTooltip != NULL && m_bEnableTooltips) {
		CRect rect(m_pActiveTooltip->GetTooltipRect());
		::InvalidateRect(m_hWnd, &rect, FALSE);
	}
	m_pActiveTooltip = NULL;
}

void CTooltipMgr::EnableTooltips(bool bEnable) {
	if (bEnable != m_bEnableTooltips) {
		m_bEnableTooltips = bEnable;
		if (m_pActiveTooltip != NULL) {
			CRect rect(m_pActiveTooltip->GetTooltipRect());
			::InvalidateRect(m_hWnd, &rect, FALSE);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CPanelMgr
/////////////////////////////////////////////////////////////////////////////////////////////

CPanelMgr::CPanelMgr(HWND hWnd) : m_tooltipMgr(hWnd) {
	m_hWnd = hWnd;
	m_fDPIScale = Helpers::ScreenScaling;
	m_pCtrlCaptureMouse = NULL;
}

CPanelMgr::~CPanelMgr(void) {
	std::list<CUICtrl*>::iterator iter;
	for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
		delete *iter;
	}
	m_ctrlList.clear();
}

void CPanelMgr::AddSlider(LPCTSTR sName, double* pdValue, bool* pbEnable, 
						   double dMin, double dMax, bool bLogarithmic, bool bInvert, int nWidth) {
	CSliderDouble* pSlider = new CSliderDouble(this, sName, nWidth, pdValue, pbEnable, dMin, dMax, 
		bLogarithmic, bInvert);
	m_ctrlList.push_back(pSlider);
}

CTextCtrl* CPanelMgr::AddText(LPCTSTR sTextInit, bool bEditable, TextChangedHandler textChangedHandler) {
	CTextCtrl* pTextCtrl = new CTextCtrl(this, sTextInit, bEditable, textChangedHandler);
	m_ctrlList.push_back(pTextCtrl);
	return pTextCtrl;
}

CButtonCtrl* CPanelMgr::AddButton(LPCTSTR sButtonText, ButtonPressedHandler buttonPressedHandler) {
	CButtonCtrl* pButtonCtrl = new CButtonCtrl(this, sButtonText, buttonPressedHandler);
	m_ctrlList.push_back(pButtonCtrl);
	return pButtonCtrl;
}

CButtonCtrl* CPanelMgr::AddUserPaintButton(PaintHandler paintHandler, ButtonPressedHandler buttonPressedHandler, 
										   LPCTSTR sTooltip) {
	CButtonCtrl* pButtonCtrl = new CButtonCtrl(this, paintHandler, buttonPressedHandler);
	if (sTooltip != NULL) {
		pButtonCtrl->SetTooltip(sTooltip);
	}
	m_ctrlList.push_back(pButtonCtrl);
	return pButtonCtrl;
}

CButtonCtrl* CPanelMgr::AddUserPaintButton(PaintHandler paintHandler, ButtonPressedHandler buttonPressedHandler, TooltipHandler ttHandler) {
	CButtonCtrl* pButtonCtrl = new CButtonCtrl(this, paintHandler, buttonPressedHandler);
	pButtonCtrl->SetTooltipHandler(ttHandler);
	m_ctrlList.push_back(pButtonCtrl);
	return pButtonCtrl;
}

void CPanelMgr::ShowHideSlider(bool bShow, double* pdValue) {
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

bool CPanelMgr::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	m_tooltipMgr.OnMouseLButton(eMouseEvent, nX, nY);

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
	return bHandled;
}

bool CPanelMgr::OnMouseMove(int nX, int nY) {
	m_tooltipMgr.OnMouseMove(nX, nY);

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

void CPanelMgr::OnPaint(CDC & dc, const CPoint& offset) {
	RepositionAll();
	std::list<CUICtrl*>::iterator iter;
	for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
		if ((*iter)->IsShown()) {
			(*iter)->OnPaint(dc, offset);
		}
	}
}

void CPanelMgr::CaptureMouse(CUICtrl* pCtrl) {
	if (m_pCtrlCaptureMouse == NULL) m_pCtrlCaptureMouse = pCtrl;
}

void CPanelMgr::ReleaseMouse(CUICtrl* pCtrl) {
	if (pCtrl == m_pCtrlCaptureMouse) m_pCtrlCaptureMouse = NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CSliderMgr
/////////////////////////////////////////////////////////////////////////////////////////////

CSliderMgr::CSliderMgr(HWND hWnd) : CPanelMgr(hWnd) {
	// slider width is smaller for very small screens
	::GetClientRect(hWnd, &m_clientRect);
	DetermineSliderWidth();
	m_nSliderHeight = (int)(SLIDER_HEIGHT*m_fDPIScale);
	m_nSliderGap = (int) (SLIDER_GAP*m_fDPIScale);
	m_nTotalAreaHeight = m_nSliderHeight*3 + SCREEN_Y_OFFSET;
	m_sliderAreaRect = CRect(0, 0, 0, 0);

	m_nYStart = 0xFFFF;
	m_nTotalSliders = 0;
}

void CSliderMgr::DetermineSliderWidth() {
	m_nSliderWidth = (int)(SLIDER_WIDTH*m_fDPIScale);
	int nNeededSpace = (int)(1200*m_fDPIScale);
	if (m_clientRect.Width() < nNeededSpace) {
		int nNeededGain = nNeededSpace - m_clientRect.Width();
		m_nSliderWidth = m_nSliderWidth - nNeededGain/4;
		m_nSliderWidth = max(MIN_SLIDER_WIDTH, m_nSliderWidth);
	}

	m_nNoLabelWidth = m_nSliderWidth + (int)(WIDTH_ADD_PIXELS*m_fDPIScale);
}

bool CSliderMgr::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	bool bHandled = CPanelMgr::OnMouseLButton(eMouseEvent, nX, nY);
	if (nY > m_nYStart) {
		return true;
	}
	return bHandled;
}

CRect CSliderMgr::PanelRect() {
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

	// Get the new width of the sliders and modify all existing sliders
	DetermineSliderWidth();
	std::list<CUICtrl*>::iterator iter;
	for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
		CSliderDouble* pSlider = dynamic_cast<CSliderDouble*>(*iter);
		if (pSlider != NULL) {
			pSlider->SetSliderLen(m_nSliderWidth);
		}
	}

	m_sliderAreaRect = CRect(nXStart, nYStart, min(clientRect.Width(), sizeScreen.cx) + nXStart, nYStart + m_nTotalAreaHeight);
	return m_sliderAreaRect;
}

void CSliderMgr::AddSlider(LPCTSTR sName, double* pdValue, bool* pbEnable, 
						   double dMin, double dMax, bool bLogarithmic, bool bInvert) {
	CPanelMgr::AddSlider(sName, pdValue, pbEnable, dMin, dMax, bLogarithmic, bInvert, m_nSliderWidth);
	m_nTotalSliders++;
}

void CSliderMgr::RepositionAll() {
	const int X_OFFSET = 5;

	CRect sliderAreaRect = PanelRect();
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
						pLastTextCtrl->m_bShow = false;
						nX -= pLastTextCtrl->GetPosition().Width();
					} else {
						pLastTextCtrl->m_bShow = true;
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
					pTextCtrl->m_bShow = (nTextWidth > 40);
				} else {
					pTextCtrl->m_bShow = (nTextWidth > 0);
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

/////////////////////////////////////////////////////////////////////////////////////////////
// CNavigationPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CNavigationPanel::CNavigationPanel(HWND hWnd, CSliderMgr* pSliderMgr) : CPanelMgr(hWnd) {
	m_pSliderMgr = pSliderMgr;
	m_clientRect = CRect(0, 0, 0, 0);
	m_nWidth = 0;
	m_nHeight = (int)(NAV_PANEL_HEIGHT*m_fDPIScale);
	m_nBorder = (int)(NAV_PANEL_BORDER*m_fDPIScale);
	m_nGap = (int)(NAV_PANEL_GAP*m_fDPIScale);
}

void CNavigationPanel::AddGap(int nGap) {
	CGapCtrl* pGapCtrl = new CGapCtrl(this, nGap);
	m_ctrlList.push_back(pGapCtrl);
}

CRect CNavigationPanel::PanelRect() {
	if (m_nWidth == 0) {
		int nNumButtons = 0;
		int nTotalGap = 0;
		std::list<CUICtrl*>::iterator iter;
		for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
			if (dynamic_cast<CButtonCtrl*>(*iter) != NULL) {
				nNumButtons++;
			} else {
				CGapCtrl* pGapCtrl = dynamic_cast<CGapCtrl*>(*iter);
				if (pGapCtrl != NULL) {
					nTotalGap += (int)(pGapCtrl->GapWidth()*m_fDPIScale);
				}
			}
		}
		int nButtonSize = m_nHeight - m_nBorder;
		m_nWidth = m_nBorder * 2 + (nNumButtons - 1) * m_nGap + nNumButtons * nButtonSize + nTotalGap;
	}

	CRect sliderRect = m_pSliderMgr->PanelRect();
	m_clientRect = CRect(CPoint((sliderRect.right + sliderRect.left - m_nWidth)/2, 
		((sliderRect.Width() < 800) ? (sliderRect.bottom - 26) : sliderRect.top) - m_nHeight),
		CSize(m_nWidth, m_nHeight));
	return m_clientRect;
}

bool CNavigationPanel::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	bool bHandled = CPanelMgr::OnMouseLButton(eMouseEvent, nX, nY);
	if (m_clientRect.PtInRect(CPoint(nX, nY))) {
		return true;
	}
	return bHandled;
}

void CNavigationPanel::RequestRepositioning() {
	RepositionAll();
}

void CNavigationPanel::RepositionAll() {
	m_clientRect = PanelRect();
	int nButtonSize = m_nHeight - m_nBorder;
	int nStartX = m_clientRect.left + m_nBorder;
	int nStartY = m_clientRect.top + m_nBorder/2;
	std::list<CUICtrl*>::iterator iter;
	for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
		CButtonCtrl* pButton = dynamic_cast<CButtonCtrl*>(*iter);
		if (pButton != NULL) {
			pButton->SetPosition(CRect(CPoint(nStartX, nStartY), CSize(nButtonSize, nButtonSize)));
			nStartX += nButtonSize + m_nGap;
		} else {
			CGapCtrl* pGap = dynamic_cast<CGapCtrl*>(*iter);
			if (pGap != NULL) {
				nStartX += (int)(pGap->GapWidth()*m_fDPIScale);
			}
		}
	}
}

static CRect InflateRect(const CRect& rect, float fAmount) {
	CRect r(rect);
	int nAmount = (int)(fAmount * r.Width());
	r.InflateRect(-nAmount, -nAmount);
	return r;
}

void CNavigationPanel::PaintHomeBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.3f);
	
	dc.MoveTo(r.left, r.top);
	dc.LineTo(r.left, r.bottom);

	int nX = r.left + (int)(r.Width()*0.4f);

	dc.MoveTo(nX, r.top);
	dc.LineTo(nX, r.bottom);

	dc.MoveTo(r.right, r.top+1);
	dc.LineTo(nX + 3, (r.bottom + r.top)/2);
	dc.LineTo(r.right+1, r.bottom);
}

void CNavigationPanel::PaintPrevBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.3f);

	int nX = r.left + (int)(r.Width()*0.2f);

	dc.MoveTo(nX, r.top);
	dc.LineTo(nX, r.bottom);

	int nX2 = nX + r.Height()/2 + 2;
	dc.MoveTo(nX2, r.top+1);
	dc.LineTo(nX + 3, (r.bottom + r.top)/2);
	dc.LineTo(nX2+1, r.bottom);
}

void CNavigationPanel::PaintNextBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.3f);

	int nX = r.left + (int)(r.Width()*0.2f);
	int nX2 = nX + r.Height()/2;

	dc.MoveTo(nX+1, r.top+1);
	dc.LineTo(nX2, (r.bottom + r.top)/2);
	dc.LineTo(nX, r.bottom);

	dc.MoveTo(nX2 + 3, r.top);
	dc.LineTo(nX2 + 3, r.bottom);
}

void CNavigationPanel::PaintEndBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.3f);

	int nX = r.left + r.Height()/2;

	dc.MoveTo(r.left+1, r.top+1);
	dc.LineTo(nX, (r.bottom + r.top)/2);
	dc.LineTo(r.left, r.bottom);

	dc.MoveTo(nX + 3, r.top);
	dc.LineTo(nX + 3, r.bottom);

	int nX2 = nX + 3 + (int)(r.Width()*0.4f);

	dc.MoveTo(nX2, r.top);
	dc.LineTo(nX2, r.bottom);
}

void CNavigationPanel::PaintZoomToFitBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.25f);

	int nW = r.Width()/3;
	int nD = r.Width()/2 - 1;

	for (int i = 0; i < 4; i++) {
		int nX = (i < 2) ? r.left : r.right;
		int nXP = (i < 2) ? nW : -nW;
		int nXD = (i < 2) ? nD : -nD;
		int nXA = (i < 2) ? 1 : -1;
		int nY = (i & 1) ? r.bottom : r.top;
		int nYP = (i & 1) ? -nW : nW;
		int nYD = (i & 1) ? -nD : nD;
		dc.MoveTo(nX, nY+nYP);
		dc.LineTo(nX, nY);
		dc.LineTo(nX+nXP+nXA, nY);
		dc.MoveTo(nX, nY);
		dc.LineTo(nX+nXD, nY+nYD);
	}
}

void CNavigationPanel::PaintZoomTo1to1Btn(const CRect& rect, CDC& dc) {
	CFont font;
	font.CreatePointFont(80, _T("MS Sans Serif"), dc, true, false);
	HFONT oldFont = dc.SelectFont(font);
	dc.DrawText(_T("1:1"), 3, (LPRECT)&rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
	dc.SelectFont(oldFont);
}

void CNavigationPanel::PaintWindowModeBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.25f);

	dc.SelectStockBrush(HOLLOW_BRUSH);
	dc.Rectangle(&r);
	int nTop = r.top + r.Height()/4;
	dc.MoveTo(r.left + 1, nTop);
	dc.LineTo(r.right, nTop);
}

void CNavigationPanel::PaintRotateCWBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.3f);

	int nX = r.left + (int)(r.Width()*0.65f);

	dc.MoveTo(r.left-2, r.bottom);
	dc.LineTo(nX, r.bottom);
	dc.LineTo(nX, r.bottom - (int)(r.Height()*0.4f));
	dc.LineTo(r.left-2, r.bottom);

	dc.MoveTo(nX+2, r.bottom);
	dc.LineTo(r.right+2, r.bottom);
	dc.LineTo(nX+2, r.top);
	dc.LineTo(nX+2, r.bottom);
}

void CNavigationPanel::PaintRotateCCWBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.3f);

	int nX = r.left + (int)(r.Width()*0.33f);

	dc.MoveTo(r.left-2, r.bottom);
	dc.LineTo(nX, r.bottom);
	dc.LineTo(nX, r.top);
	dc.LineTo(r.left-2, r.bottom);

	dc.MoveTo(nX+2, r.bottom);
	dc.LineTo(r.right+2, r.bottom);
	dc.LineTo(nX+2, r.bottom - (int)(r.Height()*0.4f));
	dc.LineTo(nX+2, r.bottom);
}

void CNavigationPanel::PaintLandscapeModeBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.3f);

	CPoint p1(r.left-1, r.bottom);
	dc.MoveTo(p1);
	CPoint p2(r.left + (int)(r.Width()*0.25f), r.top + (int)(r.Height()*0.1f));
	dc.LineTo(p2);
	CPoint p3(r.left + (int)(r.Width()*0.5f), r.top + (int)(r.Height()*0.8f));
	dc.LineTo(p3);
	dc.LineTo(r.left + (int)(r.Width()*0.75f), r.top + (int)(r.Height()*0.5f));
	dc.LineTo(r.right+1, r.bottom);
	dc.LineTo(r.left-1, r.bottom);

	int nRad = (int)(r.Width()*0.25f + 0.5f);
	dc.Ellipse(r.right - nRad - 1, r.top - nRad + 1, r.right + nRad - 1, r.top + nRad + 1);
	//dc.Arc(p2.x - nRad, p2.y - nRad, p2.x + nRad, p2.y + nRad, p3.x, p3.y, p1.x, p1.y);
}

void CNavigationPanel::PaintInfoBtn(const CRect& rect, CDC& dc) {
	CFont font;
	font.CreatePointFont(160, _T("Times New Roman"), dc, false, true);
	HFONT oldFont = dc.SelectFont(font);
	dc.DrawText(_T("i"), 1, (LPRECT)&rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
	dc.SelectFont(oldFont);
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CWndButtonPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CWndButtonPanel::CWndButtonPanel(HWND hWnd, CSliderMgr* pSliderMgr) : CPanelMgr(hWnd) {
	m_pSliderMgr = pSliderMgr;
	m_clientRect = CRect(0, 0, 0, 0);
	m_nWidth = 0;
	m_nHeight = (int)(WNDBUTTON_PANEL_HEIGHT*m_fDPIScale);
}

CRect CWndButtonPanel::PanelRect() {
	if (m_nWidth == 0) {
		int nButtonSize = m_nHeight - 4 * WNDBUTTON_BORDER;
		m_nWidth = 4 * WNDBUTTON_BORDER + (m_ctrlList.size() - 1) * WNDBUTTON_BORDER * 3 + m_ctrlList.size() * nButtonSize;
	}

	CRect sliderRect = m_pSliderMgr->PanelRect();
	m_clientRect = CRect(CPoint(sliderRect.right - m_nWidth, 0),
		CSize(m_nWidth, m_nHeight));
	return m_clientRect;
}

bool CWndButtonPanel::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	bool bHandled = CPanelMgr::OnMouseLButton(eMouseEvent, nX, nY);
	if (m_clientRect.PtInRect(CPoint(nX, nY))) {
		return true;
	}
	return bHandled;
}

void CWndButtonPanel::RequestRepositioning() {
	RepositionAll();
}

void CWndButtonPanel::RepositionAll() {
	m_clientRect = PanelRect();
	int nButtonSize = m_nHeight - 4 * WNDBUTTON_BORDER;
	int nStartX = m_clientRect.right - m_nWidth + WNDBUTTON_BORDER * 2;
	int nStartY = WNDBUTTON_BORDER * 2;
	std::list<CUICtrl*>::iterator iter;
	for (iter = m_ctrlList.begin( ); iter != m_ctrlList.end( ); iter++ ) {
		CButtonCtrl* pButton = dynamic_cast<CButtonCtrl*>(*iter);
		if (pButton != NULL) {
			pButton->SetPosition(CRect(CPoint(nStartX, nStartY), CSize(nButtonSize, nButtonSize)));
			nStartX += nButtonSize + WNDBUTTON_BORDER * 3;
		}
	}
}

void CWndButtonPanel::PaintMinimizeBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.3f);
	CPoint p1(r.left, r.bottom-1);
	dc.MoveTo(p1);
	CPoint p2(r.right, r.bottom-1);
	dc.LineTo(p2);
	CPoint p3(r.left, r.bottom);
	dc.MoveTo(p3);
	CPoint p4(r.right, r.bottom);
	dc.LineTo(p4);
}

void CWndButtonPanel::PaintRestoreBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.25f);
	int nXt = rect.left + (int)(rect.Width()*0.7f);
	int nYt = rect.top + (int)(rect.Height()*0.4f);
	dc.Rectangle(r.left, nYt, nXt, r.bottom);
	CPoint p1(r.left+1, nYt+1);
	dc.MoveTo(p1);
	CPoint p2(nXt-1, nYt+1);
	dc.LineTo(p2);
	int nXm = (r.left + nXt) >> 1;
	int nYm = (nYt + r.bottom) >> 1;
	CPoint p11(nXm, r.top);
	dc.MoveTo(p11);
	CPoint p12(r.right, r.top);
	dc.LineTo(p12);
	CPoint p21(nXm, r.top-1);
	dc.MoveTo(p21);
	CPoint p22(r.right, r.top-1);
	dc.LineTo(p22);
	CPoint p31(r.right, r.top-1);
	dc.MoveTo(p31);
	CPoint p32(r.right, nYm);
	dc.LineTo(p32);
	CPoint p41(r.right-2, nYm);
	dc.LineTo(p41);
}

void CWndButtonPanel::PaintCloseBtn(const CRect& rect, CDC& dc) {
	CRect r = InflateRect(rect, 0.25f);

	CPoint p1(r.left, r.top);
	dc.MoveTo(p1);
	CPoint p2(r.right, r.bottom);
	dc.LineTo(p2);
	CPoint p3(r.right, r.top);
	dc.MoveTo(p3);
	CPoint p4(r.left, r.bottom);
	dc.LineTo(p4);
	CPoint p11(r.left, r.top+1);
	dc.MoveTo(p11);
	CPoint p21(r.right-1, r.bottom);
	dc.LineTo(p21);
	CPoint p51(r.left+1, r.top);
	dc.MoveTo(p51);
	CPoint p52(r.right, r.bottom-1);
	dc.LineTo(p52);
	CPoint p31(r.right, r.top+1);
	dc.MoveTo(p31);
	CPoint p41(r.left+1, r.bottom);
	dc.LineTo(p41);
	CPoint p61(r.right-1, r.top);
	dc.MoveTo(p61);
	CPoint p62(r.left, r.bottom-1);
	dc.LineTo(p62);
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CUICtrl
/////////////////////////////////////////////////////////////////////////////////////////////

CUICtrl::CUICtrl(CPanelMgr* pPanelMgr) { 
	m_pMgr = pPanelMgr; 
	m_bShow = true; 
	m_bHighlight = false; 
	m_position = CRect(0, 0, 0, 0);
	m_pos2Screen = CPoint(0, 0);
}

void CUICtrl::OnPaint(CDC & dc, const CPoint& offset) {
	dc.SelectStockFont(DEFAULT_GUI_FONT);
	dc.SetBkMode(TRANSPARENT);

	m_pos2Screen = CPoint(-offset.x, -offset.y);

	CRect pos(m_position);
	pos.OffsetRect(offset);
	pos.OffsetRect(-1, 0); Draw(dc, pos, true);
	pos.OffsetRect(2, 0); Draw(dc, pos, true);
	pos.OffsetRect(-1, -1); Draw(dc, pos, true);
	pos.OffsetRect(0, 2); Draw(dc, pos, true);
	pos.OffsetRect(0, -1); Draw(dc, pos, false);
}

void CUICtrl::SetShow(bool bShow) { 
	if (bShow != m_bShow) {
		m_bShow = bShow;
		::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE); // trigger redraw of area
	}
}

void CUICtrl::SetTooltip(LPCTSTR sTooltipText) {
	m_pMgr->GetTooltipMgr().AddTooltip(this, sTooltipText);
}

void CUICtrl::SetTooltipHandler(TooltipHandler ttHandler) {
	m_pMgr->GetTooltipMgr().AddTooltipHandler(this, ttHandler);
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CTextCtrl
/////////////////////////////////////////////////////////////////////////////////////////////

CTextCtrl::CTextCtrl(CPanelMgr* pPanelMgr, LPCTSTR sTextInit, bool bEditable, 
					 TextChangedHandler textChangedHandler) : CUICtrl(pPanelMgr) {
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
			}
		} else if (m_bHighlight) {
			m_bHighlight = false;
			::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE);
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

CButtonCtrl::CButtonCtrl(CPanelMgr* pPanelMgr, LPCTSTR sButtonText, 
						 ButtonPressedHandler buttonPressedHandler) : CUICtrl(pPanelMgr) {
	 m_sText = sButtonText;
	 m_paintHandler = NULL;
	 m_buttonPressedHandler = buttonPressedHandler;
	 m_nTextWidth = GetTextRect(m_pMgr->GetHWND(), m_sText).cx;
	 m_bDragging = false;
	 m_bEnabled = false;
}

CButtonCtrl::CButtonCtrl(CPanelMgr* pPanelMgr, PaintHandler paintHandler, ButtonPressedHandler buttonPressedHandler) 
 : CUICtrl(pPanelMgr) {
	m_paintHandler = paintHandler;
	m_buttonPressedHandler = buttonPressedHandler;
	m_nTextWidth = 0;
	m_bDragging = false;
	m_bEnabled = false;
}

void CButtonCtrl::SetEnabled(bool bEnabled) {
	if (bEnabled != m_bEnabled) {
		m_bEnabled = bEnabled;
		if (this->m_bShow) {
			::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE);
		}
	}
}

bool CButtonCtrl::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	bool bRetVal = false;
	CPoint mousePos(nX, nY);
	if (m_position.PtInRect(mousePos)) {
		if (eMouseEvent == MouseEvent_BtnDown) {
			m_pMgr->CaptureMouse(this);
			m_bDragging = true;
			::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE);
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
	}
	return bRetVal;
}

bool CButtonCtrl::OnMouseMove(int nX, int nY) {
	CPoint mousePos(nX, nY);
	if (m_position.PtInRect(mousePos)) {
		if (!m_bHighlight) {
			m_bHighlight = true;
			::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE);
		}
	} else if (m_bHighlight) {
		m_bHighlight = false;
		::InvalidateRect(m_pMgr->GetHWND(), &m_position, FALSE);
	}

	return false;
}

void CButtonCtrl::Draw(CDC & dc, CRect position, bool bBlack) {
	dc.SelectStockBrush(HOLLOW_BRUSH);
	HPEN hPen, hOldPen;
	if (bBlack) {
		dc.SelectStockPen(BLACK_PEN);
	} else {
		hPen = ::CreatePen(PS_SOLID, 1, (m_bDragging && m_bHighlight) ? RGB(255, 255, 255) : m_bEnabled ? RGB(255, 255, 0) : RGB(0, 255, 0));
		hOldPen = dc.SelectPen(hPen);
	}
	dc.Rectangle(position);
	if (m_sText.GetLength() > 0) {
		dc.SelectStockFont(DEFAULT_GUI_FONT);
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(bBlack ? 0 : m_bHighlight ? RGB(255, 255, 255) : m_bEnabled ? RGB(255, 255, 0) : RGB(0, 255, 0));
		dc.DrawText(m_sText, m_sText.GetLength(), &position, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS);
	} else if (m_paintHandler != NULL) {
		if (m_bHighlight && !bBlack) {
			dc.SelectPen((HPEN)::GetStockObject(WHITE_PEN));
		}
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(bBlack ? 0 : m_bHighlight ? RGB(255, 255, 255) : m_bEnabled ? RGB(255, 255, 0) : RGB(0, 255, 0));
		m_paintHandler(position, dc);
	}
	if (!bBlack) {
		dc.SelectPen(hOldPen);
		::DeleteObject(hPen);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CSliderDouble
/////////////////////////////////////////////////////////////////////////////////////////////

CSliderDouble::CSliderDouble(CPanelMgr* pMgr, LPCTSTR sName, int nSliderLen, double* pdValue, bool* pbEnable,
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
				::InvalidateRect(m_pMgr->GetHWND(), &m_checkRectFull, FALSE);
			}
		} else if (m_bHighlightCheck) {
			m_bHighlightCheck = false;
			::InvalidateRect(m_pMgr->GetHWND(), &m_checkRectFull, FALSE);
		}
	}

	if (m_pEnable && !*m_pEnable) return false;

	if (m_bDragging) {
		double dOldValue = *m_pValue;
		*m_pValue = SliderPosToValue(nX - m_pos2Screen.x, m_nSliderStart, m_nSliderStart + m_nSliderLen);
		if (dOldValue != *m_pValue) {
			::InvalidateRect(m_pMgr->GetHWND(), NULL, FALSE);
		}
		return true;
	} else if (m_sliderRect.PtInRect(mousePos)) {
		if (!m_bHighlight) {
			m_bHighlight = true;
			CRect invRect(&m_sliderRect);
			invRect.OffsetRect(0, -8);
			::InvalidateRect(m_pMgr->GetHWND(), &invRect, FALSE);
		}
	} else if (m_bHighlight) {
		m_bHighlight = false;
		CRect invRect(&m_sliderRect);
		invRect.OffsetRect(0, -8);
		::InvalidateRect(m_pMgr->GetHWND(), &invRect, FALSE);
	}

	return false;
}

void CSliderDouble::OnPaint(CDC & dc, const CPoint& offset)  {
	if (m_pEnable && !*m_pEnable) m_bHighlight = false;
	CUICtrl::OnPaint(dc, offset);
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
		m_checkRectFull.OffsetRect(m_pos2Screen.x, m_pos2Screen.y);
		DrawCheck(dc, m_checkRect, bBlack, m_bHighlightCheck);
		position.left += m_nCheckHeight + m_nCheckHeight/2;
	}
	dc.DrawText(m_sName, _tcslen(m_sName), &position, DT_VCENTER | DT_SINGLELINE);
	
	int nYMid = position.top + ((position.bottom - position.top) >> 1);
	int nXPos = position.right - m_nNumberWidth;
	m_nSliderStart = nXPos - m_nSliderLen;
	DrawRuler(dc, m_nSliderStart, nXPos, nYMid, bBlack, m_bHighlight);
	m_sliderRect = CRect(m_nSliderStart - m_nSliderHeight, nYMid - 2, nXPos + m_nSliderHeight, nYMid + m_nSliderHeight + 8);
	m_sliderRect.OffsetRect(m_pos2Screen.x, m_pos2Screen.y);
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
