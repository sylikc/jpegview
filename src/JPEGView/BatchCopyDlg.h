// Batch copy dialog
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"

class CFileList;
class CFileDesc;

// Dialog to copy/rename files in folder
class CBatchCopyDlg : public CDialogImpl<CBatchCopyDlg>
{
public:

	enum { IDD = IDD_BATCHCOPY };

	BEGIN_MSG_MAP(CBatchCopyDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		COMMAND_ID_HANDLER(IDC_CANCEL, OnCancelDialog)
		COMMAND_ID_HANDLER(IDC_SELECTALL, OnSelectAll)
		COMMAND_ID_HANDLER(IDC_SELECTNONE, OnSelectNone)
		COMMAND_ID_HANDLER(IDC_PREVIEW, OnPreview)
		COMMAND_ID_HANDLER(IDC_SAVE_PATTERN, OnSavePattern)
		COMMAND_ID_HANDLER(IDC_RENAME, OnRename)
		NOTIFY_HANDLER(IDC_LIST_FILES, LVN_ITEMCHANGED, OnListViewItemChanged)
		NOTIFY_HANDLER(IDC_LIST_FILES, LVN_KEYDOWN, OnListViewKeyDown)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCancelDialog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSelectAll(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSelectNone(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPreview(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSavePattern(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRename(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnListViewItemChanged(WPARAM wParam, LPNMHDR lpnmhdr, BOOL& bHandled);
	LRESULT OnListViewKeyDown(WPARAM wParam, LPNMHDR lpnmhdr, BOOL& bHandled);

	CBatchCopyDlg(CFileList& fileList);
	~CBatchCopyDlg();

private:
	CFileList& m_fileList;

	CStatic m_lblFiles;
	CStatic m_lblTitlePlaceHolders;
	CStatic m_lblPlaceHolders1;
	CStatic m_lblPlaceHolders2;
	CStatic m_lblRemark;
	CStatic m_lblResult;
	CStatic m_lblCopy;
	CButton m_btnRename;
	CButton m_btnPreview;
	CButton m_btnSelectAll;
	CButton m_btnSelectNone;
	CButton m_btnSavePattern;
	CButton m_btnCancel;
	CEdit m_edtPattern;
	CListViewCtrl m_lvFiles;

	int m_nNumFiles;
	CString m_strMyPictures;
	bool m_bInOnListViewItemChanged;

	int CreateItemList();
	CString GetPatternText(); // Get text of IDC_SAVE_PATTERN edit field
	CString ReplacePlaceholders(LPCTSTR strPattern, int nIndex, const CFileDesc & fileDesc, bool bReplaceMyPictures);
	bool IsCopyNeeded(LPCTSTR strName, LPCTSTR strDirectory);
};
