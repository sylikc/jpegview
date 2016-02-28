#pragma once

// Watcher thread for a directory. Sends the two messages WM_DISPLAYED_FILE_CHANGED_ON_DISK and WM_ACTIVE_DIRECTORY_FILELIST_CHANGED
// to the given window.
class CDirectoryWatcher {
public:
	// The messages are sent to the given window.
	CDirectoryWatcher(HWND hTargetWindow);
	virtual ~CDirectoryWatcher(void);

	void SetCurrentFile(LPCTSTR fileName);
	void SetCurrentDirectory(LPCTSTR directoryName);

	void Terminate();
	void Abort();

private:
	CRITICAL_SECTION m_lock;
	HANDLE m_terminateEvent; // Event to terminate the thread
	HANDLE m_newDirectoryEvent; // New directory needs to be watched
	HANDLE m_hThread; // watchdog thread
	HWND m_hTargetWindow;
	bool m_bTerminate;

	CString m_sCurrentDirectory;
	CString m_sCurrentFile;
	BOOL m_bModificationTimeValid;
	FILETIME m_modificationTimeCurrentFile;

	static void  __cdecl ThreadFunc(void* arg);
};
