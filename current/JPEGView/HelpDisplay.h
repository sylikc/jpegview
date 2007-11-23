#pragma once

// Class to display the help screen (when F1 is pressed)
class CHelpDisplay
{
public:
	// The paint DC is used to draw the help screen
	CHelpDisplay(CPaintDC & dc);
	~CHelpDisplay();

	// Methods to create the help text lines
	void AddTitle(LPCTSTR sTitle);
	void AddLine(LPCTSTR sLine);
	void AddLine(LPCTSTR sKey, LPCTSTR sHelp);
	void AddLineInfo(LPCTSTR sKey, LPCTSTR sInfo, LPCTSTR sHelp);
	void AddLineInfo(LPCTSTR sKey, bool bInfo, LPCTSTR sHelp);

	// Call Show() to render the help screen on the given screen rectangle (it is centered there)
	void Show(const CRect & screenRect);

	// Gets the size of the help screen
	CSize GetSize();

private:

	struct TextLine {
		TextLine(COLORREF color, COLORREF colorinfo, LPCTSTR key, LPCTSTR info, LPCTSTR help) {
			Color = color;
			ColorInfo = colorinfo;
			Key = key;
			Info = info;
			Help = help;
		}

		COLORREF Color;
		COLORREF ColorInfo;
		LPCTSTR Key;
		LPCTSTR Info;
		LPCTSTR Help;
	};

	LPCTSTR CopyStr(LPCTSTR str);
	void AddTextLine(COLORREF color, COLORREF colorinfo, LPCTSTR key, LPCTSTR info, LPCTSTR help);

	float m_fScaling;
	int m_nTab1, m_nTab2;
	CPaintDC & m_dc;
	std::list<TextLine> m_lines;
	int m_nSizeX;
	int m_nSizeY;
	int m_nIncY;
	TCHAR* m_pTextBuffStart;
	int m_nTextBuffIndex;
};
