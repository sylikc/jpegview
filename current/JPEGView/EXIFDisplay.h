#pragma once

#include "EXIFReader.h"
#include "GUIControls.h"

class CHistogram;

// Displays EXIF information on the screen (used when F2 is pressed)
class CEXIFDisplay : public CPanelMgr
{
public:
	CEXIFDisplay(HWND hWnd);
	~CEXIFDisplay();

	// Methods to create the information lines
	void ClearTexts();
	void AddTitle(LPCTSTR sTitle);
	void SetComment(LPCTSTR sComment);
	void AddLine(LPCTSTR sDescription, LPCTSTR sValue);
	void AddLine(LPCTSTR sDescription, double dValue, int nDigits);
	void AddLine(LPCTSTR sDescription, int nValue);
	void AddLine(LPCTSTR sDescription, const SYSTEMTIME &time);
	void AddLine(LPCTSTR sDescription, const FILETIME &time);
	void AddLine(LPCTSTR sDescription, const Rational &number);

	void SetPosition(CPoint pos) { m_pos = pos; RepositionAll(); }
	virtual CRect PanelRect();
	virtual void RequestRepositioning();
	virtual void OnPaint(CDC & dc, const CPoint& offset);

	void SetShowHistogram(bool bShow) { m_bShowHistogram = bShow; RepositionAll(); }
	bool GetShowHistogram() { return m_bShowHistogram; }

	void SetHistogram(const CHistogram* pHistogram) { m_pHistogram = pHistogram; }

	// Painting handlers for the buttons
	static void PaintShowHistogramBtn(const CRect& rect, CDC& dc, bool bShowHistogram);
	
protected:
	virtual void RepositionAll();

private:

	struct TextLine {
		TextLine(LPCTSTR desc, LPCTSTR value) {
			Desc = desc;
			Value = value;
		}

		LPCTSTR Desc;
		LPCTSTR Value;
	};

	bool m_bShowHistogram;
	int m_nGap;
	int m_nTab1;
	int m_nLineHeight;
	int m_nTitleHeight;
	CSize m_nNoHistogramSize;
	CPoint m_pos;
	CSize m_size;
	HFONT m_hTitleFont;
	TCHAR* m_sTitle;
	TCHAR* m_sComment;
	int m_nCommentHeight;
	std::list<TextLine> m_lines;
	const CHistogram* m_pHistogram;

	void PaintHistogram(CDC & dc, int nXStart, int nYBaseLine);
};
