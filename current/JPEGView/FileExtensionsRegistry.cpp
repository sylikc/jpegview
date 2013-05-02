#include "StdAfx.h"
#include "FileExtensionsRegistry.h"
#include "Helpers.h"
#include "SettingsProvider.h"

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
    bOk = bOk && type == REG_SZ;
    if (bOk) outValue = buff;
    return bOk;
}

// Sets a string value in the registry, given an open key and the name of the string value to set.
// If the name is NULL, the default value for the key is set.
static bool SetRegistryStringValue(HKEY key, LPCTSTR name, LPCTSTR stringValue) {
    return RegSetValueEx(key, name, 0, REG_SZ, (const BYTE *)stringValue, (_tcslen(stringValue) + 1) * sizeof(TCHAR)) == ERROR_SUCCESS;
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

// Checks if the given extension is registered for JPEGView
static bool CheckExtensionRegistered(LPCTSTR sExtension, bool bNewRegistryFormat, bool & outRegisteredInHKLM) {
    outRegisteredInHKLM = false;
    HKEY key;
    CString sRegKeyPathForExtension = GetRegKeyPathForExtension(sExtension, bNewRegistryFormat);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, sRegKeyPathForExtension, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        CString value;
        bool bOk = GetRegistryStringValue(key, bNewRegistryFormat ? _T("Progid") : _T("Application"), value);
        bOk = bOk && Helpers::stristr(value, _T("JPEGView.exe")) != NULL;
        RegCloseKey(key);
        if (!bOk) {
            // maybe it's globally registered in HKLM
            sRegKeyPathForExtension = sExtension;
            if (RegOpenKeyEx(HKEY_CLASSES_ROOT, sRegKeyPathForExtension, 0, KEY_READ, &key) == ERROR_SUCCESS) {
                if (GetRegistryStringValue(key, NULL, value)) {
                    sRegKeyPathForExtension = value + _T("\\shell\\open\\command");
                    HKEY subKey;
                    if (RegOpenKeyEx(HKEY_CLASSES_ROOT, sRegKeyPathForExtension, 0, KEY_READ, &subKey) == ERROR_SUCCESS) {
                        bOk = GetRegistryStringValue(subKey, NULL, value);
                        bOk = bOk && Helpers::stristr(value, CSettingsProvider::This().GetEXEPath()) != NULL;
                        if (bOk) outRegisteredInHKLM = true;
                        RegCloseKey(subKey);
                    }
                }
                RegCloseKey(key);
            }
        }
        return bOk;
    }
    return false;
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

// Registers the given extension for JPEGView
static RegResult RegisterExtension(LPCTSTR sExtension, bool bNewRegistryFormat, bool bChangeDACLIfRequired) {
    CString sRegKeyPathForExtension = GetRegKeyPathForExtension(sExtension, bNewRegistryFormat);

    HKEY key;
    bool bCanWriteKey = (RegCreateKeyEx(HKEY_CURRENT_USER, sRegKeyPathForExtension, 0, NULL, 0, KEY_WRITE | KEY_READ, NULL, &key, NULL) == ERROR_SUCCESS);
    if (!bCanWriteKey) {
        if (ExistsKey(sRegKeyPathForExtension)) {
            // no rights to write to the key
            if (!bChangeDACLIfRequired) {
                return Reg_WarningRequiresAdminRights;
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
        return bOk ? Reg_Success : Reg_ErrorWriteKey;
    }
    return Reg_ErrorWriteKey;
}

static RegResult UnregisterExtension(LPCTSTR sExtension, bool bNewRegistryFormat, bool bChangeDACLIfRequired) {
    bool bRegisteredInHKLM;
    bool bRegistered = CheckExtensionRegistered(sExtension, bNewRegistryFormat, bRegisteredInHKLM);
    if (!bRegistered || bRegisteredInHKLM) {
        return Reg_Success;
    }

    CString sRegKeyPathForExtension = GetRegKeyPathForExtension(sExtension, bNewRegistryFormat);

    HKEY key;
    bool bCanWriteKey = (RegOpenKeyEx(HKEY_CURRENT_USER, sRegKeyPathForExtension, 0, DELETE | KEY_WRITE | KEY_READ, &key) == ERROR_SUCCESS);
    if (!bCanWriteKey) {
        if (ExistsKey(sRegKeyPathForExtension)) {
            // no rights to write to the key
            if (!bChangeDACLIfRequired) {
                return Reg_WarningRequiresAdminRights;
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
        return Reg_ErrorNoAdminRights;
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
    // Starting with Windows Vista, the Windows Explorer uses a different location
    // to register file extensions
    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);

    m_bNewRegistryFormat = osvi.dwMajorVersion >= 6;
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
    int nNumChars = _tcslen(sExtensionList);
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
    int nNumChars = _tcslen(sExtension);
    if (nNumChars >= 3) {
        CString sExtensionStripped(&((LPCTSTR)sExtension)[1]);
        sExtensionStripped.MakeLower();
        if (sExtensionStripped.GetAt(0) == _T('.')) {
            return RegisterExtension(sExtensionStripped, m_bNewRegistryFormat, bChangeDACLIfRequired);
        }
    }
    return Reg_Internal;
}

RegResult CFileExtensionsRegistry::UnregisterFileExtension(LPCTSTR sExtension, bool bChangeDACLIfRequired) {
    int nNumChars = _tcslen(sExtension);
    if (nNumChars >= 3) {
        CString sExtensionStripped(&((LPCTSTR)sExtension)[1]);
        sExtensionStripped.MakeLower();
        if (sExtensionStripped.GetAt(0) == _T('.')) {
            return UnregisterExtension(sExtensionStripped, m_bNewRegistryFormat, bChangeDACLIfRequired);
        }
    }
    return Reg_Internal;
}
