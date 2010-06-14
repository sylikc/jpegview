#pragma once

#include "WorkThread.h"
#include "ImageProcessingTypes.h"

class CProcessingThread;

// Request in processing request list
class CProcessingRequest : public CRequestBase {
public:
	// Defined request types
	enum ERequest {
		Upsampling,
		Downsampling,
		LDCAndLUT
	};

	CProcessingRequest(ERequest eRequest, const void* pSourcePixels, CSize sourceSize, void* pTargetPixels,
		CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize) : CRequestBase(NULL) {
		Request = eRequest;
		SourcePixels = pSourcePixels;
		SourceSize = sourceSize;
		TargetPixels = pTargetPixels;
		FullTargetSize = fullTargetSize;
		FullTargetOffset = fullTargetOffset;
		ClippedTargetSize = clippedTargetSize;
	}

	ERequest Request;
	CSize SourceSize;
	const void* SourcePixels;
	void* TargetPixels;
	CSize FullTargetSize;
	CPoint FullTargetOffset;
	CSize ClippedTargetSize;
};

// Request for upsampling or downsampling
class CRequestUpDownSampling : public CProcessingRequest {
public:
	// eRequest can only be Upsampling or Downsampling
	CRequestUpDownSampling(ERequest eRequest, const void* pSourcePixels, CSize sourceSize, void* pTargetPixels,
			CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
			int nChannels, double dSharpen, EFilterType eFilter, bool bSSE) 
			: CProcessingRequest(eRequest, pSourcePixels, sourceSize, pTargetPixels, fullTargetSize, fullTargetOffset, clippedTargetSize) {
		Channels = nChannels;
		Sharpen = dSharpen;
		Filter = eFilter;
		SSE = bSSE;
	}

	int Channels;
	double Sharpen;
	EFilterType Filter;
	bool SSE;
};

class CRequestLDC : public CProcessingRequest {
public:
	CRequestLDC(const void* pSourcePixels, CSize sourceSize, void* pTargetPixels,
			CSize fullTargetSize, CPoint fullTargetOffset,
			CSize ldcMapSize, const int32* pSatLUTs, const uint8* pLUT, const uint8* pLDCMap,
			float fBlackPt, float fWhitePt, float fBlackPtSteepness) 
			: CProcessingRequest(LDCAndLUT, pSourcePixels, sourceSize, pTargetPixels, fullTargetSize, fullTargetOffset, sourceSize) {
		LDCMapSize = ldcMapSize;
		SatLUTs = pSatLUTs;
		LUT = pLUT;
		LDCMap = pLDCMap;
		BlackPt = fBlackPt;
		WhitePt = fWhitePt;
		BlackPtSteepness = fBlackPtSteepness;
	}

	CSize LDCMapSize;
	const int32* SatLUTs;
	const uint8* LUT;
	const uint8* LDCMap;
	float BlackPt;
	float WhitePt;
	float BlackPtSteepness;
};

// A processing request wrapped into another request
class CWrappedRequest : public CRequestBase {
public:
	CWrappedRequest(CProcessingRequest * pRequest, int nOffset, int nSize, HANDLE hEventFinished) : CRequestBase(hEventFinished) {
		InnerRequest = pRequest;
		Offset = nOffset;
		SizeY = nSize;
	}

	CProcessingRequest* InnerRequest;
	int Offset;
	int SizeY;
};

class CProcessingThreadPool {
public:
	// Singleton instance
	static CProcessingThreadPool& This();
	// creation not MT safe, create before creating additional threads
	void CreateThreadPoolThreads();
	// to be called at program termination
	void StopAllThreads();

	// Processes the request on all thread pool threads and the current thread.
	// Note that the method does NOT take ownership of the passed object.
	void Process(CProcessingRequest* pRequest);
private:
	static CProcessingThreadPool* sm_instance;

	CProcessingThread** m_threads;
	int m_nNumThreads;

	CProcessingThreadPool(void);
};

class CProcessingThread : public CWorkThread {
public:
	CProcessingThread(void) {}
	~CProcessingThread(void) {}

	// Start a processing request on this thread, returns immediately
	void StartProcesss(CWrappedRequest* pRequest);

	// Processes a request (synchronous, on calling thread)
	static void DoProcess(CProcessingRequest* pRequest, int nOffsetY, int nSizeY);
private:

	virtual void ProcessRequest(CRequestBase& request);
};
