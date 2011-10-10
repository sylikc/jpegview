#include "StdAfx.h"
#include "ProcessingThreadPool.h"
#include "SettingsProvider.h"
#include "BasicProcessing.h"

CProcessingThreadPool* CProcessingThreadPool::sm_instance;


CProcessingThreadPool& CProcessingThreadPool::This() {
	if (sm_instance == NULL) {
		sm_instance = new CProcessingThreadPool();
	}
	return *sm_instance;
}

void CProcessingThreadPool::CreateThreadPoolThreads() {
	m_nNumThreads = CSettingsProvider::This().NumberOfCoresToUse() - 1;
	if (m_nNumThreads > 0) {
		m_threads = new CProcessingThread*[m_nNumThreads];
		for (int i = 0; i < m_nNumThreads; i++) {
			m_threads[i] = new CProcessingThread();
		}
	} else {
		m_threads = NULL;
	}
}

void CProcessingThreadPool::StopAllThreads() {
	for (int i = 0; i < m_nNumThreads; i++) {
		m_threads[i]->Terminate();
		delete m_threads[i];
	}
	m_nNumThreads = 0;
	m_threads = NULL;
}

bool CProcessingThreadPool::Process(CProcessingRequest* pRequest) {
	int nTargetCX = pRequest->ClippedTargetSize.cx;
	int nTargetCY = pRequest->ClippedTargetSize.cy;
	if (m_nNumThreads == 0) {
		CProcessingThread::DoProcess(pRequest, 0, nTargetCY);
	} else {
		if (nTargetCX * nTargetCY < 100000 || nTargetCY <= 12) {
			CProcessingThread::DoProcess(pRequest, 0, nTargetCY);
		} else {
			// Important: All slices must have a height dividable by 8, except the last one
			int nNumThreadsUsed = m_nNumThreads + 1;
			int nSliceCY;
			while ((nSliceCY = ~7 & (nTargetCY / nNumThreadsUsed)) < 8) {
				nNumThreadsUsed--;
			}
			int nLastCY = nTargetCY - (nNumThreadsUsed - 1)*nSliceCY;
			volatile LONG nRequestThreadCounter = nNumThreadsUsed - 1;
			int nCurrCY = 0;
			HANDLE eventFinished = ::CreateEvent(0, TRUE, FALSE, NULL);
			CWrappedRequest** pAllWrappedRequests = new CWrappedRequest*[nNumThreadsUsed-1];
			for (int i = 0; i < nNumThreadsUsed-1; i++) {
				pAllWrappedRequests[i] = new CWrappedRequest(pRequest, nCurrCY, nSliceCY, eventFinished);
				pAllWrappedRequests[i]->EventFinishedCounter = &nRequestThreadCounter;
				m_threads[i]->StartProcesss(pAllWrappedRequests[i]);
				nCurrCY += nSliceCY;
			}
			CProcessingThread::DoProcess(pRequest, nCurrCY, nLastCY);
			::WaitForSingleObject(eventFinished, INFINITE);
			::CloseHandle(eventFinished);
			for (int i = 0; i < nNumThreadsUsed-1; i++) {
				pAllWrappedRequests[i]->Deleted = true; // thread pool threads will remove the requests from the queue
			}
			delete [] pAllWrappedRequests;
		}
	}
	return pRequest->Success;
}

CProcessingThreadPool::CProcessingThreadPool(void) {
	m_threads = NULL;
	m_nNumThreads = 0;
}


void CProcessingThread::StartProcesss(CWrappedRequest* pRequest) {
	ProcessAsync(pRequest);
}

void CProcessingThread::DoProcess(CProcessingRequest* pRequest, int nOffsetY, int nSizeY) {
	// Processing is done in strips to reduce memory consumption and increase cache hit rate. The following constant gives the number of
	// pixels to process per strip
	const uint32 MAX_SRC_PIXELS_PER_STRIP = 1024 * 100;
	uint32 nNumberOfPixelsInSource = (uint32)((pRequest->SourceSize.cx * (double)pRequest->ClippedTargetSize.cx / pRequest->FullTargetSize.cx) *
		(pRequest->SourceSize.cy * (double)nSizeY / pRequest->FullTargetSize.cy));
	uint32 nStrips = 1 + nNumberOfPixelsInSource / MAX_SRC_PIXELS_PER_STRIP;
	uint32 nStripHeight = nSizeY / nStrips;
	if (nStrips > 1) {
		nStripHeight = nStripHeight & ~7; // must be dividable by 8, except last strip
		nStripHeight = min(nSizeY, max(nStripHeight, 16));
	}
	int nSizeProcessed = 0;
	int nCurrentSizeY = nStripHeight;
	while (nSizeProcessed < nSizeY) {
		void* pTarget = NULL;
		int nCurrentOffsetY = nOffsetY + nSizeProcessed;
		switch (pRequest->Request) {
			case CProcessingRequest::Upsampling: {
				CRequestUpDownSampling* pReq = (CRequestUpDownSampling*)pRequest;
				pTarget = CBasicProcessing::SampleUp_HQ_SSE_MMX_Core(pReq->FullTargetSize, 
					CPoint(pReq->FullTargetOffset.x, pReq->FullTargetOffset.y + nCurrentOffsetY), 
					CSize(pReq->ClippedTargetSize.cx, nCurrentSizeY),
					pReq->SourceSize, pReq->SourcePixels, 
					pReq->Channels, pReq->SSE, 
					(uint8*)(pReq->TargetPixels) + pReq->ClippedTargetSize.cx * 4 * nCurrentOffsetY);
				break;
			} 
			case CProcessingRequest::Downsampling: {
				CRequestUpDownSampling* pReq = (CRequestUpDownSampling*)pRequest;
				pTarget = CBasicProcessing::SampleDown_HQ_SSE_MMX_Core(pReq->FullTargetSize, 
					CPoint(pReq->FullTargetOffset.x, pReq->FullTargetOffset.y + nCurrentOffsetY), 
					CSize(pReq->ClippedTargetSize.cx, nCurrentSizeY),
					pReq->SourceSize, pReq->SourcePixels, 
					pReq->Channels, pReq->Sharpen, 
					pReq->Filter, pReq->SSE, 
					(uint8*)(pReq->TargetPixels) + pReq->ClippedTargetSize.cx * 4 * nCurrentOffsetY);
				break;
			} 
			case CProcessingRequest::LDCAndLUT: {
				CRequestLDC* pReq = (CRequestLDC*)pRequest;
				pTarget = CBasicProcessing::ApplyLDC32bpp_Core(pReq->FullTargetSize,
					CPoint(pReq->FullTargetOffset.x, pReq->FullTargetOffset.y + nCurrentOffsetY),
					CSize(pReq->ClippedTargetSize.cx, nCurrentSizeY),
					pReq->LDCMapSize, 
					(const uint32*)(pReq->SourcePixels) + pReq->ClippedTargetSize.cx * nCurrentOffsetY, 
					pReq->SatLUTs, pReq->LUT, pReq->LDCMap,
					pReq->BlackPt, pReq->WhitePt, pReq->BlackPtSteepness,
					(uint32*)(pReq->TargetPixels) + pReq->ClippedTargetSize.cx * nCurrentOffsetY);
				break;
			}
			case CProcessingRequest::Gauss: {
				CRequestGauss* pReq = (CRequestGauss*)pRequest;
				pTarget = CBasicProcessing::GaussFilter16bpp1Channel_Core(pReq->SourceSize,
					CPoint(pReq->FullTargetOffset.x, pReq->FullTargetOffset.y + nCurrentOffsetY),
					CSize(pReq->FullTargetSize.cx, nCurrentSizeY),
					pReq->FullTargetSize.cy,
					pReq->Radius,
					(int16*)(pReq->SourcePixels),
					(int16*)(pReq->TargetPixels) + nCurrentOffsetY);
				break;
			}
			case CProcessingRequest::UnsharpMask: {
				CRequestUnsharpMask* pReq = (CRequestUnsharpMask*)pRequest;
				pTarget = CBasicProcessing::UnsharpMask_Core(pReq->SourceSize,
					CPoint(pReq->FullTargetOffset.x, pReq->FullTargetOffset.y + nCurrentOffsetY),
					CSize(pReq->FullTargetSize.cx, nCurrentSizeY),
					pReq->Amount,
					pReq->ThresholdLUT, pReq->GrayImage, pReq->SmoothedGrayImage,
					pReq->SourcePixels, pReq->TargetPixels,
					pReq->Channels);
				break;
			}
			case CProcessingRequest::RotateImage: {
				CRequestRotate* pReq = (CRequestRotate*)pRequest;
				pTarget = CBasicProcessing::RotateHQ_Core(CPoint(pReq->FullTargetOffset.x, pReq->FullTargetOffset.y + nCurrentOffsetY),
					CSize(pReq->FullTargetSize.cx, nCurrentSizeY), pReq->Rotation, pReq->SourceSize, pReq->SourcePixels, 
					(uint8*)pReq->TargetPixels + pReq->FullTargetSize.cx * 4 * nCurrentOffsetY, pReq->Channels, pReq->BackColor);
				break;
			}
		}
		if (pTarget == NULL) {
			pRequest->Success = false;
			break;
		}
		nSizeProcessed += nCurrentSizeY;
		nCurrentSizeY = min(nStripHeight, nSizeY - nSizeProcessed);
	}
}

void CProcessingThread::ProcessRequest(CRequestBase& request) {
	CWrappedRequest* pWrappedRequest = (CWrappedRequest*)&request;
	DoProcess(pWrappedRequest->InnerRequest, pWrappedRequest->Offset, pWrappedRequest->SizeY);
}

