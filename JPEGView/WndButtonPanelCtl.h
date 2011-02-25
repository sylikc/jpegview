#pragma once

#include "PanelController.h"

class CWndButtonPanel;

// Implements functionality of the window button (minimize, restore, close) panel
class CWndButtonPanelCtl : public CPanelController
{
public:
	CWndButtonPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel);
	virtual ~CWndButtonPanelCtl();

	virtual float DimFactor() { return 0.1f; }

	virtual bool IsVisible() { return m_bVisible; }
	virtual bool IsActive() { return true; }

	virtual void SetVisible(bool bVisible);
	virtual void SetActive(bool bActive) {} // always active

	virtual bool OnMouseMove(int nX, int nY);

private:
	bool m_bVisible;
	int m_nOldMouseY;
	CWndButtonPanel* m_pWndButtonPanel;

	static void OnRestore(void* pContext, int nParameter, CButtonCtrl & sender);
};