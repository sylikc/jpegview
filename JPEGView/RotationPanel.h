#pragma once

#include "Panel.h"

class CRotationPanel : public CPanel {
public:
	// IDs of the controls on this panel
	enum {
		ID_txtTitle,
		ID_txtHint,
		ID_btnShowGrid,
		ID_btnAutoCrop,
		ID_btnAssistMode,
		ID_btnCancel,
		ID_btnApply
	};
public:
	// The panel is on the given window above the image processing panel
	CRotationPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel);

	CTextCtrl* GetTextTitle() { return GetControl<CTextCtrl*>(ID_txtTitle); }
	CTextCtrl* GetTextHint() { return GetControl<CTextCtrl*>(ID_txtHint); }
	CButtonCtrl* GetBtnShowGrid() { return GetControl<CButtonCtrl*>(ID_btnShowGrid); }
	CButtonCtrl* GetBtnAutoCrop() { return GetControl<CButtonCtrl*>(ID_btnAutoCrop); }
	CButtonCtrl* GetBtnAssistMode() { return GetControl<CButtonCtrl*>(ID_btnAssistMode); }
	CButtonCtrl* GetBtnCancel() { return GetControl<CButtonCtrl*>(ID_btnCancel); }
	CButtonCtrl* GetBtnApply() { return GetControl<CButtonCtrl*>(ID_btnApply); }

	virtual CRect PanelRect();
	virtual void RequestRepositioning();

protected:
	virtual void RepositionAll();

private:
	// Painting handlers for the buttons
	static void PaintShowGridBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintAutoCropBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintAssistedModeBtn(void* pContext, const CRect& rect, CDC& dc);

	CPanel* m_pImageProcPanel;
	CRect m_clientRect;
	int m_nWidth, m_nHeight;
	int m_nButtonSize;
};