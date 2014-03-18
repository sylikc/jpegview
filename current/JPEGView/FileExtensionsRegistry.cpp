#include "StdAfx.h"
#include "FileExtensionsRegistry.h"
#include "Helpers.h"
#include "SettingsProvider.h"
#include "NLS.h"
#include "FileList.h"

///////////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////////

RegResult ResetPermissionsForRegistryKey(LPCTSTR subKeyRelativeToHKCU);

// Auto closing registry key on destructor
class auto_hkey {
public:
    auto_hkey(HKEY hKey) {
        m_hKey = hKey;
    }
    ~auto_hkey() {
        RegCloseKey(m_hKey);
    }
private:
    HKEY m_hKey;
};

// Gets a string value from the registry, given an open key and the name of the string value to get.
// If the name is NULL, the default value for the key is returned.
static bool GetRegistryStringValue(HKEY key, LPCTSTR name, CString & outValue) {
    DWORD type;
    const int buffsize = 512;
    TCHAR buff[buffsize];
    DWORD size = buffsize * sizeof(TCHAR);
    bool bOk = RegQueryValueEx(key, name, NULL, &type, (LPBYTE)&buff, &size) == ERROR_SUCCESS;
    bOk = bOk && (type == REG_SZ || type == REG_EXPAND_SZ);
    if (bOk) outValue = buff;
    return bOk;
}

// Checks if a registry key relative to HKEY_CURRENT_USER has the specified string value.
// If expectedValue is NULL, it is just checked if the keyName exists.
static bool ExistsAndHasStringValue(LPCTSTR path, LPCTSTR keyName, LPCTSTR expectedValue) {
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_READ, &key) == ERROR_SUCCESS) {
		CString value;
		bool bOk = GetRegistryStringValue(key, keyName, value);
		bOk = bOk && (expectedValue == NULL || _tcsicmp(value, expectedValue) == 0);
		RegCloseKey(key);
		return bOk;
	}
	return false;
}

// Sets a string value in the registry, given an open key and the name of the string value to set.
// If the name is NULL, the default value for the key is set.
static bool SetRegistryStringValue(HKEY key, LPCTSTR name, LPCTSTR stringValue) {
	return RegSetValueEx(key, name, 0, REG_SZ, (const BYTE *)stringValue, ((int)_tcslen(stringValue) + 1) * sizeof(TCHAR)) == ERROR_SUCCESS;
}

// Checks if JPEGView is registered as application in the registry
static bool IsRegistered() {
    HKEY key;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Classes\\Applications\\JPEGView.exe\\shell\\open\\command"), 0, KEY_READ, &key) == ERROR_SUCCESS) {
        CString value;
        bool bOk = GetRegistryStringValue(key, NULL, value);
        bOk = bOk && Helpers::stristr(value, CSettingsProvider::This().GetEXEPath()) != NULL;
        RegCloseKey(key);
        return bOk;
    }
    return false;
}

// Gets the registry path for the registration of the given file extension, relative to HKCU
static CString GetRegKeyPathForExtension(LPCTSTR sExtension, bool bNewRegistryFormat) {
    LPCTSTR sRegKeyPathFormat = bNewRegistryFormat ? 
        _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s\\UserChoice") :
        _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s");
    CString sRegKeyPath;
    sRegKeyPath.Format(sRegKeyPathFormat, sExtension);
    return sRegKeyPath;
}

// Gets the registry path for the registration of the given file extension where the MRU list is stored, relative to HKCU
static CString GetMRURegistryPathForExtension(LPCTSTR sExtension) {
    CString sRegKeyPath;
    sRegKeyPath.Format(_T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s\\OpenWithList"), sExtension);
    return sRegKeyPath;
}

static bool CheckJPEGViewIsShellOpenHandler(LPCTSTR sClassesName) {
	CString sRegKeyPathForExtension = CString(sClassesName) + _T("\\shell\\open\\command");
    HKEY subKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, CString(_T("Software\\Classes\\")) + sRegKeyPathForExtension, 0, KEY_READ, &subKey) != ERROR_SUCCESS) {
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, sRegKeyPathForExtension, 0, KEY_READ, &subKey) != ERROR_SUCCESS) {
			return false;
		}
	}
	CString value;
    bool isRegistered = GetRegistryStringValue(subKey, NULL, value);
    isRegistered = isRegistered && Helpers::stristr(value, CSettingsProvider::This().GetEXEPath()) != NULL;
    RegCloseKey(subKey);
	return isRegistered;
}

// Checks if the given extension is registered for JPEGView in HKEY_CLASSES_ROOT\Extension or HKCU\Software\Classes\Extension
static bool CheckExtensionRegisteredInClasses(LPCTSTR sExtension) {
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, CString(_T("Software\\Classes\\")) + sExtension, 0, KEY_READ, &key) != ERROR_SUCCESS) {
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, sExtension, 0, KEY_READ, &key) != ERROR_SUCCESS) {
			return false;
		}
	}

	bool isRegistered = false;
	CString value;
    if (GetRegistryStringValue(key, NULL, value)) {
		isRegistered = CheckJPEGViewIsShellOpenHandler(value);
    }
    RegCloseKey(key);
	return isRegistered;
}

// Checks if the given extension is registered for JPEGView
static bool CheckExtensionRegistered(LPCTSTR sExtension, bool bNewRegistryFormat, bool & outRegisteredInHKLM) {
    outRegisteredInHKLM = CheckExtensionRegisteredInClasses(sExtension);

    HKEY key;
    CString sRegKeyPathForExtension = GetRegKeyPathForExtension(sExtension, bNewRegistryFormat);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, sRegKeyPathForExtension, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        CString value;
        bool userChoiceExists = GetRegistryStringValue(key, bNewRegistryFormat ? _T("Progid") : _T("Application"), value);
		bool isRegistered;
		if (userChoiceExists) {
			isRegistered = Helpers::stristr(value, _T("JPEGView.exe")) != NULL || CheckJPEGViewIsShellOpenHandler(value);
		} else {
			isRegistered = outRegisteredInHKLM;
		}
        RegCloseKey(key);
        return isRegistered;
    }
    return outRegisteredInHKLM;
}

// Checks if the given key relative to HKCU exists
static bool ExistsKey(LPCTSTR subKeyRelativeToHKCU) {
    HKEY key;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, subKeyRelativeToHKCU, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return true;
    }
    return false;
}

// Checks if the given key relative to HKEY_CLASSES_ROOT exists
static bool ExistsKeyHKCR(LPCTSTR subKeyRelativeToHKCR, bool &hasDefaultValue) {
	hasDefaultValue = false;
    HKEY key;
    if (RegOpenKeyEx(HKEY_CLASSES_ROOT, subKeyRelativeToHKCR, 0, KEY_READ, &key) == ERROR_SUCCESS) {
		CString value;
		hasDefaultValue = GetRegistryStringValue(key, NULL, value);
        RegCloseKey(key);
        return true;
    }
    return false;
}

// Checks if under the given subkey (relative to HKCU) there is a 'Hash' key
static bool IsProtectedByHash(LPCTSTR subKeyRelativeToHKCU, bool checkForRegistration) {
	HKEY key;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, subKeyRelativeToHKCU, 0, KEY_READ, &key) == ERROR_SUCCESS) {
		CString value;
		bool hasHashValue = GetRegistryStringValue(key, _T("Hash"), value);
		bool hasProgId = GetRegistryStringValue(key, _T("ProgId"), value);
		RegCloseKey(key);
        return checkForRegistration ?
			hasHashValue && !(hasProgId && value == _T("Applications\\JPEGView.exe")) :
			hasHashValue;
	}
	return false;
}

// Registers JPEGView in the MRU list for the extension so that it appears in the 'Open with' list
static void RegisterInMRU(LPCTSTR sRegMRUPath) {
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, sRegMRUPath, 0, KEY_READ | KEY_WRITE, &key) != ERROR_SUCCESS)
		return;

	auto_hkey autoKey(key);

	CString mruList;
	if (!GetRegistryStringValue(key, _T("MRUList"), mruList) || mruList.IsEmpty()) {
		// No MRU list yet, create new list containing JPEGView.exe
		if (SetRegistryStringValue(key, _T("MRUList"), _T("a"))) {
			SetRegistryStringValue(key, _T("a"), _T("JPEGView.exe"));
		}
		return;
	}

	for (int index = 0; index < mruList.GetLength(); index++) {
		CString mruEntry = CString(mruList.GetAt(index));
		CString mruValue;
		if (GetRegistryStringValue(key, mruEntry, mruValue)) {
			if (mruValue == _T("JPEGView.exe")) {
				return; // already in MRU list, do nothing
			}
		}
	}
	// append another entry to the MRU list with JPEGView.exe
	CString nextMRUEntry = CString(mruList.GetAt(mruList.GetLength() - 1) + 1);
	if (nextMRUEntry.GetAt(0) <= _T('z') && SetRegistryStringValue(key, _T("MRUList"), mruList + nextMRUEntry)) {
		SetRegistryStringValue(key, nextMRUEntry, _T("JPEGView.exe"));
	}
}

// Registers the given extension for JPEGView
static RegResult RegisterExtension(LPCTSTR sExtension, bool bNewRegistryFormat, bool bIsWindows8, bool bChangeDACLIfRequired) {
	
	bool hasDefaultValue;
	bool existsKeyHKCR = ExistsKeyHKCR(sExtension, hasDefaultValue);
	if (!existsKeyHKCR || !hasDefaultValue)
	{
		// The extension is not registered globally in HKEY_CLASSES_ROOT. Create the registration in HKEY_CURRENT_USER.
		CString keyPath = CString(_T("Software\\Classes\\")) + sExtension;
		bool existsHKCU = ExistsKey(keyPath);
		HKEY key;
		if (!existsHKCU) {
			if (RegCreateKeyEx(HKEY_CURRENT_USER, keyPath, 0, NULL, 0, KEY_WRITE | KEY_READ, NULL, &key, NULL) != ERROR_SUCCESS) {
				return Reg_ErrorWriteKey;
			}
		} else if (RegOpenKeyEx(HKEY_CURRENT_USER, keyPath, 0, KEY_WRITE | KEY_READ, &key) != ERROR_SUCCESS) {
			return Reg_ErrorWriteKey;
		}
		CString autoFile = CString(sExtension + 1) + _T("_auto_file");
		CString notUsed;
		if (!GetRegistryStringValue(key, NULL, notUsed)) {
			// write the default value as "xxx_auto_file" if none exist yet
			if (!SetRegistryStringValue(key, NULL, autoFile)) {
				RegCloseKey(key);
				return Reg_ErrorWriteKey;
			}
		}
		RegCloseKey(key);

		CString shellOpenPath = CString(_T("Software\\Classes\\")) + autoFile + _T("\\shell\\open\\command");
		if (!ExistsKey(shellOpenPath)) {
			if (RegCreateKeyEx(HKEY_CURRENT_USER, shellOpenPath, 0, NULL, 0, KEY_WRITE | KEY_READ, NULL, &key, NULL) != ERROR_SUCCESS) {
				return Reg_ErrorWriteKey;
			}
			CString startupCommand;
			startupCommand.Format(_T("\"%sJPEGView.exe\" \"%%1\""),  CSettingsProvider::This().GetEXEPath());
			if (!SetRegistryStringValue(key, NULL, startupCommand)) {
				RegCloseKey(key);
				return Reg_ErrorWriteKey;
			}
			RegCloseKey(key);
		}
	}

	CString sRegKeyPathForExtension = GetRegKeyPathForExtension(sExtension, bNewRegistryFormat);
	CString sRegMRUPathForExtension = GetMRURegistryPathForExtension(sExtension);

	if (bIsWindows8 && IsProtectedByHash(sRegKeyPathForExtension, true)) {
		RegisterInMRU(sRegMRUPathForExtension);
		return Reg_ErrorProtectedByHash;
	}

	HKEY key;
    bool bCanWriteKey = (RegCreateKeyEx(HKEY_CURRENT_USER, sRegKeyPathForExtension, 0, NULL, 0, KEY_WRITE | KEY_READ, NULL, &key, NULL) == ERROR_SUCCESS);
    if (!bCanWriteKey) {
        if (ExistsKey(sRegKeyPathForExtension)) {
            // no rights to write to the key
            if (!bChangeDACLIfRequired) {
                return Reg_WarningNeedsChangeDACL;
            }
            // try to change the DACL to get the rights
            RegResult result = ResetPermissionsForRegistryKey(sRegKeyPathForExtension);
            if (result != Reg_Success) {
                return result;
            }
        } else {
            return Reg_ErrorWriteKey;
        }
    }

    if (RegCreateKeyEx(HKEY_CURRENT_USER, sRegKeyPathForExtension, 0, NULL, 0, KEY_WRITE | KEY_READ, NULL, &key, NULL) == ERROR_SUCCESS) {
        auto_hkey autoKey(key);
        LPCTSTR sKeyName = bNewRegistryFormat ? _T("Progid") : _T("Application");
        CString value;
        bool bOk = GetRegistryStringValue(key, sKeyName, value);
        if (bOk) {
            if (Helpers::stristr(value, _T("JPEGView.exe")) == NULL) {
                // another application is set as default viewer, backup this
                if (!SetRegistryStringValue(key, CString(sKeyName) + _T("_backup"), value)) {
                    return Reg_ErrorWriteKey;
                }
            } else {
                // already registered for JPEGView
                return Reg_Success;
            }
        }
        bOk = SetRegistryStringValue(key, sKeyName, bNewRegistryFormat ? _T("Applications\\JPEGView.exe") : _T("JPEGView.exe"));

		if (bOk && bNewRegistryFormat) {
			RegisterInMRU(sRegMRUPathForExtension);
		}

        return bOk ? Reg_Success : Reg_ErrorWriteKey;
    }
    return Reg_ErrorWriteKey;
}

static RegResult UnregisterExtension(LPCTSTR sExtension, bool bNewRegistryFormat, bool bIsWindows8, bool bChangeDACLIfRequired) {
    bool bRegisteredInHKLM;
    bool bRegistered = CheckExtensionRegistered(sExtension, bNewRegistryFormat, bRegisteredInHKLM);
    if (!bRegistered || bRegisteredInHKLM) {
        return Reg_Success;
    }

    CString sRegKeyPathForExtension = GetRegKeyPathForExtension(sExtension, bNewRegistryFormat);

	if (bIsWindows8 && IsProtectedByHash(sRegKeyPathForExtension, false)) {
		return Reg_ErrorProtectedByHash;
	}

    HKEY key;
    bool bCanWriteKey = (RegOpenKeyEx(HKEY_CURRENT_USER, sRegKeyPathForExtension, 0, DELETE | KEY_WRITE | KEY_READ, &key) == ERROR_SUCCESS);
    if (!bCanWriteKey) {
        if (ExistsKey(sRegKeyPathForExtension)) {
            // no rights to write to the key
            if (!bChangeDACLIfRequired) {
                return Reg_WarningNeedsChangeDACL;
            }
            // try to change the DACL to get the rights
            RegResult result = ResetPermissionsForRegistryKey(sRegKeyPathForExtension);
            if (result != Reg_Success) {
                return result;
            }
        } else {
            return Reg_ErrorWriteKey;
        }
    }

    if (RegOpenKeyEx(HKEY_CURRENT_USER, sRegKeyPathForExtension, 0, DELETE | KEY_WRITE | KEY_READ, &key) == ERROR_SUCCESS) {
        LPCTSTR sKeyName = bNewRegistryFormat ? _T("Progid") : _T("Application");
        bool bOk = RegDeleteValue(key, sKeyName) == ERROR_SUCCESS;
        if (bOk) {
            // if deletion succeeded, check if there is a backup value and restore if yes
            CString sBackupValue;
            if (GetRegistryStringValue(key, CString(sKeyName) + _T("_backup"), sBackupValue)) {
                if (SetRegistryStringValue(key, sKeyName, sBackupValue)) {
                    RegDeleteValue(key, CString(sKeyName) + _T("_backup"));
                }
            }
        }
        RegCloseKey(key);
        return bOk ? Reg_Success : Reg_ErrorWriteKey;
    }
    return Reg_ErrorWriteKey;
}

// Removes all 'Deny' access control entries from the DACL of the given subkey relative to HKCU
// and adds the 'Interactive User' to have full access
static RegResult ResetPermissionsForRegistryKey(LPCTSTR subKeyRelativeToHKCU)
{ 
    HKEY hKey = NULL;
    if (ERROR_SUCCESS != ::RegOpenKeyEx(HKEY_CURRENT_USER, subKeyRelativeToHKCU,
        0, WRITE_DAC | READ_CONTROL, &hKey))
    {
        return Reg_ErrorNeedsWriteDACLRights;
    }

    auto_hkey autoKey(hKey);

    const int MAX_SIZE_SEC_DESC = 1024;
    SECURITY_DESCRIPTOR* pExistingSecDesc = (SECURITY_DESCRIPTOR*)new char[MAX_SIZE_SEC_DESC];
    std::auto_ptr<char> auto_ptr_sec_desc((char*)pExistingSecDesc);

    // Get the existing DACL of the registry key
    DWORD sizeDecDesc = MAX_SIZE_SEC_DESC;
    if (ERROR_SUCCESS != ::RegGetKeySecurity(hKey, (SECURITY_INFORMATION)DACL_SECURITY_INFORMATION, pExistingSecDesc, &sizeDecDesc)) {
        return Reg_ErrorChangeDACL;
    }
    BOOL hasDACL = FALSE, aclDefaulted;
    PACL pACL = NULL;
    if (!::GetSecurityDescriptorDacl(pExistingSecDesc, &hasDACL, &pACL, &aclDefaulted)) {
        return Reg_ErrorChangeDACL;
    }

    // Obtain DACL information.
    ACL_SIZE_INFORMATION asi;
    memset(&asi, 0, sizeof(ACL_SIZE_INFORMATION));
    if (pACL != NULL && !::GetAclInformation(pACL, (LPVOID)&asi, (DWORD)sizeof(ACL_SIZE_INFORMATION), AclSizeInformation)) {
        return Reg_ErrorChangeDACL;
    }

    // Prepare SID for interactive user
    SID_IDENTIFIER_AUTHORITY SecIA = SECURITY_NT_AUTHORITY;
    PSID pSid = NULL;
    if (FALSE == ::AllocateAndInitializeSid(&SecIA, 1,
        SECURITY_INTERACTIVE_RID, 0, 0, 0, 0, 0, 0, 0, &pSid))
    {
        return Reg_ErrorChangeDACL;
    }

    BOOL bSuccess = FALSE;

    DWORD dwAclSize = 2048; // that should be enough...
    PACL pDacl = (PACL) new BYTE[dwAclSize];

    if (TRUE == ::InitializeAcl(pDacl, dwAclSize, ACL_REVISION))
    {
        if (TRUE == ::AddAccessAllowedAce(pDacl, ACL_REVISION, KEY_ALL_ACCESS, pSid))
        {
           // Loop through all the existing ACEs to copy over but exclude the ACCESS_DENIED_ACE_TYPE ACEs
            if (hasDACL && pACL != NULL) {
               for (int i = 0; i < (int) asi.AceCount; i++)
                 {
                    // Get current ACE.
                    ACCESS_ALLOWED_ACE *pace;
                    if (::GetAce(pACL, i, (LPVOID*)&pace))
                    {
                        // Build the new ACL but exclude the access denied ACEs
                        if (!pace->Header.AceType == ACCESS_DENIED_ACE_TYPE) {
                            ::AddAce(pDacl, ACL_REVISION, MAXDWORD, pace, ((PACE_HEADER)pace)->AceSize);
                        }
                    }
                }
            }

            // Add the new DACL for the registry key
            SECURITY_DESCRIPTOR SecDesc;
            if (TRUE == ::InitializeSecurityDescriptor(&SecDesc,
                SECURITY_DESCRIPTOR_REVISION))
            {
                if (TRUE == ::SetSecurityDescriptorDacl(&SecDesc, TRUE, pDacl, FALSE))
                {
                    bSuccess = (ERROR_SUCCESS == ::RegSetKeySecurity(hKey,
                        (SECURITY_INFORMATION)DACL_SECURITY_INFORMATION, &SecDesc));
                }
            }
        }
    }

    delete[] (BYTE*) pDacl;
    if (pSid != NULL)
    {
        ::FreeSid(pSid);
    }

    return bSuccess ? Reg_Success : Reg_ErrorChangeDACL;
} 

///////////////////////////////////////////////////////////////////////////////////
// Class definition
///////////////////////////////////////////////////////////////////////////////////

CFileExtensionsRegistry::CFileExtensionsRegistry() {
	int version = CFileExtensionsRegistrationWindows8::GetWindowsVersion();
	// Starting with Windows Vista, the Windows Explorer uses a different location
	// to register file extensions
	m_bNewRegistryFormat = version >= 600;
	m_bIsWindows8 = version >= 602;
    m_bIsJPEGViewRegistered = IsRegistered();
}

bool CFileExtensionsRegistry::RegisterJPEGView() {
    if (!m_bIsJPEGViewRegistered) {
        HKEY key;
        if (RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\Classes\\Applications\\JPEGView.exe\\shell\\open\\command"), 0,
            NULL, 0, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
            CString value;
            value.Format(_T("\"%sJPEGView.exe\" \"%%1\""),  CSettingsProvider::This().GetEXEPath());

            bool bOk = SetRegistryStringValue(key, NULL, value);
            RegCloseKey(key);
            return bOk;
        }
        return false;
    }
    return true;
}

bool CFileExtensionsRegistry::CheckIfOpenedByJPEGView(LPCTSTR sExtensionList, bool& outRegisteredInHKLM) {
    bool bOk = false;
	int nNumChars = (int)_tcslen(sExtensionList);
    int nStart = 0;
    outRegisteredInHKLM = false;
    for (int i = 0; i <= nNumChars; i++) {
        if (sExtensionList[i] == _T(';') || sExtensionList[i] == 0) {
            CString sExtension(&sExtensionList[nStart], i - nStart);
            if (sExtension.GetLength() >= 3) {
                CString sExtensionStripped(&((LPCTSTR)sExtension)[1]);
                sExtensionStripped.MakeLower();
                if (sExtensionStripped.GetAt(0) == _T('.')) {
                    bool bThisExtRegisteredInHKLM;
                    if (!CheckExtensionRegistered(sExtensionStripped, m_bNewRegistryFormat, bThisExtRegisteredInHKLM)) {
                        return false;
                    }
                    if (bThisExtRegisteredInHKLM) outRegisteredInHKLM = true;
                    bOk = true;
                }
            }
            nStart = i + 1;
        }
    }
    return bOk;
}

RegResult CFileExtensionsRegistry::RegisterFileExtension(LPCTSTR sExtension, bool bChangeDACLIfRequired) {
	int nNumChars = (int)_tcslen(sExtension);
    if (nNumChars >= 3) {
        CString sExtensionStripped(&((LPCTSTR)sExtension)[1]);
        sExtensionStripped.MakeLower();
        if (sExtensionStripped.GetAt(0) == _T('.')) {
            return RegisterExtension(sExtensionStripped, m_bNewRegistryFormat, m_bIsWindows8, bChangeDACLIfRequired);
        }
    }
    return Reg_Internal;
}

RegResult CFileExtensionsRegistry::UnregisterFileExtension(LPCTSTR sExtension, bool bChangeDACLIfRequired) {
	int nNumChars = (int)_tcslen(sExtension);
    if (nNumChars >= 3) {
        CString sExtensionStripped(&((LPCTSTR)sExtension)[1]);
        sExtensionStripped.MakeLower();
        if (sExtensionStripped.GetAt(0) == _T('.')) {
            return UnregisterExtension(sExtensionStripped, m_bNewRegistryFormat, m_bIsWindows8, bChangeDACLIfRequired);
        }
    }
    return Reg_Internal;
}

CFileExtensionsRegistrationWindows8::CFileExtensionsRegistrationWindows8() {
}

int CFileExtensionsRegistrationWindows8::GetWindowsVersion() {
#pragma warning(push)
#pragma warning(disable:4996)
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
#pragma warning(pop)
	return osvi.dwMajorVersion * 100 + osvi.dwMinorVersion;
}

bool CFileExtensionsRegistrationWindows8::RegisterJPEGView() {
	// ProgId
	CString startupCommand;
	startupCommand.Format(_T("\"%sJPEGView.exe\" \"%%1\""), CSettingsProvider::This().GetEXEPath());
	if (!WriteStringValue(_T("Software\\Classes\\JPEGViewImageFile\\shell\\open\\command"), NULL, startupCommand)) {
		return false;
	}

	// Set as RegisteredApplications and declare capabilities
	if (!WriteStringValue(_T("Software\\RegisteredApplications"), _T("JPEGView"), _T("Software\\JPEGView\\Capabilities"))) {
		return false;
	}
	if (!WriteStringValue(_T("Software\\JPEGView\\Capabilities"), _T("ApplicationDescription"), 
		CNLS::GetString(_T("JPEGView is a lean, fast and highly configurable viewer/editor for JPEG, BMP, PNG, WEBP, TGA, GIF and TIFF images with a minimal GUI.")))) {
		return false;
	}
	if (!WriteStringValue(_T("Software\\JPEGView\\Capabilities"), _T("ApplicationName"), _T("JPEGView"))) {
		return false;
	}
	
	// Declare all supported file endings
	CString fileEndings = CFileList::GetSupportedFileEndings();
	int lenght = fileEndings.GetLength();
	LPTSTR buffer = fileEndings.GetBuffer(lenght + 1);
	LPCTSTR fileEnding = buffer;
	for (int i = 0; i <= lenght; i++) {
		if (buffer[i] == _T(';') || buffer[i] == 0) {
			fileEnding++; // strip the *
			buffer[i] = 0;
			if (!WriteStringValue(_T("Software\\JPEGView\\Capabilities\\FileAssociations"), fileEnding, _T("JPEGViewImageFile"))) {
				return false;
			}
			fileEnding = &(buffer[i + 1]); 
		}
	}
	fileEndings.ReleaseBuffer();

	return true;
}

// Redefined because WINVER is set to Windows XP to make JPEGView running under XP, therefore these interface is not defined 
MIDL_INTERFACE("1f76a169-f994-40ac-8fc8-0959e8874710")
IApplicationAssociationRegistrationUI_ : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE LaunchAdvancedAssociationUI(
		/* [string][in] */ __RPC__in_string LPCWSTR pszAppRegistryName) = 0;

};

void CFileExtensionsRegistrationWindows8::LaunchApplicationAssociationDialog() {
	const GUID CLSID_ApplicationAssociationRegistrationUI_ = { 0x1968106d, 0xf3b5, 0x44cf, { 0x89, 0x0e, 0x11, 0x6f, 0xcb, 0x9e, 0xce, 0xf1 } };

	IApplicationAssociationRegistrationUI_ *applicationAssociationRegistrationUI = NULL;

	::CoCreateInstance(CLSID_ApplicationAssociationRegistrationUI_,
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(IApplicationAssociationRegistrationUI_),
		(LPVOID*)&applicationAssociationRegistrationUI);

	if (applicationAssociationRegistrationUI) {
		applicationAssociationRegistrationUI->AddRef();
		applicationAssociationRegistrationUI->LaunchAdvancedAssociationUI(L"JPEGView");
		applicationAssociationRegistrationUI->Release();
	}
}

bool CFileExtensionsRegistrationWindows8::WriteStringValue(LPCTSTR path, LPCTSTR keyName, LPCTSTR value) {
	if (!ExistsAndHasStringValue(path, keyName, value)) {
		HKEY key;
		if (RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_WRITE, &key) != ERROR_SUCCESS) {
			if (RegCreateKeyEx(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_WRITE | KEY_READ, NULL, &key, NULL) != ERROR_SUCCESS) {
				m_lastFailedRegistryKey = path;
				return false;
			}
		}

		bool canWrite = SetRegistryStringValue(key, keyName, value);
		if (!canWrite)
			m_lastFailedRegistryKey = CString(path) + _T('-') + ((keyName == NULL) ? _T("[Default]") : keyName);

		RegCloseKey(key);

		return canWrite;
	}
	// String value already exists and has the correct value
	return true;
}