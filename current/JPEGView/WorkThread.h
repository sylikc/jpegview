#pragma once

#include "ProcessParams.h"

class CJPEGImage;

// Read ahead and processing thread
class CWorkThread
{
public:
	CWorkThread(void);
	~CWorkThread(void);

	// Request asynchronous load of JPEG, the message WM_JPEG_LOAD_COMPLETED is
	// posted to the given window's message queue when finished and the given event is signaled (if not NULL).
	// Returns identifier to query for resulting image. The query can be done as soon as the message is
	// received or the event has been signaled.
	int AsyncLoad(LPCTSTR strFileName, const CProcessParams & processParams, HWND targetWnd, HANDLE eventFinished);

	// Get loaded image, null if not (yet) available - use handle returned by AsyncLoad()
	// Marks the request for deletion - only call once with given handle
	CJPEGImage* GetLoadedImage(int nHandle);

	// Clean termination, reads the current request to the end
	void Terminate();

	// Aborts the thread immediatly. Leaves an undefined state - only call on process termination.
	void Abort();

	// Gets the request handle value used for the last request
	static LONG GetCurHandleValue() { return m_curHandle; }

private:

	// Request in request list
	struct CRequest {
		CRequest(LPCTSTR strFileName, HWND wndTarget, const CProcessParams& processParams, HANDLE eventFinished) 
			: ProcessParams(processParams) {
			FileName = strFileName;
			TargetWnd = wndTarget;
			RequestHandle = ::InterlockedIncrement(&m_curHandle);
			Image = NULL;
			Processed = false;
			EventFinished = eventFinished;
			Deleted = false;
		}

		CString FileName;
		HWND TargetWnd;
		int RequestHandle;
		CJPEGImage* Image;
		CProcessParams ProcessParams;
		bool Processed;
		HANDLE EventFinished;
		bool Deleted;
	};

	std::list<CRequest*> m_requestList;
	CRITICAL_SECTION m_csList; // the critical section protecting the list above
	HANDLE m_wakeUp; // wake up event for the tread (it sleeps while there is nothing to process)
	HANDLE m_hThread; // working thread
	volatile bool m_bTerminate; // flags termination for thread
	static volatile LONG m_curHandle;

	static void  __cdecl ThreadFunc(void* arg);
	static void ProcessReadJPEGRequest(CRequest * request);
	static void ProcessReadBMPRequest(CRequest * request);
	static void ProcessReadGDIPlusRequest(CRequest * request);
	static void SetFileDependentProcessParams(CRequest * request);
	static void DeleteMarkedRequests(CWorkThread* thisPtr);
	static unsigned char* SearchEXIFAPP1Block(unsigned char* pJPEGStream);
	static void ProcessImageAfterLoad(CRequest * request);
};
