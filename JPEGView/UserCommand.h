#pragma once

// Class to hold a user command defined in the INI file
class CUserCommand
{
public:
	// Creates a user command given the INI file definition line
	CUserCommand(int index, const CString & sCommandLine, bool allowNoMenuAssignment);
	~CUserCommand(void);

	// Index of command in INI file (this is not the index in the command list, there are probabely gaps in the index)
	int GetIndex() const { return m_nIndex;  }

	// Returns if the user command is valid, i.e. if the definition is valid
	bool IsValid() const { return m_bValid; }

	// Gets the virtual keycode the user command is bound to
	int GetKeyCode() const { return m_nKeyCode; }

	// Gets the menu item text, null if command is not in the menu
	LPCTSTR MenuItemText() const { return m_sMenuItemText.IsEmpty() ? (LPCTSTR)NULL : m_sMenuItemText; }

	// Returns if the current image and all cached image needs to be reloaded after executing the command
	bool NeedsReloadAll() const { return m_bReloadAll; }

	// Returns if the list of files must be reloaded after executing the command
	bool NeedsReloadFileList() const { return m_bReloadFileList; }

	// Returns if the current image must be reloaded after executing the command
	bool NeedsReloadCurrent() const { return m_bReloadCurrent; }

	// Return if the next image shall be displayed automatically after executing the command
	// Possible usage: E.g. after deleting a file
	bool MoveToNextAfterCommand() const { return m_bMoveToNextAfterCommand; }

	// Gets the key (as text, e.g. TAB) text (displayed in the list when F1 is pressed)
	const CString & HelpKey() const  { return m_sHelpKey; }
	// Gets the explanation text (displayed in the list when F1 is pressed)
	const CString & HelpText() const { return m_sHelpText; }

	// Gets the executable of the command, not replacing any placeholders
	CString GetExecutable() const;

    // Checks if the command can execute by asking the user for confirmation if the command requires this
	bool CanExecute(HWND hWnd, LPCTSTR sFileName) const;

	// Executes the command, the application main window and the current filename are passed
	bool Execute(HWND hWnd, LPCTSTR sFileName) const;

	// Executes the command, the application main window, the current filename and the selection are passed
	bool Execute(HWND hWnd, LPCTSTR sFileName, const CRect& selectionRect) const;

private:
	int m_nIndex;
	bool m_bValid;
	int m_nKeyCode;
	CString m_sCommand;
	CString m_sConfirmMsg;
	CString m_sMenuItemText;
	bool m_bNoWindow;
	bool m_bWaitForProcess;
	bool m_bReloadFileList;
	bool m_bReloadAll;
	bool m_bReloadCurrent;
	bool m_bUseShortFileName;
	bool m_bMoveToNextAfterCommand;
	bool m_bKeepModDate;
	bool m_bUseShellExecute;
	bool m_bIsDeleteCommand;
	CString m_sHelpKey;
	CString m_sHelpText;

	void _ParseCommandline(const CString& sCommandLine, bool removeHypen, CString& sEXE, CString& sParameters, CString& sStartupPath) const;
};
