#include "StdAfx.h"
#include "PanelMgr.h"
#include "PaintMemDCMgr.h"

CPanelMgr::CPanelMgr() {
	m_pCapturedPanelController = NULL;
}

CPanelMgr::~CPanelMgr() {
	std::list<CPanelController*>::iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		delete (*iter);
	}
}

bool CPanelMgr::IsModalPanelShown() const {
	std::list<CPanelController*>::const_iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		if ((*iter)->IsModal() && (*iter)->IsVisible()) {
			return true;
		}
	}
	return false;
}

void CPanelMgr::PrepareMemDCMgr(CPaintMemDCMgr& memDCMgr, std::list<CRect>& listExcludedRects) {
	std::list<CPanelController*>::iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		if ((*iter)->IsVisible()) {
			listExcludedRects.push_back(memDCMgr.CreatePanelRegion((*iter)->GetPanel(), (*iter)->DimFactor(), (*iter)->BlendPanel()));
		}
	}
}

void CPanelMgr::AddPanelController(CPanelController* pPanelController) {
	m_panelControllers.push_back(pPanelController);
}

void CPanelMgr::AfterNewImageLoaded() {
	std::list<CPanelController*>::iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		(*iter)->AfterNewImageLoaded();
	}
}

void CPanelMgr::AfterImageRenamed() {
	std::list<CPanelController*>::iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		(*iter)->AfterImageRenamed();
	}
}

bool CPanelMgr::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	if (m_pCapturedPanelController != NULL && m_pCapturedPanelController->IsVisible()) {
		if (m_pCapturedPanelController->OnMouseLButton(eMouseEvent, nX, nY)) {
			return true;
		}
	}
	std::list<CPanelController*>::iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		if ((*iter)->IsVisible() && *iter != m_pCapturedPanelController) { // no mouse clicks for invisible panels
			if ((*iter)->OnMouseLButton(eMouseEvent, nX, nY)) {
				return true;
			}
		}
	}
	return false;
}

bool CPanelMgr::OnMouseMove(int nX, int nY) {
	if (m_pCapturedPanelController != NULL && m_pCapturedPanelController->IsActive()) {
		if (m_pCapturedPanelController->OnMouseMove(nX, nY)) {
			return true;
		}
	}
	std::list<CPanelController*>::iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		if ((*iter)->IsActive() && *iter != m_pCapturedPanelController) { // must be routed to invisible but active panels, may get visible by mouse movements
			if ((*iter)->OnMouseMove(nX, nY)) {
				return true;
			}
		}
	}
	return false;
}

bool CPanelMgr::OnKeyDown(unsigned int nVirtualKey, bool bShift, bool bAlt, bool bCtrl) {
	std::list<CPanelController*>::iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		if ((*iter)->IsVisible() && (*iter)->IsModal()) { // no keys for invisible panels
			if ((*iter)->OnKeyDown(nVirtualKey, bShift, bAlt, bCtrl)) {
				return true;
			}
		}
	}
	return false;
}

bool CPanelMgr::OnTimer(int nTimerId) {
	std::list<CPanelController*>::iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		if ((*iter)->IsActive()) {
			if ((*iter)->OnTimer(nTimerId)) {
				return true;
			}
		}
	}
	return false;
}

void CPanelMgr::PaintPanels(CDC & dc, const CPoint& offset) {
	std::list<CPanelController*>::iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		if ((*iter)->IsVisible()) {
			(*iter)->OnPaintPanel(dc, offset);
		}
	}
}

void CPanelMgr::OnPrePaint(HDC hPaintDC) {
	std::list<CPanelController*>::iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		if ((*iter)->IsVisible()) {
			(*iter)->OnPrePaintMainDlg(hPaintDC);
		}
	}
}

void CPanelMgr::OnPostPaint(HDC hPaintDC) {
	std::list<CPanelController*>::iterator iter;
	for (iter = m_panelControllers.begin( ); iter != m_panelControllers.end( ); iter++ ) {
		if ((*iter)->IsVisible()) {
			(*iter)->OnPostPaintMainDlg(hPaintDC);
		}
	}
}