#pragma once

#include "Panel.h"

// Window button panel
class CWndButtonPanel : public CPanel {
public:
	// IDs of the controls on this panel
	enum {
		ID_btnMinimize,
		ID_btnRestore,
		ID_btnClose
	};
public:
	// The panel is on the given window on top border of image processing panel
	CWndButtonPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel);

	CButtonCtrl* GetBtnMinimize() { return GetControl<CButtonCtrl*>(ID_btnMinimize); }
	CButtonCtrl* GetBtnRestore() { return GetControl<CButtonCtrl*>(ID_btnRestore); }
	CButtonCtrl* GetBtnClose() { return GetControl<CButtonCtrl*>(ID_btnClose); }

	virtual CRect PanelRect();
	int ButtonPanelHeight() { return m_nHeight; }
	virtual void RequestRepositioning();

protected:
	virtual void RepositionAll();

private:
	// Painting handlers for the buttons
	static void PaintMinimizeBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintRestoreBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintCloseBtn(void* pContext, const CRect& rect, CDC& dc);

	CPanel* m_pImageProcPanel;
	CRect m_clientRect;
	int m_nWidth, m_nHeight;
};