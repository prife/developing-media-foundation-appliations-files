#include "StdAfx.h"
#include "AviStream.h"
#include "AviSink.h"


HRESULT CAviStream::CreateInstance(DWORD id, IMFMediaType* pMediaType, CAviSink* pSink, CAviStream** ppStream)
{
    HRESULT hr = S_OK;
    CAviStream* pStream = NULL;

    do
    {
        BREAK_ON_NULL(ppStream, E_POINTER);

        *ppStream = NULL;

        pStream = new (std::nothrow) CAviStream(id, pMediaType, pSink);
        if(pStream == NULL)
        {
            hr = E_OUTOFMEMORY;
            break;
        }

        hr = MFCreateEventQueue(&(pStream->m_pEventQueue));
        BREAK_ON_FAIL(hr);

        *ppStream = pStream;
    }
    while(false);

    if(FAILED(hr))
    {
        if(pStream != NULL)
            delete pStream;
    }

    return hr;
}

CAviStream::CAviStream(DWORD id, IMFMediaType* pMediaType, CAviSink* pSink) :
    m_cRef(1),
    m_streamId(id),
    m_pMediaType(pMediaType),
    m_pSink(pSink),
    m_endOfSegmentEncountered(false)
{
    m_pSinkCallback = pSink;
}


CAviStream::~CAviStream(void)
{
    if(m_pEventQueue != NULL)
    {
        m_pEventQueue->Shutdown();
    }
}



//
// Standard IUnknown interface implementation
//
ULONG CAviStream::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG CAviStream::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }

    return refCount;
}

HRESULT CAviStream::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(static_cast<IMFStreamSink*>(this));
    }
    else if (riid == IID_IMFStreamSink)
    {
        *ppv = static_cast<IMFStreamSink*>(this);
    }
    else if (riid == IID_IMFMediaTypeHandler)
    {
        *ppv = static_cast<IMFMediaTypeHandler*>(this);
    }
    else if (riid == IID_IMFMediaEventGenerator)
    {
        *ppv = static_cast<IMFMediaEventGenerator*>(this);
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




///////////////////////////////////////////////////////////////////////////////////////////
//
// IMFStreamSink interface implementation
//
///////////////////////////////////////////////////////////////////////////////////////////

//
// Get the sink that is associated with this stream
//
HRESULT CAviStream::GetMediaSink(IMFMediaSink** ppMediaSink)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(ppMediaSink, E_POINTER);
        BREAK_ON_NULL(m_pSink, E_UNEXPECTED);

        // QI the internal sink pointer for the requested IMFMediaSink interface
        hr = m_pSink->QueryInterface(IID_IMFMediaSink, (void**)ppMediaSink);
    }
    while(false);

    return hr;
}


//
// Get the stream ID
//
HRESULT CAviStream::GetIdentifier(DWORD* pdwIdentifier)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(pdwIdentifier, E_POINTER);

        *pdwIdentifier = m_streamId;
    }
    while(false);

    return hr;
}


//
// Get an object that will allow the caller to query the media types supported by this
// stream
//
HRESULT CAviStream::GetMediaTypeHandler(IMFMediaTypeHandler** ppHandler)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(ppHandler, E_POINTER);

        // CAviStream supports this interface - therefore just QI it from the this pointer
        hr = this->QueryInterface(IID_IMFMediaTypeHandler, (void**)ppHandler);
    }
    while(false);

    return hr;
}


//
// Receive a sample for this stream sink, store it in the sample queue, and call the media
// sink, notifying it of the arrival of a new sample.
//
HRESULT CAviStream::ProcessSample(IMFSample* pSample)
{
    HRESULT hr = S_OK;
    CComPtr<IMFSample> pMediaSample = pSample;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        BREAK_ON_NULL(pSample, E_POINTER);
        BREAK_ON_NULL(m_pSinkCallback, E_UNEXPECTED);

        // add the sample to the internal sample queue
        EXCEPTION_TO_HR( m_sampleQueue.AddTail(pMediaSample) );

        // schedule an asynchronous work item on the sink that will cause it to pull out
        // the new sample that has just arrived
        hr = m_pSink->ScheduleNewSampleProcessing();
    }
    while(false);

    return hr;
}



//
// Place a marker in the queue of samples, in order to track how the stream processes 
// samples.  CAviStream ignores the marker value parameter.
//
HRESULT CAviStream::PlaceMarker(
            MFSTREAMSINK_MARKER_TYPE eMarkerType,   // marker type
            const PROPVARIANT* pvarMarkerValue,     // marker value
            const PROPVARIANT* pvarContextValue)    // context information for the event
{
    HRESULT hr = S_OK;
    CComPtr<IMFSample> pSample;
    PROPVARIANT contextCopy = {};

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // Use a fake empty sample to queue marker information.
        hr = MFCreateSample(&pSample);
        BREAK_ON_FAIL(hr);

        // Store the marker type in the sample.
        hr = pSample->SetUINT32(MFSTREAMSINK_MARKER_FLAG, eMarkerType);
        BREAK_ON_FAIL(hr);

        // if the marker was sent with a context value, store that value in the sample
        if(pvarContextValue != NULL)
        {
            // Deep copy the context data into the temporary PROPVARIANT value.
            hr = PropVariantCopy(&contextCopy, pvarContextValue);
            BREAK_ON_FAIL(hr);

            // Store a copy of the PROPVARIANT structure with context information in the 
            // dummy sample as a memory blob.
            hr = pSample->SetBlob(
                        MFSTREAMSINK_MARKER_CONTEXT_BLOB,      // GUID identifying value
                        (UINT8*)&contextCopy,                  // store the context copy
                        sizeof(contextCopy));                  // size of the blob
        }

        // store the fake container sample on the queue
        hr = ProcessSample(pSample);
    }
    while(false);

    // Don't call PropVariantClear() on the marker context blob since it is stored in the
    // sample - it will be cleared after the marker data is fired in the MEStreamSinkMarker
    // event.

    return hr;
}



//
// Flush the sink - process all of the samples in the queue
//
HRESULT CAviStream::Flush(void)
{
    HRESULT hr = S_OK;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // loop through all of the items on the queue - drop regular samples and send
        // on the markers immediately with the E_ABORT result
        while(!m_sampleQueue.IsEmpty())
        {
            // try firing all of the markers at the head of the queue
            hr = TryFireMarkerEvent(E_ABORT);
            BREAK_ON_FAIL(hr);

            // if there are any samples in the queue, the head sample will NOT be a marker,
            // since the TryFireMarkerEvent() already processed all of the markers at the
            // head.  Therefore it's a normal sample.  Drop it.
            if(!m_sampleQueue.IsEmpty())
            {
                EXCEPTION_TO_HR( m_sampleQueue.RemoveHeadNoReturn() );
            }
        }
    }
    while(false);

    return hr;
}





////////////////////////////////////////////////////////////////////////
//
//  IMFMediaEventGenerator interface implementation
//
////////////////////////////////////////////////////////////////////////


//
// Begin processing an event from the event queue asynchronously
//
HRESULT CAviStream::BeginGetEvent(
            IMFAsyncCallback* pCallback,    // callback of the object interested in events
            IUnknown* punkState)            // some custom state object returned with event
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // get the next event from the event queue
        hr = m_pEventQueue->BeginGetEvent(pCallback, punkState);
    }
    while(false);

    return hr;
}

//
// Complete asynchronous event processing
//
HRESULT CAviStream::EndGetEvent(
            IMFAsyncResult* pResult,    // result of an asynchronous operation
            IMFMediaEvent** ppEvent)    // event extracted from the queue
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        hr = m_pEventQueue->EndGetEvent(pResult, ppEvent);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Synchronously retrieve the next event from the event queue
//
HRESULT CAviStream::GetEvent(
            DWORD dwFlags,              // flag with the event behavior
            IMFMediaEvent** ppEvent)    // event extracted from the queue
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // get the event from the queue
        hr = m_pEventQueue->GetEvent(dwFlags, ppEvent);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}

//
// Store the event in the internal event queue of the source
//
HRESULT CAviStream::QueueEvent(
            MediaEventType met,             // media event type
            REFGUID guidExtendedType,       // GUID_NULL for standard events or an extension GUID
            HRESULT hrStatus,               // status of the operation
            const PROPVARIANT* pvValue)     // a VARIANT with some event value or NULL
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // queue the passed in event on the internal event queue
        hr = m_pEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


///////////////////////////////////////////////////////////////////////////////////////////////
// IMFMediaTypeHandler interface implementation
///////////////////////////////////////////////////////////////////////////////////////////////


//
// Check if the specified media type is supported on the stream - always return S_OK
//
HRESULT CAviStream::IsMediaTypeSupported(IMFMediaType* pMediaType, IMFMediaType** ppMediaType)
{
    // don't return any closest match types
    if(ppMediaType != NULL)
        *ppMediaType = NULL;

    // accept any media type
    return S_OK;
}


//
// The stream supports all types - so the internal supported media type count is zero
//
HRESULT CAviStream::GetMediaTypeCount(DWORD* pdwTypeCount)
{
    if(pdwTypeCount == NULL)
        return E_POINTER;

    *pdwTypeCount = 0;

    return S_OK;
}


//
// Get one of the supported media types by index - this stream supports only one, so
// always return MF_E_NO_MORE_TYPES
//
HRESULT CAviStream::GetMediaTypeByIndex(DWORD dwIndex, IMFMediaType** ppType)
{
    return MF_E_NO_MORE_TYPES;
}


//
// Set the current media type on the stream
//
HRESULT CAviStream::SetCurrentMediaType(IMFMediaType* pMediaType)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(pMediaType, E_POINTER);

        // clear out the m_pMediaType variable and store the media type
        m_pMediaType = NULL;
        m_pMediaType = pMediaType;
    }
    while(false);

    return hr;
}


//
// Get the current media type set on this stream
//
HRESULT CAviStream::GetCurrentMediaType(IMFMediaType** ppMediaType)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(ppMediaType, E_POINTER);

        if (m_pMediaType == NULL)
        {
            hr = MF_E_NOT_INITIALIZED;
            break;
        }

        *ppMediaType = m_pMediaType;
        (*ppMediaType)->AddRef();
    }
    while(false);

    return hr;
}


//
// Get the major media type currently set on this stream
//
HRESULT CAviStream::GetMajorType(GUID* pguidMajorType)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(pguidMajorType, E_POINTER);

        if (m_pMediaType == NULL)
        {
            hr = MF_E_NOT_INITIALIZED;
            break;
        }

        hr = m_pMediaType->GetMajorType(pguidMajorType);
    }
    while(false);

    return hr;
}






//////////////////////////////////////////////////////////////////////////////////
//
// Public helper methods
//
//////////////////////////////////////////////////////////////////////////////////

//
// Get the time stamp on the sample that's at the head of the queue
//
HRESULT CAviStream::GetNextSampleTimestamp(LONGLONG* pTimestamp)
{
    HRESULT hr = S_OK;
    LONGLONG sampleDuration = 0;
    DWORD sampleSize = 0;
    DWORD subsamplesInSample = 0;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
        BREAK_ON_NULL(pTimestamp, E_POINTER);

        // see if the next sample is a marker object - if it is, fire the corresponding 
        // event and remove the sample from the queue
        hr = TryFireMarkerEvent(S_OK);
        BREAK_ON_FAIL(hr);

        // make sure the queue is not empty - if it is, return a result code indicating
        // whether data for this sample will still arrive, or whether the sink should not
        // wait for data from this stream, and proceed regardless.
        if(m_sampleQueue.IsEmpty())
        {
            if(m_endOfSegmentEncountered)
            {   // data for this stream won't be available for a while
                hr = S_FALSE;
            }
            else
            {   // no data available on this stream yet, but is expected to arrive shortly
                hr = E_PENDING;
            }
            break;
        }           

        // get the time stamp
        EXCEPTION_TO_HR( hr = m_sampleQueue.GetHead()->GetSampleTime(pTimestamp) );
    }
    while(false);

    return hr;
}


//
// Get the amount of data in the next sample at the head of the sample queue
//
HRESULT CAviStream::GetNextSampleLength(DWORD* pLength)
{
    HRESULT hr = S_OK;
    
    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
        BREAK_ON_NULL(pLength, E_POINTER);

        // make sure the queue is not empty
        if(m_sampleQueue.IsEmpty())
        {
            hr = S_FALSE;
            break;
        }

        // get the length of the sample at the head of the queue
        EXCEPTION_TO_HR( hr = m_sampleQueue.GetHead()->GetTotalLength(pLength) );
    }
    while(false);

    return hr;
}


//
// Get the data in the sample at the head of the queue, removing it from the queue
//
HRESULT CAviStream::GetNextSample(BYTE* pBuffer, DWORD* pBufferSize, bool* pIsKeyFrame)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaBuffer> pMediaBuffer;
    BYTE* internalBuffer = NULL;
    DWORD bufferMaxLength = 0;
    DWORD bufferCurrentLength = 0;
    LONG result = 0;
    UINT32 isKeyFrame = 0;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
        BREAK_ON_NULL(pBufferSize, E_POINTER);
        BREAK_ON_NULL(pBuffer, E_INVALIDARG);

        if(m_sampleQueue.IsEmpty())
        {
            hr = E_UNEXPECTED;
            break;
        }

        // Copy data in the next sample into the provided buffer
        hr = CopyNextSampleData(pBuffer, pBufferSize);
        BREAK_ON_FAIL(hr);

        // check the sample for the CleanPoint variable - if it's there and set to 1, then 
        // this is a keyframe.
        EXCEPTION_TO_HR( 
            m_sampleQueue.GetHead()->GetUINT32(MFSampleExtension_CleanPoint, &isKeyFrame) );
        if(isKeyFrame != 0)
        {
            *pIsKeyFrame = true;
        }
        else
        {
            *pIsKeyFrame = false;
        }

        // if we got here, then we successfully got data from the sample - remove the sample 
        // from the queue
        EXCEPTION_TO_HR( m_sampleQueue.RemoveHeadNoReturn() );

        // we processed a sample - ask for another one
        hr = QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    if(FAILED(hr))
    {
        *pBufferSize = 0;
    }

    return hr;
}


//
// Copy the data in the sample at the head of the internal queue into the provided buffer
//
HRESULT CAviStream::CopyNextSampleData(BYTE* pBuffer, DWORD* pBufferSize)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaBuffer> pMediaBuffer;
    BYTE* internalBuffer = NULL;
    DWORD bufferMaxLength = 0;
    DWORD bufferCurrentLength = 0;
    LONG result = 0;

    do
    {
        // convert the sample contents to a single contiguous buffer and get the poitner
        EXCEPTION_TO_HR( hr = m_sampleQueue.GetHead()->ConvertToContiguousBuffer(&pMediaBuffer) );
        BREAK_ON_FAIL(hr);

        // lock the buffer to get a pointer to it
        hr = pMediaBuffer->Lock(&internalBuffer, &bufferMaxLength,  &bufferCurrentLength);
        BREAK_ON_FAIL(hr);

        // copy the data into the passed-in buffer - if there is not enough space in there,
        // buffer, return E_UNEXPECTED
        result = memcpy_s(pBuffer, *pBufferSize, internalBuffer, bufferCurrentLength);
        if(result != 0)
        {
            hr = E_UNEXPECTED;
            break;
        }

        // return the useful number of bytes in the buffer
        *pBufferSize = bufferCurrentLength;

        // unlock the buffer
        hr = pMediaBuffer->Unlock();
        BREAK_ON_FAIL(hr);
    }
    while(false);

    if(FAILED(hr))
    {
        *pBufferSize = 0;
    }

    return hr;
}







//
// Start playback - member method called by the sink.
//
HRESULT CAviStream::OnStarted(void)
{
    HRESULT hr = S_OK;

    do
    {
        // fire an event indicating that this stream has started processing data
        hr = QueueEvent(MEStreamSinkStarted, GUID_NULL, S_OK, NULL);
        BREAK_ON_FAIL(hr);
            
        // request a sample
        hr = QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
        BREAK_ON_FAIL(hr);
        
        // request another sample - we will store two samples in the queue
        hr = QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
        BREAK_ON_FAIL(hr);        
    }
    while(false);

    return hr;
}


//
// Start playback - member method called by the sink.
//
HRESULT CAviStream::OnPaused(void)
{
    HRESULT hr = S_OK;

    do
    {
        // fire an event indicating that this stream has started processing data
        hr = QueueEvent(MEStreamSinkPaused, GUID_NULL, hr, NULL);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}



//
// Start playback - member method called by the sink.
//
HRESULT CAviStream::OnStopped(void)
{
    HRESULT hr = S_OK;

    do
    {
        // fire an event indicating that this stream has started processing data
        hr = QueueEvent(MEStreamSinkStopped, GUID_NULL, hr, NULL);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}




//
// See if a marker is at the top of the queue, and if it is, fire an event and remove
// the marker from the queue.  Loop until all of the markers have been extracted from
// the top of the queue.
//
HRESULT CAviStream::TryFireMarkerEvent(HRESULT markerResult)
{
    HRESULT hr = S_OK;
    UINT32 markerId = 0;
    UINT32 blobSize = 0;
    PROPVARIANT markerContextData = {};
    CComPtr<IMFSample> pHeadSample;

    do
    {
        // if the sample queue is empty, just exit
        if(m_sampleQueue.IsEmpty())
        {
            break;
        }

        // get the sample from the head of the queue, making sure to check for exceptions
        EXCEPTION_TO_HR( pHeadSample = m_sampleQueue.GetHead() );
        BREAK_ON_FAIL(hr);
        BREAK_ON_NULL(pHeadSample, E_UNEXPECTED);

        // check for the marker value that would indicate this is a marker dummy sample
        hr = pHeadSample->GetUINT32(MFSTREAMSINK_MARKER_FLAG, &markerId); 
        
        // if this is a normal sample, it won't have the marker flag attribute - just exit
        if(hr == MF_E_ATTRIBUTENOTFOUND)
        {
            hr = S_OK;
            break;
        }
        BREAK_ON_FAIL(hr);

        // if this is an end-of-segment marker, then it indicates the end of the current 
        // stream - save a flag so that the sink can keep pulling samples from other streams
        // and ignore the fact that this one is empty
        if(markerId == MFSTREAMSINK_MARKER_ENDOFSEGMENT)
        {
            m_endOfSegmentEncountered = true;
        }
        BREAK_ON_FAIL(hr);

        // get the data stored in the dummy marker sample and associated with the event
        hr = pHeadSample->GetBlob( MFSTREAMSINK_MARKER_CONTEXT_BLOB, // GUID of the context
                    (UINT8*)&markerContextData,             // pointer to the destination
                    sizeof(markerContextData),              // size of the destination
                    &blobSize);                             // number of bytes copied
        
        // If the context was sent with the marker and stored in the sample, fire a marker
        // event with the context data in it.  Otherwise if the context was not found, fire
        // an event with NULL instead of context pointer.
        if(hr == S_OK)
        {
            hr = QueueEvent(MEStreamSinkMarker, GUID_NULL, markerResult, &markerContextData);
        }
        else if(hr == MF_E_ATTRIBUTENOTFOUND)
        {
            hr = QueueEvent(MEStreamSinkMarker, GUID_NULL, markerResult, NULL);
        }
        BREAK_ON_FAIL(hr);

        // remove the marker sample from the head of the queue
        m_sampleQueue.RemoveHeadNoReturn();
    }
    // loop until we break out because the event at the head of the queue is not a marker
    while(true);

    // clear the local copy of the context data object (this is necessary since it wasn't 
    // cleared after PropVariantCopy() call in the PlaceMarker() function).
    PropVariantClear(&markerContextData);

    return hr;
}
















