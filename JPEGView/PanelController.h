#pragma once

#include "Panel.h"

class CMainDlg;
class CJPEGImage;

// Abstract base class for the panel controllers
// Panel controllers are intermediate classes that link the main dialog with a panel. They implement
// handlers for the GUI controls on the panels and pass mouse and painting events to the panels.
class CPanelController : public INotifiyMouseCapture
{
public:
	// Binds controller with main dialog. A modal panel means that no other modal panel can be active at the same time.
	// The image processing panel, navigation panel and window buttons are also disabled for a modal panel.
	CPanelController(CMainDlg* pMainDlg, bool bIsModal);
	virtual ~CPanelController() {}

	bool IsModal() { return m_bIsModal; }
	virtual bool IsVisible() = 0;  // is panel visible
	virtual bool IsActive() = 0;   // is panel active (but maybe temporarily not visible)

	CPanel* GetPanel() { return m_pPanel; }

	CRect PanelRect() { return m_pPanel->PanelRect(); } // panel rectangle in client (Main window) coordinates

	virtual bool BlendPanel() { return false; } // Blend panel with background when painting
	virtual float DimFactor() = 0; // Blending factor of background when painting panel, 0 no dimming, 1 full dimming to black

	virtual void SetVisible(bool bVisible) = 0;
	virtual void SetActive(bool bActive) = 0;

	// Override to cancel a model panel, does nothing for non modal panels
	virtual void CancelModalPanel() {};

	virtual void AfterNewImageLoaded() {} // called after a new image is loaded
	virtual void AfterImageRenamed() {} // called ater an image is renamed

	// Only called by CPanelMgr - passes events to the panel controller
	// If returning true, the panel is consuming the event
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);
	virtual bool MouseCursorCaptured();
	virtual bool OnKeyDown(unsigned int nVirtualKey, bool bShift, bool bAlt, bool bCtrl) { return false; }
	virtual bool OnTimer(int nTimerId) { return false; }
	virtual void OnPaintPanel(CDC & dc, const CPoint& offset); // dc will be a memory DC
	virtual void OnPrePaintMainDlg(HDC hPaintDC) {}
	virtual void OnPostPaintMainDlg(HDC hPaintDC) {}

protected:
	void InvalidateMainDlg(); // Invalidate whole main dialog area
	CJPEGImage* CurrentImage(); // Current image displayed

	// Implementation of INotifiyMouseCapture, called by CPanel
	virtual void MouseCapturedOrReleased(bool bCaptured, CUICtrl* pCtrl);

	CMainDlg* m_pMainDlg;
	CPanel* m_pPanel;
	bool m_bIsModal;
};