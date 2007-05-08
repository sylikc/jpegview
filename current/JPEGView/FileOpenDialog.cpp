#include "StdAfx.h"
#include "FileOpenDialog.h"
#include "NLS.h"
#include "Helpers.h"

bool CFileOpenDialog::m_bSized = false;

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
	return 0;
}

LRESULT CFileOpenDialog::OnSelChange(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/) {
	if (!m_bSized) {
		// This is a hack but only this member is called late enough to be able to change the size
		// after Windows itself has set the size
		HWND wndParent = this->GetParent();

		int nScreenX = ::GetSystemMetrics(SM_CXSCREEN);
		int nScreenY = ::GetSystemMetrics(SM_CYSCREEN);
		int nSizeX = nScreenX*80/100;
		int nSizeY = nScreenY*80/100;
		::MoveWindow(wndParent, (nScreenX-nSizeX)/2, (nScreenY-nSizeY)/2, 
			nSizeX, nSizeY, TRUE);
		m_bSized = true;
	}
	return 0;
}

