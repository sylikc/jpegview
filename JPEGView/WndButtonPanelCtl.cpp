#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "WndButtonPanelCtl.h"
#include "WndButtonPanel.h"

CWndButtonPanelCtl::CWndButtonPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel) : CPanelController(pMainDlg, false) {
	m_bVisible = false;
	m_nOldMouseY = 0;
	m_pPanel = m_pWndButtonPanel = new CWndButtonPanel(pMainDlg->GetHWND(), this, pImageProcPanel);
	m_pWndButtonPanel->GetBtnMinimize()->SetButtonPressedHandler(&(CMainDlg::OnExecuteCommand), pMainDlg, IDM_MINIMIZE);
	m_pWndButtonPanel->GetBtnRestore()->SetButtonPressedHandler(&OnRestore, this);
	m_pWndButtonPanel->GetBtnClose()->SetButtonPressedHandler(&(CMainDlg::OnExecuteCommand), pMainDlg, IDM_EXIT);;
}

CWndButtonPanelCtl::~CWndButtonPanelCtl() {
	delete m_pWndButtonPanel;
	m_pWndButtonPanel = NULL;
}

void CWndButtonPanelCtl::SetVisible(bool bVisible) { 
	if (m_bVisible != bVisible) {
		m_bVisible = bVisible;
		InvalidateMainDlg();
	}
}

bool CWndButtonPanelCtl::OnMouseMove(int nX, int nY) {
	if (!m_pMainDlg->IsFullScreenMode()) {
		return false;
	}
	bool bHandled = CPanelController::OnMouseMove(nX, nY);
	if (!bHandled) {
		if (!m_bVisible) {
			int nWndBtnAreaStart = m_pWndButtonPanel->ButtonPanelHeight();
			if (m_nOldMouseY != 0 && m_nOldMouseY > nWndBtnAreaStart && nY <= nWndBtnAreaStart) {
				m_bVisible = true;
				m_pMainDlg->InvalidateRect(m_pWndButtonPanel->PanelRect(), FALSE);
			}
		} else {
			if (nY > m_pWndButtonPanel->ButtonPanelHeight()*2) {
				m_bVisible = false;
				m_pMainDlg->InvalidateRect(m_pWndButtonPanel->PanelRect(), FALSE);
				m_pWndButtonPanel->GetTooltipMgr().RemoveActiveTooltip();
			}
		}
	}
	m_nOldMouseY = nY;
	return bHandled;
}

void CWndButtonPanelCtl::OnRestore(void* pContext, int nParameter, CButtonCtrl & sender) {
	CMainDlg::ToggleWindowMode(((CWndButtonPanelCtl*)pContext)->m_pMainDlg, sender, false);
}