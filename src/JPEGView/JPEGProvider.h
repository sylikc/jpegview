#pragma once

class CJPEGImage;
class CImageLoadThread;
class CFileList;
class CProcessParams;

// Class that reads and processes image files (not only JPEG, any supported format) using read ahead with
// additional read ahead threads (typically only one).
class CJPEGProvider
{
public:
	// hint for caching - read ahead or read backwards
	enum EReadAheadDirection {
		NONE, // Do not create a read-ahead request
		FORWARD,
		BACKWARD,
		TOGGLE // toggle between two images
	};

	// handlerWnd: Window to send the asynchronous message when an image has finished loading (WM_IMAGE_LOAD_COMPLETED)
	// nNumThreads: Number of read ahead threads to start (should be 1)
	// nNumBuffers: Number of buffers to use (in most situations nNumThreads+1 is a good choice)
	CJPEGProvider(HWND handlerWnd, int nNumThreads, int nNumBuffers);
	~CJPEGProvider(void);

	// Read and process the specified image file.
	// Parameters
	// pFileList: List of files, required for read-ahead. If NULL, no read-ahead will be done.
	// eDirection: Read-ahead direction.
	// strFileName: File name (with path) of the image to load and process. Typically pFileList->Current().
	// nFrameIndex: Index of frame for multiframe images, zero otherwise.
	// processParams: Parameters for processing the image.
	// Return value
	// Returns the processed JPEG image.
	// The caller must not delete the retuned image but notify the provider when no longer used with NotifyNotUsed()
	// The CJPEGProvider class then decides whether to delete or to cache the image.
	// Remarks
	// When the requested image is already cached, it is returned immediately. Otherwise the method
	// blocks until the image is ready. If not specified otherwise, a read-ahead request for the next image is
	// created automatically so that the next image will be ready immediately when requested in the future.
	CJPEGImage* RequestImage(CFileList* pFileList, EReadAheadDirection eDirection, LPCTSTR strFileName, int nFrameIndex,
		const CProcessParams & processParams, bool& bOutOfMemory);

	// Notifies that the specified image is no longer used and its memory can be freed.
	// The CJPEGProvider class may decide to keep the image cached.
	// In all cases, accessing the image after having called this method may causes access violation.
	void NotifyNotUsed(CJPEGImage* pImage);

	// Clears the request for the given image from the request queue.
	// This forces reloading of the image next time it is requested.
	// The passed image is deleted and must not be accessed anymore.
	// Returns if the request was erased or just marked as deleted
	bool ClearRequest(CJPEGImage* pImage, bool releaseLockedFile = false);

	// Clears all requests by marking them as invalid - forces reloading of all images next time they are requested
	// Any references to CJPEGImage objects get invalid by this call.
	void ClearAllRequests();

	// Free as much memory as possible by clearing all requests that are not in use. Returns if some memory could be freed.
	bool FreeAllPossibleMemory();

	// Tells the provider that a file has been renamed externally so that pending requests to read this file can be updated.
	void FileHasRenamed(LPCTSTR sOldFileName, LPCTSTR sNewFileName);

	// Must be called by the message handler window (see constructor) when the WM_IMAGE_LOAD_COMPLETED
	// message was received.
	void OnImageLoadCompleted(int nHandle);

private:
	// stores a request for loading and processing a JPEG image
	struct CImageRequest {
		CString FileName; // file name with path of image
		int FrameIndex; // zero based frame index for multiframe images
		CJPEGImage* Image; // loaded image
		bool Ready; // true if the request has finished loading and the image is ready (if NULL, loading failed)
		int Handle; // request handle used for that request in ImageLoadThread
		bool InUse; // true if the Image is currently in use and thus the request is locked for deletion
		bool Deleted; // true if the request is marked for deletion (but cannot be deleted now as it is not ready)
		bool IsActive; // true if this request is active (i.e. requested but not ready or ready and in use).
		bool OutOfMemory; // true if the image failed loading due to out of memory
		int AccessTimeStamp; // LRU handling
		CImageLoadThread* HandlingThread; // thread that is loading the image, NULL when image is ready
		HANDLE EventFinished; // event fired when image has finished loading

		CImageRequest(LPCTSTR fileName, int nFrameIndex) {
			FileName = fileName;
			FrameIndex = nFrameIndex;
			Image = NULL;
			Ready = false;
			Handle = -1;
			InUse = false;
			Deleted = false;
			IsActive = true;
			OutOfMemory = false;
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
	int m_nNumThread; // number of threads in m_pWorkThreads
	int m_nNumBuffers;
	int m_nCurrentTimeStamp;
	EReadAheadDirection m_eOldDirection;

	bool WaitForAsyncRequest(int nHandle, int nMessage);
	void GetLoadedImageFromWorkThread(CImageRequest* pRequest);
	CImageLoadThread* SearchThreadForNewRequest(void);
	void RemoveUnusedImages(bool bRemoveAlsoReadAhead);
	CImageRequest* StartRequestAndWaitUntilReady(LPCTSTR sFileName, int nFrameIndex, const CProcessParams & processParams);
	CImageRequest* StartNewRequest(LPCTSTR sFileName, int nFrameIndex, const CProcessParams & processParams);
	void StartNewRequestBundle(CFileList* pFileList, EReadAheadDirection eDirection, const CProcessParams & processParams, int nNumRequests, CImageRequest* pLastReadyRequest);
	CImageRequest* FindRequest(LPCTSTR strFileName, int nFrameIndex);
	void ClearOldestInactiveRequest();
	void DeleteElementAt(std::list<CImageRequest*>::iterator iteratorAt); // also deletes the request and the image in the request
	void DeleteElement(CImageRequest* pRequest);
	bool IsDestructivelyProcessed(CJPEGImage* pImage);
};
