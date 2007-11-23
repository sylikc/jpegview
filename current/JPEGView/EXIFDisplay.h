#pragma once

#include "EXIFReader.h"

// Displays EXIF information on the screen (used when F2 is pressed)
class CEXIFDisplay
{
public:
	CEXIFDisplay();
	~CEXIFDisplay();

	// Methods to create the information lines
	void AddTitle(LPCTSTR sTitle);
	void AddLine(LPCTSTR sDescription, LPCTSTR sValue);
	void AddLine(LPCTSTR sDescription, double dValue, int nDigits);
	void AddLine(LPCTSTR sDescription, int nValue);
	void AddLine(LPCTSTR sDescription, const SYSTEMTIME &time);
	void AddLine(LPCTSTR sDescription, const FILETIME &time);
	void AddLine(LPCTSTR sDescription, const Rational &number);

	// Gets the size needed to display the information
	CSize GetSize(CDC & dc);

	// Call Show() to render the EXIF info on the given DC and position
	void Show(CDC & dc, int nX, int nY);

private:

	struct TextLine {
		TextLine(LPCTSTR desc, LPCTSTR value) {
			Desc = desc;
			Value = value;
		}

		LPCTSTR Desc;
		LPCTSTR Value;
	};

	float m_fScaling;
	int m_nGap;
	int m_nTab1;
	int m_nLineHeight;
	int m_nTitleHeight;
	CSize m_size;
	HFONT m_hTitleFont;
	TCHAR* m_sTitle;
	std::list<TextLine> m_lines;
};
