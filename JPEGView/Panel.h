#pragma once

#include "GUIControls.h"
#include <map>

typedef std::map<int, CUICtrl*>::iterator ControlsIterator;

typedef std::map<int, CUICtrl*>::const_iterator ControlsConstIterator;

// Interface to get notifications about mouse capturing and releasing by controls
class INotifiyMouseCapture
{
public:
	virtual void MouseCapturedOrReleased(bool bCaptured, CUICtrl* pCtrl) = 0;
};

// Manages and lay-outs UI controls (of base class CUICtrl) on a panel, i.e. a rectangular area.
class CPanel {
public:
	// The UI controls are created on the specified window
	// if bFramed is true, the panel is painted with a frame around its border
	// if bClickThrough is false, mouse clicks that are not consumed the a UI control are consumed by the panel (if clicked on panel),
	// if bClickThrough is true they are not consumed by the panel (click through panel)
	CPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, bool bFramed = false, bool bClickThrough = false);
	virtual ~CPanel(void);

	// Get size and position of the panel
	virtual CRect PanelRect() = 0;

	// Gets the control with this ID, null if no such control
	CUICtrl* GetControl(int nID);
	template<class T> T GetControl(int nID) { return dynamic_cast<T>(GetControl(nID)); }

	// Gets the controls map (ID, Control)
	const std::map<int, CUICtrl*> & GetControls() { return m_controls; }

	// Mouse events must be passed to the panel using the two methods below.
	// The mouse event is consumed by a UI control if the return value is true.
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);
	virtual bool MouseCursorCaptured();

	// Must be called when painting the panel (hWnd given in constructor)
	virtual void OnPaint(CDC & dc, const CPoint& offset);

	// Gets the HWND all UI controls are placed on
	HWND GetHWND() const { return m_hWnd; }

	// Called by some controls to capture the mouse within the control
	void CaptureMouse(CUICtrl* pCtrl);
	void ReleaseMouse(CUICtrl* pCtrl);

	// Request recalculation of the layout
	virtual void RequestRepositioning() = 0;

	// Gets the manager for tooltips
	CTooltipMgr& GetTooltipMgr() { return m_tooltipMgr; }

protected:
	// Add a slider, controlling the value pdValue and labeled sName.
	CSliderDouble* AddSlider(int nID, LPCTSTR sName, double* pdValue, bool* pbEnable, 
				double dMin, double dMax, double dDefaultValue, bool bAllowPreviewAndReset, bool bLogarithmic, bool bInvert, int nWidth);

	// Adds a text control to the panel. The text can be editable or static. The handler
	// procedure gets called for editable texts when the text has been changed. It must be null when
	// bEditable is false. pContext is passed to the textChangedHandler when called.
	CTextCtrl* AddText(int nID, LPCTSTR sTextInit, bool bEditable, TextChangedHandler* textChangedHandler = NULL, void* pContext = NULL);

	// Adds an URL control to the panel.
	CURLCtrl* AddURL(int nID, LPCTSTR sText, LPCTSTR sURL, bool outlineText, void* pContext = NULL);

	// Adds a button to the panel. The given handler procedure is called when the button is pressed and pContext and the parameter is passed to it.
	CButtonCtrl* AddButton(int nID, LPCTSTR sButtonText, ButtonPressedHandler* buttonPressedHandler = NULL, void* pContext = NULL, int nParameter = 0);

	// Adds a user painted button.
	// The given handler procedure is called when the button is pressed and pContext and the parameter is passed to it.
	// The paint handler is called for painting, passing the pPaintContext to it.
	CButtonCtrl* AddUserPaintButton(int nID, LPCTSTR sTooltip, PaintHandler* paintHandler, ButtonPressedHandler* buttonPressedHandler = NULL, void* pPaintContext = NULL, void* pContext = NULL, int nParameter = 0);
	CButtonCtrl* AddUserPaintButton(int nID, TooltipHandler* ttHandler, PaintHandler* paintHandler, ButtonPressedHandler* buttonPressedHandler = NULL, void* pPaintContext = NULL, void* pContext = NULL, int nParameter = 0);

	// recalculates the layout of the panel, i.e. positions all UI controls
	virtual void RepositionAll() = 0;

	// Paints a frame around the panel
	void PaintFrameAroundPanel(CDC & dc, const CRect& rect, const CPoint& offset);

	HWND m_hWnd;
	bool m_bFramed;
	bool m_bClickThrough;
	float m_fDPIScale;
	std::map<int, CUICtrl*> m_controls;
	CUICtrl* m_pCtrlCaptureMouse;
	CTooltipMgr m_tooltipMgr;
	INotifiyMouseCapture* m_pNotifyMouseCapture;
};