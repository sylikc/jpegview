#pragma once

enum RegResult {
    Reg_Success,
    Reg_WarningRequiresAdminRights,
    Reg_ErrorNoAdminRights,
    Reg_ErrorWriteKey,
    Reg_ErrorChangeDACL,
    Reg_Internal
};

// Class to support registring file extensions in the Windows registry to be opened by JPEGView
class CFileExtensionsRegistry
{
public:
    CFileExtensionsRegistry();

    // Checks if JPEGView is registered in HKEY_CURRENT_USER\Software\Classes\Applications
    bool IsJPEGViewRegistered() { return m_bIsJPEGViewRegistered; }

    // Register JPEGView in HKEY_CURRENT_USER\Software\Classes\Applications
    bool RegisterJPEGView();
    
    // Checks if all given list of extensions in the form "*.ext1;*.ext2" are opened by JPEGView
    // outRegisteredInHKLM contains true if it's registered to JPEGView but in HKLM registry root, not in HKCU
    bool CheckIfOpenedByJPEGView(LPCTSTR sExtensionList, bool& outRegisteredInHKLM);

    // Registers JPEGView as default viewer for the given extension in the form "*.ext1"
    RegResult RegisterFileExtension(LPCTSTR sExtension, bool bChangeDACLIfRequired);

    // Unregisters JPEGView as default viewer for the given extension in the form "*.ext1"
    RegResult UnregisterFileExtension(LPCTSTR sExtension, bool bChangeDACLIfRequired);

private:
    bool m_bNewRegistryFormat;
    bool m_bIsJPEGViewRegistered;
};