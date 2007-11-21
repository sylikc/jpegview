#pragma once

// CSliderMgr.OnMouseLButton uses this enum
enum EMouseEvent {
	MouseEvent_BtnDown,
	MouseEvent_BtnUp
};

class CTextCtrl;
class CButtonCtrl;
class CSliderMgr;

// Defines a handler procedure called by CTextCtrl when the text has changed
// If true is returned, the text shall be set to the new text, if false, the text remains (or is set
// by the handler procedure)
typedef bool TextChangedHandler(CTextCtrl & sender, LPCTSTR sChangedText);

// Defines a handler procedure called by CButtonCtrl when the button is pressed
typedef void ButtonPressedHandler(CButtonCtrl & sender);

//-------------------------------------------------------------------------------------------------

// Base class for UI controls
class CUICtrl {
public:
	CUICtrl(CSliderMgr* pSliderMgr);
	
	CRect GetPosition() const { return m_position; }
	void SetPosition(CRect position) { m_position = position; }
	bool IsShown() const { return m_bShow; }
	void SetShow(bool bShow);

public:
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) = 0;
	virtual bool OnMouseMove(int nX, int nY) = 0;
	virtual void OnPaint(CDC & dc);
	
protected:
	friend class CSliderMgr;

	void DrawGetDC(bool bBlack);
	virtual void Draw(CDC & dc, CRect position, bool bBlack) = 0;

	CSliderMgr* m_pMgr;
	bool m_bShow;
	bool m_bHighlight;
	CRect m_position;
};

//-------------------------------------------------------------------------------------------------

// Control for showing static or editable text
class CTextCtrl : public CUICtrl {
public:
	CTextCtrl(CSliderMgr* pSliderMgr, LPCTSTR sTextInit, bool bEditable, TextChangedHandler textChangedHandler);
	
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
	CButtonCtrl(CSliderMgr* pSliderMgr, LPCTSTR sButtonText, ButtonPressedHandler buttonPressedHandler);

	// gets the width of the button text in pixels
	int GetTextLabelWidth() const { return m_nTextWidth; }

public:
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);

protected:
	virtual void Draw(CDC & dc, CRect position, bool bBlack);

private:
	CString m_sText;
	int m_nTextWidth;
	bool m_bDragging;
	ButtonPressedHandler* m_buttonPressedHandler;
};

//-------------------------------------------------------------------------------------------------

// Transparent slider control allowing to select a value in a range
class CSliderDouble : public CUICtrl {
public:
	CSliderDouble(CSliderMgr* pMgr, LPCTSTR sName, int nSliderLen, double* pdValue, bool* pbEnable,
		double dMin, double dMax, bool bLogarithmic, bool bInvert);

	double* GetValuePtr() { return m_pValue; }
	int GetNameLabelWidth() const { return m_nNameWidth; }

public:
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);
	virtual void OnPaint(CDC & dc);

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

	void DrawRulerGetDC(bool bHighlight);
	void DrawRuler(CDC & dc, int nXStart, int nXEnd, int nY, bool bBlack, bool bHighlight);
	void DrawCheckGetDC(bool bHighlight);
	void DrawCheck(CDC & dc, CRect position, bool bBlack, bool bHighlight);
	int ValueToSliderPos(double dValue, int nSliderStart, int nSliderEnd);
	double SliderPosToValue(int nSliderPos, int nSliderStart, int nSliderEnd);
};

//-------------------------------------------------------------------------------------------------

// Manages a set of sliders and other controls by placing them on the bottom of the screen and
// routing events to the sliders, buttons, etc.
class CSliderMgr
{
public:

	// The sliders are placed on the given window (at bottom)
	CSliderMgr(HWND hWnd);
	~CSliderMgr(void);

	// Add a slider, controlling the value pdValue and labeled sName.
	void AddSlider(LPCTSTR sName, double* pdValue, bool* pbEnable, double dMin, double dMax, 
		bool bLogarithmic, bool bInvert);
	// Show/hide a slider, identified by its controlled value
	void ShowHideSlider(bool bShow, double* pdValue);
	// height of slider area
	int SliderAreaHeight() const { return m_nTotalAreaHeight; }
	// rectangle used by the image processing area
	CRect SliderAreaRect();

	// Adds a text control to the slider area. The text can be editable or static. The handler
	// procedure gets called for editable texts when the text has been changed. It must be null when
	// bEditable is false.
	CTextCtrl* AddText(LPCTSTR sTextInit, bool bEditable, TextChangedHandler textChangedHandler);

	// Adds a button to the slider area. The given handler procedure is called when the button is pressed.
	CButtonCtrl* AddButton(LPCTSTR sButtonText, ButtonPressedHandler buttonPressedHandler);

	// Mouse events must be passed to the slider manager using the two methods below.
	// The mouse event is consumed by the sliders if the return value is true.
	bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	bool OnMouseMove(int nX, int nY);
	// Must be called when painting the window (hWnd given in constructor)
	void OnPaint(CPaintDC & dc);

	// Gets the HWND all UI controls are placed on
	HWND GetHWND() const { return m_hWnd; }

	void CaptureMouse(CUICtrl* pCtrl);
	void ReleaseMouse(CUICtrl* pCtrl);

	// Request recalculation of the layout
	void RequestRepositioning() { m_clientRect = CRect(0, 0, 0, 0); }

private:
	HWND m_hWnd;
	float m_fDPIScale;
	int m_nSliderWidth;
	int m_nSliderHeight;
	int m_nNoLabelWidth;
	int m_nSliderGap;
	int m_nTotalAreaHeight;
	int m_nTotalSliders;
	int m_nYStart;
	std::list<CUICtrl*> m_ctrlList;
	CUICtrl* m_pCtrlCaptureMouse;
	CRect m_clientRect;
	CRect m_sliderAreaRect;

	void RepositionAll();
};
