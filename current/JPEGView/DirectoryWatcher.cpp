#include "StdAfx.h"
#include "DirectoryWatcher.h"
#include "MessageDef.h"
#include <process.h>

/////////////////////////////////////////////////////////////////////////////////////////////
// Static helpers
/////////////////////////////////////////////////////////////////////////////////////////////

static BOOL GetLastModificationTime(LPCTSTR fileName, FILETIME & lastModificationTime)
{
	HANDLE hFile = ::CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != NULL) {
		BOOL bSuccess;
		bSuccess = ::GetFileTime(hFile, NULL, NULL, &lastModificationTime);
		::CloseHandle(hFile);
		return bSuccess;
	}
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Public
/////////////////////////////////////////////////////////////////////////////////////////////

CDirectoryWatcher::CDirectoryWatcher(HWND hTargetWindow) {
	m_hTargetWindow = hTargetWindow;
	memset(&m_lock, 0, sizeof(CRITICAL_SECTION));
	::InitializeCriticalSection(&m_lock);
	m_terminateEvent = ::CreateEvent(0, TRUE, FALSE, NULL);
	m_newDirectoryEvent = ::CreateEvent(0, TRUE, FALSE, NULL);
	m_bModificationTimeValid = FALSE;

	m_hThread = (HANDLE)_beginthread(ThreadFunc, 0, this);
}

CDirectoryWatcher::~CDirectoryWatcher(void) {
	Abort();
	::DeleteCriticalSection(&m_lock);
	::CloseHandle(m_terminateEvent);
	::CloseHandle(m_newDirectoryEvent);
}

void CDirectoryWatcher::Terminate() { 
	m_bTerminate = true;
	if (m_hThread != NULL) {
		::SetEvent(m_terminateEvent);
		::WaitForSingleObject(m_hThread, 1000);
		m_hThread = NULL;
	}
}

void CDirectoryWatcher::Abort() {
	try {
		if (m_hThread != NULL) {
			::SetEvent(m_terminateEvent);
			if (WAIT_TIMEOUT == ::WaitForSingleObject(m_hThread, 100)) {
				::TerminateThread(m_hThread, 1);
				::WaitForSingleObject(m_hThread, 100);
			}
			m_hThread = NULL;
		}
	} catch (...) {
		// do not crash
	}
}

void CDirectoryWatcher::SetCurrentFile(LPCTSTR fileName)
{
	::EnterCriticalSection(&m_lock);
	m_sCurrentFile = fileName;
	m_bModificationTimeValid = !m_sCurrentFile.IsEmpty() && GetLastModificationTime(m_sCurrentFile, m_modificationTimeCurrentFile);
	::LeaveCriticalSection(&m_lock);
}

void CDirectoryWatcher::SetCurrentDirectory(LPCTSTR directoryName)
{
	TCHAR fullName[MAX_PATH];
	memset(fullName, 0, sizeof(TCHAR) * MAX_PATH);
	GetFullPathName(directoryName, MAX_PATH, (LPTSTR)fullName, NULL);

	::EnterCriticalSection(&m_lock);

	m_sCurrentDirectory = fullName;

	::LeaveCriticalSection(&m_lock);

	::SetEvent(m_newDirectoryEvent);
}


/////////////////////////////////////////////////////////////////////////////////////////////
// Private
/////////////////////////////////////////////////////////////////////////////////////////////

void CDirectoryWatcher::ThreadFunc(void* arg) {

	CDirectoryWatcher* thisPtr = (CDirectoryWatcher*) arg;
	bool bTerminate = false;
	bool bSetupNewDirectory = true;
	HANDLE waitHandles[4];
	memset(waitHandles, 0, sizeof(HANDLE) * 4);
	do {
		waitHandles[0] = thisPtr->m_terminateEvent;
		waitHandles[1] = thisPtr->m_newDirectoryEvent;
		int numHandles = bSetupNewDirectory ? 2 : 4;

		::EnterCriticalSection(&thisPtr->m_lock);
		if (bSetupNewDirectory && !thisPtr->m_sCurrentDirectory.IsEmpty()) {
			waitHandles[2] = ::FindFirstChangeNotification(thisPtr->m_sCurrentDirectory, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME);
			if (waitHandles[2] != NULL && waitHandles[2] != INVALID_HANDLE_VALUE)
			{
				numHandles++;
				waitHandles[3] = ::FindFirstChangeNotification(thisPtr->m_sCurrentDirectory, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
				if (waitHandles[3] != NULL && waitHandles[3] != INVALID_HANDLE_VALUE) {
					numHandles++;
				}
			}
		}
		::LeaveCriticalSection(&thisPtr->m_lock);

		// wait for events on file system or wakeup event
		DWORD waitStatus = ::WaitForMultipleObjects(numHandles, waitHandles, FALSE, INFINITE);

		switch (waitStatus) {
			case WAIT_OBJECT_0:
				// m_terminateEvent is set
				bTerminate = true;
				break;
			case WAIT_OBJECT_0 + 1:
				// m_newDirectoryEvent is set
				::ResetEvent(thisPtr->m_newDirectoryEvent);
				bSetupNewDirectory = true;
				break;
			case WAIT_OBJECT_0 + 2:
				// file added or deleted in directory, send message to registered window
				bSetupNewDirectory = ::FindNextChangeNotification(waitHandles[2]) == FALSE;
				::PostMessage(thisPtr->m_hTargetWindow, WM_ACTIVE_DIRECTORY_FILELIST_CHANGED, 0, 0);
				::WaitForSingleObject(thisPtr->m_terminateEvent, 500); // don't flood the window with change notifications
				break;
			case WAIT_OBJECT_0 + 3:
				{
				// file written in directory, check if the displayed file has changed and send message to registered window if yes
				bSetupNewDirectory = ::FindNextChangeNotification(waitHandles[3]) == FALSE;
				
				// wait a short time, otherwise the file may can not be read yet
				::WaitForSingleObject(thisPtr->m_terminateEvent, 250);

				bool sendMessage = false;

				::EnterCriticalSection(&thisPtr->m_lock);
				FILETIME fileTime;
				if (thisPtr->m_bModificationTimeValid) {
					bool canReadModificationTime = true;
					if (!GetLastModificationTime(thisPtr->m_sCurrentFile, fileTime)) {
						::LeaveCriticalSection(&thisPtr->m_lock);
						::WaitForSingleObject(thisPtr->m_terminateEvent, 250);
						::EnterCriticalSection(&thisPtr->m_lock);
						if (!GetLastModificationTime(thisPtr->m_sCurrentFile, fileTime)) {
							canReadModificationTime = false;
						}
					}
					sendMessage = canReadModificationTime && ::memcmp(&fileTime, &thisPtr->m_modificationTimeCurrentFile, sizeof(FILETIME)) != 0;
				}
				::LeaveCriticalSection(&thisPtr->m_lock);

				if (sendMessage) {
					::PostMessage(thisPtr->m_hTargetWindow, WM_DISPLAYED_FILE_CHANGED_ON_DISK, 0, 0);
				}
				break;
				}
			default:
				// unexpected, terminate
				bTerminate = true;
				break;
		}

		if (bTerminate || bSetupNewDirectory) {
			if (waitHandles[2] != NULL && waitHandles[2] != INVALID_HANDLE_VALUE) {
				::FindCloseChangeNotification(waitHandles[2]);
				waitHandles[2] = NULL;
			}
			if (waitHandles[3] != NULL && waitHandles[3] != INVALID_HANDLE_VALUE) { 
				::FindCloseChangeNotification(waitHandles[3]);
				waitHandles[3] = NULL;
			}
		}
	} while (!bTerminate);

	_endthread();
}