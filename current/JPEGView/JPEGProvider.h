#pragma once

class CJPEGImage;
class CImageLoadThread;
class CFileList;
class CProcessParams;

// Class that reads and processes JPEG files using read ahead with own threads
class CJPEGProvider
{
public:
	// hint for caching - read ahead or opposite
	enum EReadAheadDirection {
		FORWARD,
		BACKWARD
	};

	// Window handle is needed to send the asynchronous messages (e.g. WM_JPEG_LOAD_COMPLETED)
	// nNumThreads: Number of read ahead threads to start (should be 1)
	// nNumBuffers: Number of buffers to use (in most situations nNumThreads+1 is a good choice)
	CJPEGProvider(HWND handlerWnd, int nNumThreads, int nNumBuffers);
	~CJPEGProvider(void);

	// Read and process the JPEG file, returns the JPEG image.
	// The caller must not delete this image but notify the provider when no longer used with NotifyNotUsed()
	// The JPEGProvider then can decide whether to delete or to cache the object.
	// strFileName is normally pFileList->Current(), however it is possible to request other files and
	// set the pFileList parameter to NULL to prevent read ahead.
	CJPEGImage* RequestJPEG(CFileList* pFileList, EReadAheadDirection eDirection, LPCTSTR strFileName, 
		const CProcessParams & processParams);

	// Notifies that this image is no longer used. The provider may decides to keep the image.
	// In all cases, accessing the image after having called this method is illegal.
	void NotifyNotUsed(CJPEGImage* pImage);

	// Clears the request for the given image from the request queue.
	// This forces reloading of the image next time it is requested.
	// The passed image is deleted and must not be accessed anymore.
	// Returns if the request was erased or just marked as deleted
	bool ClearRequest(CJPEGImage* pImage);

	// Clears all requests (mark them as invalid) - forces reloading of all images next time they are requested
	// Any references to CJPEGImage objects get invalid by this call.
	void ClearAllRequests();

	// Tells the provider that a file has been renamed externally
	void FileHasRenamed(LPCTSTR sOldFileName, LPCTSTR sNewFileName);

	// Must be called by the message handler window (see constructor) when the WM_JPEG_LOAD_COMPLETED
	// message was received
	void OnJPEGLoaded(int nHandle);

private:
	// stores a request for loading and processing a JPEG image
	struct CImageRequest {
		CString FileName;
		CJPEGImage* Image;
		bool Ready;
		int Handle;
		bool InUse;
		bool Deleted;
		bool ReadAhead;
		int AccessTimeStamp;
		CImageLoadThread* HandlingThread;
		HANDLE EventFinished;

		CImageRequest(LPCTSTR fileName) {
			FileName = fileName;
			Image = NULL;
			Ready = false;
			Handle = -1;
			InUse = false;
			Deleted = false;
			ReadAhead = false;
			AccessTimeStamp = -1;
			HandlingThread = NULL;
			EventFinished = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		}

		~CImageRequest() {
			::CloseHandle(EventFinished);
			EventFinished = NULL;
		}
	};

	std::list<CImageRequest*> m_requestList;
	HWND m_hHandlerWnd;
	CImageLoadThread** m_pWorkThreads;
	int m_nNumThread;
	int m_nNumBuffers;
	int m_nCurrentTimeStamp;
	EReadAheadDirection m_eOldDirection;

	bool WaitForAsyncRequest(int nHandle, int nMessage);
	void GetLoadedImageFromWorkThread(CImageRequest* pRequest);
	CImageLoadThread* SearchThread(void);
	void RemoveUnusedImages(bool bRemoveAlsoReadAhead);
	CImageRequest* StartNewRequest(LPCTSTR sFileName, const CProcessParams & processParams);
	void StartNewRequestBundle(CFileList* pFileList, EReadAheadDirection eDirection, const CProcessParams & processParams, int nNumRequests);
	CImageRequest* FindRequest(LPCTSTR strFileName);
	void ClearOldestReadAhead();
};
