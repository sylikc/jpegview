#include "StdAfx.h"
#include "FileOpenDialog.h"
#include "NLS.h"
#include "Helpers.h"
#include "dlgs.h"

bool CFileOpenDialog::m_bSized = false;

// Command code to set the thumbnail view on the file open dialog
const unsigned int ODM_VIEW_THUMBS= 0x702d;

CFileOpenDialog::CFileOpenDialog(HWND parentWindow, LPCTSTR sInitialFileName, LPCTSTR sFileEndings) : 
CFileDialog(TRUE, _T("jpg"), sInitialFileName, 
			OFN_EXPLORER | OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST,
			Helpers::CReplacePipe(CString(CNLS::GetString(_T("Images"))) + _T(" (") + sFileEndings + _T(")|") + sFileEndings + _T("|") +
			CNLS::GetString(_T("All Files")) + _T("|*.*|")), parentWindow) {
}

LRESULT CFileOpenDialog::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (!m_bSized) {
		HWND wndParent = this->GetParent();
		// moving the window out of the visible range prevents flickering
		::SetWindowPos(wndParent, 0, 20000, 20000, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW);
	}

	// Will be processed after the dialog is really initialized
	::PostMessage(m_hWnd, MYWM_POSTINIT, 0, 0);

	return 0;
}

LRESULT CFileOpenDialog::OnPostInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	HWND wndParent = this->GetParent(); // real dialog is parent of us

	SizeDialog();

	// Get the shell window hosting the list view of the files and send a WM_COMMAND to it to
	// switch to thumbnail view
	// http://msdn.microsoft.com/msdnmag/issues/04/03/CQA/
	HWND hLst2 = ::GetDlgItem(wndParent, lst2);
	if (hLst2 != NULL) {
		::SendMessage(hLst2, WM_COMMAND, ODM_VIEW_THUMBS, 0);
	}
	return 0;
}

void CFileOpenDialog::SizeDialog() {
	if (!m_bSized) {
		HWND wndParent = this->GetParent(); // real dialog is parent of us

		int nScreenX = ::GetSystemMetrics(SM_CXSCREEN);
		int nScreenY = ::GetSystemMetrics(SM_CYSCREEN);
		int nSizeX = nScreenX*80/100;
		int nSizeY = nScreenY*80/100;
		::MoveWindow(wndParent, (nScreenX-nSizeX)/2, (nScreenY-nSizeY)/2, 
			nSizeX, nSizeY, TRUE);
		m_bSized = true;
	}
}


