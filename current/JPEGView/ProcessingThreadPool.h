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
		LDCAndLUT,
		Gauss,
		UnsharpMask,
		RotateImage,
		TrapezoidTransform
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
		Success = true;
	}

	ERequest Request;
	CSize SourceSize;
	const void* SourcePixels;
	void* TargetPixels;
	CSize FullTargetSize;
	CPoint FullTargetOffset;
	CSize ClippedTargetSize;
	bool Success;
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

class CRequestGauss : public CProcessingRequest {
public:
	CRequestGauss(const int16* pSourcePixels, CSize fullSize, CPoint offset, CSize rect, double dRadius, int16* pTargetPixels)
					: CProcessingRequest(Gauss, pSourcePixels, fullSize, pTargetPixels, rect, offset, rect) {
		Radius = dRadius;
	}

	double Radius;
};

class CRequestUnsharpMask : public CProcessingRequest {
public:
	CRequestUnsharpMask(const void* pSourcePixels, CSize fullSize, CPoint offset, CSize rect, double dAmount, double dThreshold, 
		const int16* pThresholdLUT, const int16* pGrayImage, const int16* pSmoothedGrayImage, void* pTargetPixels, int nChannels)
					: CProcessingRequest(UnsharpMask, pSourcePixels, fullSize, pTargetPixels, rect, offset, rect) {
		Amount = dAmount;
		Threshold = dThreshold;
		ThresholdLUT = pThresholdLUT;
		GrayImage = pGrayImage;
		SmoothedGrayImage = pSmoothedGrayImage;
		Channels = nChannels;
	}

	double Amount;
	double Threshold;
	const int16* ThresholdLUT;
	const int16* GrayImage;
	const int16* SmoothedGrayImage;
	int Channels;
};

class CRequestRotate : public CProcessingRequest {
public:
	CRequestRotate(const void* pSourcePixels, CPoint targetOffset, CSize targetSize, double dRotation, 
		CSize sourceSize, void* pTargetPixels, int nChannels, COLORREF backColor)
			: CProcessingRequest(RotateImage, pSourcePixels, sourceSize, pTargetPixels, targetSize, targetOffset, targetSize) {
		Rotation = dRotation;
		Channels = nChannels;
		BackColor = backColor;
	}

	double Rotation;
	int Channels;
	COLORREF BackColor;
};

class CRequestTrapezoid : public CProcessingRequest {
public:
	CRequestTrapezoid(const void* pSourcePixels, CPoint targetOffset, CSize targetSize, const CTrapezoid& trapezoid, 
		CSize sourceSize, void* pTargetPixels, int nChannels, COLORREF backColor)
			: CProcessingRequest(TrapezoidTransform, pSourcePixels, sourceSize, pTargetPixels, targetSize, targetOffset, targetSize) {
		Trapezoid = trapezoid;
		Channels = nChannels;
		BackColor = backColor;
	}

	CTrapezoid Trapezoid;
	int Channels;
	COLORREF BackColor;
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
	bool Process(CProcessingRequest* pRequest);
private:
	static CProcessingThreadPool* sm_instance;

	CProcessingThread** m_threads;
	int m_nNumThreads;

	CProcessingThreadPool(void);
};

class CProcessingThread : public CWorkThread {
public:
	CProcessingThread(void) : CWorkThread(false) {}
	~CProcessingThread(void) {}

	// Start a processing request on this thread, returns immediately
	void StartProcesss(CWrappedRequest* pRequest);

	// Processes a request (synchronous, on calling thread)
	static void DoProcess(CProcessingRequest* pRequest, int nOffsetY, int nSizeY);
private:

	virtual void ProcessRequest(CRequestBase& request);
};
