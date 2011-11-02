
#pragma once

#include "ProcessParams.h"
#include "WorkThread.h"

class CJPEGImage;

// Read ahead and processing thread
class CImageLoadThread : public CWorkThread
{
public:
	CImageLoadThread(void);
	~CImageLoadThread(void);

	// Request asynchronous load of JPEG, the message WM_JPEG_LOAD_COMPLETED is
	// posted to the given window's message queue when finished and the given event is signaled (if not NULL).
	// Returns identifier to query for resulting image. The query can be done as soon as the message is
	// received or the event has been signaled.
	int AsyncLoad(LPCTSTR strFileName, const CProcessParams & processParams, HWND targetWnd, HANDLE eventFinished);

	// Get loaded image, null if not (yet) available - use handle returned by AsyncLoad()
	// Marks the request for deletion - only call once with given handle
	CJPEGImage* GetLoadedImage(int nHandle);

	// Gets if the request failed because not enough memory is available
	bool IsRequestFailedOutOfMemory(int nHandle);

	// Gets the request handle value used for the last request
	static LONG GetCurHandleValue() { return m_curHandle; }

private:

	// Request in request list
	class CRequest : public CRequestBase {
	public:
		CRequest(LPCTSTR strFileName, HWND wndTarget, const CProcessParams& processParams, HANDLE eventFinished) 
			: CRequestBase(eventFinished), ProcessParams(processParams) {
			FileName = strFileName;
			TargetWnd = wndTarget;
			RequestHandle = ::InterlockedIncrement(&m_curHandle);
			Image = NULL;
			OutOfMemory = false;
		}

		CString FileName;
		HWND TargetWnd;
		int RequestHandle;
		CJPEGImage* Image;
		CProcessParams ProcessParams;
		bool OutOfMemory;
	};

	static volatile LONG m_curHandle;

	virtual void ProcessRequest(CRequestBase& request);
	virtual void AfterFinishProcess(CRequestBase& request);

	static void ProcessReadJPEGRequest(CRequest * request);
	static void ProcessReadBMPRequest(CRequest * request);
	static void ProcessReadWEBPRequest(CRequest * request);
	static void ProcessReadGDIPlusRequest(CRequest * request);
	static void SetFileDependentProcessParams(CRequest * request);
	static bool ProcessImageAfterLoad(CRequest * request);
};
