#pragma once

// CUICtrl.OnMouseLButton uses this enum
enum EMouseEvent {
	MouseEvent_BtnDown,
	MouseEvent_BtnUp,
	MouseEvent_BtnDblClk
};

class CTextCtrl;
class CButtonCtrl;
class CPanel;
class CUICtrl;

// Defines a handler procedure called by CTextCtrl when the text has changed
// If true is returned, the text shall be set to the new text, if false, the text remains (or is set
// by the handler procedure)
typedef bool TextChangedHandler(void* pContext, CTextCtrl & sender, LPCTSTR sChangedText);

// Defines a handler procedure called by CButtonCtrl when the button is pressed
typedef void ButtonPressedHandler(void* pContext, int nParameter, CButtonCtrl & sender);

// Defines a handler procedure for painting a user painted button
typedef void PaintHandler(void* pContext, const CRect& rect, CDC& dc);

// Defines a handler procedure for returning a tooltip for a button
typedef LPCTSTR TooltipHandler(void* pContext);

// Defines a method to decide something (returns a boolean)
typedef bool DecisionMethod(void* pContext);

//-------------------------------------------------------------------------------------------------
// Tooltip support

// Tooltip class, draws the tooltip centered below the anchor rectangle (given by bounding box of pBoundCtrl)
class CTooltip {
public:
	CTooltip(HWND hWnd, const CUICtrl* pBoundCtrl, LPCTSTR sTooltip);
	CTooltip(HWND hWnd, const CUICtrl* pBoundCtrl, TooltipHandler ttHandler, void* pContext = NULL);

	const CUICtrl* GetBoundCtrl() const { return m_pBoundCtrl; }
	const CString& GetTooltip() const;

	void Paint(CDC & dc) const;
	CRect GetTooltipRect() const;
private:
	HWND m_hWnd;
	const CUICtrl* m_pBoundCtrl;
	TooltipHandler* m_ttHandler;
	void* m_pContext;
	CString m_sTooltip;
	CRect m_TooltipRect;
	CRect m_oldRectBoundCtrl;

	CRect CalculateTooltipRect() const;
};
//-------------------------------------------------------------------------------------------------

// Tooltip manager
class CTooltipMgr {
public:
	CTooltipMgr(HWND hWnd);

public:
	void OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	void OnMouseMove(int nX, int nY);
	void OnPaint(CDC & dc);

	void AddTooltip(CUICtrl* pBoundCtrl, CTooltip* tooltip); // takes ownership of the tooltip
	void AddTooltip(CUICtrl* pBoundCtrl, LPCTSTR sTooltip);
	void AddTooltipHandler(CUICtrl* pBoundCtrl, TooltipHandler ttHandler, void* pContext = NULL);
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
	CUICtrl(CPanel* pPanel);
	
	CRect GetPosition() const { return m_position; }
	virtual CSize GetMinSize() = 0;
	bool IsShown() const { return m_bShow; }
	void SetShow(bool bShow) { SetShow(bShow, true); }
	void SetShow(bool bShow, bool bInvalidate);
	void SetTooltip(LPCTSTR sTooltipText);
	void SetTooltipHandler(TooltipHandler ttHandler, void* pContext = NULL);

public:
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) = 0;
	virtual bool OnMouseMove(int nX, int nY) = 0;
	virtual void OnPaint(CDC & dc, const CPoint& offset);
	
	void SetPosition(CRect position) { m_position = position; }
protected:
	virtual void PreDraw(CDC & dc, CRect position);
	virtual void Draw(CDC & dc, CRect position, bool bBlack) = 0;
	
	CPanel* m_pPanel;
	bool m_bShow;
	bool m_bHighlight;
	CRect m_position;
	CPoint m_pos2Screen; // the offset from the paint position to the screen position (used when painting to memory DC)
};

//-------------------------------------------------------------------------------------------------

// Represents some extra gap between UI elements
class CGapCtrl : public CUICtrl {
public:
	CGapCtrl(CPanel* pPanel, int nGap) : CUICtrl(pPanel) { m_nGap = nGap; m_bShow = false; }

	virtual CSize GetMinSize() { return CSize(m_nGap, 0); }
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) { return false; }
	virtual bool OnMouseMove(int nX, int nY) { return false; }

	int GapWidth() { return m_nGap; }

protected:
	virtual void Draw(CDC & dc, CRect position, bool bBlack) {}
private:
	int m_nGap;
};

//-------------------------------------------------------------------------------------------------

// Control for showing static or editable text
class CTextCtrl : public CUICtrl {
public:
	CTextCtrl(CPanel* pPanel, LPCTSTR sTextInit, bool bEditable, TextChangedHandler* textChangedHandler = NULL, void* pContext = NULL);
	~CTextCtrl();

	void SetTextChangedHandler(TextChangedHandler* textChangedHandler, void* pContext) { m_textChangedHandler = textChangedHandler; m_pContext = pContext; }

	// sets the text of the text control
	void SetText(LPCTSTR sText);
	// gets the text of the text control
	LPCTSTR GetText() { return m_sText; }
	// gets the width of the text in pixels. In case of multiline, the returned size is the singleline size.
	int GetTextLabelWidth() const { return m_textSize.cx; }
	// gets the height of the text in pixels.
	int GetTextLabelHeight() const { return m_textSize.cy; }
	// gets the text rectangle height when using a given width in pixels (also works for multiline)
	int GetTextRectangleHeight(HWND hWnd, int nWidth);
	// gets if the text is editable
	bool IsEditable() const { return m_bEditable; }
	// gets if the text is right aligned to the screen
	bool IsRightAligned() const { return m_bRightAligned; }
	// gets if the text is bold
	bool IsBold() const { return m_bBold; }
	// sets if the text is editable
	void SetEditable(bool bEditable);
	// sets if the text can be multiline
	void SetAllowMultiline(bool bMultiline) { m_bAllowMultiline = bMultiline; }
	// sets the maximal text width in pixels in the edit mode. Ignored when m_bEditable == false
	void SetMaxTextWidth(int nMaxWidth) { m_nMaxTextWidth = nMaxWidth; }
	// Sets this text as right aligned on the right border of the screen (true) or left aligned (default, false)
	void SetRightAligned(bool bValue) { m_bRightAligned = bValue; }
	// Sets if the text is bold or not
	void SetBold(bool bValue);
	// terminates the edit mode and accept the text change if the flag is true. If false, the text change is undone.
	void TerminateEditMode(bool bAcceptNewName);

public:
	virtual CSize GetMinSize() { return m_textSize; }
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);

protected:
	virtual void Draw(CDC & dc, CRect position, bool bBlack);

private:
	void CreateBoldFont(CDC & dc);
	CSize GetTextRectangle(HWND hWnd, LPCTSTR sText);

	// Subclass procedure 
	static LRESULT APIENTRY EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	WNDPROC m_OrigEditProc; // saves original windows procedure

	CString m_sText;
	CSize m_textSize;
	int m_nMaxTextWidth;
	bool m_bEditable;
	bool m_bRightAligned;
	bool m_bBold;
	bool m_bAllowMultiline;
	CEdit* m_pEdit;
	TextChangedHandler* m_textChangedHandler;
	void* m_pContext;
	HFONT m_hBoldFont;
};

//-------------------------------------------------------------------------------------------------

// Control for showing a button
class CButtonCtrl : public CUICtrl {
public:
	CButtonCtrl(CPanel* pPanel, LPCTSTR sButtonText, ButtonPressedHandler* buttonPressedHandler = NULL, void* pContext = NULL, int nParameter = 0);
	CButtonCtrl(CPanel* pPanel, PaintHandler* paintHandler, ButtonPressedHandler* buttonPressedHandler = NULL, void* pPaintContext = NULL, void* pContext = NULL, int nParameter = 0);

	void SetButtonPressedHandler(ButtonPressedHandler* buttonPressedHandler, void* pContext, int nParameter = 0, bool bActive = false) { 
		m_buttonPressedHandler = buttonPressedHandler; 
		m_pContext = pContext; 
		m_nParameter = nParameter;
		SetActive(bActive); }

	// gets the width of the button text in pixels
	int GetTextLabelWidth() const { return m_textSize.cx; }

	// Sets if the button is active (highlighted)
	void SetActive(bool bActive);

	// extends the active area of the button over the normal position
	void SetExtendedActiveArea(CRect rect) { m_extendedArea = rect; }

	bool IsUserPaintButton() { return m_paintHandler != NULL; }

	// Sets the dimming factor for the button background, 0 for no dimming 1.0 for blackness
	void SetDimmingFactor(float factor) { m_dimmingFactor = factor; }

public:
	virtual CSize GetMinSize();
	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);

protected:
	virtual void PreDraw(CDC & dc, CRect position);
	virtual void Draw(CDC & dc, CRect position, bool bBlack);

private:
	CString m_sText;
	CSize m_textSize;
	float m_dimmingFactor;
	bool m_bDragging;
	bool m_bActive;
	CRect m_extendedArea;
	ButtonPressedHandler* m_buttonPressedHandler;
	void* m_pContext;
	int m_nParameter;
	PaintHandler* m_paintHandler;
	void* m_pPaintContext;
};

//-------------------------------------------------------------------------------------------------

// Transparent slider control allowing to select a value in a range
class CSliderDouble : public CUICtrl {
public:
	CSliderDouble(CPanel* pMgr, LPCTSTR sName, int nSliderLen, double* pdValue, bool* pbEnable,
		double dMin, double dMax, double dDefaultValue, bool bAllowPreviewAndReset, bool bLogarithmic, bool bInvert);

	double* GetValuePtr() { return m_pValue; }
	int GetNameLabelWidth() const { return m_nNameWidth; }

public:
	virtual CSize GetMinSize();
	void SetSliderLen(int nSliderLen) { m_nSliderLen = nSliderLen; }
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
	double m_dDefaultValue;
	double m_dSavedValue;
	CRect m_sliderRect;
	CRect m_checkRect;
	CRect m_checkRectFull;
	int m_nNumberWidth;
	int m_nCheckHeight;
	int m_nNameWidth;
	CSize m_textSize;
	bool m_bLogarithmic;
	int m_nSign;
	bool m_bDragging;
	bool m_bNumberClicked;
	bool m_bHighlight;
	bool m_bHighlightCheck;
	bool m_bHighlightNumber;
	bool m_bAllowPreviewAndReset;

	void DrawRuler(CDC & dc, int nXStart, int nXEnd, int nY, bool bBlack, bool bHighlight);
	void DrawCheck(CDC & dc, CRect position, bool bBlack, bool bHighlight);
	void SetValue(double dValue);
	int ValueToSliderPos(double dValue, int nSliderStart, int nSliderEnd);
	double SliderPosToValue(int nSliderPos, int nSliderStart, int nSliderEnd);
};
