#pragma once

#include "Panel.h"

class CUnsharpMaskParams;

// Panel for unsharp masking an image
class CUnsharpMaskPanel : public CPanel {
public:
	// IDs of the controls on this panel
	enum {
		ID_txtTitle,
		ID_slRadius,
		ID_slAmount,
		ID_slThreshold,
		ID_btnCancel,
		ID_btnApply
	};
public:
	// The panel is on the given window above the image processing panel
	CUnsharpMaskPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel, CUnsharpMaskParams* pParams);

	CButtonCtrl* GetBtnCancel() { return GetControl<CButtonCtrl*>(ID_btnCancel); }
	CButtonCtrl* GetBtnApply() { return GetControl<CButtonCtrl*>(ID_btnApply); }

	virtual CRect PanelRect();
	virtual void RequestRepositioning();

protected:
	virtual void RepositionAll();

private:

	CPanel* m_pImageProcPanel;
	CRect m_clientRect;
	int m_nWidth, m_nHeight;
	int m_nMaxSliderWidth;
};