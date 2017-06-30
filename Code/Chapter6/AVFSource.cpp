#include "stdafx.h"
#include "AVFSource.h"


//
// Create an instance of the AVFSource object
//
HRESULT AVFSource::CreateInstance(AVFSource** ppAVFSource)
{
    HRESULT hr = S_OK;
    AVFSource* pAVFSource = NULL;

    do
    {
        BREAK_ON_NULL (ppAVFSource, E_POINTER);

        // create an instance of the object
        AVFSource* pAVFSource = new (std::nothrow) AVFSource(&hr);
        BREAK_ON_NULL(pAVFSource, E_OUTOFMEMORY);        
        BREAK_ON_FAIL(hr);

        // Set the out pointer.
        *ppAVFSource = pAVFSource;
    }
    while(false);

    if (FAILED(hr) && pAVFSource != NULL)
    {
        delete pAVFSource;
    }

    return hr;
}



//
// Standard IUnknown interface implementation
//
ULONG AVFSource::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG AVFSource::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT AVFSource::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(static_cast<IMFMediaEventGenerator*>(this));
    }
    else if (riid == IID_IMFMediaEventGenerator)
    {
        *ppv = static_cast<IMFMediaEventGenerator*>(this);
    }
    else if (riid == IID_IMFMediaSource)
    {
        *ppv = static_cast<IMFMediaSource*>(this);
    }
    else if (riid == IID_IMFAsyncCallback)
    {
        *ppv = static_cast<IMFAsyncCallback*>(this);
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



//
//  IMFMediaEventGenerator interface implementation
//
////////////////////////////////////////////////////////////////////////


//
// Begin processing an event from the event queue asynchronously
//
HRESULT AVFSource::BeginGetEvent(
            IMFAsyncCallback* pCallback,    // callback of the object interested in events
            IUnknown* punkState)            // some custom state object returned with event
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaEventQueue> pLocQueue = m_pEventQueue;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // make sure the source is not shut down
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // get the next event from the event queue
        hr = pLocQueue->BeginGetEvent(pCallback, punkState);
    }
    while(false);

    return hr;
}

//
// Complete asynchronous event processing
//
HRESULT AVFSource::EndGetEvent(
            IMFAsyncResult* pResult,    // result of an asynchronous operation
            IMFMediaEvent** ppEvent)    // event extracted from the queue
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaEventQueue> pLocQueue = m_pEventQueue;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // make sure the source is not shut down
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        hr = pLocQueue->EndGetEvent(pResult, ppEvent);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Synchronously retrieve the next event from the event queue
//
HRESULT AVFSource::GetEvent(
            DWORD dwFlags,              // flag with the event behavior
            IMFMediaEvent** ppEvent)    // event extracted from the queue
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaEventQueue> pLocQueue = m_pEventQueue;

    do
    {
        // Check whether the source is shut down but do it in a separate locked section-
        // GetEvent() may block, and we don't want to hold the critical section during
        // that time.
        {
            CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

            hr = CheckShutdown();
            BREAK_ON_FAIL(hr);
        }

        // Get the event from the queue - note, may block!
        hr = pLocQueue->GetEvent(dwFlags, ppEvent);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}

//
// Store the event in the internal event queue of the source
//
HRESULT AVFSource::QueueEvent(
            MediaEventType met,             // media event type
            REFGUID guidExtendedType,       // GUID_NULL for standard events or an extension GUID
            HRESULT hrStatus,               // status of the operation
            const PROPVARIANT* pvValue)     // a VARIANT with some event value or NULL
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // make sure the source is not shut down
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // queue the passed in event on the internal event queue
        hr = m_pEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}





/////////////////////////////////////////////////////////////////////////
// IMFAsyncCallback implementation
/////////////////////////////////////////////////////////////////////////

//
// Get the behavior information (duration, etc.) of the asynchronous callback operation - 
// not implemented.
//
HRESULT AVFSource::GetParameters(DWORD*, DWORD*)
{
    return E_NOTIMPL;
}


// 
// Do an asynchronous task - execute a queued command (operation).
//
HRESULT AVFSource::Invoke(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;
    CComPtr<IMFAsyncResult> pAsyncResult = pResult;
    CComPtr<IMFAsyncResult> pCallerResult;
    CComPtr<ISourceOperation> pCommand;
    CComPtr<IUnknown> pState;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // Get the state object associated with this asynchronous call
        hr = pAsyncResult->GetState(&pState);
        BREAK_ON_FAIL(hr);

        // QI the IUnknown state variable for the ISourceOperation interface
        hr = pState->QueryInterface(IID_ISourceOperation, (void**)&pCommand);
        BREAK_ON_FAIL(hr);

        // Make sure the source is not shut down - if the source is shut down, just exit
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // figure out what the requested command is, and then dispatch it to one of the 
        // internal handler objects
        switch (pCommand->Type())
        {
            case SourceOperationOpen:
                hr = InternalOpen(pCommand);
                break;
            case SourceOperationStart:
                hr = InternalStart(pCommand);
                break;
            case SourceOperationStop:
                hr = InternalStop();
                break;
            case SourceOperationPause:
                hr = InternalPause();
                break;
            case SourceOperationStreamNeedData:
                hr = InternalRequestSample();
                break;
            case SourceOperationEndOfStream:
                hr = InternalEndOfStream();
                break;
        }
    }
    while(false);

    return hr;
}






//////////////////////////////////////////////////////////////////////////////////////////
//
//  IMFMediaSource interface implementation
//
/////////////////////////////////////////////////////////////////////////////////////////



//
// Get a descriptor object for the current presentation
//
HRESULT AVFSource::CreatePresentationDescriptor(IMFPresentationDescriptor** ppPresentationDescriptor)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // sanity check parameters and internal variables
        BREAK_ON_NULL (ppPresentationDescriptor, E_POINTER);
        BREAK_ON_NULL (m_pPresentationDescriptor, MF_E_NOT_INITIALIZED);

        // make sure the source is not shut down
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // create a copy of the stored presentation descriptor
        hr = m_pPresentationDescriptor->Clone(ppPresentationDescriptor);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}

//
// Get the characteristics and capabilities of the MF source 
// 
HRESULT AVFSource::GetCharacteristics(DWORD* pdwCharacteristics)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // sanity check the passed in parameter
        BREAK_ON_NULL (pdwCharacteristics, E_POINTER);

        // make sure the source is not shut down
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // Indicate that the source can pause and supports seeking
        *pdwCharacteristics = MFMEDIASOURCE_CAN_PAUSE | MFMEDIASOURCE_CAN_SEEK;
    }
    while(false);

    return hr;
}

//
//  Start playback at the specified time
//
HRESULT AVFSource::Start(IMFPresentationDescriptor* pPresentationDescriptor,
            const GUID* pguidTimeFormat,            // format of the following time variable
            const PROPVARIANT* pvarStartPosition)   // stream time where to start playback
{
    HRESULT hr = S_OK;
    bool isSeek = false;
    CComPtr<ISourceOperation> pOperation;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        BREAK_ON_NULL (pvarStartPosition, E_INVALIDARG);
        BREAK_ON_NULL (pPresentationDescriptor, E_INVALIDARG);

        // The IMFMediaSource::Start() function can support various time formats for input,
        // but this implementation supports only the default version - time indicated in 
        // 100-ns units
        if ((pguidTimeFormat != NULL) && (*pguidTimeFormat != GUID_NULL))
        {
            hr = MF_E_UNSUPPORTED_TIME_FORMAT;
            break;
        }

        // make sure we have the start time in the pvarStartPosition PROPVARIANT structure
        if ((pvarStartPosition->vt != VT_I8) && (pvarStartPosition->vt != VT_EMPTY))
        {
            hr = MF_E_UNSUPPORTED_TIME_FORMAT;
            break;
        }

        // make sure the source is not shut down
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // make sure the source is initialized
        hr = IsInitialized();
        BREAK_ON_FAIL(hr);

        // figure out whether the caller is trying to seek or to just start playback
        if (pvarStartPosition->vt == VT_I8)
        {
            if (m_state != SourceStateStopped)
            {
                isSeek = true;
            }
        }

        // create the new command that will tell us to start playback
        pOperation = new (std::nothrow) SourceOperation(
            SourceOperationStart,                   // store command type - start command
            pPresentationDescriptor);               // store presentation descriptor param
        BREAK_ON_NULL(pOperation, E_OUTOFMEMORY);

        // set the internal information in the new command
        hr = pOperation->SetData(*pvarStartPosition, isSeek);
        BREAK_ON_FAIL(hr);

        // queue the start command work item
        hr = MFPutWorkItem(
                MFASYNC_CALLBACK_QUEUE_STANDARD,        // work queue to use
                this,                                   // IMFAsyncCallback object to call
                static_cast<IUnknown*>(pOperation));    // state variable - the command
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Pause the source
//
HRESULT AVFSource::Pause()
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
    CComPtr<ISourceOperation> pOperation;

    do
    {
        // make sure the source is not shut down
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // create a new SourceOperationType command
        pOperation = new (std::nothrow) SourceOperation(SourceOperationPause);
        BREAK_ON_NULL (pOperation, E_OUTOFMEMORY);

        // put the command on the work queue
        hr = MFPutWorkItem(
                MFASYNC_CALLBACK_QUEUE_STANDARD,        // work queue to use
                this,                                   // IMFAsyncCallback object to call
                static_cast<IUnknown*>(pOperation));    // state variable - the command
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Stop playback
//
HRESULT AVFSource::Stop()
{
    HRESULT hr = S_OK;
    CComPtr<ISourceOperation> pOperation;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // make sure the source is not shut down
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // make sure the source is initialized
        hr = IsInitialized();
        BREAK_ON_FAIL(hr);

        // create a new SourceOperationType command
        pOperation = new (std::nothrow) SourceOperation(SourceOperationStop);
        BREAK_ON_NULL (pOperation, E_OUTOFMEMORY);

        // put the command on the work queue
        hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, this, static_cast<IUnknown*>(pOperation));
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}




//
//  Shutdown the source
//
HRESULT AVFSource::Shutdown()
{
    HRESULT hr = S_OK;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // make sure the source is not shut down
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // go through every underlying stream and send a shutdown command to it
        for (DWORD x = 0; x < m_mediaStreams.size(); x++)
        {
            AVFStream* pStream = NULL;

            pStream = m_mediaStreams[x];
            BREAK_ON_FAIL(hr);
            BREAK_ON_NULL(pStream, E_UNEXPECTED);

            pStream->Shutdown();
            pStream->Release();
        }
        BREAK_ON_FAIL(hr);

        // shut down the event queue
        if (m_pEventQueue)
        {
            m_pEventQueue->Shutdown();
        }

        // clear the vector of streams
        EXCEPTION_TO_HR( m_mediaStreams.clear() );

        // delete the AVI file parser object
        if (m_pAVIFileParser)
        {
            delete m_pAVIFileParser;
            m_pAVIFileParser = NULL;
        }

        // set the internal state variable to indicate that the source is shut down
        m_state = SourceStateShutdown;
    }
    while(false);

    return hr;
}









/////////////////////////////////////////////////////////////////////////
// Public helper methods
/////////////////////////////////////////////////////////////////////////

//
// Begin the asynchronous open operation
//
HRESULT AVFSource::BeginOpen(LPCWSTR pwszURL, IMFAsyncCallback* pCallback, IUnknown* pUnkState)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
    CComPtr<IMFAsyncResult> pResult;
    CComPtr<ISourceOperation> pOperation;

    do
    {
        // Pack the needed arguments into an AsyncResult object
        hr = MFCreateAsyncResult(NULL, pCallback, pUnkState, &pResult);
        BREAK_ON_FAIL(hr);

        // create a new SourceOperation object with the Open command
        pOperation = new (std::nothrow) SourceOperation(SourceOperationOpen, pwszURL, pResult);
        BREAK_ON_NULL (pOperation, E_OUTOFMEMORY);

        // Submit the request into the background thread
        hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, this, pOperation);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// End the asynchronous open operation
//
HRESULT AVFSource::EndOpen(IMFAsyncResult* pAsyncResult)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL (pAsyncResult, E_POINTER);

        hr = pAsyncResult->GetStatus();
    }
    while(false);

    return hr;
}


//
// Return an error if the source is shut down, and S_OK otherwise
//
HRESULT AVFSource::CheckShutdown() const
{
    if (m_state == SourceStateShutdown)
    {
        return MF_E_SHUTDOWN;
    }
    else
    {
        return S_OK;
    }
}

// 
// Return a failure if the source is not initialized
//
HRESULT AVFSource::IsInitialized() const
{
    if (m_state == SourceStateOpening || m_state == SourceStateUninitialized)
    {
        return MF_E_NOT_INITIALIZED;
    }
    else
    {
        return S_OK;
    }
}











/////////////////////////////////////////////////////////////////////////
// Private helper methods
/////////////////////////////////////////////////////////////////////////



//
// Helper function that schedules the passed-in command on the work queue
//
HRESULT AVFSource::SendOperation(SourceOperationType operationType)
{
    HRESULT hr = S_OK;
    CComPtr<ISourceOperation> pOperation;

    do
    {
        // create a new SourceOperationType command
        pOperation = new (std::nothrow) SourceOperation(operationType);
        BREAK_ON_NULL (pOperation, E_OUTOFMEMORY);

        // queue the command on the queue
        hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, this, static_cast<IUnknown*>(pOperation));
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Initialize the underlying AVFFileParser, and open the file in the operation object.
//
HRESULT AVFSource::InternalOpen(ISourceOperation* pOp)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
    WCHAR* pUrl = NULL;
    CComPtr<ISourceOperation> pOperation = pOp;
    CComPtr<IMFAsyncResult> pCallerResult;
    
    do
    {
        BREAK_ON_NULL(pOperation, E_UNEXPECTED);

        // Get the async result that will be sent once the open operation is complete.
        hr = pOperation->GetCallerAsyncResult(&pCallerResult);
        BREAK_ON_FAIL(hr);
        BREAK_ON_NULL(pCallerResult, E_UNEXPECTED);

        // get the file URL from the operation
        pUrl = pOperation->GetUrl();
        BREAK_ON_NULL(pUrl, E_UNEXPECTED);

        // Create the AVI parser
        hr = AVIFileParser::CreateInstance(pUrl, &m_pAVIFileParser);
        BREAK_ON_FAIL(hr);

        // parse the file header and instantiate the individual stream objects
        hr = ParseHeader();
    }
    while(false);

    // return the result whether we succeeded or failed
    if(pCallerResult != NULL)
    {
        // Store the result of the initialization operation in the caller result.
        pCallerResult->SetStatus(hr);

        // Notify the caller of the BeginOpen() method that the open is complete.
        MFInvokeCallback(pCallerResult);
    }

    return hr;
}


//
// Start playback or seek to the specified location.
//
HRESULT AVFSource::InternalStart(ISourceOperation* pCommand)
{
    HRESULT hr = S_OK;
    CComPtr<IMFPresentationDescriptor> pPresentationDescriptor;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // get the presentation descriptor from the start operation
        hr = pCommand->GetPresentationDescriptor(&pPresentationDescriptor);
        BREAK_ON_FAIL(hr);

        // activate the streams associated with the presentation descriptor
        hr = SelectStreams(pPresentationDescriptor, pCommand->GetData(), 
            pCommand->IsSeek());
        BREAK_ON_FAIL(hr);

        // set the start position in the file
        hr = m_pAVIFileParser->SetOffset(pCommand->GetData());
        BREAK_ON_FAIL(hr);

        // update the internal state variable
        m_state = SourceStateStarted;

        // we have just started - which means that none of the streams have hit the
        // end of stream indicator yet.  Once all of the streams have ended, the source
        // will stop.
        m_pendingEndOfStream = 0;
        
        // if we got here, then everything succeed.  If this was a seek request, queue the 
        // result of the seek command.  If this is a start request, queue the result of the
        // start command.
        if (pCommand->IsSeek())
        {
            hr = m_pEventQueue->QueueEventParamVar(
                        MESourceSeeked,                // seek result
                        GUID_NULL,                     // no extended event data
                        S_OK,                          // succeeded
                        &pCommand->GetData());         // operation object
        }
        else
        {
            hr = m_pEventQueue->QueueEventParamVar(
                        MESourceStarted,               // start result
                        GUID_NULL,                     // no extended event data
                        S_OK,                          // succeeded
                        &pCommand->GetData());         // operation object
        }
    }
    while(false);

    // if we failed, fire an event indicating status of the operation - there is no need to fire
    // status if start succeeded, since that would have been handled above in the while loop
    if (FAILED(hr))
    {
        m_pEventQueue->QueueEventParamVar(MESourceStarted, GUID_NULL, hr, NULL);
    }

    return hr;
}



//
// Internal implementation of the stop command - needed to asynchronously handle stop
//
HRESULT AVFSource::InternalStop(void)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // go through every stream, and if it is active, send a stop command to it
        for (DWORD x = 0; x < m_mediaStreams.size(); x++)
        {
            AVFStream* pStream = NULL;
            
            pStream = m_mediaStreams[x];
            BREAK_ON_FAIL(hr);
            BREAK_ON_NULL(pStream, E_UNEXPECTED);

            if (pStream->IsActive())
            {
                hr = pStream->Stop();
                BREAK_ON_FAIL(hr);
            }
        }
        BREAK_ON_FAIL(hr);

        // update the internal state variable
        m_state = SourceStateStopped;
    }
    while(false);

    // fire an event indicating status of the operation
    m_pEventQueue->QueueEventParamVar(MESourceStopped, GUID_NULL, hr, NULL);

    return hr;
}


//
// Internal implementation of the pause command - needed to asynchronously handle pause
//
HRESULT AVFSource::InternalPause(void)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // if the source is not started, it cannot be paused
        if (m_state != SourceStateStarted)
        {
            hr = MF_E_INVALID_STATE_TRANSITION;
            break;
        }

        // go through every stream, and if it is active, send a pause command to it
        for (DWORD x = 0; x < m_mediaStreams.size(); x++)
        {
            AVFStream* pStream = NULL;
            
            pStream = m_mediaStreams[x];
            BREAK_ON_FAIL(hr);
            BREAK_ON_NULL(pStream, E_UNEXPECTED);

            if (pStream->IsActive())
            {
                hr = pStream->Pause();
                BREAK_ON_FAIL(hr);
            }
        }
        BREAK_ON_FAIL(hr);

        // update the internal state variable
        m_state = SourceStatePaused;
    }
    while(false);

    // fire an event indicating status of the operation
    m_pEventQueue->QueueEventParamVar(MESourcePaused, GUID_NULL, hr, NULL);

    return hr;
}


//
// Handle the request sample command - pull out a sample of the right type from the underlying
// AVI file parser, and send it to the stream
//
HRESULT AVFSource::InternalRequestSample(void)
{
    bool needMoreData = false;
    HRESULT hr = S_OK;

    do
    {
        needMoreData = false;

        // go through each of the streams, figure out if they need data, pull out a sample 
        // from the underlying AVI file parser, and deliver it to the right stream
        for (DWORD x = 0; x < m_mediaStreams.size(); x++)
        {
            AVFStream* pStream = NULL;
            
            pStream = m_mediaStreams[x];
            BREAK_ON_FAIL(hr);
            BREAK_ON_NULL(pStream, E_UNEXPECTED);

            // if the current stream needs more data, process its requests
            if (pStream->NeedsData())
            {
                // store a flag indicating that somebody did need data
                needMoreData = true;

                // call a function to send a sample to the stream
                hr = SendSampleToStream(pStream);
                BREAK_ON_FAIL(hr);
            }
        }

        BREAK_ON_FAIL(hr);

        // loop while some stream needs more data - stop only once none of the streams are
        // requesting more samples
    } 
    while (needMoreData);

    if(FAILED(hr))
    {
        QueueEvent(MEError, GUID_NULL, hr, NULL);
    }

    return hr;
}


//
// Load a sample of the right type from the parser and send it to the passed-in stream
//
HRESULT AVFSource::SendSampleToStream(AVFStream* pStream)
{
    HRESULT hr = S_OK;
    CComPtr<IMFSample> pSample;

    do
    {
        // if this is a video stream, then get a video sample from the AVI file parser
        // if this is an audio stream, get an audio sample
        if (pStream->IsVideoStream())
        {                        
            // get a video sample from the underlying AVI parser
            hr = m_pAVIFileParser->GetNextVideoSample(&pSample);
            BREAK_ON_FAIL(hr);

            // deliver the video sample
            hr = pStream->DeliverSample(pSample);
            BREAK_ON_FAIL(hr);

            // if this is the end of the video stream, tell the stream that there
            // are no more samples
            if (m_pAVIFileParser->IsVideoEndOfStream())
            {
                hr = pStream->EndOfStream();
                BREAK_ON_FAIL(hr);
            }
        }
        else if (pStream->IsAudioStream())
        {
            // get a audio sample from the underlying AVI parser
            hr = m_pAVIFileParser->GetNextAudioSample(&pSample);
            BREAK_ON_FAIL(hr);

            // deliver the audio sample
            hr = pStream->DeliverSample(pSample);
            BREAK_ON_FAIL(hr);

            // if this is the end of the audio stream, tell the stream that there
            // are no more samples
            if (m_pAVIFileParser->IsAudioEndOfStream())
            {
                hr = pStream->EndOfStream();
                BREAK_ON_FAIL(hr);
            }
        }
    }
    while(false);

    return hr;
}



//
// Handle an end of stream event
//
HRESULT AVFSource::InternalEndOfStream(void)
{
    HRESULT hr = S_OK;

    // increment the counter which indicates how many streams have signaled their end
    m_pendingEndOfStream++;

    // if all of the streams have ended, fire an MEEndOfPresentation event
    if (m_pendingEndOfStream == m_mediaStreams.size())
    {
        hr = m_pEventQueue->QueueEventParamVar(MEEndOfPresentation, GUID_NULL, S_OK, NULL);
    }

    return hr;
}



//
// Parse the file header of the current file
//
HRESULT AVFSource::ParseHeader(void)
{
    HRESULT hr = S_OK;

    do
    {
        // Sanity checks against input arguments.
        BREAK_ON_NULL (m_pAVIFileParser, E_UNEXPECTED);

        // Parse the AVI file header.
        hr = m_pAVIFileParser->ParseHeader();
        BREAK_ON_FAIL(hr);

        // Create the individual streams from the presentation descriptor
        hr = InternalCreatePresentationDescriptor();
        BREAK_ON_FAIL(hr);
    }
    while(false);

    // if the header was successfully parsed, and if the streams have been created,
    // update the internal state variables
    if (SUCCEEDED(hr))
    {
        m_state = SourceStateStopped;
    }

    return hr;
}



//
// Activate the streams exposed by the source
//
HRESULT AVFSource::SelectStreams(IMFPresentationDescriptor *pPresentationDescriptor, const PROPVARIANT varStart, bool isSeek)
{
    HRESULT hr = S_OK;
    BOOL selected = FALSE;
    bool wasSelected = false;
    DWORD streamId = 0;

    CComPtr<IMFStreamDescriptor> pStreamDescriptor;
    CComPtr<IUnknown> pUnkStream;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
        
        for (DWORD x = 0; x < m_mediaStreams.size(); x++)
        {
            AVFStream* pStream = NULL;
            
            pStream = m_mediaStreams[x];
            BREAK_ON_FAIL(hr);
            BREAK_ON_NULL(pStream, E_UNEXPECTED);

            pStreamDescriptor = NULL;

            // get a stream descriptor for the current stream from the presentation descriptor 
            hr = pPresentationDescriptor->GetStreamDescriptorByIndex(x, &selected, &pStreamDescriptor);
            BREAK_ON_FAIL(hr);

            // get the stream ID from the stream descriptor
            hr = pStreamDescriptor->GetStreamIdentifier(&streamId);
            BREAK_ON_FAIL(hr);

            if (pStream == NULL)
            {
                hr = E_INVALIDARG;
                break;
            }

            // figure out if the current stream was already selected - IE if it was active
            wasSelected = pStream->IsActive();

            // activate the stream
            pStream->Activate(selected == TRUE);

            // get the IUnknown pointer for the AVFStream object
            pUnkStream = pStream;
            
            // if the stream was selected queue the an event with the result of the operation
            if (selected)
            {
                // if the stream was already selected, something must have been updated - fire the
                // MEUpdatedStream event
                if (wasSelected)
                {
                    hr = m_pEventQueue->QueueEventParamUnk(
                                MEUpdatedStream,              // event - the stream was updated
                                GUID_NULL,                    // basic event - no extension GUID
                                hr,                           // result of the operation
                                pUnkStream);                 // pointer to the stream
                }
                else
                {
                    hr = m_pEventQueue->QueueEventParamUnk(
                                MENewStream,                  // the new stream was activated
                                GUID_NULL,                    // basic event - no extension GUID
                                hr,                           // result of the operation
                                pUnkStream);                 // pointer to the stream
                }

                // start playing the stream
                hr = pStream->Start(varStart, isSeek);
                BREAK_ON_FAIL(hr);
            }
        }
    }
    while(false);

    return hr;
}



//
// Create the presentation descriptor object
//
HRESULT AVFSource::InternalCreatePresentationDescriptor(void)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    IMFStreamDescriptor* streamDescriptors[2];

    do
    {
        BREAK_ON_NULL (m_pAVIFileParser, E_POINTER);

        // make sure this is a supported file format
        if (!m_pAVIFileParser->IsSupportedFormat())
        {
            hr = MF_E_INVALID_FORMAT;
            break;
        }

        // create a video stream descriptor if there is video in the file
        hr = CreateVideoStream(&(streamDescriptors[m_mediaStreams.size()]));
        BREAK_ON_FAIL(hr);

        // create an audio stream descriptor if there is audio in the file
        hr = CreateAudioStream(&(streamDescriptors[m_mediaStreams.size()]));
        BREAK_ON_FAIL(hr);

        // if we got here, we have successfully created at a stream descriptor for the audio 
        // stream (if there is one), and a video stream (if the file has video).  Now create
        // the presentation descriptor which will hold the stream descriptors.
        hr = MFCreatePresentationDescriptor(
                    (DWORD)m_mediaStreams.size(),   // number of streams created
                    streamDescriptors,              // array of stream descriptors
                    &m_pPresentationDescriptor);   // get the presentation descriptor
        BREAK_ON_FAIL(hr);


        // activate all of the streams in the beginning - that's their default state
        for (DWORD i = 0; i < m_mediaStreams.size(); i++)
        {
            hr = m_pPresentationDescriptor->SelectStream(i);
            BREAK_ON_FAIL(hr);
        }
        BREAK_ON_FAIL(hr);

        // get the duration of the file from the AVI file parser
        LONGLONG fileDuration = m_pAVIFileParser->Duration();

        // set the file duration on the presentation descriptor - length of the file
        // in 100-ns units
        hr = m_pPresentationDescriptor->SetUINT64(MF_PD_DURATION, (UINT64)(fileDuration));
    }
    while(false);

    // all of the stream descriptors have now been stored in the presentation descriptor -
    // therefore release all of the stream descriptor pointers we have left over
    for (DWORD i = 0; i < m_mediaStreams.size(); i++)
    {
        SafeRelease(streamDescriptors[i]);
    }

    return hr;
}


//
// Create the video stream and return the corresponding stream descriptor
//
HRESULT AVFSource::CreateVideoStream(IMFStreamDescriptor** ppStreamDescriptor)
{
    HRESULT hr = S_OK;

    IMFMediaType* pMediaType = NULL;
    AVFStream* pAVFStream = NULL;

    do
    {
        // if the file has a video stream, create an AVFStream object and a stream 
        // descriptor for it
        if (m_pAVIFileParser->HasVideo())
        {
            CComPtr<IMFMediaTypeHandler> pHandler;

            // get the media type for the video stream
            hr = m_pAVIFileParser->GetVideoMediaType(&pMediaType);
            BREAK_ON_FAIL(hr);

            // create the stream descriptor
            hr = MFCreateStreamDescriptor(
                        (DWORD)m_mediaStreams.size()+1,// stream ID
                        1,                             // number of media types
                        &pMediaType,                   // media type for the stream
                        ppStreamDescriptor);           // get the descriptor
            BREAK_ON_FAIL(hr);

            // get a media type handler for the stream
            hr = (*ppStreamDescriptor)->GetMediaTypeHandler(&pHandler);
            BREAK_ON_FAIL(hr);

            // set current type of the stream visible to source users
            hr = pHandler->SetCurrentMediaType(pMediaType);
            BREAK_ON_FAIL(hr);

            // Create AVFStream object that is implementing the IMFMediaStream interface
            hr = AVFStream::CreateInstance(&pAVFStream, this, *ppStreamDescriptor);
            BREAK_ON_FAIL(hr);

            // tell the AVFStream object that it's a video stream
            pAVFStream->SetVideoStream();
            
            // store the stream in a vector for later reuse
            EXCEPTION_TO_HR( m_mediaStreams.push_back(pAVFStream) );
        }
    }
    while(false);

    SafeRelease(pMediaType);

    return hr;
}



//
// Create the audio stream and return the corresponding stream descriptor
//
HRESULT AVFSource::CreateAudioStream(IMFStreamDescriptor** ppStreamDescriptor)
{
    HRESULT hr = S_OK;

    IMFMediaType* pMediaType = NULL;
    AVFStream* pAVFStream = NULL;

    do
    {
        // if the file has an audio stream, create a stream descriptor for an audio stream 
        // and store it in the presentation descriptor
        if (m_pAVIFileParser->HasAudio())
        {
            CComPtr<IMFMediaTypeHandler> pHandler;

            // get the media type for the stream from the AVI file parser
            hr = m_pAVIFileParser->GetAudioMediaType(&pMediaType);
            BREAK_ON_FAIL(hr);

            // create the stream descriptor
            hr = MFCreateStreamDescriptor(
                        (DWORD)m_mediaStreams.size()+1,  // stream ID
                        1,                               // number of media types
                        &pMediaType,                     // media type for the stream
                        ppStreamDescriptor);             // get the descriptor
            BREAK_ON_FAIL(hr);

            // get a media type handler for the stream 
            hr = (*ppStreamDescriptor)->GetMediaTypeHandler(&pHandler);
            BREAK_ON_FAIL(hr);

            // set current type of the stream visible to source users
            hr = pHandler->SetCurrentMediaType(pMediaType);
            BREAK_ON_FAIL(hr);

            // Create our Audio IMFMediaStream.
            hr = AVFStream::CreateInstance(&pAVFStream, this, (*ppStreamDescriptor));
            BREAK_ON_FAIL(hr);

            // set the current media type for the stream
            pAVFStream->SetAudioStream();
            
            // store the stream in a vector for later reuse
            EXCEPTION_TO_HR( m_mediaStreams.push_back(pAVFStream) );
        }
    }
    while(false);

    SafeRelease(pMediaType);

    return hr;
}



//
// Constructor
//
AVFSource::AVFSource(HRESULT* pHr) : 
    m_cRef(1),
    m_pAVIFileParser(NULL),
    m_state(SourceStateUninitialized),
    m_pendingEndOfStream(0)
{
    // Initialize the event queue that will execute all of the source's
    // IMFEventGenerator duties.
    *pHr = MFCreateEventQueue(&m_pEventQueue);
}

//
// Destructor
//
AVFSource::~AVFSource()
{
    if (NULL != m_pAVIFileParser)
    {
        delete m_pAVIFileParser;
        m_pAVIFileParser = NULL;
    }
}