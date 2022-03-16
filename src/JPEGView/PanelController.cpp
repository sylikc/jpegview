#include "StdAfx.h"
#include "PanelController.h"
#include "resource.h"
#include "MainDlg.h"
#include "PanelMgr.h"

CPanelController::CPanelController(CMainDlg* pMainDlg, bool bIsModal) { 
	m_pMainDlg = pMainDlg;
	m_bIsModal = bIsModal;
	m_pPanel = NULL;
}

void CPanelController::MouseCapturedOrReleased(bool bCaptured, CUICtrl* pCtrl) {
	m_pMainDlg->GetPanelMgr()->SetCapturedPanelController(bCaptured ? this : NULL);
}

bool CPanelController::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) { 
	return m_pPanel->OnMouseLButton(eMouseEvent, nX, nY); 
}

bool CPanelController::OnMouseMove(int nX, int nY) {
	return m_pPanel->OnMouseMove(nX, nY);
}

bool CPanelController::MouseCursorCaptured() {
	return m_pPanel->MouseCursorCaptured();
}

void CPanelController::OnPaintPanel(CDC & dc, const CPoint& offset)  { 
	m_pPanel->OnPaint(dc, offset); 
}

void CPanelController::InvalidateMainDlg() { 
	m_pMainDlg->Invalidate(); 
}

CJPEGImage* CPanelController::CurrentImage() { 
	return m_pMainDlg->GetCurrentImage(); 
}