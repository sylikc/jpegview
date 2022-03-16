#include "StdAfx.h"
#include "HelpDisplay.h"
#include "HelpersGUI.h"
#include "NLS.h"

static const int MAX_BUFF = 6000;

static const COLORREF colorLines = RGB(0, 0, 0);
static const COLORREF colorInfo = RGB(0, 128, 0);
static const COLORREF colorInfoLight = RGB(160, 0, 0);

CHelpDisplay::CHelpDisplay(CDC & dc) : m_dc(dc) {
	m_fScaling = HelpersGUI::ScreenScaling;
	CSize size;
	HelpersGUI::SelectDefaultGUIFont(dc);
	dc.GetTextExtent(_T("teststring"), 10, &size);
	m_nTab1 =  (int) (size.cx * 2.1f);
	m_nTab2 = (int) (size.cx * 4.2f);
	m_nSizeX = m_nSizeY = m_nIncY = 0;
	m_pTextBuffStart = new TCHAR[MAX_BUFF];
	m_nTextBuffIndex = 0;
}

CHelpDisplay::~CHelpDisplay() {
	delete [] m_pTextBuffStart;
}

void CHelpDisplay::AddLine(LPCTSTR sLine) {
	AddTextLine(colorLines, colorInfo, sLine, NULL, NULL);
}

bool CHelpDisplay::AddLine(LPCTSTR sKey, LPCTSTR sHelp) {
	if (_tcscmp(sKey, _T("n.a.")) != 0) {
		AddTextLine(colorLines, colorInfo, sKey, NULL, sHelp);
		return true;
	}
	return false;
}

bool CHelpDisplay::AddLineInfo(LPCTSTR sKey, LPCTSTR sInfo, LPCTSTR sHelp) {
	if (_tcscmp(sKey, _T("n.a.")) != 0) {
		AddTextLine(colorLines, colorInfo, sKey, sInfo, sHelp);
		return true;
	}
	return false;
}

bool CHelpDisplay::AddLineInfo(LPCTSTR sKey, bool bInfo, LPCTSTR sHelp) {
	if (_tcscmp(sKey, _T("n.a.")) != 0) {
		AddTextLine(colorLines, bInfo ? colorInfoLight : colorInfo, sKey, bInfo ? CNLS::GetString(_T("on")) : CNLS::GetString(_T("off")), sHelp);
		return true;
	}
	return false;
}

CSize CHelpDisplay::GetSize() {
	// Measure
	HelpersGUI::SelectDefaultGUIFont(m_dc);
	m_nSizeX = 0;
	m_nSizeY = 0;
	m_nIncY = 0;
	std::list<TextLine>::iterator iter;
	for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
		CSize size;
		m_dc.GetTextExtent(iter->Key, (int)_tcslen(iter->Key), &size);
		m_nSizeX = max(m_nSizeX, size.cx);
		if (m_nIncY == 0) m_nIncY = size.cy;
		m_nSizeY += m_nIncY;
		if (iter->Info != NULL) {
			m_dc.GetTextExtent(iter->Info, (int)_tcslen(iter->Info), &size);
			m_nSizeX = max(m_nSizeX, size.cx + m_nTab1);
		}
		if (iter->Help != NULL) {
			m_dc.GetTextExtent(iter->Help, (int)_tcslen(iter->Help), &size);
			m_nSizeX = max(m_nSizeX, size.cx + m_nTab2);
		}
	}
	m_nSizeX += 10;
	m_nSizeY += 7;
	return CSize(m_nSizeX, m_nSizeY);
}

void CHelpDisplay::Show() {
	// Measure
	if (m_nSizeX == 0) {
		GetSize();
	}

	// Paint
	HelpersGUI::SelectDefaultGUIFont(m_dc);
	CBrush brushBk1; brushBk1.CreateSolidBrush(::GetSysColor(COLOR_3DFACE) - 0x000A0A0A);
	CBrush brushBk2; brushBk2.CreateSolidBrush(::GetSysColor(COLOR_3DFACE));

	CRect fullRect(CPoint(0, 0), CSize(m_nSizeX, m_nSizeY));
	m_dc.SetBkMode(TRANSPARENT);
	int nStartX = fullRect.left + 5;
	int nStartY = fullRect.top;
	int nI = 0;
	std::list<TextLine>::iterator iter;
	for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
		m_dc.FillRect(CRect(fullRect.left, nStartY, fullRect.right, nStartY + m_nIncY),
			((nI++ & 1) == 0) ? brushBk1 : brushBk2);

		int nKeyLen = (int)_tcslen(iter->Key);
		m_dc.SetTextColor(iter->Color);
		m_dc.TextOut(nStartX, nStartY, iter->Key, nKeyLen);
		CSize textLenKey;
		m_dc.GetTextExtent(iter->Key, nKeyLen, &textLenKey);
		if (iter->Info != NULL) {
			m_dc.SetTextColor(iter->ColorInfo);
			m_dc.TextOut(max(nStartX + m_nTab1, nStartX + textLenKey.cx + 2), nStartY, iter->Info, (int)_tcslen(iter->Info));
		}
		if (iter->Help != NULL) {
			m_dc.SetTextColor(iter->Color);
			m_dc.TextOut(nStartX + m_nTab2, nStartY, iter->Help, (int)_tcslen(iter->Help));
		}
		nStartY += m_nIncY;
	}
	m_dc.FillRect(CRect(fullRect.left, nStartY, fullRect.right, nStartY + m_nIncY), brushBk1);
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