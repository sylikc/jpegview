#pragma once

// Base class for requests processed by a worker thread (i.e. an instance of CWorkThread class) 
class CRequestBase {
public:

	CRequestBase(HANDLE eventFinished) {
		EventFinished = eventFinished;
		EventFinishedCounter = NULL;
		Processed = false;
		Deleted = false;
		Type = 0;
	}

	CRequestBase() {
		EventFinished = NULL;
		EventFinishedCounter = NULL;
		Processed = false;
		Deleted = false;
		Type = 0;
	}

	int Type; // Can be used to set the type of the request, default is 0
	HANDLE EventFinished; // Event signaled when processing is finished
	volatile LONG* EventFinishedCounter; // if not NULL, this counter is decremented after having handled the request and the event is not fired until it gets zero
	volatile bool Processed; // Set to true when processing is finished
	volatile bool Deleted; // Marks requests for deletion from the request queue
};


// Represents a worker thread processing requests in a request queue
class CWorkThread
{
public:
	CWorkThread(bool bCoInitialize);
	virtual ~CWorkThread(void);

	// Clean termination, finishs the currently processed request before terminating
	void Terminate();

	// Aborts the thread immediatly. Leaves an undefined state - only call on process termination.
	void Abort();

protected:

	// Posts the request on the request queue and waits until the request is processed.
	// The request is removed from the request queue and deleted automatically after it has been processed.
	// Note that ownership of the request object is taken over by this method - allocate the
	// request object with 'new' but do not delete it!
	void ProcessAndWait(CRequestBase* pRequest);

	// Posts a request on the request queue and returns immediately to the caller without waiting for the request
	// to be processed. The request.EventFinished event will be fired after the request has been processed.
	// After processing, the request stays in the request queue in an inactive state. To delete the request from
	// the queue and free the request object itself, the caller must set the request.Deleted flag as soon as the
	// request is handled by the caller.
	// Ownership of the request object is taken over by the method.
	void ProcessAsync(CRequestBase* pRequest);

	// Called in the context of the worker thread to process the request
	virtual void ProcessRequest(CRequestBase& request) = 0;

	// Called in the context of the worker thread after it has been signaled that the request has been processed
	virtual void AfterFinishProcess(CRequestBase& request) {}

	std::list<CRequestBase*> m_requestList; // list of requests, also contains processed requests not yet removed by client
	CRITICAL_SECTION m_csList; // the critical section protecting the request list (m_requestList)
	HANDLE m_wakeUp; // wake up event for the tread (it sleeps while there is nothing to process)
	HANDLE m_hThread; // working thread
	volatile bool m_bTerminate; // flags termination for thread

private:
	static void  __cdecl ThreadFunc(void* arg);
	static void DeleteAllRequestsMarkedForDeletion(CWorkThread* thisPtr);

	bool m_bCoInitialize;
};
