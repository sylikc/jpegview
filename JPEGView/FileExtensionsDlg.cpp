#include "StdAfx.h"
#include "FileExtensionsDlg.h"
#include "NLS.h"
#include "HelpersGUI.h"
#include "SettingsProvider.h"
#include "FileExtensionsRegistry.h"

///////////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////////

static CString FormatHint(LPCTSTR format, LPCTSTR argument) {
    CString sHint;
    sHint.Format(format, argument);
    return sHint;
}

static bool ShowRegistryError(HWND hWnd, LPCTSTR registryKey, LPCTSTR fileEndings) {
    CString sError = CNLS::GetString(_T("Error while writing the following registry key:"));
    sError += _T("\n");
    sError += registryKey;
    if (fileEndings != NULL) {
        sError += fileEndings;
    }
    return MessageBox(hWnd, sError, CNLS::GetString(_T("Error")), MB_OKCANCEL | MB_ICONERROR) == IDOK;
}

///////////////////////////////////////////////////////////////////////////////////
// Class implementation
///////////////////////////////////////////////////////////////////////////////////

CFileExtensionsDlg::CFileExtensionsDlg() {
    m_bInOnListViewItemChanged = false;
    m_pRegistry = new CFileExtensionsRegistry();
}

CFileExtensionsDlg::~CFileExtensionsDlg() {
    delete m_pRegistry;
}

LRESULT CFileExtensionsDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_lblMessage.Attach(GetDlgItem(IDC_HINT_FILE_EXT));
	m_lvFileExtensions.Attach(GetDlgItem(IDC_FILEEXTENSIONS));
    m_btnOk.Attach(GetDlgItem(IDC_FE_OK));
    m_btnCancel.Attach(GetDlgItem(IDC_FE_CANCEL));

	CenterWindow(GetParent());

	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

	this->SetWindowText(CNLS::GetString(_T("Set JPEGView as default image viewer")));

    CString s1 = CNLS::GetString(_T("JPEGView is registered as default viewer for the selected file formats."));
    CString s2 = CNLS::GetString(_T("Registration applies to the current user only."));
    m_lblMessage.SetWindowText(s1 + _T("\n") + s2);
    m_btnOk.SetWindowText(CNLS::GetString(_T("OK")));
    m_btnCancel.SetWindowText(CNLS::GetString(_T("Cancel")));

	DWORD nNewStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES;
	m_lvFileExtensions.SetExtendedListViewStyle(nNewStyle, nNewStyle);
	m_lvFileExtensions.InsertColumn(0, CNLS::GetString(_T("Extension")), LVCFMT_LEFT, (int)(HelpersGUI::ScreenScaling*120));
	m_lvFileExtensions.InsertColumn(1, CNLS::GetString(_T("File type")), LVCFMT_LEFT, (int)(HelpersGUI::ScreenScaling*365));

    FillFileExtensionsList();

	return TRUE;
}

LRESULT CFileExtensionsDlg::OnCancelDialog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CFileExtensionsDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
    std::list<CString> endingsToRegister;
    std::list<CString> endingsToUnregister;
    std::list<CString> endingsCantUnregister;

    int numItems = m_lvFileExtensions.GetItemCount();
	for (int i = 0; i < numItems; i++) {
        TCHAR buff[48];
        m_lvFileExtensions.GetItemText(i, 0, (LPTSTR)&buff, 48);
        
        TCHAR* extensions[4];
        TCHAR* pStart = (TCHAR*)buff;
        extensions[0] = pStart;
        int nNumExtensions = 1;
        int index = 0;
        while (buff[index] != 0 && nNumExtensions < 4) {
            if (buff[index] == _T(';')) {
                buff[index] = 0;
                pStart = &(buff[index + 1]);
                extensions[nNumExtensions++] = pStart;
            }
            index++;
        }

		bool bIsSelected = m_lvFileExtensions.GetCheckState(i);
        int oldSelectionState = (int)m_lvFileExtensions.GetItemData(i);
        if (!bIsSelected && oldSelectionState == 2) {
            for (int i = 0; i < nNumExtensions; i++) {
                bool bRegisteredInHKLM;
                m_pRegistry->CheckIfOpenedByJPEGView(extensions[i], bRegisteredInHKLM);
                if (bRegisteredInHKLM) {
                    endingsCantUnregister.push_back(CString(extensions[i]));
                } else {
                    endingsToUnregister.push_back(CString(extensions[i]));
                }
            }
        } else if (!bIsSelected && (oldSelectionState != 0)) {
            for (int i = 0; i < nNumExtensions; i++) {
                bool bRegisteredInHKLM;
                if (m_pRegistry->CheckIfOpenedByJPEGView(extensions[i], bRegisteredInHKLM)) {
                    endingsToUnregister.push_back(CString(extensions[i]));
                }
            }
        } else if (bIsSelected && (oldSelectionState == 0)) {
            for (int i = 0; i < nNumExtensions; i++) {
                bool bRegisteredInHKLM;
                if (!m_pRegistry->CheckIfOpenedByJPEGView(extensions[i], bRegisteredInHKLM)) {
                    endingsToRegister.push_back(CString(extensions[i]));
                }
            }
        }
	}

    // Warn for file extensions that cannot be unregistered because they are written in HKLM registry area
    if (endingsCantUnregister.size() > 0) {
        CString sText = CNLS::GetString(_T("The following file extensions cannot be unregistered because they have been registered in the 'local machine' registry area."));
        sText += _T("\n");
        sText += CNLS::GetString(_T("Registering or unregistering file extensions in this registry area requires administration privileges and cannot be done in JPEGView."));
        for (std::list<CString>::const_iterator it = endingsCantUnregister.begin(); it != endingsCantUnregister.end(); ++it) {
            sText += _T("\n");
            sText += *it;
        }
        sText += _T("\n");
        sText += CNLS::GetString(_T("Press 'OK' to continue with registration or 'Cancel' to not modify any registrations."));
        if (MessageBox(sText, CNLS::GetString(_T("Unregistering not possible")), MB_OKCANCEL | MB_ICONINFORMATION) == IDCANCEL) {
            return 0;
        }
    }

    // make sure JPEGView is registered in HKEY_CURRENT_USER\Software\Classes\Applications
    if (!m_pRegistry->RegisterJPEGView()) {
        if (!ShowRegistryError(m_hWnd, _T("HKCU\\Software\\Classes\\Applications\\JPEGView.exe\\shell\\open\\command"), NULL)) {
            return 0;
        }
    }

    CString sFailedEndings;
    CString sPrivilegeNeededEndings;
	CString sRunAsAdminRequiredEndings;
    std::list<CString> endingsToRegisterSecondTry;
    std::list<CString> endingsToUnregisterSecondTry;

    // Register new file extensions
    for (std::list<CString>::const_iterator it = endingsToRegister.begin(); it != endingsToRegister.end(); ++it) {
        RegResult result = m_pRegistry->RegisterFileExtension(*it, false);
        if (result == Reg_WarningRequiresAdminRights) {
            sPrivilegeNeededEndings += _T("\n");
            sPrivilegeNeededEndings += *it;
            endingsToRegisterSecondTry.push_back(*it);
        } else if (result == Reg_NeedsWriteToHKLMAsAdministrator) {
			sRunAsAdminRequiredEndings += _T("\n");
			sRunAsAdminRequiredEndings += *it;
		} else if (result != Reg_Success) {
            sFailedEndings += _T("\n");
            sFailedEndings += *it;
        }
    }

    // Unregister file extensions
    for (std::list<CString>::const_iterator it = endingsToUnregister.begin(); it != endingsToUnregister.end(); ++it) {
        RegResult result = m_pRegistry->UnregisterFileExtension(*it, false);
        if (result == Reg_WarningRequiresAdminRights) {
            sPrivilegeNeededEndings += _T("\n");
            sPrivilegeNeededEndings += *it;
            endingsToUnregisterSecondTry.push_back(*it);
        } else if (result != Reg_Success) {
            sFailedEndings += _T("\n");
            sFailedEndings += *it;
        }
    }

    if (sPrivilegeNeededEndings.GetLength() > 0) {
        CString msg(CNLS::GetString(_T("The following image extensions are protected by the operating system.")));
        msg += _T("\n");
        msg += CNLS::GetString(_T("JPEGView can try to remove the restrictions automatically."));
        msg += _T("\n");
        msg += CNLS::GetString(_T("However this will fail if the current user has no administration privileges."));
        msg += sPrivilegeNeededEndings;
        if (::MessageBox(m_hWnd, msg, CNLS::GetString(_T("Warning")), MB_OKCANCEL | MB_ICONWARNING) == IDCANCEL) {
            return 0;
        }

        for (std::list<CString>::const_iterator it = endingsToRegisterSecondTry.begin(); it != endingsToRegisterSecondTry.end(); ++it) {
            RegResult result = m_pRegistry->RegisterFileExtension(*it, true);
            if (result == Reg_ErrorNoAdminRights) {
                if (::MessageBox(m_hWnd, CNLS::GetString(_T("Insufficient rights to remove the restrictions. Continue?")), CNLS::GetString(_T("Error")), MB_YESNO | MB_ICONERROR) == IDNO) {
                    return 0;
                }
            } else if (result != Reg_Success) {
                sFailedEndings += _T("\n");
                sFailedEndings += *it;
            }
        }

        for (std::list<CString>::const_iterator it = endingsToUnregisterSecondTry.begin(); it != endingsToUnregisterSecondTry.end(); ++it) {
            RegResult result = m_pRegistry->UnregisterFileExtension(*it, true);
            if (result == Reg_ErrorNoAdminRights) {
                if (::MessageBox(m_hWnd, CNLS::GetString(_T("Insufficient rights to remove the restrictions. Continue?")), CNLS::GetString(_T("Error")), MB_YESNO | MB_ICONERROR) == IDNO) {
                    return 0;
                }
            } else if (result != Reg_Success) {
                sFailedEndings += _T("\n");
                sFailedEndings += *it;
            }
        }
    }

	if (sRunAsAdminRequiredEndings.GetLength() > 0) {
        CString msg(CNLS::GetString(_T("The following image extensions can only be registered when run as administrator:")));
        msg += _T("\n");
		msg += sRunAsAdminRequiredEndings;
		msg += _T("\n\n");
        msg += CNLS::GetString(_T("To run JPEGView with administrator rights:"));
        msg += _T("\n");
        msg += CNLS::GetString(_T("Right-click JPEGView.exe in the Windows Explorer and select 'Run as administrator'"));
        ::MessageBox(m_hWnd, msg, CNLS::GetString(_T("Warning")), MB_OK | MB_ICONWARNING);
	}

    if (sFailedEndings.GetLength() > 0) {
        if (!ShowRegistryError(m_hWnd, _T("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\"), sFailedEndings)) {
            return 0;
        }
    }

    EndDialog(IDOK);
	return 0;
}

LRESULT CFileExtensionsDlg::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CFileExtensionsDlg::OnListViewItemChanged(WPARAM /*wParam*/, LPNMHDR lpnmhdr, BOOL& bHandled) {
	if (m_bInOnListViewItemChanged) {
		return 0;
	}

	m_bInOnListViewItemChanged = true;
	LPNMLISTVIEW lpnmListView = (LPNMLISTVIEW) lpnmhdr;
	UINT nOldState = lpnmListView->uOldState & LVIS_STATEIMAGEMASK;
	UINT nNewState = lpnmListView->uNewState & LVIS_STATEIMAGEMASK;
	if (nOldState != 0 && nOldState != nNewState) {
        int numItems = m_lvFileExtensions.GetItemCount();
		BOOL bNewState = (nNewState >> 12) - 1;
		for (int i = 0; i < numItems; i++) {
			if (m_lvFileExtensions.GetItemState(i, LVIS_SELECTED)) {
				m_lvFileExtensions.SetCheckState(i, bNewState);
			}
		}
	}
	m_bInOnListViewItemChanged = false;

	return 0;
}

LRESULT CFileExtensionsDlg::OnListViewKeyDown(WPARAM /*wParam*/, LPNMHDR lpnmhdr, BOOL& bHandled) {
	// Select all items if CTRL-A is pressed
	LPNMLVKEYDOWN lpKeyDown = (LPNMLVKEYDOWN) lpnmhdr;
	if (lpKeyDown->wVKey == _T('A')) {
		if (::GetKeyState(VK_CONTROL) & 0xF000) {
            int numItems = m_lvFileExtensions.GetItemCount();
			for (int i = 0; i < numItems; i++) {
				m_lvFileExtensions.SetCheckState(i, TRUE);
			}
		}
	}
	return 0;
}

void CFileExtensionsDlg::FillFileExtensionsList() {
    InsertExtension(_T("*.jpg;*.jpeg"), FormatHint(CNLS::GetString(_T("%s images")), _T("JPEG")));
    InsertExtension(_T("*.png"), FormatHint(CNLS::GetString(_T("%s images")), _T("PNG")));
    InsertExtension(_T("*.tif;*.tiff"), FormatHint(CNLS::GetString(_T("%s images")), _T("TIFF")));
    InsertExtension(_T("*.bmp"), FormatHint(CNLS::GetString(_T("%s images")), _T("Windows bitmap")));
    InsertExtension(_T("*.gif"), FormatHint(CNLS::GetString(_T("%s images")), _T("GIF")));
    InsertExtension(_T("*.tga"), FormatHint(CNLS::GetString(_T("%s images")), _T("TGA")));
    InsertExtension(_T("*.webp"), FormatHint(CNLS::GetString(_T("%s images")), _T("Google WEBP")));
    InsertExtensions(CSettingsProvider::This().FilesProcessedByWIC(), CNLS::GetString(_T("%s images (processed by Window Imaging Component - WIC)")));
    InsertExtensions(CSettingsProvider::This().FileEndingsRAW(), CNLS::GetString(_T("%s camera raw images (embedded JPEGs only)")));
}

void CFileExtensionsDlg::InsertExtensions(LPCTSTR sExtensionList, LPCTSTR sHint) {
    int nNumChars = _tcslen(sExtensionList);
    int nStart = 0;
    for (int i = 0; i <= nNumChars; i++) {
        if (sExtensionList[i] == _T(';') || sExtensionList[i] == 0) {
            CString sExtension(&sExtensionList[nStart], i - nStart);
            if (sExtension.GetLength() >= 3) {
                CString sExtensionUpper(&((LPCTSTR)sExtension)[2]);
                sExtensionUpper.MakeUpper();
                InsertExtension(sExtension, FormatHint(sHint, sExtensionUpper));
            }
            nStart = i + 1;
        }
    }
}

void CFileExtensionsDlg::InsertExtension(LPCTSTR sExtension, LPCTSTR sHint)
{
    int numItems = m_lvFileExtensions.GetItemCount();
    m_lvFileExtensions.InsertItem(numItems, sExtension);
    m_lvFileExtensions.SetItemText(numItems, 1, sHint);

    bool bIsRegisteredInHKLM = false;
    bool bIsRegistered = m_pRegistry->IsJPEGViewRegistered() && m_pRegistry->CheckIfOpenedByJPEGView(sExtension, bIsRegisteredInHKLM);
    m_lvFileExtensions.SetCheckState(numItems, bIsRegistered);

    int originalCheckState = (bIsRegisteredInHKLM && bIsRegistered) ? 2 : bIsRegistered ? 1 : 0;
    m_lvFileExtensions.SetItemData(numItems, (DWORD_PTR)originalCheckState);
}