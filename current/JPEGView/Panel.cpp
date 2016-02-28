#include "StdAfx.h"
#include "Panel.h"
#include "HelpersGUI.h"
#include "SettingsProvider.h"

/////////////////////////////////////////////////////////////////////////////////////////////
// CPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CPanel::CPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, bool bFramed, bool bClickThrough) : m_tooltipMgr(hWnd) {
	m_hWnd = hWnd;
	m_bFramed = bFramed;
	m_bClickThrough = bClickThrough;
	m_fDPIScale = HelpersGUI::ScreenScaling;
	m_pCtrlCaptureMouse = NULL;
	m_pNotifyMouseCapture = pNotifyMouseCapture;
}

CPanel::~CPanel(void) {
	ControlsIterator iter;
	for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
		delete iter->second;
	}
	m_controls.clear();
}

CUICtrl* CPanel::GetControl(int nID) {
	ControlsIterator iter = m_controls.find(nID);
	if (iter == m_controls.end()) {
		return NULL;
	}
	return iter->second;
}

CSliderDouble* CPanel::AddSlider(int nID, LPCTSTR sName, double* pdValue, bool* pbEnable, 
								 double dMin, double dMax, double dDefaultValue, bool bAllowPreviewAndReset, bool bLogarithmic, bool bInvert, int nWidth) {
	CSliderDouble* pSlider = new CSliderDouble(this, sName, nWidth, pdValue, pbEnable, dMin, dMax, dDefaultValue, 
		bAllowPreviewAndReset, bLogarithmic, bInvert);
	m_controls[nID] = pSlider;
	return pSlider;
}

CTextCtrl* CPanel::AddText(int nID, LPCTSTR sTextInit, bool bEditable, TextChangedHandler* textChangedHandler, void* pContext) {
	CTextCtrl* pTextCtrl = new CTextCtrl(this, sTextInit, bEditable, textChangedHandler, pContext);
	m_controls[nID] = pTextCtrl;
	return pTextCtrl;
}

CButtonCtrl* CPanel::AddButton(int nID, LPCTSTR sButtonText, ButtonPressedHandler* buttonPressedHandler, void* pContext, int nParameter) {
	CButtonCtrl* pButtonCtrl = new CButtonCtrl(this, sButtonText, buttonPressedHandler, pContext, nParameter);
	m_controls[nID] = pButtonCtrl;
	return pButtonCtrl;
}

CButtonCtrl* CPanel::AddUserPaintButton(int nID, LPCTSTR sTooltip, PaintHandler* paintHandler, ButtonPressedHandler* buttonPressedHandler, 
										void* pPaintContext, void* pContext, int nParameter) {
	CButtonCtrl* pButtonCtrl = new CButtonCtrl(this, paintHandler, buttonPressedHandler, pPaintContext, pContext, nParameter);
	if (sTooltip != NULL) {
		pButtonCtrl->SetTooltip(sTooltip);
	}
	m_controls[nID] = pButtonCtrl;
	return pButtonCtrl;
}

CButtonCtrl* CPanel::AddUserPaintButton(int nID, TooltipHandler* ttHandler, PaintHandler* paintHandler, ButtonPressedHandler* buttonPressedHandler,
										void* pPaintContext, void* pContext, int nParameter) {
	CButtonCtrl* pButtonCtrl = new CButtonCtrl(this, paintHandler, buttonPressedHandler, pPaintContext, pContext, nParameter);
	pButtonCtrl->SetTooltipHandler(ttHandler, pPaintContext);
	m_controls[nID] = pButtonCtrl;
	return pButtonCtrl;
}

bool CPanel::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	m_tooltipMgr.OnMouseLButton(eMouseEvent, nX, nY);

	bool bHandled = false;
	CUICtrl* ctrlCapturedMouse = m_pCtrlCaptureMouse;
	ControlsIterator iter;
	for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
		CUICtrl* pCtrl = iter->second;
		if (pCtrl->IsShown() && (ctrlCapturedMouse == NULL || pCtrl == ctrlCapturedMouse)) {
			if (pCtrl->OnMouseLButton(eMouseEvent, nX, nY)) {
				bHandled = true;
			}
		}
	}
	if (!bHandled && !m_bClickThrough) {
		CRect panelRect = PanelRect();
		if (panelRect.PtInRect(CPoint(nX, nY))) {
			return true;
		}
	}
	return bHandled;
}

bool CPanel::OnMouseMove(int nX, int nY) {
	m_tooltipMgr.OnMouseMove(nX, nY);

	bool bHandled = false;
	CUICtrl* ctrlCapturedMouse = m_pCtrlCaptureMouse;
	ControlsIterator iter;
	for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
		CUICtrl* pCtrl = iter->second;
		if (pCtrl->IsShown() && (ctrlCapturedMouse == NULL || pCtrl == ctrlCapturedMouse)) {
			if (pCtrl->OnMouseMove(nX, nY)) {
				bHandled = true;
			}
		}
	}
	return bHandled || m_pCtrlCaptureMouse != NULL;
}

void CPanel::OnPaint(CDC & dc, const CPoint& offset) {
	RepositionAll();
	ControlsIterator iter;
	for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
		CUICtrl* pCtrl = iter->second;
		if (pCtrl->IsShown()) {
			pCtrl->OnPaint(dc, offset);
		}
	}
	if (m_bFramed) {
		PaintFrameAroundPanel(dc, PanelRect(), offset);
	}
}

void CPanel::CaptureMouse(CUICtrl* pCtrl) {
	if (m_pCtrlCaptureMouse == NULL) {
		m_pCtrlCaptureMouse = pCtrl;
		if (m_pNotifyMouseCapture != NULL) {
			m_pNotifyMouseCapture->MouseCapturedOrReleased(true, pCtrl);
		}
	}
}

void CPanel::ReleaseMouse(CUICtrl* pCtrl) {
	if (pCtrl == m_pCtrlCaptureMouse) {
		m_pCtrlCaptureMouse = NULL;
		if (m_pNotifyMouseCapture != NULL) {
			m_pNotifyMouseCapture->MouseCapturedOrReleased(false, pCtrl);
		}
	}
}

void CPanel::PaintFrameAroundPanel(CDC & dc, const CRect& rect, const CPoint& offset) {
	HPEN hPen = ::CreatePen(PS_SOLID, 1, CSettingsProvider::This().ColorGUI());
	HPEN hOldPen = dc.SelectPen(hPen);;
	dc.SelectStockBrush(HOLLOW_BRUSH);
	CRect rectCopy = rect;
	rectCopy.OffsetRect(offset);
	dc.Rectangle(rectCopy);
	if (HelpersGUI::ScreenScaling >= 2) {
		rectCopy.DeflateRect(1, 1);
		dc.Rectangle(rectCopy);
	}
	dc.SelectPen(hOldPen);
	::DeleteObject(hPen);
}
