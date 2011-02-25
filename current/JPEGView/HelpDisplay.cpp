#include "StdAfx.h"
#include "HelpDisplay.h"
#include "HelpersGUI.h"
#include "NLS.h"

static const int MAX_BUFF = 6000;

static const COLORREF colorTitle = RGB(0, 0, 255);
static const COLORREF colorLines = RGB(0, 0, 0);
static const COLORREF colorInfo = RGB(0, 128, 0);
static const COLORREF colorInfoLight = RGB(160, 0, 0);

CHelpDisplay::CHelpDisplay(CPaintDC & dc) : m_dc(dc) {
	m_fScaling = HelpersGUI::ScreenScaling;
	CSize size;
	dc.SelectStockFont(SYSTEM_FONT);
	dc.GetTextExtent(_T("teststring"), 10, &size);
	m_nTab1 =  (int) (size.cx * 1.8f);
	m_nTab2 = (int) (size.cx * 2.6f);
	m_nSizeX = m_nSizeY = m_nIncY = 0;
	m_pTextBuffStart = new TCHAR[MAX_BUFF];
	m_nTextBuffIndex = 0;
}

CHelpDisplay::~CHelpDisplay() {
	delete [] m_pTextBuffStart;
}

void CHelpDisplay::AddTitle(LPCTSTR sTitle) {
	AddTextLine(colorTitle, colorInfo, sTitle, NULL, NULL);
}

void CHelpDisplay::AddLine(LPCTSTR sLine) {
	AddTextLine(colorLines, colorInfo, sLine, NULL, NULL);
}

void CHelpDisplay::AddLine(LPCTSTR sKey, LPCTSTR sHelp) {
	AddTextLine(colorLines, colorInfo, sKey, NULL, sHelp);
}

void CHelpDisplay::AddLineInfo(LPCTSTR sKey, LPCTSTR sInfo, LPCTSTR sHelp) {
	AddTextLine(colorLines, colorInfo, sKey, sInfo, sHelp);
}

void CHelpDisplay::AddLineInfo(LPCTSTR sKey, bool bInfo, LPCTSTR sHelp) {
	AddTextLine(colorLines, bInfo ? colorInfoLight : colorInfo, sKey, bInfo ? CNLS::GetString(_T("on")) : CNLS::GetString(_T("off")), sHelp);
}

CSize CHelpDisplay::GetSize() {
	// Measure
	m_dc.SelectStockFont(DEFAULT_GUI_FONT);
	m_nSizeX = 0;
	m_nSizeY = 0;
	m_nIncY = 0;
	std::list<TextLine>::iterator iter;
	for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
		CSize size;
		m_dc.GetTextExtent(iter->Key, _tcslen(iter->Key), &size);
		m_nSizeX = max(m_nSizeX, size.cx);
		if (m_nIncY == 0) m_nIncY = size.cy;
		m_nSizeY += m_nIncY;
		if (iter->Info != NULL) {
			m_dc.GetTextExtent(iter->Info, _tcslen(iter->Info), &size);
			m_nSizeX = max(m_nSizeX, size.cx + m_nTab1);
		}
		if (iter->Help != NULL) {
			m_dc.GetTextExtent(iter->Help, _tcslen(iter->Help), &size);
			m_nSizeX = max(m_nSizeX, size.cx + m_nTab2);
		}
	}
	m_nSizeX += 10;
	m_nSizeY += 10;
	return CSize(m_nSizeX, m_nSizeY);
}

void CHelpDisplay::Show(const CRect & screenRect) {
	// Measure
	if (m_nSizeX == 0) {
		GetSize();
	}

	// Paint
	m_dc.SelectStockFont(SYSTEM_FONT);
	CBrush brushBk1; brushBk1.CreateSolidBrush(::GetSysColor(COLOR_3DFACE) - 0x00060606);
	CRect fullRect(CPoint(screenRect.Width()/2 - m_nSizeX/2, screenRect.Height()/2 - m_nSizeY/2),
		CSize(m_nSizeX, m_nSizeY));
	m_dc.FillRect(&fullRect, ::GetSysColorBrush(COLOR_3DFACE));
	m_dc.FrameRect(&fullRect, ::GetSysColorBrush(COLOR_3DSHADOW));
	m_dc.SetBkMode(TRANSPARENT);
	int nStartX = fullRect.left + 5;
	int nStartY = fullRect.top + 2;
	int nI = 0;
	std::list<TextLine>::iterator iter;
	for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
		if ((nI++ & 1) == 0 && nI > 2) {
			m_dc.FillRect(CRect(fullRect.left + 1, nStartY, fullRect.right - 1, nStartY + m_nIncY), brushBk1);
		}
		m_dc.SetTextColor(iter->Color);
		m_dc.TextOut(nStartX, nStartY, iter->Key, _tcslen(iter->Key));
		if (iter->Info != NULL) {
			m_dc.SetTextColor(iter->ColorInfo);
			m_dc.TextOut(nStartX + m_nTab1, nStartY, iter->Info, _tcslen(iter->Info));
		}
		if (iter->Help != NULL) {
			m_dc.SetTextColor(iter->Color);
			m_dc.TextOut(nStartX + m_nTab2, nStartY, iter->Help, _tcslen(iter->Help));
		}
		nStartY += m_nIncY;
		if (nI == 1) {
			m_dc.SelectStockFont(DEFAULT_GUI_FONT);
			nStartY += 3;
		}
	}
}

LPCTSTR CHelpDisplay::CopyStr(LPCTSTR str) {
	if (str == NULL) {
		return NULL;
	}
	int nStartIdx = m_nTextBuffIndex;
	while (*str != 0 && m_nTextBuffIndex < MAX_BUFF) {
		m_pTextBuffStart[m_nTextBuffIndex++] = *str++;
	}
	if (m_nTextBuffIndex < MAX_BUFF) {
		m_pTextBuffStart[m_nTextBuffIndex++] = 0;
		return &(m_pTextBuffStart[nStartIdx]);
	} else {
		return NULL;
	}
}

void CHelpDisplay::AddTextLine(COLORREF color, COLORREF colorinfo, LPCTSTR key, LPCTSTR info, LPCTSTR help) {
	m_nSizeX = 0;
	m_lines.push_back(TextLine(color, colorInfo, CopyStr(key), CopyStr(info), CopyStr(help)));
}