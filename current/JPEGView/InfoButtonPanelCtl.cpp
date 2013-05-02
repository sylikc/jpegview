#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "InfoButtonPanelCtl.h"
#include "InfoButtonPanel.h"
#include "EXIFDisplayCtl.h"

CInfoButtonPanelCtl::CInfoButtonPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel) : CPanelController(pMainDlg, false) {
	m_bVisible = false;
	m_nOldMouseY = 0;
	m_pPanel = m_pInfoButtonPanel = new CInfoButtonPanel(pMainDlg->GetHWND(), this, pImageProcPanel);
	m_pInfoButtonPanel->GetBtnInfo()->SetButtonPressedHandler(&CMainDlg::OnExecuteCommand, pMainDlg, IDM_SHOW_FILEINFO);
}

CInfoButtonPanelCtl::~CInfoButtonPanelCtl() {
	delete m_pInfoButtonPanel;
	m_pInfoButtonPanel = NULL;
}

bool CInfoButtonPanelCtl::IsVisible() { 
	return m_bVisible && m_pMainDlg->IsFullScreenMode() && !m_pMainDlg->GetEXIFDisplayCtl()->IsActive(); 
}

void CInfoButtonPanelCtl::SetVisible(bool bVisible) { 
	if (m_bVisible != bVisible) {
		m_bVisible = bVisible;
		InvalidateMainDlg();
	}
}

bool CInfoButtonPanelCtl::OnMouseMove(int nX, int nY) {
	if (!m_pMainDlg->IsFullScreenMode()) {
		return false;
	}
	bool bHandled = CPanelController::OnMouseMove(nX, nY);
	if (!bHandled) {
		if (!m_bVisible) {
			int nWndBtnAreaStart = m_pInfoButtonPanel->ButtonPanelHeight();
			if (m_nOldMouseY != 0 && m_nOldMouseY > nWndBtnAreaStart && nY <= nWndBtnAreaStart) {
				m_bVisible = true;
				m_pMainDlg->InvalidateRect(m_pInfoButtonPanel->PanelRect(), FALSE);
			}
		} else {
			if (nY > m_pInfoButtonPanel->ButtonPanelHeight()*2) {
				m_bVisible = false;
				m_pMainDlg->InvalidateRect(m_pInfoButtonPanel->PanelRect(), FALSE);
				m_pInfoButtonPanel->GetTooltipMgr().RemoveActiveTooltip();
			}
		}
	}
	m_nOldMouseY = nY;
	return bHandled;
}

bool CInfoButtonPanelCtl::IsPointInInfoButtonPanel(CPoint pt) {
	if (IsVisible()) {
		return PanelRect().PtInRect(pt);
	}
	return false;
}
