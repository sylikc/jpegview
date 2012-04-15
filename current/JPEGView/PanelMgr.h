#pragma once

#include "PanelController.h"

class CPaintMemDCMgr;

// Manages the panel controllers and panels. Note that deleting the panel manager deletes all managed panels.
class CPanelMgr
{
public:
	CPanelMgr();
	~CPanelMgr();

	// All managed panels must be added with this method
	void AddPanelController(CPanelController* pPanelController);

	// Is current a modal panel visible?
	bool IsModalPanelShown() const;

    // Cancels an open modal panel if one is open
    void CancelModalPanel();

	// The captured panel controller gets the mouse events first
	void SetCapturedPanelController(CPanelController* pPanelController) { m_pCapturedPanelController = pPanelController; }

	// Prepares the given memory DC manager for offscreen painting of all managed panels. The panel areas are added
	// to the given list of exclusion rectangles (to exclude from clipping region)
	void PrepareMemDCMgr(CPaintMemDCMgr& memDCMgr, std::list<CRect>& listExcludedRects);

	// Called by main dialog -> routed to managed panels
	void AfterNewImageLoaded();
	void AfterImageRenamed();

	// Called by main dialog -> events routed to managed panels
	// Returning bool means that a panel has consumed the event
	bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	bool OnMouseMove(int nX, int nY);
	bool OnKeyDown(unsigned int nVirtualKey, bool bShift, bool bAlt, bool bCtrl);
	bool OnTimer(int nTimerId);
	void PaintPanels(CDC & dc, const CPoint& offset);
	void OnPrePaint(HDC hPaintDC);
	void OnPostPaint(HDC hPaintDC);

private:
	std::list<CPanelController*> m_panelControllers;
	CPanelController* m_pCapturedPanelController;
};