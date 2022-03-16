
#include "StdAfx.h"
#include "CropSizeDlg.h"
#include "NLS.h"
#include "SettingsProvider.h"
#include "Helpers.h"

int CCropSizeDlg::sm_nWidth = 0;
int CCropSizeDlg::sm_nHeight = 0;
bool CCropSizeDlg::sm_bScreen = true;

CCropSizeDlg::CCropSizeDlg(void) {
	if (sm_nWidth == 0 && sm_nHeight == 0) {
		CSize sizeDefault = CSettingsProvider::This().DefaultFixedCropSize();
		sm_nWidth = sizeDefault.cx;
		sm_nHeight = sizeDefault.cy;
	}
}

CCropSizeDlg::~CCropSizeDlg(void) {
}

LRESULT CCropSizeDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CPoint mousePos;
	CRect wndRect;
	GetWindowRect(&wndRect);
	::GetCursorPos(&mousePos);
	this->SetWindowPos(m_hWnd, mousePos.x - wndRect.Width()/2, mousePos.y - wndRect.Height()/2, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

	this->SetWindowText(CNLS::GetString(_T("Set Fixed Crop Size")));

	m_btnClose.Attach(GetDlgItem(IDC_BTN_CLOSE));
	m_lblWidth.Attach(GetDlgItem(IDC_LBL_X));
	m_lblHeight.Attach(GetDlgItem(IDC_LBL_Y));
	m_lblPixel.Attach(GetDlgItem(IDC_LBL_PIXEL));
	m_lblPixel2.Attach(GetDlgItem(IDC_LBL_PIXEL2));
	m_rbScreen.Attach(GetDlgItem(IDC_RB_SCREEN));
	m_rbImage.Attach(GetDlgItem(IDC_RB_IMAGE));
	m_edtWidth.Attach(GetDlgItem(IDC_EDT_X));
	m_edtHeight.Attach(GetDlgItem(IDC_EDT_Y));

	m_btnClose.SetWindowText(CNLS::GetString(_T("Close")));
	m_lblWidth.SetWindowText(CNLS::GetString(_T("Width")));
	m_lblHeight.SetWindowText(CNLS::GetString(_T("Height")));
	m_lblPixel.SetWindowText(CNLS::GetString(_T("Pixels")));
	m_lblPixel2.SetWindowText(CNLS::GetString(_T("Pixels")));
	m_rbScreen.SetWindowText(CNLS::GetString(_T("Screen Pixels")));
	m_rbImage.SetWindowText(CNLS::GetString(_T("Image Pixels")));

	if (sm_bScreen) {
		m_rbScreen.SetCheck(TRUE);
	} else {
		m_rbImage.SetCheck(TRUE);
	}

	CString strWidth;
	strWidth.Format(_T("%d"), sm_nWidth);
	m_edtWidth.SetWindowText(strWidth);

	CString strHeight;
	strHeight.Format(_T("%d"), sm_nHeight);
	m_edtHeight.SetWindowText(strHeight);

	return TRUE;
}

LRESULT CCropSizeDlg::OnCloseDialog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	Close();
	return 0;
}

LRESULT CCropSizeDlg::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	Close();
	return 0;
}

void CCropSizeDlg::Close() {
	const int MAX_LEN = 16;
	TCHAR sWidth[MAX_LEN];
	TCHAR sHeight[MAX_LEN];
	m_edtWidth.GetWindowText(sWidth, MAX_LEN);
	m_edtHeight.GetWindowText(sHeight, MAX_LEN);

	_stscanf_s(sWidth, _T("%d"), &sm_nWidth);
	_stscanf_s(sHeight, _T("%d"), &sm_nHeight);

	sm_nWidth = max(0, sm_nWidth);
	sm_nHeight = max(0, sm_nHeight);

	sm_bScreen = m_rbScreen.GetCheck() != 0;

	EndDialog(IDCANCEL);
}
