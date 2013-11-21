#pragma once

enum RegResult {
    Reg_Success,
    Reg_WarningNeedsChangeDACL,  // DACL needs to be changed to write the enty
    Reg_ErrorNeedsWriteDACLRights, // Right to change DACL is not present
    Reg_ErrorWriteKey, // Error writing the key
    Reg_ErrorChangeDACL, // Error changing the DACL
	Reg_ErrorProtectedByHash, // Key is protected by kryptographic hash (Windows 8 only)
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
	bool m_bIsWindows8;
    bool m_bIsJPEGViewRegistered;
};