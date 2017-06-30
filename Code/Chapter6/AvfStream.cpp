#include "stdafx.h"
#include "AVFStream.h"
#include "AvfSource.h"

//
// Create an instance of the AVFStream object with the specified presentation descriptor
//
HRESULT AVFStream::CreateInstance(AVFStream** ppMediaStream, AVFSource *pMediaSource, IMFStreamDescriptor *pStreamDescriptor)
{
    HRESULT hr = S_OK;
    AVFStream* pMediaStream = NULL;

    do
    {
        BREAK_ON_NULL(ppMediaStream, E_POINTER);
        BREAK_ON_NULL(pMediaSource, E_INVALIDARG);
        BREAK_ON_NULL(pStreamDescriptor, E_INVALIDARG);

        *ppMediaStream = NULL;

        // create a new stream object
        pMediaStream = new AVFStream();
        BREAK_ON_NULL(pMediaStream, E_OUTOFMEMORY);

        // initialize the stream with the stream descriptor
        hr = pMediaStream->Init(pMediaSource, pStreamDescriptor);
        BREAK_ON_FAIL (hr);

        // if we got here, everything succeeded - set the output pointer to the new stream
        *ppMediaStream = pMediaStream;
    }
    while(false);

    if (FAILED(hr))
    {
        delete pMediaStream;
    }

    return hr;
}


//////////////////////////////////////////////////////////////////////////////////////////
//
//  IUnknown interface implementation
//
/////////////////////////////////////////////////////////////////////////////////////////

ULONG AVFStream::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG AVFStream::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT AVFStream::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(this);
    }
    else if (riid == IID_IMFMediaEventGenerator)
    {
        *ppv = static_cast<IMFMediaEventGenerator*>(this);
    }
    else if (riid == IID_IMFMediaStream)
    {
        *ppv = static_cast<IMFMediaStream*>(this);
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




//////////////////////////////////////////////////////////////////////////////////////////
//
//  IMFMediaEventGenerator interface implementation
//
//////////////////////////////////////////////////////////////////////////////////////////

//
// Begin asynchronous event processing of the next event in the queue
//
HRESULT AVFStream::BeginGetEvent(IMFAsyncCallback* pCallback,IUnknown* punkState)
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    return m_pEventQueue->BeginGetEvent(pCallback, punkState);
}

//
// Complete asynchronous event processing
//
HRESULT AVFStream::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    return m_pEventQueue->EndGetEvent(pResult, ppEvent);
}


//
// Get the next event in the event queue
//
HRESULT AVFStream::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
    
    return m_pEventQueue->GetEvent(dwFlags, ppEvent);
}


//
// Add a new event to the event queue
//
HRESULT AVFStream::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    return m_pEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
}








//////////////////////////////////////////////////////////////////////////////////////////
//
//  IMFMediaStream interface implementation
//
//////////////////////////////////////////////////////////////////////////////////////////


//
// Get the source associated with this stream
//
HRESULT AVFStream::GetMediaSource(IMFMediaSource** ppMediaSource)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        BREAK_ON_NULL (ppMediaSource, E_POINTER);
        BREAK_ON_NULL (m_pMediaSource, E_UNEXPECTED);

        hr = m_pMediaSource->QueryInterface(IID_IMFMediaSource, (void**)ppMediaSource);
    }
    while(false);

    return hr;
}


//
// Get the stream descriptor for this stream
//
HRESULT AVFStream::GetStreamDescriptor(IMFStreamDescriptor** ppStreamDescriptor)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        BREAK_ON_NULL (ppStreamDescriptor, E_POINTER);
        BREAK_ON_NULL (m_pStreamDescriptor, E_UNEXPECTED);

        hr = m_pStreamDescriptor.CopyTo(ppStreamDescriptor);
    }
    while(false);

    return hr;
}


//
// Request the next sample from the stream.
//
HRESULT AVFStream::RequestSample(IUnknown* pToken)
{
    HRESULT hr = S_OK;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // make sure the stream is not shut down
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // make sure the stream is not stopped
        if (m_state == SourceStateStopped)
        {
            hr = MF_E_MEDIA_SOURCE_WRONGSTATE;
            break;
        }

        // check for the end of stream - fire an end of stream event only if there
        // are no more samples, and we received an end of stream notification
        if (m_endOfStream && m_pSampleList.IsEmpty())
        {
            hr = MF_E_END_OF_STREAM;
            break;
        }

        // Add the token to the CInterfaceList even if it is NULL
        EXCEPTION_TO_HR ( m_pTokenList.AddTail(pToken) );
        BREAK_ON_FAIL(hr);

        // increment the number of requested samples
        m_nSamplesRequested++;

        // dispatch the samples
        hr = DispatchSamples();
    }
    while(false);

    // if something failed and we are not shut down, fire an event indicating the error
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
        if (FAILED(hr) && (m_state != SourceStateShutdown))
        {
            hr = m_pMediaSource->QueueEvent(MEError, GUID_NULL, hr, NULL);
        }
    }

    return hr;
}


//
// Dispatch samples stored in the stream object, and request samples if more are needed.
//
HRESULT AVFStream::DispatchSamples(void)
{
    HRESULT hr = S_OK;

    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // if the stream is not started, just exit
        if (m_state != SourceStateStarted)
        {
            hr = S_OK;
            break;
        }

        // send out the samples
        hr = SendSamplesOut();
        BREAK_ON_FAIL(hr);

        // if there are no more samples stored in the stream, and if we have been notified
        // that this is the end of stream, send the end of stream events.  Otherwise, if 
        // the stream needs more data, request additional data from the source.
        if (m_pSampleList.IsEmpty() && m_endOfStream)
        {
            // send the end of stream event to anyone listening to this stream
            hr = m_pEventQueue->QueueEventParamVar(MEEndOfStream, GUID_NULL, S_OK, NULL);
            BREAK_ON_FAIL(hr);

            // tell the source that the end of stream has been reached
            hr = m_pMediaSource->SendOperation(SourceOperationEndOfStream);
            BREAK_ON_FAIL(hr);
        }        
        else if (NeedsData())   
        {
            // send an event to the source indicating that a stream needs more data
            hr = m_pMediaSource->SendOperation(SourceOperationStreamNeedData);
            BREAK_ON_FAIL(hr);
        }
    }
    while(false);

    // if there was a failure, queue an MEError event
    if (FAILED(hr) && (m_state != SourceStateShutdown))
    {
        m_pMediaSource->QueueEvent(MEError, GUID_NULL, hr, NULL);
    }

    return hr;
}


//
// Send out events with samples
//
HRESULT AVFStream::SendSamplesOut(void)
{
    HRESULT hr = S_OK;
    CComPtr<IUnknown> pUnkSample;
    CComPtr<IMFSample> pSample;
    CComPtr<IUnknown> pToken;

    do
    {
        // loop while there are samples in the stream object, and while samples have been 
        // requested
        while (!m_pSampleList.IsEmpty() && m_nSamplesRequested > 0)
        {
            // reset the pUnkSample variable
            pUnkSample = NULL;

            // get the next sample and a sample token
            EXCEPTION_TO_HR( pSample = m_pSampleList.RemoveHead() );
            BREAK_ON_FAIL(hr);
            
            // if there are tokens, then get one, and associate it with the sample
            if(!m_pTokenList.IsEmpty())
            {
                EXCEPTION_TO_HR( pToken = m_pTokenList.RemoveHead() );
                BREAK_ON_FAIL(hr);

                // if there is a sample token, store it in the sample
                hr = pSample->SetUnknown(MFSampleExtension_Token, pToken);
                BREAK_ON_FAIL(hr);
            }

            // get the IUnknown pointer for the sample
            pUnkSample = pSample;
            
            // queue an event indicating that a new sample is available, and pass it a 
            // pointer to  the sample
            hr = m_pEventQueue->QueueEventParamUnk(MEMediaSample, GUID_NULL, S_OK, 
                pUnkSample); 
            BREAK_ON_FAIL(hr);
            
            // decrement the counter indicating how many samples have been requested
            m_nSamplesRequested--;
        }
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Receive a sample for the stream, and immediately dispatch it.
//
HRESULT AVFStream::DeliverSample(IMFSample *pSample)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // store the sample in the sample list
        EXCEPTION_TO_HR( m_pSampleList.AddTail(pSample) );
        BREAK_ON_FAIL(hr);

        // Call the sample dispatching function.
        hr = DispatchSamples();
    }
    while(false);
        
    return hr;
}


//
// Activate or deactivate the stream
//
void AVFStream::Activate(bool active)
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    // if the stream is already active, do nothing
    if (m_active == active)
    {
        return;
    }

    // store the activation state of the stream
    m_active = active;
    
    // if the stream has been deactivated, release all samples and tokens associated
    // with it 
    if (!m_active)
    {
        // release all samples and tokens
        m_pSampleList.RemoveAll();
        m_pTokenList.RemoveAll();
    }
}


//
// Start or seek the stream
//
HRESULT AVFStream::Start(const PROPVARIANT& varStart, bool isSeek)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);
        
        // if the this is a seek request, queue a seek successfull event - if it is a start
        // request queue the start successfull event
        if (isSeek)
        {
            hr = QueueEvent(MEStreamSeeked, GUID_NULL, S_OK, &varStart);
        }
        else
        {
            hr = QueueEvent(MEStreamStarted, GUID_NULL, S_OK, &varStart);
        }
        BREAK_ON_FAIL(hr);

        // update the internal state variable
        m_state = SourceStateStarted;

        // send the samples stored in the stream
        hr = DispatchSamples();
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Pause the stream
//
HRESULT AVFStream::Pause()
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // update the internal state variable
        m_state = SourceStatePaused;

        // fire a stream paused event
        hr = QueueEvent(MEStreamPaused, GUID_NULL, S_OK, NULL);
    }
    while(false);

    return hr;
}


//
// Stop the stream
//
HRESULT AVFStream::Stop()
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // release all of the samples associated with the stream
        m_pSampleList.RemoveAll();
        m_pTokenList.RemoveAll();

        // update the internal state variable
        m_state = SourceStateStopped;

        // queue an event indicating that we stopped successfully
        hr = QueueEvent(MEStreamStopped, GUID_NULL, S_OK, NULL);
    }
    while(false);

    return hr;
}


//
// Receive the end of stream notification
//
HRESULT AVFStream::EndOfStream()
{
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
        
        // update an internal variable indicating that no new samples will be coming
        m_endOfStream = true;
    }

    // dispatch any samples still stored in the stream
    return DispatchSamples();
}


//
// Shutdown the stream
//
HRESULT AVFStream::Shutdown()
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        m_state = SourceStateShutdown;

        // shutdown the event queue
        if (m_pEventQueue)
        {
            m_pEventQueue->Shutdown();
        }

        // release any samples still in the stream
        m_pSampleList.RemoveAll();
        m_pTokenList.RemoveAll();
    }
    while(false);

    return hr;
}


//
// Return true if the stream is active and needs more samples; false otherwise
//
bool AVFStream::NeedsData()
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    // the stream will indicate that it needs samples if it is active, the end of 
    // stream has not been reached, and it has internally stored less than the maximum
    // number of samples to buffer
    return (m_active && !m_endOfStream && (m_pSampleList.GetCount() < SAMPLE_BUFFER_SIZE));
}

AVFStream::AVFStream() : m_cRef(1),
                         m_pMediaSource(NULL),
                         m_state(SourceStateUninitialized),
                         m_endOfStream(false),
                         m_active(true),
                         m_isVideo(false),
                         m_isAudio(false),
                         m_nSamplesRequested(0)
{
}


//
// Initialise the stream object - initialization is broken out of the constructor to detect
// failures.
//
HRESULT AVFStream::Init(AVFSource *pMediaSource, IMFStreamDescriptor *pStreamDescriptor)
{
    HRESULT hr = S_OK;

    do
    {
        // create the event queue
        hr = MFCreateEventQueue(&m_pEventQueue);
        BREAK_ON_FAIL(hr);

        BREAK_ON_NULL (pMediaSource, E_INVALIDARG);
        BREAK_ON_NULL (pStreamDescriptor, E_INVALIDARG);

        // store a reference to the media source
        m_pMediaSource = pMediaSource;
        m_pMediaSource->AddRef();

        // store the passed-in stream descriptor
        m_pStreamDescriptor = pStreamDescriptor;
    }
    while(false);

    return hr;
}


//
// Check whether the stream is shut down - if it is, return a failure.
//
HRESULT AVFStream::CheckShutdown()
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
// Destructor
//
AVFStream::~AVFStream()
{
    SafeRelease(m_pMediaSource);
}
