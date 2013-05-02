#pragma once

#include "PanelController.h"

class CInfoButtonPanel;

// Implements functionality of the info button panel (info button on top, left corner of screen)
class CInfoButtonPanelCtl : public CPanelController
{
public:
	CInfoButtonPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel);
	virtual ~CInfoButtonPanelCtl();

	virtual float DimFactor() { return 0.1f; }

	virtual bool IsVisible();
	virtual bool IsActive() { return true; }

	virtual void SetVisible(bool bVisible);
	virtual void SetActive(bool bActive) {} // always active

	virtual bool OnMouseMove(int nX, int nY);

	// Gets if the given point is in the panel rectangle. When the panel is not visible, false is returned.
	bool IsPointInInfoButtonPanel(CPoint pt);

private:
	bool m_bVisible;
	int m_nOldMouseY;
	CInfoButtonPanel* m_pInfoButtonPanel;
};