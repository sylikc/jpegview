
#pragma once

#include "ProcessParams.h"
#include "WorkThread.h"
#include <gdiplus.h>

class CJPEGImage;

// returned image data by CImageLoadThread.GetLoadedImage
class CImageData
{
public:
	// Gets the image
	CJPEGImage* Image;
	// True if the request failed due to memory
	bool IsRequestFailedOutOfMemory;

	CImageData(CJPEGImage* pImage, bool isRequestFailedOutOfMemory) {
		Image = pImage;
		IsRequestFailedOutOfMemory = isRequestFailedOutOfMemory;
	}
};

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
	// The file to load is given by its filename (with path) and the frame index (for multiframe images). The
	// frame index is zero when the image only has one frame.
	int AsyncLoad(LPCTSTR strFileName, int nFrameIndex, const CProcessParams & processParams, HWND targetWnd, HANDLE eventFinished);

	// Get loaded image, image is null if not (yet) available - use handle returned by AsyncLoad()
	// Marks the request for deletion - only call once with given handle
	CImageData GetLoadedImage(int nHandle);

	// Releases the cached image file if an image of the given name is cached
	void ReleaseFile(LPCTSTR strFileName);

	// Gets the request handle value used for the last request
	static LONG GetCurHandleValue() { return m_curHandle; }

private:

	// Request in request list
	class CRequest : public CRequestBase {
	public:
		CRequest(LPCTSTR strFileName, int nFrameIndex, HWND wndTarget, const CProcessParams& processParams, HANDLE eventFinished) 
			: CRequestBase(eventFinished), ProcessParams(processParams) {
			FileName = strFileName;
			FrameIndex = nFrameIndex;
			TargetWnd = wndTarget;
			RequestHandle = ::InterlockedIncrement(&m_curHandle);
			Image = NULL;
			OutOfMemory = false;
		}

		CString FileName;
		int FrameIndex;
		HWND TargetWnd;
		int RequestHandle;
		CJPEGImage* Image;
		CProcessParams ProcessParams;
		bool OutOfMemory;
	};

	// Request to release image file
	class CReleaseFileRequest : public CRequestBase {
	public:
		CReleaseFileRequest(LPCTSTR strFileName) : CRequestBase() {
			FileName = strFileName;
			Type = ReleaseFileRequest;
		}

		enum { ReleaseFileRequest = 1 };

		CString FileName;
	};

	static volatile LONG m_curHandle;

	Gdiplus::Bitmap* m_pLastBitmap;
	CString m_sLastFileName;

	virtual void ProcessRequest(CRequestBase& request);
	virtual void AfterFinishProcess(CRequestBase& request);
	void DeleteCachedGDIBitmap();

	void ProcessReadJPEGRequest(CRequest * request);
	void ProcessReadBMPRequest(CRequest * request);
	void ProcessReadTGARequest(CRequest * request);
	void ProcessReadWEBPRequest(CRequest * request);
	void ProcessReadRAWRequest(CRequest * request);
	void ProcessReadGDIPlusRequest(CRequest * request);
	void ProcessReadWICRequest(CRequest* request);

	static void SetFileDependentProcessParams(CRequest * request);
	static bool ProcessImageAfterLoad(CRequest * request);
};
