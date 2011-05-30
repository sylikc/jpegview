#include "StdAfx.h"
#include "EXIFDisplay.h"
#include "Helpers.h"
#include "HelpersGUI.h"
#include "HistogramCorr.h"
#include "NLS.h"
#include <math.h>

#define BUTTON_SIZE 18
#define HISTOGRAM_HEIGHT 50
#define MAX_WIDTH 320
#define PREFIX_GAP 10

static LPTSTR CopyStrAlloc(LPCTSTR str) {
	if (str == NULL) {
		return NULL;
	}
	int nLen = _tcslen(str);
	TCHAR* pNewStr = new TCHAR[nLen + 1];
	_tcscpy_s(pNewStr, nLen + 1, str);
	return pNewStr;
}

static CRect InflateRect(const CRect& rect, float fAmount) {
	CRect r(rect);
	int nAmount = (int)(fAmount * r.Width());
	r.InflateRect(-nAmount, -nAmount);
	return r;
}

CEXIFDisplay::CEXIFDisplay(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture) : CPanel(hWnd, pNotifyMouseCapture, true, true) {
	m_bShowHistogram = false;
	m_nGap = (int)(m_fDPIScale * 10);
	m_nTab1 = 0;
	m_nLineHeight = 0;
	m_nTitleHeight = 0;
	m_pos = CPoint(0, 0);
	m_size = CSize(0, 0);
	m_sPrefix = NULL;
	m_sTitle = NULL;
	m_sComment = NULL;
	m_nCommentHeight = 0;
	m_hTitleFont = 0;
	m_nNoHistogramSize = CSize(0, 0);
	m_pHistogram = NULL;

	AddUserPaintButton(ID_btnShowHideHistogram, &ShowHistogramTooltip, &PaintShowHistogramBtn, NULL, this, this);
}

CEXIFDisplay::~CEXIFDisplay() {
	ClearTexts();
	if (m_hTitleFont != 0) {
		::DeleteObject(m_hTitleFont);
	}
}

void CEXIFDisplay::ClearTexts() {
	delete[] m_sPrefix;
	m_sPrefix = NULL;
	delete[] m_sTitle;
	m_sTitle = NULL;
	delete[] m_sComment;
	m_sComment = NULL;
	m_nCommentHeight = 0;
	m_nPrefixLenght = 0;
	m_nTitleWidth = 0;
	std::list<TextLine>::iterator iter;
	for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
		delete[] iter->Desc;
		delete[] iter->Value;
	}
	m_lines.clear();
	RequestRepositioning();
}

void CEXIFDisplay::AddTitle(LPCTSTR sTitle) {
	delete[] m_sTitle;
	m_sTitle = CopyStrAlloc(sTitle);
}

void CEXIFDisplay::AddPrefix(LPCTSTR sPrefix) {
	delete[] m_sPrefix;
	m_sPrefix = CopyStrAlloc(sPrefix);
}

void CEXIFDisplay::SetComment(LPCTSTR sComment) {
	delete[] m_sComment;
	CString s(sComment);
	s.TrimLeft();
	s.TrimRight();
	m_sComment = (s.GetLength() == 0) ? NULL : CopyStrAlloc(s);
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

CRect CEXIFDisplay::PanelRect() {
	if (m_nLineHeight == 0) {
		CDC dc(::GetDC(m_hWnd));
		::SelectObject(dc, ::GetStockObject(DEFAULT_GUI_FONT));
		m_hTitleFont = HelpersGUI::CreateBoldFontOfSelectedFont(dc);

		if (m_hTitleFont != 0) {
			::SelectObject(dc, m_hTitleFont);
		}

		m_nTitleHeight = 0;
		m_nPrefixLenght = 0;
		m_nTitleWidth = 0;
		int nTitleLength = 0;
		int nMaxLenght1 = 0, nMaxLength2 = 0;
		CSize size;
		if (m_sPrefix != NULL) {
			::GetTextExtentPoint32(dc, m_sPrefix, _tcslen(m_sPrefix), &size);
			nTitleLength = m_nPrefixLenght = size.cx;
			m_nTitleHeight = size.cy + 9;
		}
		if (m_sTitle != NULL) {
			::GetTextExtentPoint32(dc, m_sTitle, _tcslen(m_sTitle), &size);
			if (size.cx > (int)(MAX_WIDTH * HelpersGUI::ScreenScaling)) {
				CRect rectTitle(0, 0, (int)(MAX_WIDTH * HelpersGUI::ScreenScaling), 2);
				::DrawText(dc, m_sTitle, _tcslen(m_sTitle), &rectTitle, DT_CALCRECT | DT_NOPREFIX | DT_WORDBREAK | DT_WORD_ELLIPSIS);
				m_nTitleWidth = rectTitle.Width();
				nTitleLength += m_nTitleWidth;
				m_nTitleHeight = max(m_nTitleHeight, rectTitle.Height() + 9);
			} else {
				m_nTitleWidth = size.cx;
				nTitleLength += m_nTitleWidth;
				m_nTitleHeight = max(m_nTitleHeight, size.cy + 9);
			}
			int nGap = (int)(PREFIX_GAP * HelpersGUI::ScreenScaling);
			m_nPrefixLenght += nGap;
			nTitleLength += nGap;
		}

		::SelectObject(dc, ::GetStockObject(DEFAULT_GUI_FONT));

		int nLen1 = 0, nLen2 = 0;
		std::list<TextLine>::iterator iter;
		for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
			if (iter->Desc != NULL) {
				::GetTextExtentPoint32(dc, iter->Desc, _tcslen(iter->Desc), &size);
				m_nLineHeight = max(m_nLineHeight, size.cy);
				nMaxLenght1 = max(nMaxLenght1, size.cx);
			}
			nLen2 = nLen1;
			nLen1 = 0;
			if (iter->Value != NULL) {
				::GetTextExtentPoint32(dc, iter->Value, _tcslen(iter->Value), &size);
				m_nLineHeight = max(m_nLineHeight, size.cy);
				nMaxLength2 = max(nMaxLength2, size.cx);
				nLen1 = size.cx;
			}
		}

		int nButtonWidth = (int)(m_fDPIScale * BUTTON_SIZE);
		bool bNeedsExpansionForButton = (nMaxLength2 - max(nLen1, nLen2)) < nButtonWidth + m_nGap;
		int nNeededWidthNoBorders = max(nTitleLength, nMaxLenght1 + nMaxLength2 + m_nGap) + (bNeedsExpansionForButton ? m_nGap + nButtonWidth : 0);
		int nExpansionX = 0, nExpansionY = 0;
		if (m_bShowHistogram) {
			nExpansionX = max(0, 256 - nNeededWidthNoBorders);
			nExpansionY = HISTOGRAM_HEIGHT + m_nGap;
		}

		m_size = CSize(nNeededWidthNoBorders + m_nGap*2 + nExpansionX, 
			m_nTitleHeight + m_lines.size()*m_nLineHeight + m_nGap*2 + nExpansionY);

		if (m_sComment != NULL) {
			CRect rectComment(0, 0, m_size.cx - m_nGap*2, 200);
			::DrawText(dc, m_sComment, _tcslen(m_sComment), &rectComment, DT_CALCRECT | DT_NOPREFIX | DT_WORDBREAK | DT_WORD_ELLIPSIS);
			m_nCommentHeight = min(3 * m_nLineHeight, rectComment.Height());
			m_size.cy += (m_nGap >> 1) + m_nCommentHeight;
		}

		m_nNoHistogramSize = CSize(m_size.cx - nExpansionX, m_size.cy - nExpansionY);
		m_nTab1 = nMaxLenght1 + m_nGap;
	}
	return CRect(m_pos, m_size);
}


void CEXIFDisplay::RequestRepositioning() {
	m_nLineHeight = 0;
}

void CEXIFDisplay::OnPaint(CDC & dc, const CPoint& offset) {
	CPanel::OnPaint(dc, offset);

	int nX = m_pos.x + offset.x;
	int nY = m_pos.y + offset.y;

	if (m_hTitleFont != 0) {
		::SelectObject(dc, m_hTitleFont);
	}
	::SetBkMode(dc, TRANSPARENT);
	::SetTextColor(dc, RGB(255, 255, 255));
	if (m_sPrefix != NULL) {
		::TextOut(dc, nX + m_nGap, nY + m_nGap, m_sPrefix, _tcslen(m_sPrefix));
	}
	if (m_sTitle != NULL) {
		int nXStart = nX + m_nGap + m_nPrefixLenght;
		CRect rectTitle(nXStart, nY + m_nGap, nXStart + m_nTitleWidth, nY + m_nGap + m_nTitleHeight);
		::DrawText(dc, m_sTitle, _tcslen(m_sTitle), &rectTitle, DT_NOPREFIX | DT_WORDBREAK | DT_WORD_ELLIPSIS);
	}

	::SetTextColor(dc, RGB(243, 242, 231));
	::SelectObject(dc, ::GetStockObject(DEFAULT_GUI_FONT));

	int nRunningY = nY + m_nTitleHeight + m_nGap;

	if (m_sComment != NULL) {
		CRect rectComment(nX + m_nGap, nRunningY, nX + m_size.cx - m_nGap, nRunningY + m_nCommentHeight);
		::DrawText(dc, m_sComment, _tcslen(m_sComment), &rectComment, DT_NOPREFIX | DT_WORDBREAK | DT_WORD_ELLIPSIS);
		nRunningY += m_nCommentHeight;
		nRunningY += m_nGap >> 1;
	}

	std::list<TextLine>::iterator iter;
	for (iter = m_lines.begin( ); iter != m_lines.end( ); iter++ ) {
		if (iter->Desc != NULL) {
			::TextOut(dc, nX + m_nGap, nRunningY, iter->Desc, _tcslen(iter->Desc));
		}
		if (iter->Value != NULL) {
			::TextOut(dc, nX + m_nGap + m_nTab1, nRunningY, iter->Value, _tcslen(iter->Value));
		}
		nRunningY += m_nLineHeight;
	}

	if (m_bShowHistogram) {
		::SelectObject(dc, ::GetStockObject(WHITE_PEN));
		int nHistogramYBase = nY + m_size.cy - m_nGap;
		int nHistogramXBase = nX + (m_size.cx - 256) / 2;
		dc.MoveTo(nHistogramXBase, nHistogramYBase);
		dc.LineTo(nHistogramXBase + 256, nHistogramYBase);
		if (m_pHistogram != NULL) {
			PaintHistogram(dc, nHistogramXBase, nHistogramYBase);
		}
	}
}

void CEXIFDisplay::PaintHistogram(CDC & dc, int nXStart, int nYBaseLine) {
	const int* pChannelGrey = m_pHistogram->GetChannelGrey();
	int nMaxValue = 0;
	for (int i = 0; i < 256; i++) {
		nMaxValue = max(pChannelGrey[i], nMaxValue);
	}
	double dScaling = (nMaxValue == 0) ? 0.0f : HISTOGRAM_HEIGHT / sqrt((double)nMaxValue);

	HPEN hPen = ::CreatePen(PS_SOLID, 1, RGB(190, 190, 170));
	HGDIOBJ hOldPen = ::SelectObject(dc, hPen);
	for (int i = 0; i < 256; i++) {
		int nLineHeight = (int)(sqrt((double)pChannelGrey[i]) * dScaling + 0.5);
		dc.MoveTo(nXStart + i, nYBaseLine - nLineHeight);
		dc.LineTo(nXStart + i, nYBaseLine);
	}
	::SelectObject(dc, hOldPen);
	::DeleteObject(hPen);
}

void CEXIFDisplay::RepositionAll() {
	CRect panelRect = PanelRect();

	CUICtrl* pButton = GetControl(ID_btnShowHideHistogram);
	if (pButton != NULL) {
		int nButtonSize = (int)(m_fDPIScale * BUTTON_SIZE);
		pButton->SetPosition(CRect(CPoint(panelRect.left + m_nNoHistogramSize.cx - m_nGap - nButtonSize, panelRect.top + m_nNoHistogramSize.cy - m_nGap - nButtonSize), CSize(nButtonSize, nButtonSize)));
	}
}

void CEXIFDisplay::PaintShowHistogramBtn(void* pContext, const CRect& rect, CDC& dc) {
	CEXIFDisplay* pThis = (CEXIFDisplay*)pContext;
	CRect r = InflateRect(rect, 0.3f);
	r.OffsetRect(CPoint(0, 1));
	CPoint p1(r.left + 1, r.top + 1);
	CPoint p2(r.right, r.top);
	int nMiddleX = (r.left + r.right) / 2;
	CPoint p3(nMiddleX, r.top + (r.right - r.left) / 2);
	if (pThis->GetShowHistogram()) {
		dc.MoveTo(CPoint(p1.x, p3.y));
		dc.LineTo(CPoint(p3.x, p1.y));
		dc.MoveTo(CPoint(p3.x, p1.y));
		dc.LineTo(CPoint(p2.x, p3.y + 1));
	} else {
		dc.MoveTo(p1);
		dc.LineTo(p3);
		dc.MoveTo(p3);
		dc.LineTo(p2);
	}
}

LPCTSTR CEXIFDisplay::ShowHistogramTooltip(void* pContext) {
	CEXIFDisplay* pThis = (CEXIFDisplay*)pContext;
	if (pThis->GetShowHistogram()) {
		return CNLS::GetString(_T("Hide histogram"));
	} else {
		return CNLS::GetString(_T("Show histogram"));
	}
}


