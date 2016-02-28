#pragma once

#include "Panel.h"

// Info button panel - panel with buttons shown top, left
class CInfoButtonPanel : public CPanel {
public:
	// IDs of the controls on this panel
	enum {
		ID_btnEXIFInfo
	};
public:
	// The panel is on the given window on top border above the image processing panel
	CInfoButtonPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel);

	CButtonCtrl* GetBtnInfo() { return GetControl<CButtonCtrl*>(ID_btnEXIFInfo); }

	virtual CRect PanelRect();
	int ButtonPanelHeight() { return m_nHeight; }
	virtual void RequestRepositioning();

protected:
	virtual void RepositionAll();

private:
	// Painting handlers for the buttons
	static void PaintInfoBtn(void* pContext, const CRect& rect, CDC& dc);

	CPanel* m_pImageProcPanel;
	CRect m_clientRect;
	int m_nWidth, m_nHeight;
};