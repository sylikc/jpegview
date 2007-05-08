#include "StdAfx.h"
#include "UserCommand.h"
#include "NLS.h"
#include <shlobj.h>
#include <shellapi.h>

//////////////////////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////////////////////

// Extracts from a string the part that is between two appostrophs e.g. "text" from "command:'text'"
static CString ParseString(const CString & sSource, int nStartIdx) {
	int nStart = sSource.Find(_T('\''), nStartIdx);
	if (nStart < 0) {
		return CString();
	}
	int nEnd = sSource.Find(_T('\''), nStart + 1);
	if (nEnd < 0) {
		return CString();
	}
	return sSource.Mid(nStart + 1, nEnd - nStart - 1);
}

// Replaces file name and adds quotes if needed (if there is no \ before or after)
static void SmartNameReplace(CString& strSource, LPCTSTR sOldText, LPCTSTR sNewText) {
	int nIndex = strSource.Find(sOldText);
	if (nIndex >= 0) {
		int nOldTextLen = _tcslen(sOldText);
		bool bBackSlashBefore = (nIndex  > 0 && strSource.GetAt(nIndex - 1) == _T('\\'));
		bool bBackSlashAfter = (nIndex+nOldTextLen < strSource.GetLength()) && (strSource.GetAt(nIndex+nOldTextLen) == _T('\\'));
		if (!bBackSlashBefore && !bBackSlashAfter) {
			strSource.Replace(sOldText, CString(_T("\"")) + sNewText + _T("\""));
		} else {
			strSource.Replace(sOldText, sNewText);
		}
	}
}

// replaces the defined placeholder strings in the input string
static CString ReplacePlaceholders(CString sMsg, LPCTSTR sFileName, bool bUseShortFileName) {
	const int BUFFER_LEN = MAX_PATH;
	TCHAR buffer[BUFFER_LEN];
	CString sNewMsg = sMsg;

	if (bUseShortFileName) {
		::GetShortPathName(sFileName, (LPTSTR)&buffer, BUFFER_LEN);
		sFileName = (LPCTSTR)&buffer;
	}

	sNewMsg.Replace(_T("%filename%"), CString(_T("\"")) + sFileName + _T("\""));
	LPCTSTR sFileTitle = _tcsrchr(sFileName, _T('\\'));
	if (sFileTitle == NULL) {
		sFileTitle = sFileName;
	} else {
		sFileTitle++;
	}
	SmartNameReplace(sNewMsg, _T("%filetitle%"), sFileTitle);
	if (sMsg.Find(_T("%directory%")) >= 0) {
		CString sDirectory = sFileName;
		sDirectory = sDirectory.Mid(0, sFileTitle - sFileName - 1);
		if (sDirectory.IsEmpty()) {
			sDirectory = _T("."); // current dir
		}
		SmartNameReplace(sNewMsg, _T("%directory%"), sDirectory);
	}
	if (sMsg.Find(_T("%mydocuments%")) >= 0) {
		TCHAR buff[MAX_PATH];
		::SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, buff);
		SmartNameReplace(sNewMsg, _T("%mydocuments%"), buff);
	}
	if (sMsg.Find(_T("%mypictures%")) >= 0) {
		TCHAR buff[MAX_PATH];
		::SHGetFolderPath(NULL, CSIDL_MYPICTURES, NULL, SHGFP_TYPE_CURRENT, buff);
		SmartNameReplace(sNewMsg, _T("%mypictures%"), buff);
	}
	return sNewMsg;
}

//////////////////////////////////////////////////////////////////////////////////////
// CUserCommand
//////////////////////////////////////////////////////////////////////////////////////

CUserCommand::CUserCommand(const CString & sCommandLine) {
	CString sCmdLineLower = sCommandLine;
	sCmdLineLower.MakeLower();
	LPCTSTR lpCmdLine = (LPCTSTR)sCommandLine;

	m_bValid = false;
	m_bWaitForProcess = false;
	m_bReloadFileList = false;
	m_bReloadAll = false;
	m_bReloadCurrent = false;
	m_bUseShortFileName = false;
	m_bMoveToNextAfterCommand = false;
	m_bNoWindow = false;
	m_bKeepModDate = false;
	m_bUseShellExecute = false;
	m_nKeyCode = -1;

	// Keycode (virtual keycode)
	int nIdxKeyCode = sCmdLineLower.Find(_T("keycode:"));
	if (nIdxKeyCode < 0) {
		return;
	}
	TCHAR keyChar[2];
	if (_stscanf(&(lpCmdLine[nIdxKeyCode + 8]), _T("%1s"), &keyChar) != 1) {
		return;
	}
	if (keyChar[0] >= _T('A') && keyChar[0] <= _T('Z')) {
		m_nKeyCode = (int)keyChar[0];
	} else {
		if (_stscanf(&(lpCmdLine[nIdxKeyCode + 8]), _T("%d"), &m_nKeyCode) != 1) {
			return;
		}
		if (m_nKeyCode < 0 || m_nKeyCode > 255) {
			return;
		}
	}

	// Command to execute
	int nIdxCmd = sCmdLineLower.Find(_T("cmd:"));
	if (nIdxCmd < 0) {
		return;
	}
	m_sCommand = ParseString(sCommandLine, nIdxCmd + 4);
	if (m_sCommand.GetLength() == 0) {
		return;
	}

	// Confirm message (optional)
	int nIdxConfirm = sCmdLineLower.Find(_T("confirm:"));
	if (nIdxConfirm >= 0) {
		m_sConfirmMsg = ParseString(sCommandLine, nIdxConfirm + 8);
	}

	// Help text (optional)
	int nIdxHelpText = sCmdLineLower.Find(_T("helptext:"));
	if (nIdxHelpText >= 0) {
		CString sHelp = ParseString(sCommandLine, nIdxHelpText + 9);
		sHelp.Replace(_T("\\t"), _T("\t"));
		int nIndex = sHelp.Find(_T('\t'));
		int nIndex2 = sHelp.ReverseFind(_T('\t'));
		m_sHelpKey = sHelp.Left(max(0, nIndex));
		m_sHelpText = sHelp.Right(sHelp.GetLength() - nIndex2 - 1);
	}

	// Flags (optional)
	int nIdxFlags = sCmdLineLower.Find(_T("flags:"));
	if (nIdxFlags >= 0) {
		CString sFlags = ParseString(sCommandLine, nIdxFlags + 6);
		sFlags.MakeLower();
		if (sFlags.Find(_T("waitforterminate")) >= 0) {
			m_bWaitForProcess = true;
		}
		if (sFlags.Find(_T("reloadfilelist")) >= 0) {
			m_bReloadFileList = true;
		}
		if (sFlags.Find(_T("reloadcurrent")) >= 0) {
			m_bReloadCurrent = true;
		}
		if (sFlags.Find(_T("reloadall")) >= 0) {
			m_bReloadAll = true;
		}
		if (sFlags.Find(_T("shortfilename")) >= 0) {
			m_bUseShortFileName = true;
		}
		if (sFlags.Find(_T("movetonext")) >= 0) {
			m_bMoveToNextAfterCommand = true;
		}
		if (sFlags.Find(_T("nowindow")) >= 0) {
			m_bNoWindow = true;
		}
		if (sFlags.Find(_T("keepmoddate")) >= 0) {
			m_bKeepModDate = true;
		}
		if (sFlags.Find(_T("shellexecute")) >= 0) {
			m_bUseShellExecute = true;
		}
	}

	m_bValid = true;
}

CUserCommand::~CUserCommand(void) {
}

bool CUserCommand::Execute(HWND hWnd, LPCTSTR sFileName) const {
	if (!m_bValid || sFileName == NULL) {
		return false;
	}
	if (m_sConfirmMsg.GetLength() > 0) {
		CString sMsg = CNLS::GetString(m_sConfirmMsg);
		sMsg.Replace(_T("\\n"), _T("\n"));
		sMsg = ReplacePlaceholders(sMsg, sFileName, false);
		if (IDYES != ::MessageBox(hWnd, sMsg, CNLS::GetString(_T("Confirm")), MB_YESNOCANCEL | MB_ICONWARNING)) {
			return false;
		}
	}

	// Get last write time if we need to restore it
	BOOL bRestoreWriteTime = FALSE;
	FILETIME lastWriteTime;
	if (m_bKeepModDate) {
		HANDLE hFile = ::CreateFile(sFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			bRestoreWriteTime = ::GetFileTime(hFile, NULL, NULL, &lastWriteTime);
			::CloseHandle(hFile);
		}
	}

	CString sCommandLine = ReplacePlaceholders(m_sCommand, sFileName, m_bUseShortFileName);
	bool bSuccess = true;
	if (m_bUseShellExecute) {
		CString sEXE, sParameters, sStartupPath;
		_ParseCommandline(sCommandLine, sEXE, sParameters, sStartupPath);
		int nRetVal = (int)::ShellExecute(hWnd, _T("open"), sEXE, sParameters, sStartupPath, SW_SHOW);
		bSuccess = nRetVal > 32;
	} else {
		STARTUPINFO startupInfo;
		memset(&startupInfo, 0, sizeof(STARTUPINFO));
		startupInfo.cb = sizeof(STARTUPINFO);
		PROCESS_INFORMATION processInfo;
		memset(&processInfo, 0, sizeof(PROCESS_INFORMATION));

		bool bIsCmdProcess = (_tcsnicmp(m_sCommand, _T("cmd "), 4) == 0) || (_tcsnicmp(m_sCommand, _T("cmd.exe"), 7) == 0); 
		if (::CreateProcess(NULL, sCommandLine.GetBuffer(512), NULL, NULL, FALSE, (bIsCmdProcess || m_bNoWindow) ? CREATE_NO_WINDOW : 0, NULL,
			NULL, &startupInfo, &processInfo)) {
			sCommandLine.ReleaseBuffer();
			if (m_bWaitForProcess) {
				::WaitForSingleObject(processInfo.hProcess, INFINITE);
			}
			::CloseHandle(processInfo.hProcess);
			::CloseHandle(processInfo.hThread);
		} else {
			sCommandLine.ReleaseBuffer();
			bSuccess = false;
		}
	}

	// Restore last write time if we need to restore it
	if (m_bKeepModDate && bRestoreWriteTime) {
		HANDLE hFile = ::CreateFile(sFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			::SetFileTime(hFile, NULL, NULL, &lastWriteTime);
			::CloseHandle(hFile);
		}
	}

	return bSuccess;
}

void CUserCommand::_ParseCommandline(const CString& sCommandLine, 
									 CString& sEXE, CString& sParameters, CString& sStartupPath) const {
	if (sCommandLine.GetLength() == 0) {
		return;
	}
	int nIndex = 0;
	if (sCommandLine[0] == _T('"')) {
		nIndex = sCommandLine.Find(_T('"'), 1);
		if (nIndex < 0) {
			nIndex = sCommandLine.GetLength();
		}
		sEXE = CString((LPCTSTR)sCommandLine + 1, nIndex - 1);
	} else {
		nIndex = sCommandLine.Find(_T(' '));
		if (nIndex < 0) {
			nIndex = sCommandLine.GetLength();
		}
		sEXE = CString((LPCTSTR)sCommandLine, nIndex);
	}
	nIndex++;
	if (nIndex < sCommandLine.GetLength()) {
		sParameters = CString((LPCTSTR)sCommandLine + nIndex);
		// remove enclosing hypens, some programs do not like this for parameters
		if (sParameters[0] == _T('"') && sParameters[sParameters.GetLength()-1] == _T('"')) {
			sParameters = sParameters.Mid(1, sParameters.GetLength()-2);
		}
	} else {
		sParameters = _T("");
	}
	nIndex = sEXE.ReverseFind(_T('\\'));
	if (nIndex < 0) {
		sStartupPath = _T("");
	} else {
		sStartupPath = CString((LPCTSTR)sEXE, nIndex+1);
	}
}
