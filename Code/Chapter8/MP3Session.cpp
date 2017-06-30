#include "MP3Session.h"


CMP3Session::CMP3Session(PCWSTR pUrl) :
    m_sourceSampleReadyEvent(NULL),
    m_syncMftWorkerQueue(MFASYNC_CALLBACK_QUEUE_LONG_FUNCTION),
    m_sessionStarted(false),
    CMP3SessionTopoBuilder(pUrl)
{
}


//
// Initialize the CMP3Session class
//
HRESULT CMP3Session::Init(void)
{
    HRESULT hr = S_OK;

    do
    {
        // allocate a special worker thread for the blocking synchronous MFT operations
        hr = MFAllocateWorkQueue(&m_syncMftWorkerQueue);
        BREAK_ON_FAIL(hr);

        // create the event queue
        hr = MFCreateEventQueue(&m_pEventQueue);
        BREAK_ON_FAIL(hr);

        // create an event that will signal the waiting synchronous MFT thread that the source
        // has a new sample ready
        m_sourceSampleReadyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        BREAK_ON_NULL(m_sourceSampleReadyEvent, E_UNEXPECTED);

        // crate the custom MP3 topology
        hr = LoadCustomTopology();
    }
    while(false);

    return hr;
}


CMP3Session::~CMP3Session(void)
{
    if(m_syncMftWorkerQueue != MFASYNC_CALLBACK_QUEUE_LONG_FUNCTION)
    {
        MFUnlockWorkQueue(m_syncMftWorkerQueue);
    }

    if(m_sourceSampleReadyEvent != NULL)
    {
        CloseHandle(m_sourceSampleReadyEvent);
    }
}



//////////////////////////////////////////////////////////////////////////////////////////
//
//  IUnknown interface implementation
//
/////////////////////////////////////////////////////////////////////////////////////////

ULONG CMP3Session::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG CMP3Session::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT CMP3Session::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(static_cast<IMFMediaSession*>(this));
    }
    else if (riid == IID_IMFMediaSession)
    {
        *ppv = static_cast<IMFMediaSession*>(this);
    }
    else if (riid == IID_IMFGetService)
    {
        *ppv = static_cast<IMFGetService*>(this);
    }
    else
    {
        *ppv = NULL;
        hr = E_NOINTERFACE;
    }

    if(SUCCEEDED(hr))
        AddRef();

    return hr;
}




/////////////////////////////////////////////////////////////////////////
// IMFAsyncCallback implementation
/////////////////////////////////////////////////////////////////////////

//
// Get the behavior information (duration, etc.) of the asynchronous callback operation - 
// not implemented.
//
HRESULT CMP3Session::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue)
{
    return E_NOTIMPL;
}


// 
// Asynchronous worker function - called when the source has finished initializing, and when
// an event has occurred.
//
HRESULT CMP3Session::Invoke(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;
    CComPtr<IUnknown> pUnkState;
    CComQIPtr<IAsyncState> pAsyncState;

    do
    {
        // see if the event indicates a failure - if so, fail and return the MEError event
        hr = pResult->GetStatus();
        BREAK_ON_FAIL(hr);
        
        // get the IAsyncState state from the result
        hr = pResult->GetState(&pUnkState);
        BREAK_ON_FAIL(hr);

        pAsyncState = pUnkState;
        BREAK_ON_NULL(pAsyncState, E_UNEXPECTED);
        
        // figure out the type of the operation from the state, and then proxy the call to
        // the right function
        if(pAsyncState->EventType() == AsyncEventType_ByteStreamHandlerEvent)
        {
            hr = HandleByteStreamHandlerEvent(pResult);
        }
        else if(pAsyncState->EventType() == AsyncEventType_StreamSinkEvent)
        {
            hr = HandleStreamSinkEvent(pResult);
        }
        else if(pAsyncState->EventType() == AsyncEventType_SourceEvent)
        {
            hr = HandleSourceEvent(pResult);
        }
        else if(pAsyncState->EventType() == AsyncEventType_SourceStreamEvent)
        {
            hr = HandleSourceStreamEvent(pResult);
        }
        else if(pAsyncState->EventType() == AsyncEventType_SyncMftSampleRequest)
        {
            hr = HandleSynchronousMftRequest(pResult);
        }
    }
    while(false);

    // if we got a failure, queue an error event
    if(FAILED(hr))
    {
        hr = m_pEventQueue->QueueEventParamVar(MEError, GUID_NULL, hr, NULL);
    }

    return hr;
}



//////////////////////////////////////////////////////////////////////////////////////////
//
//  IMFGetService interface implementation
//
//////////////////////////////////////////////////////////////////////////////////////////


//
// Get the object that implements the requested interface from the topology - not 
// implemented here, just return S_OK
//
HRESULT CMP3Session::GetService(REFGUID guidService, REFIID riid, LPVOID *ppvObject)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(ppvObject, E_POINTER);

        // if the caller is asking for the rate control service, query for the 
        // IMFRateControl interface from the presentation clock
        if(guidService == MF_RATE_CONTROL_SERVICE)
        {
            BREAK_ON_NULL(m_pClock, E_NOINTERFACE);

            hr = m_pClock->QueryInterface(riid, ppvObject);
        }
        else
        {
            hr = MF_E_UNSUPPORTED_SERVICE;
        }
    }
    while(false);

    return hr;
}




//////////////////////////////////////////////////////////////////////////////////////////
//
//  IMFMediaSession interface implementation
//
//////////////////////////////////////////////////////////////////////////////////////////


//
// Set the internal topology of the session - not really used, just store the pointer.
//
HRESULT CMP3Session::SetTopology(DWORD dwSetTopologyFlags, IMFTopology *pTopology)
{
    if(pTopology == NULL)
        return E_POINTER;

    m_pTopology = pTopology;
    
    return S_OK;
}



//
// Clear the internal topology
//
HRESULT CMP3Session::ClearTopologies(void)
{
    m_pTopology = NULL;

    return S_OK;
}


//
// Start playback
//
HRESULT CMP3Session::Start(const GUID* pguidTimeFormat, const PROPVARIANT* pvarStartPosition)
{
    HRESULT hr = S_OK;
    CComPtr<IAsyncState> pState;

    // QI the IMFPresentationTimeSource from the sink - the audio renderer
    CComQIPtr<IMFPresentationTimeSource> pTimeSource = m_pSink;

    do
    {
        // start the session only once - all subsequent calls must be because the session is
        // paused, so there is no need to reinitialize the various parameters
        if(!m_sessionStarted)
        {
            m_sessionStarted = true;
        
            // start the source and pass in the presentation descriptor initialized earlier
            hr = m_pSource->Start(m_pPresentation, pguidTimeFormat, pvarStartPosition);
            BREAK_ON_FAIL(hr);

            // begin receiving events from the stream sink
            pState = new (std::nothrow) CAsyncState(AsyncEventType_StreamSinkEvent);        
            BREAK_ON_NULL(pState, E_OUTOFMEMORY);

            hr = m_pStreamSink->BeginGetEvent(this, pState);
            BREAK_ON_FAIL(hr);

            // begin receiving events from the source
            pState = new (std::nothrow) CAsyncState(AsyncEventType_SourceEvent);
            BREAK_ON_NULL(pState, E_OUTOFMEMORY);

            // start getting the next event from the source
            hr = m_pSource->BeginGetEvent(this, pState);
            BREAK_ON_FAIL(hr);

            // the audio renderer is supposed to be the time source - make sure we got the
            // IMFPresentationTimeSource pointer for the audio renderer
            BREAK_ON_NULL(pTimeSource, E_UNEXPECTED);

            // create the presentation clock
            hr = MFCreatePresentationClock(&m_pClock);
            BREAK_ON_FAIL(hr);

            // set the time source on the presentation clock - the audio renderer
            hr = m_pClock->SetTimeSource(pTimeSource);
            BREAK_ON_FAIL(hr);

            // set the presentation clock on the sink
            hr = m_pSink->SetPresentationClock(m_pClock);
            BREAK_ON_FAIL(hr);

            // start the clock at the beginning - time 0
            hr = m_pClock->Start(0);
        }
        else
        {
            // unpause the clock from the old position
            hr = m_pClock->Start(PRESENTATION_CURRENT_POSITION);
        }
    }
    while(false);

    return hr;
}


//
// Pause - just pass the command to the clock
//
HRESULT CMP3Session::Pause(void)
{
    return m_pClock->Pause();
}


//
// Stop - just pass the command to the clock
//
HRESULT CMP3Session::Stop(void)
{
    return m_pClock->Stop();
}


//
// Close the session - not implemented
//
HRESULT CMP3Session::Close(void)
{
    return S_OK;
}



//
// Shut down the session - not implemented
//
HRESULT CMP3Session::Shutdown(void)
{
    return S_OK;
}


//
// Get the clock for the current session
//
HRESULT CMP3Session::GetClock(IMFClock** ppClock)
{
    if(ppClock == NULL)
        return E_POINTER;

    return m_pClock->QueryInterface(IID_IMFClock, (void**)ppClock);
}


//
// Get the capabilities of the session
//
HRESULT CMP3Session::GetSessionCapabilities(DWORD* pdwCaps)
{
    if(pdwCaps == NULL)
        return E_POINTER;

    *pdwCaps = MFSESSIONCAP_START | MFSESSIONCAP_PAUSE;

    return S_OK;
}



//
// Get the topology associated with the session - not implemented
//
HRESULT CMP3Session::GetFullTopology(DWORD dwGetFullTopologyFlags, TOPOID TopoId, IMFTopology** ppFullTopology)
{
    return E_NOTIMPL;
}




//////////////////////////////////////////////////////////////////////////////////////////
//
//  IMFMediaEventGenerator interface implementation
//
//////////////////////////////////////////////////////////////////////////////////////////

//
// Begin asynchronous event processing of the next event in the queue
//
HRESULT CMP3Session::BeginGetEvent(IMFAsyncCallback* pCallback,IUnknown* punkState)
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    return m_pEventQueue->BeginGetEvent(pCallback, punkState);
}

//
// Complete asynchronous event processing
//
HRESULT CMP3Session::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    return m_pEventQueue->EndGetEvent(pResult, ppEvent);
}


//
// Get the next event in the event queue
//
HRESULT CMP3Session::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
    
    return m_pEventQueue->GetEvent(dwFlags, ppEvent);
}


//
// Add a new event to the event queue
//
HRESULT CMP3Session::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    return m_pEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
}



///////////////////////////////////////////////////////////////////////////////////////////
//
// Helper methods
//
///////////////////////////////////////////////////////////////////////////////////////////






//
// Handle an event from the stream sink
// 
HRESULT CMP3Session::HandleStreamSinkEvent(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaEvent> pEvent;
    MediaEventType eventType;
    CComPtr<IAsyncState> pState;

    do
    {
        // Get the event from the event queue.
        hr = m_pStreamSink->EndGetEvent(pResult, &pEvent);
        BREAK_ON_FAIL(hr);

        // Get the event type.
        hr = pEvent->GetType(&eventType);
        BREAK_ON_FAIL(hr);

        // request the next event immediately
        pState = new (std::nothrow) CAsyncState(AsyncEventType_StreamSinkEvent);
        BREAK_ON_NULL(pState, E_OUTOFMEMORY);

        // start getting the next event from the stream sink
        hr = m_pStreamSink->BeginGetEvent(this, pState);
        BREAK_ON_FAIL(hr);

        if(eventType == MEStreamSinkStarted)
        {
            // the sink has started
        }
        else if(eventType == MEStreamSinkRequestSample)
        {
            // create a state object that indicates that this is a synchronous MFT work item
            // that should be executed on the resampler on a separate queue
            pState = new (std::nothrow) CAsyncState(AsyncEventType_SyncMftSampleRequest);
            BREAK_ON_NULL(pState, E_OUTOFMEMORY);
            
            // schedule the synchronous MFT work on its own separate worker queue, since 
            // that work item can block - and we want to continue to use the main queue
            hr = MFPutWorkItem(m_syncMftWorkerQueue, this, pState);
        }
    }
    while(false);

    return hr;
}


//
// Handle the event coming from the source
//
HRESULT CMP3Session::HandleSourceEvent(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaEvent> pEvent;
    MediaEventType eventType;
    CComPtr<IAsyncState> pState;
    PROPVARIANT eventVariant;

    do
    {
        // clear the PROPVARIANT
        PropVariantInit(&eventVariant);

        // Get the event from the event queue.
        hr = m_pSource->EndGetEvent(pResult, &pEvent);
        BREAK_ON_FAIL(hr);

        // Get the event type.
        hr = pEvent->GetType(&eventType);
        BREAK_ON_FAIL(hr);

        // request the next event immediately
        pState = new (std::nothrow) CAsyncState(AsyncEventType_SourceEvent);
        BREAK_ON_NULL(pState, E_OUTOFMEMORY);

        // start getting the next event from the media source object
        hr = m_pSource->BeginGetEvent(this, pState);
        BREAK_ON_FAIL(hr);

        // Handle the new stream event that is fired when a new stream is added to the 
        // source.  Get the stream pointer and store it.
        if(eventType == MENewStream)
        {
            // get the data stored in the event
            hr = pEvent->GetValue(&eventVariant);
            BREAK_ON_FAIL(hr);

            // get the IMFMediaStream pointer from the stored IUnknown pointer
            m_pSourceStream = eventVariant.punkVal;
            BREAK_ON_NULL(m_pSourceStream, E_UNEXPECTED);

            // we got a new source stream - start listening for events coming from it
            pState = new (std::nothrow) CAsyncState(AsyncEventType_SourceStreamEvent);
            BREAK_ON_NULL(pState, E_OUTOFMEMORY);

            // start getting an event from the source's media stream object
            hr = m_pSourceStream->BeginGetEvent(this, pState);
        }
        else if(eventType == MESourceStarted)
        {
            // source started
        }
    }
    while(false);

    // free any internal values that can be freed in the PROPVARIANT
    PropVariantClear(&eventVariant);

    return hr;
}


//
// Handle an event sent by the source stream.
//
HRESULT CMP3Session::HandleSourceStreamEvent(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaEvent> pEvent;
    MediaEventType eventType;
    CComPtr<IAsyncState> pState;
    PROPVARIANT eventVariant;

    do
    {
        // clear the PROPVARIANT
        PropVariantInit(&eventVariant);

        // Get the event from the event queue.
        hr = m_pSourceStream->EndGetEvent(pResult, &pEvent);
        BREAK_ON_FAIL(hr);

        // Get the event type.
        hr = pEvent->GetType(&eventType);
        BREAK_ON_FAIL(hr);

        pState = new (std::nothrow) CAsyncState(AsyncEventType_SourceStreamEvent);
        BREAK_ON_NULL(pState, E_OUTOFMEMORY);

        // start getting the next event from the source stream
        hr = m_pSourceStream->BeginGetEvent(this, pState);
        BREAK_ON_FAIL(hr);

        // handle the passed-in event
        if(eventType == MEStreamStarted)
        {
        }
        else if(eventType == MEMediaSample)
        {
            // get the data stored in the event
            hr = pEvent->GetValue(&eventVariant);
            BREAK_ON_FAIL(hr);

            // get the pointer to the new sample from the stored IUnknown pointer, and cache
            // it in the m_pReadySourceSample member variable
            m_pReadySourceSample = eventVariant.punkVal;
            BREAK_ON_NULL(m_pReadySourceSample, E_UNEXPECTED);

            // set the event that signals that there is a new sample ready - the synchronous
            // MFT thread should be blocked waiting for this event in the PullDataFromSource
            // function.
            SetEvent(m_sourceSampleReadyEvent);
        }
    }
    while(false);

    // free any internal values that can be freed in the PROPVARIANT
    PropVariantClear(&eventVariant);

    return hr;
}


//
// Synchronous MFT work item - this is run off of a separate work queue/thread and will
// block while the source is fetching data.
//
HRESULT CMP3Session::HandleSynchronousMftRequest(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;
    CComPtr<IMFSample> pSample;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // get data from the resampler - this function will call itself in order to get data
        // from the decoder MFT and then will block while waiting for data from the source
        hr = PullDataFromMFT(m_pResampler, &pSample);
        BREAK_ON_FAIL(hr);

        // send the received sample to the sink
        hr = m_pStreamSink->ProcessSample(pSample);
        BREAK_ON_FAIL(hr);
    }
    while(false);
    
    return hr;
}




//
// Get a sample from the specified MFT
//
HRESULT CMP3Session::PullDataFromMFT(IMFTransform* pMft, IMFSample** ppNewSample)
{
    HRESULT hr = S_OK;
    MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
    DWORD processOutputStatus = 0;
    CComPtr<IMFSample> pMftInputSample;
    DWORD inputStreamId = 0;                         // assume the input stream ID is zero

    do
    {
        BREAK_ON_NULL(pMft, E_POINTER);
        BREAK_ON_NULL(ppNewSample, E_POINTER);

        // initialize the MFT_OUTPUT_DATA_BUFFER for the MFT
        hr = InitOutputDataBuffer(pMft, &outputDataBuffer);
        BREAK_ON_FAIL(hr);
       
        // Try to get output data from the MFT.  If the MFT returns that it needs input, 
        // get data from the upstream component, send it to the MFT's input, and try again.
        while(true)
        {
            // try to get an output sample from the MFT
            hr = pMft->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);

            // if ProcessOutput() did not say that it needs more input, then it must have 
            // either succeeded or failed unexpectedly - in either case, break out of the 
            // loop
            if(hr != MF_E_TRANSFORM_NEED_MORE_INPUT)
            {
                break;
            }

            // Pull data from the upstream MF component.  If this is the resampler, then its
            // upstream component is the decoder.  If this is the decoder, then its upstream
            // component is the source.
            if(pMft == m_pResampler)
            {
                hr = PullDataFromMFT(m_pDecoder, &pMftInputSample);
            }
            else
            {   // this is the decoder - get data from the source                
                hr = PullDataFromSource(&pMftInputSample);
            }
            BREAK_ON_FAIL(hr);

            // once we have a new sample, feed it into the current MFT's input
            hr = pMft->ProcessInput(inputStreamId, pMftInputSample, 0);
            BREAK_ON_FAIL(hr);
        }
        BREAK_ON_FAIL(hr);

        // if we got here, then we must have successfully extracted the new sample from the
        // MFT - store it in the output parameter
        *ppNewSample = outputDataBuffer.pSample;
    }
    while(false);

    return hr;
}



//
// Initialize the MFT_OUTPUT_DATA_BUFFER parameter for the specified MFT
//
HRESULT CMP3Session::InitOutputDataBuffer(IMFTransform* pMFTransform, 
            MFT_OUTPUT_DATA_BUFFER* pOutputBuffer)
{
    HRESULT hr = S_OK;
    CComPtr<IMFTransform> pMft = pMFTransform;
    MFT_OUTPUT_STREAM_INFO outputStreamInfo;
    DWORD outputStreamId = 0;                      // assume output stream ID is zero
    CComPtr<IMFSample> pOutputSample;
    CComPtr<IMFMediaBuffer> pMediaBuffer;

    do
    {
        ZeroMemory(&outputStreamInfo, sizeof(outputStreamInfo));
        ZeroMemory(pOutputBuffer, sizeof(*pOutputBuffer));

        // get the information on the output stream
        hr = pMft->GetOutputStreamInfo(outputStreamId, &outputStreamInfo);
        BREAK_ON_FAIL(hr);

        // if the MFT does not provide its own samples then we need to create a new sample 
        // and place it into the outputDataBuffer structure before calling the MFT
        if( (outputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0 &&
            (outputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES) == 0)
        {
            // create a sample
            hr = MFCreateSample(&pOutputSample);
            BREAK_ON_FAIL(hr);

            // create a memory buffer for the sample - just use a 1 MB buffer for simplicity
            hr = MFCreateMemoryBuffer(1048576, &pMediaBuffer);
            BREAK_ON_FAIL(hr);

            // add the buffer to the sample
            hr = pOutputSample->AddBuffer(pMediaBuffer);
            BREAK_ON_FAIL(hr);

            // store the sample in the output buffer object
            pOutputBuffer->pSample = pOutputSample.Detach();
        }

        // set the output stream ID
        pOutputBuffer->dwStreamID = outputStreamId;
    }
    while(false);

    return hr;
}




//
// Get output from the source - first request a sample from the source, and then block,
// waiting for an event that will notify us that the source has a new sample for us.
//
HRESULT CMP3Session::PullDataFromSource(IMFSample** ppNewSample)
{
    HRESULT hr = S_OK;

    do
    {
        // signal to the source stream that we need a new sample
        hr = m_pSourceStream->RequestSample(NULL);
        BREAK_ON_FAIL(hr);

        // wait forever for a new sample to arrive from the source - this event is set in
        // the HandleSourceStreamEvent() function, when it receives a MEMediaSample event
        WaitForSingleObject(m_sourceSampleReadyEvent, INFINITE);

        // at this point we should have a sample from the source ready for consumption
        BREAK_ON_NULL(m_pReadySourceSample, E_UNEXPECTED);

        // grab the sample produced by the source and consume it
        *ppNewSample = m_pReadySourceSample.Detach();

        // reset the event
        ResetEvent(m_sourceSampleReadyEvent);
    }
    while(false);

    return hr;
}