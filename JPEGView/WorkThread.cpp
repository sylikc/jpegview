
#include "StdAfx.h"
#include "WorkThread.h"
#include <process.h>

/////////////////////////////////////////////////////////////////////////////////////////////
// Public
/////////////////////////////////////////////////////////////////////////////////////////////

CWorkThread::CWorkThread(void) {
	m_bTerminate = false;
	memset(&m_csList, 0, sizeof(CRITICAL_SECTION));
	::InitializeCriticalSection(&m_csList);
	m_wakeUp = ::CreateEvent(0, TRUE, FALSE, NULL);

	m_hThread = (HANDLE)_beginthread(ThreadFunc, 0, this);
}

CWorkThread::~CWorkThread(void) {
	if (!m_bTerminate) {
		Terminate();
	}
	::DeleteCriticalSection(&m_csList);
	::CloseHandle(m_wakeUp);
}

void CWorkThread::ProcessAndWait(CRequestBase* pRequest) {
	bool bCreateEvent = pRequest->EventFinished == NULL;
	if (bCreateEvent == NULL) {
		pRequest->EventFinished = ::CreateEvent(0, TRUE, FALSE, NULL);
	}

	ProcessAsync(pRequest);
	::WaitForSingleObject(pRequest->EventFinished, INFINITE);

	if (bCreateEvent) {
		::CloseHandle(pRequest->EventFinished);
	}
	pRequest->Deleted = true; // make sure the request is removed from the queue
}

void CWorkThread::ProcessAsync(CRequestBase* pRequest) {
	::EnterCriticalSection(&m_csList);
	m_requestList.push_back(pRequest);
	::LeaveCriticalSection(&m_csList);

	// there is something to process now
	::SetEvent(m_wakeUp);
}

bool CWorkThread::RequestQueueEmpty() {
	::EnterCriticalSection(&m_csList);
	int nSize = m_requestList.size();
	::LeaveCriticalSection(&m_csList);
	return nSize == 0;
}

void CWorkThread::Terminate() { 
	m_bTerminate = true;
	if (m_hThread != NULL) {
		::SetEvent(m_wakeUp);
		::WaitForSingleObject(m_hThread, 10000);
		m_hThread = NULL;
	}
}

void CWorkThread::Abort() {
	try {
		if (m_hThread != NULL) {
			::SetEvent(m_wakeUp);
			if (WAIT_TIMEOUT == ::WaitForSingleObject(m_hThread, 100)) {
				::TerminateThread(m_hThread, 1);
				::WaitForSingleObject(m_hThread, 100);
			}
			m_hThread = NULL;
		}
	} catch (...) {
		// do not crash
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Private
/////////////////////////////////////////////////////////////////////////////////////////////

void CWorkThread::ThreadFunc(void* arg) {

	CWorkThread* thisPtr = (CWorkThread*) arg;
	do {
		::EnterCriticalSection(&thisPtr->m_csList);

		// Delete the requests marked as deleted from request queue
		DeleteMarkedRequests(thisPtr);

		// search a request that is not yet processed
		CRequestBase* requestHandled = NULL;
		int nNumRequests = 0;
		std::list<CRequestBase*>::iterator iter;
		for (iter = thisPtr->m_requestList.begin( ); iter != thisPtr->m_requestList.end( ); iter++ ) {
			if ((*iter)->Processed == false) {
				requestHandled = *iter;
				nNumRequests++;
			}
		}

		::LeaveCriticalSection(&thisPtr->m_csList);

		// process this request
		if (requestHandled != NULL) {
			thisPtr->ProcessRequest(*requestHandled);
			requestHandled->Processed = true;

			// signal end of processing
			if (requestHandled->EventFinished != NULL) {
				if (requestHandled->EventFinishedCounter == NULL) {
					::SetEvent(requestHandled->EventFinished);
				} else {
					LONG nNewValue = ::InterlockedDecrement(requestHandled->EventFinishedCounter);
					if (nNewValue <= 0) {
						::SetEvent(requestHandled->EventFinished);
					}
				}
			}
			if (!thisPtr->m_bTerminate) {
				thisPtr->AfterFinishProcess(*requestHandled);
			}
			nNumRequests--;
		}

		// if there are no more requests, sleep until woke up
		if (nNumRequests == 0 && !thisPtr->m_bTerminate) {
			::WaitForSingleObject(thisPtr->m_wakeUp, INFINITE);
			::ResetEvent(thisPtr->m_wakeUp);
		}
	} while (!thisPtr->m_bTerminate);
	_endthread();
}

void CWorkThread::DeleteMarkedRequests(CWorkThread* thisPtr) {
	bool bDeleted = false;
	std::list<CRequestBase*>::iterator iter;
	for (iter = thisPtr->m_requestList.begin( ); iter != thisPtr->m_requestList.end( ); iter++ ) {
		if ((*iter)->Deleted) {
			delete *iter;
			thisPtr->m_requestList.erase(iter);
			bDeleted = true;
			break;
		}
	}
	if (bDeleted) {
		DeleteMarkedRequests(thisPtr);
	}
}
