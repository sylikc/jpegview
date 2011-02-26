#pragma once

#include "PanelController.h"

class CWndButtonPanel;

// Implements functionality of the window button panel (minimize, restore, close buttons on top, right corner of screen)
class CWndButtonPanelCtl : public CPanelController
{
public:
	CWndButtonPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel);
	virtual ~CWndButtonPanelCtl();

	virtual float DimFactor() { return 0.1f; }

	virtual bool IsVisible();
	virtual bool IsActive() { return true; }

	virtual void SetVisible(bool bVisible);
	virtual void SetActive(bool bActive) {} // always active

	virtual bool OnMouseMove(int nX, int nY);

	// Gets if the given point is in the panel rectangle. When the panel is not visible, false is returned.
	bool IsPointInWndButtonPanel(CPoint pt);

private:
	bool m_bVisible;
	int m_nOldMouseY;
	CWndButtonPanel* m_pWndButtonPanel;

	static void OnRestore(void* pContext, int nParameter, CButtonCtrl & sender);
};