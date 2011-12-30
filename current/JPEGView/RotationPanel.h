#pragma once

#include "TransformPanel.h"

class CRotationPanel : public CTransformPanel {
public:
	// IDs of the controls on this panel
	enum {
		ID_btnAssistMode = ID_btnApply + 1
	};
public:
	// The panel is on the given window above the image processing panel
	CRotationPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel);

	CButtonCtrl* GetBtnAssistMode() { return GetControl<CButtonCtrl*>(ID_btnAssistMode); }
private:
	// Painting handlers for the buttons;
	static void PaintAssistedModeBtn(void* pContext, const CRect& rect, CDC& dc);
};