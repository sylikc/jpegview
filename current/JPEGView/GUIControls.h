#pragma once

// CSliderMgr.OnMouseLButton uses this enum
enum EMouseEvent {
	MouseEvent_BtnDown,
	MouseEvent_BtnUp
};

class CTextCtrl;
class CButtonCtrl;
class CPanelMgr;
class CSliderMgr;
class CUICtrl;

// Defines a handler procedure called by CTextCtrl when the text has changed
// If true is returned, the text shall be set to the new text, if false, the text remains (or is set
// by the handler procedure)
typedef bool TextChangedHandler(CTextCtrl & sender, LPCTSTR sChangedText);

// Defines a handler procedure called by CButtonCtrl when the button is pressed
typedef void ButtonPressedHandler(CButtonCtrl & sender);

// Defines a handler procedure for painting a user painted button
typedef void PaintHandler(const CRect& rect, CDC& dc);

//-------------------------------------------------------------------------------------------------
// Tooltip support

// Tooltip class, draws the tooltip centered below the anchor rectangle
class CTooltip {
public:
	CTooltip(HWND hWnd, const CUICtrl* pBoundCtrl, LPCTSTR sTooltip) : 
	  m_hWnd(hWnd), m_pBoundCtrl(pBoundCtrl), m_sTooltip(sTooltip), m_TooltipRect(0, 0, 0, 0) {}

	const CUICtrl* GetBoundCtrl() const { return m_pBoundCtrl; }
	const CString& GetTooltip() const { return m_sTooltip; }

	void Paint(CDC & dc) const;
	CRect GetTooltipRect() const;
private:
	HWND m_hWnd;
	const CUICtrl* m_pBoundCtrl;
	CString m_sTooltip;
	CRect m_TooltipRect;

	CRect CalculateTooltipRect() const;
};

// Tooltip manager
class CTooltipMgr {
public:
	CTooltipMgr(HWND hWnd);

public:
	void OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	void OnMouseMove(int nX, int nY);
	void OnPaint(CDC & dc);

	void AddTooltip(CUICtrl* pBoundCtrl, LPCTSTR sTooltip);
	void RemoveActiveTooltip();
	void EnableTooltips(bool bEnable); // default is true
private:

	HWND m_hWnd;
	CTooltip* m_pActiveTooltip;
	CTooltip* m_pMouseOnTooltip;
	CTooltip* m_pBlockedTooltip;
	std::list<CTooltip*> m_TooltipList;
	bool m_bEnableTooltips;
};

//-------------------------------------------------------------------------------------------------

// Base class for UI controls
class CUICtrl {
public:
	CUICtrl(CPanelMgr* pPanelMgr);
	
	CRect GetPosition() const { return m_position; }
	void SetPosition(CRect position) { m_position = position; }
	bool IsShown() const { return m_bShow; }
	void SetShow(bool bShow);
	void SetTooltip(LPCTSTR sTooltipText);

public:
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) = 0;
	virtual bool OnMouseMove(int nX, int nY) = 0;
	virtual void OnPaint(CDC & dc, const CPoint& offset);
	
protected:
	friend class CSliderMgr;

	virtual void Draw(CDC & dc, CRect position, bool bBlack) = 0;

	CPanelMgr* m_pMgr;
	bool m_bShow;
	bool m_bHighlight;
	CRect m_position;
	CPoint m_pos2Screen; // the offset from the paint position to the screen position (used when painting to memory DC)
};

//-------------------------------------------------------------------------------------------------

// Represents some extra gap between buttons - only recognized bz navigation panel
class CGapCtrl : public CUICtrl {
public:
	CGapCtrl(CPanelMgr* pPanelMgr, int nGap) : CUICtrl(pPanelMgr) { m_nGap = nGap; m_bShow = false; }

	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) { return false; }
	virtual bool OnMouseMove(int nX, int nY) { return false; }
	virtual void Draw(CDC & dc, CRect position, bool bBlack) {}

	int GapWidth() { return m_nGap; }

private:
	int m_nGap;
};

//-------------------------------------------------------------------------------------------------

// Control for showing static or editable text
class CTextCtrl : public CUICtrl {
public:
	CTextCtrl(CPanelMgr* pPanelMgr, LPCTSTR sTextInit, bool bEditable, TextChangedHandler textChangedHandler);
	
	// sets the text of the text control
	void SetText(LPCTSTR sText);
	// gets the width of the text in pixels
	int GetTextLabelWidth() const { return m_nTextWidth; }
	// gets if the text is editable
	bool IsEditable() const { return m_bEditable; }
	// gets if the text is right aligned to the screen
	bool IsRightAligned() const { return m_bRightAligned; }
	// sets if the text is editable
	void SetEditable(bool bEditable);
	// sets the maximal text width in pixels
	void SetMaxTextWidth(int nMaxWidth) { m_nMaxTextWidth = nMaxWidth; }
	// Sets this text as right aligned on the right border of the screen (true) or left aligned (default, false)
	void SetRightAligned(bool bValue) { m_bRightAligned = bValue; }
	// terminates the edit mode and accept the text change if the flag is true. If false, the text change
	// is undone.
	void TerminateEditMode(bool bAcceptNewName);

public:
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);

protected:
	virtual void Draw(CDC & dc, CRect position, bool bBlack);

private:

	// Subclass procedure 
	static LRESULT APIENTRY EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	WNDPROC m_OrigEditProc; // saves original windows procedure

	CString m_sText;
	int m_nTextWidth;
	int m_nMaxTextWidth;
	bool m_bEditable;
	bool m_bRightAligned;
	CEdit* m_pEdit;
	TextChangedHandler* m_textChangedHandler;
};

//-------------------------------------------------------------------------------------------------

// Control for showing a button
class CButtonCtrl : public CUICtrl {
public:
	CButtonCtrl(CPanelMgr* pPanelMgr, LPCTSTR sButtonText, ButtonPressedHandler buttonPressedHandler);
	CButtonCtrl(CPanelMgr* pPanelMgr, PaintHandler paintHandler, ButtonPressedHandler buttonPressedHandler);

	// gets the width of the button text in pixels
	int GetTextLabelWidth() const { return m_nTextWidth; }

	void SetEnabled(bool bEnabled);

public:
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);

protected:
	virtual void Draw(CDC & dc, CRect position, bool bBlack);

private:
	CString m_sText;
	int m_nTextWidth;
	bool m_bDragging;
	bool m_bEnabled;
	ButtonPressedHandler* m_buttonPressedHandler;
	PaintHandler* m_paintHandler;
};

//-------------------------------------------------------------------------------------------------

// Transparent slider control allowing to select a value in a range
class CSliderDouble : public CUICtrl {
public:
	CSliderDouble(CPanelMgr* pMgr, LPCTSTR sName, int nSliderLen, double* pdValue, bool* pbEnable,
		double dMin, double dMax, bool bLogarithmic, bool bInvert);

	double* GetValuePtr() { return m_pValue; }
	int GetNameLabelWidth() const { return m_nNameWidth; }

public:
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);
	virtual void OnPaint(CDC & dc, const CPoint& offset);

protected:
	virtual void Draw(CDC & dc, CRect position, bool bBlack);

private:
	CString m_sName;
	int m_nSliderStart, m_nSliderLen, m_nSliderHeight;
	double* m_pValue;
	bool* m_pEnable;
	double m_dMin, m_dMax;
	CRect m_sliderRect;
	CRect m_checkRect;
	CRect m_checkRectFull;
	int m_nNumberWidth;
	int m_nCheckHeight;
	int m_nNameWidth;
	bool m_bLogarithmic;
	int m_nSign;
	bool m_bDragging;
	bool m_bHighlight;
	bool m_bHighlightCheck;

	void DrawRuler(CDC & dc, int nXStart, int nXEnd, int nY, bool bBlack, bool bHighlight);
	void DrawCheck(CDC & dc, CRect position, bool bBlack, bool bHighlight);
	int ValueToSliderPos(double dValue, int nSliderStart, int nSliderEnd);
	double SliderPosToValue(int nSliderPos, int nSliderStart, int nSliderEnd);
};

//-------------------------------------------------------------------------------------------------


// Manages and lay-outs the UI controls on a panel
class CPanelMgr {
public:
	// The UI controls are created on the given window
	CPanelMgr(HWND hWnd);
	virtual ~CPanelMgr(void);

	// Get size and position of the panel
	virtual CRect PanelRect() = 0;

	// Add a slider, controlling the value pdValue and labeled sName.
	void AddSlider(LPCTSTR sName, double* pdValue, bool* pbEnable, double dMin, double dMax, 
		bool bLogarithmic, bool bInvert, int nWidth);
	// Show/hide a slider, identified by its controlled value
	void ShowHideSlider(bool bShow, double* pdValue);

	// Adds a text control to the slider area. The text can be editable or static. The handler
	// procedure gets called for editable texts when the text has been changed. It must be null when
	// bEditable is false.
	CTextCtrl* AddText(LPCTSTR sTextInit, bool bEditable, TextChangedHandler textChangedHandler);

	// Adds a button to the slider area. The given handler procedure is called when the button is pressed.
	CButtonCtrl* AddButton(LPCTSTR sButtonText, ButtonPressedHandler buttonPressedHandler);

	// Adds a user painted button
	CButtonCtrl* AddUserPaintButton(PaintHandler paintHandler, ButtonPressedHandler buttonPressedHandler, LPCTSTR sTooltip);

	// Mouse events must be passed to the panel using the two methods below.
	// The mouse event is consumed by a UI control if the return value is true.
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);
	// Must be called when painting the panel (hWnd given in constructor)
	virtual void OnPaint(CDC & dc, const CPoint& offset);

	// Gets the HWND all UI controls are placed on
	HWND GetHWND() const { return m_hWnd; }

	void CaptureMouse(CUICtrl* pCtrl);
	void ReleaseMouse(CUICtrl* pCtrl);

	// Request recalculation of the layout
	virtual void RequestRepositioning() = 0;

	// Gets the manager for all tooltips
	CTooltipMgr& GetTooltipMgr() { return m_tooltipMgr; }

protected:
	// recalculates the layout
	virtual void RepositionAll() = 0;

	HWND m_hWnd;
	float m_fDPIScale;
	std::list<CUICtrl*> m_ctrlList;
	CUICtrl* m_pCtrlCaptureMouse;
	CTooltipMgr m_tooltipMgr;
};

// Manages a set of sliders and other controls by placing them on the bottom of the screen and
// routing events to the sliders, buttons, etc.
class CSliderMgr : public CPanelMgr {
public:
	// The sliders are placed on the given window (at bottom)
	CSliderMgr(HWND hWnd);

	// height of slider area
	int SliderAreaHeight() const { return m_nTotalAreaHeight; }
	virtual CRect PanelRect();

	void AddSlider(LPCTSTR sName, double* pdValue, bool* pbEnable, double dMin, double dMax, 
		bool bLogarithmic, bool bInvert);

	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);

	// Request recalculation of the layout
	virtual void RequestRepositioning() { m_clientRect = CRect(0, 0, 0, 0); }

protected:
	virtual void RepositionAll();

private:
	int m_nSliderWidth;
	int m_nSliderHeight;
	int m_nNoLabelWidth;
	int m_nSliderGap;
	int m_nTotalAreaHeight;
	int m_nYStart;
	CRect m_clientRect;
	CRect m_sliderAreaRect;
	int m_nTotalSliders;
};

// Navigation panel containing buttons and sliders
class CNavigationPanel : public CPanelMgr {
public:
	// The panel is on the given window on top of the given slider manager
	CNavigationPanel(HWND hWnd, CSliderMgr* pSliderMgr);

	void AddGap(int nGap);

	virtual CRect PanelRect();
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual void RequestRepositioning();

	// Painting handlers for the buttons
	static void PaintHomeBtn(const CRect& rect, CDC& dc);
	static void PaintPrevBtn(const CRect& rect, CDC& dc);
	static void PaintNextBtn(const CRect& rect, CDC& dc);
	static void PaintEndBtn(const CRect& rect, CDC& dc);
	static void PaintRotateCWBtn(const CRect& rect, CDC& dc);
	static void PaintRotateCCWBtn(const CRect& rect, CDC& dc);
	static void PaintInfoBtn(const CRect& rect, CDC& dc);
	static void PaintLandscapeModeBtn(const CRect& rect, CDC& dc);

protected:
	virtual void RepositionAll();

private:

	CSliderMgr* m_pSliderMgr;
	CRect m_clientRect;
	int m_nWidth, m_nHeight;
	int m_nBorder;
	int m_nGap;
};
