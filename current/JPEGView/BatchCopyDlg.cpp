#include "StdAfx.h"
#include "BatchCopyDlg.h"
#include "FileList.h"
#include "SettingsProvider.h"
#include "NLS.h"

///////////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////////

// Finds a format string with format %Nx where N is a number from 2 to 9
static int FindNumberFormat(LPCTSTR strText) {
	LPCTSTR s = strText;
	while (*s != 0 && s[1] != 0 && s[2] != 0) {
		if (*s == _T('%') && s[1] >= _T('2') && s[1] <= _T('9')) {
			return s - strText;
		}
		s++;
	}
	return -1;
}

// Converts a file time to local system time, outputs value in systemtime
static void FileTimeToLocalSystemTime(const FILETIME & fileTime, SYSTEMTIME & systemTime) {
	memset(&systemTime, 0, sizeof(SYSTEMTIME));
	::FileTimeToSystemTime(&fileTime, &systemTime);
	::SystemTimeToTzSpecificLocalTime(NULL, &systemTime, &systemTime);
}

// Creates an absolute path from a relative path, relative to given directory
static CString MakeAbsolutePath(LPCTSTR strFileName, LPCTSTR strDirectory) {
	// Check if path already absolute
	if (_tcsstr(strFileName, _T(":\\")) != NULL || _tcsstr(strFileName, _T("\\\\")) == strFileName) {
		return CString(strFileName);
	}
	CString strFullName = CString(strDirectory) + _T('\\') + strFileName;
	int nIndexFileTitle = strFullName.ReverseFind(_T('\\'));
	CString strRelDirectory(strFullName, nIndexFileTitle);
	LPTSTR notUsed;
	TCHAR buffFullPathName[MAX_PATH];
	::GetFullPathName(strRelDirectory, MAX_PATH, buffFullPathName, &notUsed);
	return CString(buffFullPathName) + _T('\\') + ((LPCTSTR)strFullName + nIndexFileTitle + 1);
}

// Finds the first number string in the given string, including leading zeros
static CString FindNumber(LPCTSTR strFileName) {
	int nStartIdx = -1;
	int nLen = _tcslen(strFileName);
	for (int i = 0; i < nLen; i++) {
		if (strFileName[i] >= _T('0') && strFileName[i] <= _T('9')) {
			if (nStartIdx < 0) nStartIdx = i;
		} else {
			if (nStartIdx >= 0) return CString(&(strFileName[nStartIdx]), i - nStartIdx);
		}
	}
	return CString("");
}

///////////////////////////////////////////////////////////////////////////////////
// Class implementation
///////////////////////////////////////////////////////////////////////////////////

CBatchCopyDlg::CBatchCopyDlg(CFileList& fileList) : m_fileList(fileList) {
	m_nNumFiles = 0;
	m_bInOnListViewItemChanged = false;

	TCHAR buff[MAX_PATH];
	::SHGetFolderPath(NULL, CSIDL_MYPICTURES, NULL, SHGFP_TYPE_CURRENT, buff);
	m_strMyPictures = buff;
}

CBatchCopyDlg::~CBatchCopyDlg() {
}

LRESULT CBatchCopyDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_lblFiles.Attach(GetDlgItem(IDC_FILES_IN_FOLDER));
	m_lblTitlePlaceHolders.Attach(GetDlgItem(IDC_LBL_PLACEHOLDERS));
	m_lblPlaceHolders1.Attach(GetDlgItem(IDC_PLACEHOLDERS1));
	m_lblPlaceHolders2.Attach(GetDlgItem(IDC_PLACEHOLDERS2));
	m_lblRemark.Attach(GetDlgItem(IDC_REMARK));
	m_lblCopy.Attach(GetDlgItem(IDC_LBL_COPY));
	m_lblResult.Attach(GetDlgItem(IDC_RESULT));
	m_btnRename.Attach(GetDlgItem(IDC_RENAME));
	m_btnPreview.Attach(GetDlgItem(IDC_PREVIEW));
	m_btnSelectAll.Attach(GetDlgItem(IDC_SELECTALL));
	m_btnSelectNone.Attach(GetDlgItem(IDC_SELECTNONE));
	m_btnSavePattern.Attach(GetDlgItem(IDC_SAVE_PATTERN));
	m_btnCancel.Attach(GetDlgItem(IDC_CANCEL));
	m_edtPattern.Attach(GetDlgItem(IDC_PATTERN));
	m_lvFiles.Attach(GetDlgItem(IDC_LIST_FILES));

	CenterWindow(GetParent());

	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

	this->SetWindowText(CNLS::GetString(_T("Batch rename/copy of files")));

	CString s;
	s.Format(_T("%s '%s'"), CNLS::GetString(_T("Image files in folder")), m_fileList.CurrentDirectory());
	m_lblFiles.SetWindowText(s);
	m_lblTitlePlaceHolders.SetWindowText(CNLS::GetString(_T("Placeholders:")));
	m_lblPlaceHolders1.SetWindowText(CString(_T("%x : ")) + CNLS::GetString(_T("Number (consecutively numbered)")) + _T("\n") +
		CString(_T("%Nx : ")) + CNLS::GetString(_T("Number with N digits")) + _T("\n") +
		CString(_T("%n : ")) + CNLS::GetString(_T("Extracted number from original file name")) + _T("\n") +
		CString(_T("%f : ")) + CNLS::GetString(_T("Original file name (with extension)")) + _T("\n") +
		CString(_T("%F : ")) + CNLS::GetString(_T("Original file name (without extension)")) + _T("\n") +
		CString(_T("%e : ")) + CNLS::GetString(_T("Original file extension")) + _T("\n") +
		CString(_T("%pictures% : ")) + CNLS::GetString(_T("'My Pictures' folder")));
		
	m_lblPlaceHolders2.SetWindowText(CString(_T("%d %m %y : ")) + CNLS::GetString(_T("Day / Month / Year (as numbers)")) + _T("\n") +
		CString(_T("%2y : ")) + CNLS::GetString(_T("Year short form (2 digits)")) + _T("\n") +
		CString(_T("%M %3M : ")) + CNLS::GetString(_T("Month / Short form (text)")) );
	m_lblRemark.SetWindowText(CString(CNLS::GetString(_T("Target folders that are not yet existing are created as needed."))) + _T("\n") +
		CNLS::GetString(_T("Date placeholders use the modification date of the image.")));
	m_lblCopy.SetWindowText(CNLS::GetString(_T("Rename/copy images to:")));

	m_btnSelectAll.SetWindowText(CNLS::GetString(_T("Select all")));
	m_btnSelectNone.SetWindowText(CNLS::GetString(_T("Select none")));
	m_btnRename.SetWindowText(CNLS::GetString(_T("Rename")));
	m_btnPreview.SetWindowText(CNLS::GetString(_T("Preview")));
	m_btnSavePattern.SetWindowText(CNLS::GetString(_T("Save as template")));
	m_btnCancel.SetWindowText(CNLS::GetString(_T("Close")));

	m_edtPattern.LimitText(MAX_PATH);
	m_edtPattern.SetWindowText(CSettingsProvider::This().CopyRenamePattern());
	DWORD nNewStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES;
	m_lvFiles.SetExtendedListViewStyle(nNewStyle, nNewStyle);
	m_lvFiles.InsertColumn(0, CNLS::GetString(_T("Old name")), LVCFMT_LEFT, (int)(Helpers::ScreenScaling*160));
	m_lvFiles.InsertColumn(1, CNLS::GetString(_T("Date")), LVCFMT_LEFT, (int)(Helpers::ScreenScaling*130));
	m_lvFiles.InsertColumn(2, CNLS::GetString(_T("New name (>> : file gets copied)")), LVCFMT_LEFT, (int)(Helpers::ScreenScaling*600));

	m_nNumFiles = CreateItemList();

	return TRUE;
}

LRESULT CBatchCopyDlg::OnCancelDialog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CBatchCopyDlg::OnSelectAll(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	for (int i = 0; i < m_nNumFiles; i++) {
		m_lvFiles.SetCheckState(i, TRUE);
	}
	return 0;
}

LRESULT CBatchCopyDlg::OnSelectNone(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	for (int i = 0; i < m_nNumFiles; i++) {
		m_lvFiles.SetCheckState(i, FALSE);
	}
	return 0;
}

LRESULT CBatchCopyDlg::OnSavePattern(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CString strPattern = GetPatternText();
	if (strPattern.IsEmpty()) return 0;
	CSettingsProvider::This().SaveCopyRenamePattern(strPattern);
	return 0;
}

LRESULT CBatchCopyDlg::OnPreview(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CString strPattern = GetPatternText();
	if (strPattern.IsEmpty()) return 0;

	std::list<CFileDesc>::const_iterator iter;
	const std::list<CFileDesc> & fileList = m_fileList.GetFileList();
	int nIndex = 0, nSelectedIndex = 0;
	for (iter = fileList.begin( ); iter != fileList.end( ); iter++ ) {
		if (m_lvFiles.GetCheckState(nIndex) && iter->GetTitle() != NULL) {
			CString strNewName = ReplacePlaceholders(strPattern, nSelectedIndex, *iter, false);
			bool bCopyNeeded = IsCopyNeeded(strNewName, m_fileList.CurrentDirectory());
			m_lvFiles.SetItemText(nIndex, 2, bCopyNeeded ? _T(">> ") + strNewName : strNewName);
			nSelectedIndex++;
		} else {
			m_lvFiles.SetItemText(nIndex, 2, _T(""));
		}
		nIndex++;
	}
	return 0;
}

LRESULT CBatchCopyDlg::OnRename(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_lblResult.SetWindowText(_T(""));
	CString strPattern = GetPatternText();
	if (strPattern.IsEmpty()) return 0;

	HCURSOR hOldCursor = ::SetCursor(::LoadCursor(NULL, IDC_WAIT));

	int nFilesCopied = 0;
	int nFilesRenamed = 0;
	int nDirsCreated = 0;
	std::list<CFileDesc>::iterator iter;
	std::list<CFileDesc> & fileList = m_fileList.GetFileList();
	int nIndex = 0, nSelectedIndex = 0;
	for (iter = fileList.begin( ); iter != fileList.end( ); iter++ ) {
		if (m_lvFiles.GetCheckState(nIndex) && iter->GetTitle() != NULL) {
			
			m_lblResult.SetWindowText(iter->GetTitle());
			m_lblResult.UpdateWindow();

			CString strNewName = ReplacePlaceholders(strPattern, nSelectedIndex++, *iter, true);
			bool bCopyNeeded = IsCopyNeeded(strNewName, m_fileList.CurrentDirectory()); // enables to force copy by giving .\ path
			strNewName = MakeAbsolutePath(strNewName, m_fileList.CurrentDirectory());
			bool bSuccess = true;
			DWORD lastError = 0;
			if (bCopyNeeded) {
				CString strNewPath(strNewName, _tcsrchr(strNewName, _T('\\')) - strNewName);
				// First create directory if it does not yet exist
				if (::GetFileAttributes(strNewPath) == INVALID_FILE_ATTRIBUTES) {
					int nErrorCode = ::SHCreateDirectoryEx(NULL, strNewPath, NULL);
					bSuccess = (nErrorCode == ERROR_SUCCESS);
					if (bSuccess) nDirsCreated++;
					else lastError = nErrorCode;
				}
				if (bSuccess) {
					bSuccess = ::CopyFile(iter->GetName(), strNewName, TRUE) != 0;
					if (bSuccess) nFilesCopied++;
				}
			} else {
				bSuccess = ::MoveFile(iter->GetName(), strNewName) != 0;
				if (bSuccess) {
					nFilesRenamed++;
					iter->SetName(strNewName);
				}
			}
			if (!bSuccess) {
				// Failure, ask user if to continue
				if (lastError == 0) lastError = ::GetLastError();
				CString strError;
				strError.Format(CNLS::GetString(_T("The file '%s' could not be %s.")), iter->GetTitle(),
					CNLS::GetString(bCopyNeeded ? _T("copied") : _T("renamed")));
				strError += _T("\n");
				strError += CNLS::GetString(_T("Reason: "));
				LPTSTR lpMsgBuf = NULL;
				::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, lastError,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
				strError += lpMsgBuf;
				LocalFree(lpMsgBuf);
				strError += _T("\n");
				strError += CNLS::GetString(_T("Continue rename/copy?"));
				if (IDNO == ::MessageBox(NULL, strError, CNLS::GetString(_T("Error during rename/copy")), MB_YESNO | MB_ICONSTOP)) {
					break;
				}
			}
		}
		nIndex++;
	}

	::SetCursor(hOldCursor);

	CString strResult;
	strResult.Format(CNLS::GetString(_T("%d file(s) renamed, %d file(s) copied, %d folder(s) created")),
		nFilesRenamed, nFilesCopied, nDirsCreated);
	m_lblResult.SetWindowText(strResult);

	// if files were copied, maybe more files are now in the folders, reload file list
	if (nFilesCopied > 0) {
		m_fileList.Reload();
	}
	m_lvFiles.DeleteAllItems();
	m_nNumFiles = CreateItemList();

	return 0;
}

LRESULT CBatchCopyDlg::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CBatchCopyDlg::OnListViewItemChanged(WPARAM /*wParam*/, LPNMHDR lpnmhdr, BOOL& bHandled) {
	if (m_bInOnListViewItemChanged) {
		return 0;
	}

	m_bInOnListViewItemChanged = true;
	LPNMLISTVIEW lpnmListView = (LPNMLISTVIEW) lpnmhdr;
	UINT nOldState = lpnmListView->uOldState & LVIS_STATEIMAGEMASK;
	UINT nNewState = lpnmListView->uNewState & LVIS_STATEIMAGEMASK;
	if (nOldState != 0 && nOldState != nNewState) {
		BOOL bNewState = (nNewState >> 12) - 1;
		for (int i = 0; i < m_nNumFiles; i++) {
			if (m_lvFiles.GetItemState(i, LVIS_SELECTED)) {
				m_lvFiles.SetCheckState(i, bNewState);
			}
		}
	}
	m_bInOnListViewItemChanged = false;
	return 0;
}

LRESULT CBatchCopyDlg::OnListViewKeyDown(WPARAM /*wParam*/, LPNMHDR lpnmhdr, BOOL& bHandled) {
	// Select all items if CTRL-A is pressed
	LPNMLVKEYDOWN lpKeyDown = (LPNMLVKEYDOWN) lpnmhdr;
	if (lpKeyDown->wVKey == _T('A')) {
		if (::GetKeyState(VK_CONTROL) & 0xF000) {
			for (int i = 0; i < m_nNumFiles; i++) {
				m_lvFiles.SetCheckState(i, TRUE);
			}
		}
	}
	return 0;
}

int CBatchCopyDlg::CreateItemList() {
	std::list<CFileDesc>::const_iterator iter;
	const std::list<CFileDesc> & fileList = m_fileList.GetFileList();
	int nIndex = 0;
	for (iter = fileList.begin( ); iter != fileList.end( ); iter++ ) {
		m_lvFiles.InsertItem(nIndex, iter->GetTitle());
		SYSTEMTIME systemTime;
		FileTimeToLocalSystemTime(iter->GetLastModTime(), systemTime);
		TCHAR dateBuff[64];
		::GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &systemTime, NULL, dateBuff, 64);
		TCHAR timeBuff[64];
		::GetTimeFormat(LOCALE_USER_DEFAULT, 0, &systemTime, NULL, timeBuff, 64);
		m_lvFiles.SetItemText(nIndex, 1, CString(dateBuff) + _T(" ") + timeBuff);

		nIndex++;
	}

	return nIndex;
}

CString CBatchCopyDlg::GetPatternText() {
	TCHAR strPattern[MAX_PATH];
	((short*)strPattern)[0] = MAX_PATH;
	m_edtPattern.GetLine(0, strPattern);
	return CString(strPattern);
}

bool CBatchCopyDlg::IsCopyNeeded(LPCTSTR strName, LPCTSTR strDirectory) {
	CString strNewName(strName);
	strNewName.Replace(_T("%pictures%"), m_strMyPictures);
	if (_tcschr(strNewName, _T('\\')) == NULL) {
		// no path given, file is in same directory thus rename
		return false;
	}
	LPTSTR notUsed;
	TCHAR buffFullPathName[MAX_PATH];
	::GetFullPathName(strDirectory, MAX_PATH, buffFullPathName, &notUsed);
	int nPathLen = _tcsrchr(strNewName, _T('\\')) - strNewName;
	return _tcsnicmp(strNewName, buffFullPathName, nPathLen) != 0;
}

CString CBatchCopyDlg::ReplacePlaceholders(LPCTSTR strPattern, int nIndex, const CFileDesc & fileDesc, bool bReplaceMyPictures) {
	CString strNewName(strPattern);
	if (strNewName.Find(_T("%x")) != -1) {
		CString strNumber;
		strNumber.Format(_T("%d"), nIndex + 1);
		strNewName.Replace(_T("%x"), strNumber);
	}
	if (strNewName.Find(_T("%n")) != -1) {
		strNewName.Replace(_T("%n"), FindNumber(fileDesc.GetTitle()));
	}
	int nIndexPrc = FindNumberFormat(strNewName);
	if (nIndexPrc != -1) {
		TCHAR chNumDigits = strNewName.GetAt(nIndexPrc+1);
		CString strNumber;
		CString sFormat;
		sFormat.Format(_T("%%0%cd"), chNumDigits);
		strNumber.Format(sFormat, nIndex + 1);
		sFormat.Format(_T("%%%cx"), chNumDigits);
		strNewName.Replace(sFormat, strNumber);
	}
	if (strNewName.Find(_T("%f")) != -1) {
		strNewName.Replace(_T("%f"), fileDesc.GetTitle());
	}
	LPCTSTR pStartExt = _tcsrchr(fileDesc.GetTitle(), _T('.'));
	if (strNewName.Find(_T("%F")) != -1) {
		CString sTitle(fileDesc.GetTitle(), (pStartExt == NULL) ? _tcslen(fileDesc.GetTitle()) : pStartExt - fileDesc.GetTitle());
		strNewName.Replace(_T("%F"), sTitle);
	}
	if (strNewName.Find(_T("%e")) != -1 && pStartExt != NULL) {
		CString sExtension(pStartExt + 1);
		strNewName.Replace(_T("%e"), sExtension);
	}
	SYSTEMTIME systemTime;
	FileTimeToLocalSystemTime(fileDesc.GetLastModTime(), systemTime);
	if (strNewName.Find(_T("%d")) != -1) {
		CString sDay;
		sDay.Format(_T("%02d"), systemTime.wDay);
		strNewName.Replace(_T("%d"), sDay);
	}
	if (strNewName.Find(_T("%m")) != -1) {
		CString sMonth;
		sMonth.Format(_T("%02d"), systemTime.wMonth);
		strNewName.Replace(_T("%m"), sMonth);
	}
	if (strNewName.Find(_T("%y")) != -1) {
		CString sYear;
		sYear.Format(_T("%04d"), systemTime.wYear);
		strNewName.Replace(_T("%y"), sYear);
	}
	if (strNewName.Find(_T("%2y")) != -1) {
		CString sYear;
		sYear.Format(_T("%02d"), systemTime.wYear % 100);
		strNewName.Replace(_T("%2y"), sYear);
	}
	if (strNewName.Find(_T("%M")) != -1) {
		CString sMonth;
		LPTSTR sBuffer = sMonth.GetBuffer(32);
		::GetDateFormat(LOCALE_USER_DEFAULT, 0, &systemTime, _T("MMMM"), sBuffer, 32);
		sMonth.ReleaseBuffer();
		strNewName.Replace(_T("%M"), sMonth);
	}
	if (strNewName.Find(_T("%3M")) != -1) {
		CString sMonth;
		LPTSTR sBuffer = sMonth.GetBuffer(8);
		::GetDateFormat(LOCALE_USER_DEFAULT, 0, &systemTime, _T("MMM"), sBuffer, 8);
		sMonth.ReleaseBuffer();
		strNewName.Replace(_T("%3M"), sMonth);
	}
	if (bReplaceMyPictures) {
		strNewName.Replace(_T("%pictures%"), m_strMyPictures);
	}
	return strNewName;
}
