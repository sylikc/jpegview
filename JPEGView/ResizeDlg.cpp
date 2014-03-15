#include "StdAfx.h"
#include "ResizeDlg.h"
#include "NLS.h"
#include "MaxImageDef.h"
#include "Helpers.h"

double CResizeDlg::sm_dPercents = 100.0;
int CResizeDlg::sm_nSelectedFilter = 2;

CResizeDlg::CResizeDlg(CSize originalSize) {
	m_blockUpdate = false;
	m_originalSize = originalSize;
	m_newSize = CSize(0, 0);
	m_eFilter = Resize_NoAliasing;
}

CResizeDlg::~CResizeDlg(void) {
}

LRESULT CResizeDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	this->SetWindowText(CNLS::GetString(_T("Resize Image")));

	m_btnOk.Attach(GetDlgItem(IDOK));
	m_btnCancel.Attach(GetDlgItem(IDCANCEL));

	m_lblPercents.Attach(GetDlgItem(IDC_RS_LABEL_NEW_PERCENTS));
	m_lblWidth.Attach(GetDlgItem(IDC_RS_LABEL_NEW__WIDTH));
	m_lblHeight.Attach(GetDlgItem(IDC_RS_LABEL_NEW_HEIGHT));
	m_lblFilter.Attach(GetDlgItem(IDC_RS_LABEL_FILTER));
	m_lblPixel.Attach(GetDlgItem(IDC_RS_PIXELS));
	m_lblPixel2.Attach(GetDlgItem(IDC_RS_PIXELS2));

	m_edtPercents.Attach(GetDlgItem(IDC_RS_ED_PERCENTS));
	m_edtWidth.Attach(GetDlgItem(IDC_RS_ED_WIDTH));
	m_edtHeight.Attach(GetDlgItem(IDC_RS_ED_HEIGHT));

	m_edtPercents.LimitText(4);
	m_edtWidth.LimitText(5);
	m_edtHeight.LimitText(5);

	m_cbFilter.Attach(GetDlgItem(IDC_RS_CB_FILTER));

	m_btnOk.SetWindowText(CNLS::GetString(_T("OK")));
	m_btnCancel.SetWindowText(CNLS::GetString(_T("Cancel")));
	m_lblPercents.SetWindowText(CNLS::GetString(_T("New size:")));
	m_lblWidth.SetWindowText(CNLS::GetString(_T("New width:")));
	m_lblHeight.SetWindowText(CNLS::GetString(_T("New height:")));
	m_lblFilter.SetWindowText(CNLS::GetString(_T("Filter:")));
	m_lblPixel.SetWindowText(CNLS::GetString(_T("Pixels")));
	m_lblPixel2.SetWindowText(CNLS::GetString(_T("Pixels")));

	m_cbFilter.AddString(CNLS::GetString(_T("Box")));
	m_cbFilter.AddString(CNLS::GetString(_T("Lanczos/Bicubic")));
	m_cbFilter.AddString(CNLS::GetString(_T("Sharpen low")));
	m_cbFilter.AddString(CNLS::GetString(_T("Sharpen medium")));

	m_cbFilter.SetCurSel(sm_nSelectedFilter);

	m_blockUpdate = true;
	SetNumber(m_edtPercents, Helpers::RoundToInt(sm_dPercents));
	SetNumber(m_edtWidth, Helpers::RoundToInt(sm_dPercents * 0.01 * m_originalSize.cx));
	SetNumber(m_edtHeight, Helpers::RoundToInt(sm_dPercents * 0.01 * m_originalSize.cy));
	m_blockUpdate = false;

	m_dLastPercent = sm_dPercents;

	return TRUE;
}

LRESULT CResizeDlg::OnResizeAndClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	sm_dPercents = m_dLastPercent;

	sm_nSelectedFilter = m_cbFilter.GetCurSel();
	switch (sm_nSelectedFilter) {
	case 0: 
		m_eFilter = Resize_PointFilter;
		break;
	case 1:
		m_eFilter = Resize_NoAliasing;
		break;
	case 2:
		m_eFilter = Resize_SharpenLow;
		break;
	case 3:
		m_eFilter = Resize_SharpenMedian;
		break;
	default:
		m_eFilter = Resize_NoAliasing;
		break;
	}

	CSize newSize = CSize(ConvertToNumber(m_edtWidth), ConvertToNumber(m_edtHeight));
	if (!CheckValidSize(newSize)) {
		::MessageBox(m_hWnd, CNLS::GetString(_T("The new size is too large - width or height is larger than 65535 or total number of pixels is too large!")),
			CNLS::GetString(_T("Error")), MB_OK | MB_ICONERROR);
		return 0;
	}

	m_newSize = newSize;

	EndDialog(IDOK);
	return 0;
}

LRESULT CResizeDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CResizeDlg::OnPercentChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_blockUpdate) return 0;
	m_blockUpdate = true;
	m_dLastPercent = ConvertToNumber(m_edtPercents);
	SetNumber(m_edtWidth, Helpers::RoundToInt(m_dLastPercent * 0.01 * m_originalSize.cx));
	SetNumber(m_edtHeight, Helpers::RoundToInt(m_dLastPercent * 0.01 * m_originalSize.cy));
	m_blockUpdate = false;
	return 0;
}

LRESULT CResizeDlg::OnWidthChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_blockUpdate) return 0;
	m_blockUpdate = true;
	int nWidth = ConvertToNumber(m_edtWidth);
	m_dLastPercent = 100.0 * nWidth / m_originalSize.cx;
	SetNumber(m_edtPercents, Helpers::RoundToInt(m_dLastPercent));
	SetNumber(m_edtHeight, Helpers::RoundToInt(m_dLastPercent * 0.01 * m_originalSize.cy));
	m_blockUpdate = false;
	return 0;
}

LRESULT CResizeDlg::OnHeightChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_blockUpdate) return 0;
	m_blockUpdate = true;
	int nHeight = ConvertToNumber(m_edtHeight);
	m_dLastPercent = 100.0 * nHeight / m_originalSize.cy;
	SetNumber(m_edtPercents, Helpers::RoundToInt(m_dLastPercent));
	SetNumber(m_edtWidth, Helpers::RoundToInt(m_dLastPercent * 0.01 * m_originalSize.cx));
	m_blockUpdate = false;
	return 0;
}

int CResizeDlg::ConvertToNumber(CEdit& editControl) {
	const int MAX_LEN = 16;
	TCHAR sPercents[MAX_LEN];
	editControl.GetWindowText(sPercents, MAX_LEN);
	int value = -1;
	_stscanf_s(sPercents, _T("%d"), &value);
	return value;
}

void CResizeDlg::SetNumber(CEdit& editControl, int number) {
	CString strNumber;
	if (number > 0) {
		strNumber.Format(_T("%d"), number);
	}
	editControl.SetWindowText(strNumber);
}

bool CResizeDlg::CheckValidSize(CSize size) {
	if (size.cx <= 0 || size.cy <= 0) {
		return false;
	}
	if (((long long)size.cx) * size.cy > MAX_IMAGE_PIXELS) {
		return false;
	}
	if (size.cx > MAX_IMAGE_DIMENSION || size.cy > MAX_IMAGE_DIMENSION) {
		return false;
	}
	return true;
}