#include "StdAfx.h"
#include "HelpDisplay.h"
#include "Helpers.h"
#include "NLS.h"

static const COLORREF colorTitle = RGB(0, 255, 255);
static const COLORREF colorLines = RGB(0, 255, 0);
static const COLORREF colorInfo = RGB(160, 255, 160);
static const COLORREF colorInfoLight = RGB(255, 255, 255);

CHelpDisplay::CHelpDisplay(CPaintDC & dc) : m_dc(dc) {
	m_fScaling = Helpers::ScreenScaling;
	CSize size;
	dc.SelectStockFont(SYSTEM_FONT);
	dc.GetTextExtent(_T("teststring"), 10, &size);
	m_nTab1 =  (int) (size.cx * 1.8f);
	m_nTab2 = (int) (size.cx * 2.6f);
}

void CHelpDisplay::AddTitle(LPCTSTR sTitle) {
	m_lines.push_back(TextLine(colorTitle, colorInfo, sTitle, NULL, NULL));
}

void CHelpDisplay::AddLine(LPCTSTR sLine) {
	m_lines.push_back(TextLine(colorLines, colorInfo, sLine, NULL, NULL));
}

void CHelpDisplay::AddLine(LPCTSTR sKey, LPCTSTR sHelp) {
	m_lines.push_back(TextLine(colorLines, colorInfo, sKey, NULL, sHelp));
}

void CHelpDisplay::AddLineInfo(LPCTSTR sKey, LPCTSTR sInfo, LPCTSTR sHelp) {
	m_lines.push_back(TextLine(colorLines, colorInfo, sKey, sInfo, sHelp));
}

void CHelpDisplay::AddLineInfo(LPCTSTR sKey, bool bInfo, LPCTSTR sHelp) {
	m_lines.push_back(TextLine(colorLines, bInfo ? colorInfoLight : colorInfo, sKey, bInfo ? CNLS::GetString(_T("on")) : CNLS::GetString(_T("off")), sHelp));
}

void CHelpDisplay::Show(const CRect & screenRect) {
	// Measure
	int nMaxSizeX = 0;
	int nSizeY = 0;
	int nIncY = 0;
	std::list<TextLine>::iterator iter;
	for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
		CSize size;
		m_dc.GetTextExtent(iter->Key, _tcslen(iter->Key), &size);
		nMaxSizeX = max(nMaxSizeX, size.cx);
		if (nIncY == 0) nIncY = size.cy;
		nSizeY += nIncY;
		if (iter->Info != NULL) {
			m_dc.GetTextExtent(iter->Info, _tcslen(iter->Info), &size);
			nMaxSizeX = max(nMaxSizeX, size.cx + m_nTab1);
		}
		if (iter->Help != NULL) {
			m_dc.GetTextExtent(iter->Help, _tcslen(iter->Help), &size);
			nMaxSizeX = max(nMaxSizeX, size.cx + m_nTab2);
		}
	}
	// Paint
	CRect fullRect(CPoint(screenRect.Width()/2 - nMaxSizeX/2 - 5, screenRect.Height()/2 - nSizeY/2 - 5),
		CSize(nMaxSizeX + 10, nSizeY + 10));
	m_dc.FillRect(&fullRect, (HBRUSH)::GetStockObject(BLACK_BRUSH));
	m_dc.FrameRect(&fullRect, (HBRUSH)::GetStockObject(GRAY_BRUSH));
	m_dc.SetBkMode(TRANSPARENT);
	int nStartX = fullRect.left + 5;
	int nStartY = fullRect.top + 5;
	for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
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
		nStartY += nIncY;
	}
}