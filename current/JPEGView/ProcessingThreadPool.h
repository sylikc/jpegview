#pragma once

#include "WorkThread.h"

class CProcessingThread;

// Request for performing an image processing operation parallel on all thread pool threads
class CProcessingRequest : public CRequestBase {
public:
	CProcessingRequest(const void* pSourcePixels, CSize sourceSize, void* pTargetPixels,
		CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize) : CRequestBase(NULL) {
		SourcePixels = pSourcePixels;
		SourceSize = sourceSize;
		TargetPixels = pTargetPixels;
		FullTargetSize = fullTargetSize;
		FullTargetOffset = fullTargetOffset;
		ClippedTargetSize = clippedTargetSize;
		StripPadding = 8;
		Success = true;
	}

	// Process one strip of the image, starting at row offsetY and having the specified y-size
	virtual bool ProcessStrip(int offsetY, int sizeY) = 0;

	// Geometrical parameters and pixels of the image to be processed.
	// This is the full size image, ProcessStrip() passes the strip to be processed.
	CSize SourceSize;
	const void* SourcePixels;
	void* TargetPixels;
	CSize FullTargetSize;
	CPoint FullTargetOffset;
	CSize ClippedTargetSize;
	int StripPadding; // Height of strip is padded to multiple of this

	// Processing thread can signal failure by setting this flag to false. Must not be set to true by processing threads!
	bool Success;
};

// Thread pool for executing processing requests on multiple threads in parallel, processing a strip
// of the image on each thread.
class CProcessingThreadPool {
public:
	// Singleton instance
	static CProcessingThreadPool& This();
	// Creation is not thread safe. Call once, before creating additional threads.
	void CreateThreadPoolThreads();
	// to be called at program termination
	void StopAllThreads();

	// Processes the request using all thread pool threads and the current thread.
	// Note that the method does NOT take ownership of the passed request object.
	// The processing work is distributed to the thread pool threads. The pRequest->ProcessStrip()
	// method is called to process a strip of the image.
	bool Process(CProcessingRequest* pRequest);
private:
	static CProcessingThreadPool* sm_instance;

	CProcessingThread** m_threads;
	int m_nNumThreads;

	CProcessingThreadPool(void);
};

