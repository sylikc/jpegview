// CropSizeDlg.h : interface of the CCropSizeDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"

// About dialog
class CCropSizeDlg : public CDialogImpl<CCropSizeDlg>
{
public:
	enum { IDD = IDD_SET_CROP_SIZE };

	BEGIN_MSG_MAP(CCropSizeDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		COMMAND_ID_HANDLER(IDC_BTN_CLOSE, OnCloseDialog)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseDialog)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseDialog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CCropSizeDlg(void);
	~CCropSizeDlg(void);

	static CSize GetCropSize() { return CSize(sm_nWidth, sm_nHeight); }
	static bool UseScreenPixels() { return sm_bScreen; }

private:
	CButton m_btnClose;
	CStatic m_lblWidth;
	CStatic m_lblHeight;
	CStatic m_lblPixel;
	CStatic m_lblPixel2;
	CButton m_rbScreen;
	CButton m_rbImage;
	CEdit m_edtWidth;
	CEdit m_edtHeight;

	static int sm_nWidth;
	static int sm_nHeight;
	static bool sm_bScreen;

	void Close();
};
