// Change size of image dialog
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"
#include "ImageProcessingTypes.h"

// Allows to change image size, i.e. resample the image.
class CResizeDlg : public CDialogImpl<CResizeDlg>
{
public:
	enum { IDD = IDD_RESIZE };

	BEGIN_MSG_MAP(CResizeDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnResizeAndClose)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_HANDLER(IDC_RS_ED_PERCENTS, EN_CHANGE, OnPercentChanged)
		COMMAND_HANDLER(IDC_RS_ED_WIDTH, EN_CHANGE, OnWidthChanged)
		COMMAND_HANDLER(IDC_RS_ED_HEIGHT, EN_CHANGE, OnHeightChanged)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnResizeAndClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPercentChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWidthChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHeightChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CResizeDlg(CSize originalSize);
	~CResizeDlg(void);

	// only valid when dialog confirmed with OK
	CSize GetNewSize() { return m_newSize; }

	EResizeFilter GetFilter() { return m_eFilter; }

private:
	CButton m_btnOk;
	CButton m_btnCancel;
	CStatic m_lblPercents;
	CStatic m_lblWidth;
	CStatic m_lblHeight;
	CStatic m_lblFilter;
	CStatic m_lblPixel;
	CStatic m_lblPixel2;
	CEdit m_edtPercents;
	CEdit m_edtWidth;
	CEdit m_edtHeight;
	CComboBox m_cbFilter;

	CSize m_originalSize;
	CSize m_newSize;
	EResizeFilter m_eFilter;
	bool m_blockUpdate;
	double m_dLastPercent;

	static double sm_dPercents;
	static int sm_nSelectedFilter;

	void Close();
	int ConvertToNumber(CEdit& editControl);
	void SetNumber(CEdit& editControl, int number);
	bool CheckValidSize(CSize size);
};
