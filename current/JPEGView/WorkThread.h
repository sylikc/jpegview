#pragma once

// Base class for processing requests processed in a CWorkThread class
class CRequestBase {
public:

	CRequestBase(HANDLE eventFinished) {
		EventFinished = eventFinished;
		EventFinishedCounter = NULL;
		Processed = false;
		Deleted = false;
	}

	CRequestBase() {
		EventFinished = NULL;
		EventFinishedCounter = NULL;
		Processed = false;
		Deleted = false;
	}

	HANDLE EventFinished; // Event signaled when processing is finished
	volatile LONG* EventFinishedCounter; // if not NULL, this counter is decremented on having handled the request and the event is not fired until it is zero
	volatile bool Processed; // Set to true when processing is finished
	volatile bool Deleted; // Marks requests for deletion from the request queue
};


// Class for a treat handling processing requests in a request queue
class CWorkThread
{
public:
	CWorkThread(void);
	virtual ~CWorkThread(void);

	// Gets if the request queue is currently empty, meaning the thread has nothing to do
	bool RequestQueueEmpty();

	// Clean termination, reads the current request to the end
	void Terminate();

	// Aborts the thread immediatly. Leaves an undefined state - only call on process termination.
	void Abort();

protected:

	// Posts the request on the request queue and waits until the request is processed.
	// The request is removed from the request queue automatically after it has been processed.
	// Note that ownership of the request object is taken over by this method - allocate the
	// request object with 'new' but do not delete it!
	void ProcessAndWait(CRequestBase* pRequest);

	// Posts a request on the request queue and returns immediately to the caller without waiting for the request
	// to be processed.
	// The request is not removed from the request queue automatically - the caller must do this by setting
	// request.Deleted to true as soon as the request has finished processing and is handled by the caller.
	// Ownership of the request object is taken over by the method.
	void ProcessAsync(CRequestBase* pRequest);

	// Called in the context of the working thread to process the request
	virtual void ProcessRequest(CRequestBase& request) = 0;

	// Called in the context of the working thread after it has been signaled that the request has been processed
	virtual void AfterFinishProcess(CRequestBase& request) {}

	std::list<CRequestBase*> m_requestList;
	CRITICAL_SECTION m_csList; // the critical section protecting the list above
	HANDLE m_wakeUp; // wake up event for the tread (it sleeps while there is nothing to process)
	HANDLE m_hThread; // working thread
	volatile bool m_bTerminate; // flags termination for thread

private:
	static void  __cdecl ThreadFunc(void* arg);
	static void DeleteMarkedRequests(CWorkThread* thisPtr);
};
