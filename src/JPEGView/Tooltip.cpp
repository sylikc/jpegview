#include "StdAfx.h"
#include "GUIControls.h"
#include "HelpersGUI.h"

/////////////////////////////////////////////////////////////////////////////////////////////
// Tooltip support
/////////////////////////////////////////////////////////////////////////////////////////////

CTooltip::CTooltip(HWND hWnd, const CUICtrl* pBoundCtrl, LPCTSTR sTooltip) : 
	m_hWnd(hWnd), m_pBoundCtrl(pBoundCtrl), m_sTooltip(sTooltip), m_TooltipRect(0, 0, 0, 0), m_oldRectBoundCtrl(0, 0, 0, 0) {
	m_ttHandler = NULL;
}

CTooltip::CTooltip(HWND hWnd, const CUICtrl* pBoundCtrl, TooltipHandler ttHandler, void* pContext) :
	m_hWnd(hWnd), m_pBoundCtrl(pBoundCtrl), m_TooltipRect(0, 0, 0, 0) {
	m_ttHandler = ttHandler;
	m_pContext = pContext;
}

const CString& CTooltip::GetTooltip() const {
	if (m_ttHandler != NULL) {
		LPCTSTR sNewTooltip = m_ttHandler(m_pContext);
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
	DWORD backgroundColor = RGB(0, 0, 0);
	DWORD foregroundColor = RGB(255, 255, 255);
	brush.CreateSolidBrush(backgroundColor);
	dc.SelectBrush(brush);
	dc.SelectStockPen(WHITE_PEN);
	dc.Rectangle(tooltipRect.left, tooltipRect.top, tooltipRect.right, tooltipRect.bottom);
	dc.SetTextColor(foregroundColor);
	dc.SetBkColor(backgroundColor);
	HelpersGUI::SelectDefaultGUIFont(dc);
	dc.DrawText(sTooltip, sTooltip.GetLength(), tooltipRect, DT_CENTER | DT_VCENTER | DT_NOPREFIX | DT_SINGLELINE);
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
	HelpersGUI::SelectDefaultGUIFont(dc);
	CRect rectText(0, 0, 400, 0);
	const CString& sTooltip = GetTooltip();
	::DrawText(dc, sTooltip, sTooltip.GetLength(), &rectText, DT_CENTER | DT_VCENTER | DT_CALCRECT | DT_NOPREFIX | DT_SINGLELINE);
	::ReleaseDC(m_hWnd, dc);

	const int BORDERX = HelpersGUI::ScaleToScreen(6);
	const int BORDERY = HelpersGUI::ScaleToScreen(7);
	CRect clientRect;
	::GetClientRect(m_hWnd, &clientRect);
	CRect boundRect = m_pBoundCtrl->GetPosition();
	int nLeft = min(clientRect.Width() - rectText.Width() - BORDERX*2, max(0, boundRect.CenterPoint().x - rectText.Width()/2));
	int nTop = boundRect.bottom + HelpersGUI::ScaleToScreen(6);
	return CRect(nLeft, nTop, nLeft + rectText.Width() + BORDERX*2, nTop + rectText.Height() + BORDERY*2 - 1);
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

void CTooltipMgr::AddTooltipHandler(CUICtrl* pBoundCtrl, TooltipHandler ttHandler, void* pContext) {
	AddTooltip(pBoundCtrl, new CTooltip(m_hWnd, pBoundCtrl, ttHandler, pContext));
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