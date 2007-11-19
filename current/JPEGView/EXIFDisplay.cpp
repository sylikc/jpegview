#include "StdAfx.h"
#include "EXIFDisplay.h"
#include "Helpers.h"

static LPTSTR CopyStrAlloc(LPCTSTR str) {
	if (str == NULL) {
		return NULL;
	}
	int nLen = _tcslen(str);
	TCHAR* pNewStr = new TCHAR[nLen + 1];
	_tcscpy(pNewStr, str);
	return pNewStr;
}

CEXIFDisplay::CEXIFDisplay(CPaintDC & dc) : m_dc(dc) {
	m_fScaling = Helpers::ScreenScaling;
	m_nGap = (int)(m_fScaling * 10);
	m_nTab1 = 0;
	m_nLineHeight = 0;
	m_nTitleHeight = 0;
	m_size = CSize(0, 0);
	m_sTitle = NULL;
	m_hTitleFont = 0;
}

CEXIFDisplay::~CEXIFDisplay() {
	delete[] m_sTitle;
	std::list<TextLine>::iterator iter;
	for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
		delete[] iter->Desc;
		delete[] iter->Value;
	}
	if (m_hTitleFont != 0) {
		::DeleteObject(m_hTitleFont);
	}
}

void CEXIFDisplay::AddTitle(LPCTSTR sTitle) {
	delete[] m_sTitle;
	m_sTitle = CopyStrAlloc(sTitle);
}

void CEXIFDisplay::AddLine(LPCTSTR sDescription, LPCTSTR sValue) {
	m_lines.push_back(TextLine(CopyStrAlloc(sDescription), CopyStrAlloc(sValue)));
}

void CEXIFDisplay::AddLine(LPCTSTR sDescription, double dValue, int nDigits) {
	TCHAR buffFormat[8];
	_stprintf_s(buffFormat, 8, _T("%%.%df"), nDigits);
	TCHAR buff[32];
	_stprintf_s(buff, 32, buffFormat, dValue);
	AddLine(sDescription, buff);
}

void CEXIFDisplay::AddLine(LPCTSTR sDescription, int nValue){
	TCHAR buff[32];
	_stprintf_s(buff, 32, _T("%d"), nValue);
	AddLine(sDescription, buff);
}

void CEXIFDisplay::AddLine(LPCTSTR sDescription, const SYSTEMTIME &time) {
	CString sTime = Helpers::SystemTimeToString(time);
	AddLine(sDescription, sTime);
}

void CEXIFDisplay::AddLine(LPCTSTR sDescription, const FILETIME &time) {
	SYSTEMTIME systemTime;
	::FileTimeToSystemTime(&time, &systemTime);
	AddLine(sDescription, systemTime);
}

void CEXIFDisplay::AddLine(LPCTSTR sDescription, const Rational &number) {
	if (number.Denumerator == 1) {
		AddLine(sDescription, number.Numerator);
	} else {
		TCHAR buff[32];
		_stprintf_s(buff, 32, _T("%d/%d"), number.Numerator, number.Denumerator);
		AddLine(sDescription, buff);
	}
}

CSize CEXIFDisplay::GetSize() {
	if (m_nLineHeight == 0) {
		TCHAR buff[32];
		::SelectObject(m_dc, ::GetStockObject(DEFAULT_GUI_FONT));
		if (::GetTextFace(m_dc, 64, buff) != 0) {
			TEXTMETRIC textMetrics;
			::GetTextMetrics(m_dc, &textMetrics);
			LOGFONT logFont;
			memset(&logFont, 0, sizeof(LOGFONT));
			logFont.lfHeight = textMetrics.tmHeight;
			logFont.lfWeight = FW_BOLD;
			_tcsncpy(logFont.lfFaceName, buff, 32);  
			m_hTitleFont = ::CreateFontIndirect(&logFont);
		}

		if (m_hTitleFont != 0) {
			::SelectObject(m_dc, m_hTitleFont);
		}

		int nTitleLength = 0;
		int nMaxLenght1 = 0, nMaxLength2 = 0;
		CSize size;
		if (m_sTitle != NULL) {
			::GetTextExtentPoint32(m_dc, m_sTitle, _tcslen(m_sTitle), &size);
			nTitleLength = size.cx;
			m_nTitleHeight = size.cy + 2;
		}

		::SelectObject(m_dc, ::GetStockObject(DEFAULT_GUI_FONT));

		std::list<TextLine>::iterator iter;
		for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
			if (iter->Desc != NULL) {
				::GetTextExtentPoint32(m_dc, iter->Desc, _tcslen(iter->Desc), &size);
				m_nLineHeight = max(m_nLineHeight, size.cy);
				nMaxLenght1 = max(nMaxLenght1, size.cx);
			}
			if (iter->Value != NULL) {
				::GetTextExtentPoint32(m_dc, iter->Value, _tcslen(iter->Value), &size);
				m_nLineHeight = max(m_nLineHeight, size.cy);
				nMaxLength2 = max(nMaxLength2, size.cx);
			}
		}

		m_size = CSize(max(nTitleLength, nMaxLenght1 + nMaxLength2 + m_nGap) + m_nGap*2, 
			m_nTitleHeight + m_lines.size()*m_nLineHeight + m_nGap*2);
		m_nTab1 = nMaxLenght1 + m_nGap;
	}
	return m_size;
}

void CEXIFDisplay::Show(int nX, int nY) {
	if (m_nLineHeight == 0) {
		m_size = GetSize();
	}

	if (m_hTitleFont != 0) {
		::SelectObject(m_dc, m_hTitleFont);
	}
	::SetBkMode(m_dc, TRANSPARENT);
	::SetTextColor(m_dc, RGB(255, 255, 255));
	if (m_sTitle != NULL) {
		::TextOut(m_dc, nX + m_nGap, nY + m_nGap, m_sTitle, _tcslen(m_sTitle));
	}

	::SetTextColor(m_dc, RGB(243, 242, 231));
	::SelectObject(m_dc, ::GetStockObject(DEFAULT_GUI_FONT));

	int nRunningY = nY + m_nTitleHeight + m_nGap;
	std::list<TextLine>::iterator iter;
	for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
		if (iter->Desc != NULL) {
			::TextOut(m_dc, nX + m_nGap, nRunningY, iter->Desc, _tcslen(iter->Desc));
		}
		if (iter->Value != NULL) {
			::TextOut(m_dc, nX + m_nGap + m_nTab1, nRunningY, iter->Value, _tcslen(iter->Value));
		}
		nRunningY += m_nLineHeight;
	}

	HPEN hPen = ::CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
	HGDIOBJ hOldPen = ::SelectObject(m_dc, hPen);
	::SelectObject(m_dc, ::GetStockObject(HOLLOW_BRUSH));
	::Rectangle(m_dc, nX, nY, nX + m_size.cx, nY + m_size.cy);
	::SelectObject(m_dc, hOldPen);
	::DeleteObject(hPen);
}
