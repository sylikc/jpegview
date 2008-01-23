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

void CProcessingThreadPool::Process(CProcessingRequest* pRequest) {
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
}

CProcessingThreadPool::CProcessingThreadPool(void) {
	m_threads = NULL;
	m_nNumThreads = 0;
}


void CProcessingThread::StartProcesss(CWrappedRequest* pRequest) {
	ProcessAsync(pRequest);
}

void CProcessingThread::DoProcess(CProcessingRequest* pRequest, int nOffsetY, int nSizeY) {
	if (pRequest->Request == CProcessingRequest::Upsampling) {
		CRequestUpDownSampling* pReq = (CRequestUpDownSampling*)pRequest;
		CBasicProcessing::SampleUp_HQ_SSE_MMX_Core(pReq->FullTargetSize, 
			CPoint(pReq->FullTargetOffset.x, pReq->FullTargetOffset.y + nOffsetY), 
			CSize(pReq->ClippedTargetSize.cx, nSizeY),
			pReq->SourceSize, pReq->SourcePixels, 
			pReq->Channels, pReq->SSE, 
			(uint8*)(pReq->TargetPixels) + pReq->ClippedTargetSize.cx * 4 * nOffsetY);
	} else if (pRequest->Request == CProcessingRequest::Downsampling) {
		CRequestUpDownSampling* pReq = (CRequestUpDownSampling*)pRequest;
		CBasicProcessing::SampleDown_HQ_SSE_MMX_Core(pReq->FullTargetSize, 
			CPoint(pReq->FullTargetOffset.x, pReq->FullTargetOffset.y + nOffsetY), 
			CSize(pReq->ClippedTargetSize.cx, nSizeY),
			pReq->SourceSize, pReq->SourcePixels, 
			pReq->Channels, pReq->Sharpen, 
			pReq->Filter, pReq->SSE, 
			(uint8*)(pReq->TargetPixels) + pReq->ClippedTargetSize.cx * 4 * nOffsetY);
	} else if (pRequest->Request == CProcessingRequest::LDCAndLUT) {
		CRequestLDC* pReq = (CRequestLDC*)pRequest;
		CBasicProcessing::ApplyLDC32bpp_Core(pReq->FullTargetSize,
			CPoint(pReq->FullTargetOffset.x, pReq->FullTargetOffset.y + nOffsetY),
			CSize(pReq->ClippedTargetSize.cx, nSizeY),
			pReq->LDCMapSize, 
			(const uint32*)(pReq->SourcePixels) + pReq->ClippedTargetSize.cx * nOffsetY, 
			pReq->LUT, pReq->LDCMap,
			pReq->BlackPt, pReq->WhitePt, pReq->BlackPtSteepness,
			(uint32*)(pReq->TargetPixels) + pReq->ClippedTargetSize.cx * nOffsetY);
	}
}

void CProcessingThread::ProcessRequest(CRequestBase& request) {
	CWrappedRequest* pWrappedRequest = (CWrappedRequest*)&request;
	DoProcess(pWrappedRequest->InnerRequest, pWrappedRequest->Offset, pWrappedRequest->SizeY);
}

