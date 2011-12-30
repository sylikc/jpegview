#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "TransformPanelCtl.h"
#include "TransformPanel.h"
#include "NavigationPanelCtl.h"
#include "ZoomNavigatorCtl.h"
#include "WndButtonPanelCtl.h"

CTransformPanelCtl::CTransformPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel, CTransformPanel* pTransformPanel) : CPanelController(pMainDlg, true) {
	m_bVisible = false;
	m_bShowGrid = true;
	m_bAutoCrop = true;
	m_bTransforming = false;
	m_bOldShowNavPanel = false;
	m_nMouseX = m_nMouseY = 0;

	m_pPanel = m_pTransformPanel = pTransformPanel;
	m_pTransformPanel->GetBtnShowGrid()->SetButtonPressedHandler(&OnShowGridLines, this, 0, m_bShowGrid);
	m_pTransformPanel->GetBtnAutoCrop()->SetButtonPressedHandler(&OnAutoCrop, this, 0, m_bAutoCrop);
	m_pTransformPanel->GetBtnCancel()->SetButtonPressedHandler(&OnCancel, this);
	m_pTransformPanel->GetBtnApply()->SetButtonPressedHandler(&OnApply, this);
}

CTransformPanelCtl::~CTransformPanelCtl() { 
	delete m_pTransformPanel;
	m_pTransformPanel = NULL;
}

bool CTransformPanelCtl::IsVisible() {
	return m_bVisible;
}

bool CTransformPanelCtl::IsActive() {
	return m_bVisible;
}

void CTransformPanelCtl::SetVisible(bool bVisible) {
	if (bVisible != m_bVisible) {
		if (bVisible) {
			StartPanel();
		} else {
			TerminatePanel();
		}
	}
}

void CTransformPanelCtl::SetActive(bool bActive) {
	SetVisible(bActive);
}

bool CTransformPanelCtl::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	if (eMouseEvent == MouseEvent_BtnUp && m_bTransforming) {
		EndTransforming();
		return true;
	}
	if (CPanelController::OnMouseLButton(eMouseEvent, nX, nY)) {
		return true;
	}
	if (eMouseEvent == MouseEvent_BtnDown && 
		!m_pMainDlg->GetZoomNavigatorCtl()->IsPointInZoomNavigatorThumbnail(CPoint(nX, nY)) &&
		!m_pMainDlg->GetWndButtonPanelCtl()->IsPointInWndButtonPanel(CPoint(nX, nY))) {
		StartTransforming(nX, nY);
		return true;
	}
	return false;
}

bool CTransformPanelCtl::OnMouseMove(int nX, int nY) {
	m_nMouseX = nX;
	m_nMouseY = nY;
	if (m_bTransforming) {
		DoTransforming();
		return true;
	}
	return CPanelController::OnMouseMove(nX, nY);
}

bool CTransformPanelCtl::OnKeyDown(unsigned int nVirtualKey, bool bShift, bool bAlt, bool bCtrl) {
	if (nVirtualKey == VK_ESCAPE) {
		TerminatePanel();
	}
	return true; // all other keys are eaten up in this panel without doing anything
}

void CTransformPanelCtl::StartPanel() {
	if (CurrentImage() == NULL) {
		return;
	}
	m_bVisible = true;
	m_bOldShowNavPanel = m_pMainDlg->PrepareForModalPanel();
	UpdatePanelTitle();
	InvalidateMainDlg();
}

void CTransformPanelCtl::TerminatePanel() {
	m_bVisible = false;
	m_pMainDlg->GetNavPanelCtl()->SetActive(m_bOldShowNavPanel);
	InvalidateMainDlg();
}

void CTransformPanelCtl::Apply() {
	if (CurrentImage() == NULL) {
		return;
	}
	ApplyTransformation();
	if (!m_bAutoCrop) {
		m_pMainDlg->ExecuteCommand(IDM_FIT_TO_SCREEN_NO_ENLARGE);
	}
	m_pMainDlg->AdjustWindowToImage(false);
	InvalidateMainDlg();
}


void CTransformPanelCtl::OnShowGridLines(void* pContext, int nParameter, CButtonCtrl & sender) {
	CTransformPanelCtl* pThis = (CTransformPanelCtl*)pContext;
	pThis->m_bShowGrid = !pThis->m_bShowGrid;
	pThis->m_pTransformPanel->GetBtnShowGrid()->SetActive(pThis->m_bShowGrid);
	pThis->InvalidateMainDlg();
}

void CTransformPanelCtl::OnAutoCrop(void* pContext, int nParameter, CButtonCtrl & sender) {
	CTransformPanelCtl* pThis = (CTransformPanelCtl*)pContext;
	pThis->m_bAutoCrop = !pThis->m_bAutoCrop;
	pThis->m_pTransformPanel->GetBtnAutoCrop()->SetActive(pThis->m_bAutoCrop);
}

void CTransformPanelCtl::OnCancel(void* pContext, int nParameter, CButtonCtrl & sender) {
	((CTransformPanelCtl*)pContext)->TerminatePanel();
}

void CTransformPanelCtl::OnApply(void* pContext, int nParameter, CButtonCtrl & sender) {
	CTransformPanelCtl* pThis = (CTransformPanelCtl*)pContext;
	HCURSOR hOldCursor = ::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_WAIT)));
	pThis->Apply();
	::SetCursor(hOldCursor);
	pThis->TerminatePanel();
}