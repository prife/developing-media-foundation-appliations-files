#include "StdAfx.h"
#include "AviSink.h"


//
// CAviSink constructor - create a sink for the specified file name
//
CAviSink::CAviSink(const WCHAR* pFilename, HRESULT* pHr) : 
    m_pFilename(NULL),
    m_pFileWriter(NULL),
    m_pSampleData(NULL),
    m_dwSampleData(0),
    m_sinkState(SinkStopped)
{
    HRESULT hr = S_OK;
    CAviStream* pStream = NULL;

    do
    {
        BREAK_ON_NULL(pFilename, E_UNEXPECTED);

        // store the file name of the target file
        m_pFilename = new (std::nothrow) WCHAR[wcslen(pFilename) + 1];
        BREAK_ON_NULL(m_pFilename, E_OUTOFMEMORY);

        // copy the file name into an internal member variable
        StringCchCopy(m_pFilename, wcslen(pFilename)+1, pFilename);

        // create a stream and add it to the sink
        hr = CAviStream::CreateInstance(
            0,              // stream ID
            NULL,           // don't specify a media type
            this,           // store a pointer to the sink in the stream
            &pStream);      // retrieve the resulting stream
        BREAK_ON_FAIL(hr);

        // add the stream sink object to the m_streamSinks vector
        EXCEPTION_TO_HR( m_streamSinks.push_back(pStream) );

        // create a second stream
        hr = CAviStream::CreateInstance(1, NULL, this, &pStream);
        BREAK_ON_FAIL(hr);

        // add the second stream sink object to the m_streamSinks vector
        EXCEPTION_TO_HR( m_streamSinks.push_back(pStream) );
    }
    while(false);

    if(pHr != NULL)
    {
        *pHr = hr;
    }
}


CAviSink::~CAviSink(void)
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    if(m_sinkState != SinkShutdown)
        Shutdown();
    
    if(m_pFilename != NULL)
    {
        delete m_pFilename;
        m_pFilename = NULL;
    }

    if(m_pSampleData != NULL)
    {
        delete m_pSampleData;
        m_pSampleData = NULL;
    }
}




//
// Standard IUnknown interface implementation
//
ULONG CAviSink::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG CAviSink::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT CAviSink::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(static_cast<IMFMediaSink*>(this));
    }
    else if (riid == IID_IMFMediaSink)
    {
        *ppv = static_cast<IMFMediaSink*>(this);
    }
    else if (riid == IID_IMFClockStateSink)
    {
        *ppv = static_cast<IMFClockStateSink*>(this);
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
// Get the characteristics of the sink
//
HRESULT CAviSink::GetCharacteristics(DWORD* pdwCharacteristics)
{
    if(pdwCharacteristics == NULL)
        return E_POINTER;
    
    // rateless sink with a fixed number of streams
    *pdwCharacteristics = MEDIASINK_RATELESS | MEDIASINK_FIXED_STREAMS;
    
    return S_OK;
}


// 
// Add a new stream to the sink - not supported, since the sink supports a fixed number of 
// streams.
//
HRESULT CAviSink::AddStreamSink(
            DWORD dwStreamSinkIdentifier,   // new stream ID
            IMFMediaType* pMediaType,       // media type of the new stream - can be NULL
            IMFStreamSink** ppStreamSink)   // resulting stream
{
    return MF_E_STREAMSINKS_FIXED;
}


//
// Remove/delete a stream from the sink, identified by stream ID.  Not implemented since the
// sink supports a fixed number of streams.
//
HRESULT CAviSink::RemoveStreamSink(DWORD dwStreamSinkIdentifier)
{
    return MF_E_STREAMSINKS_FIXED;
}



//
// Get the number of stream sinks currently registered with the sink
//
HRESULT CAviSink::GetStreamSinkCount(DWORD* pcStreamSinkCount)
{
    HRESULT hr = S_OK;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        BREAK_ON_NULL(pcStreamSinkCount, E_POINTER);

        *pcStreamSinkCount = (DWORD)m_streamSinks.size();
    }
    while(false);

    return hr;
}



//
// Get stream by index
//
HRESULT CAviSink::GetStreamSinkByIndex(DWORD dwIndex, IMFStreamSink** ppStreamSink)
{
    HRESULT hr = S_OK;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
        BREAK_ON_NULL(ppStreamSink, E_POINTER);

        // make sure the sink is in a good state
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);
        
        // make sure the index is in the right range
        if(dwIndex < 0 || dwIndex >= m_streamSinks.size())
        {
            hr = E_INVALIDARG;
            break;
        }

        // get the IMFStreamSink pointer for a stream with the specified index
        hr = m_streamSinks[dwIndex]->QueryInterface(IID_IMFStreamSink, 
            (void**)ppStreamSink);
    }
    while(false);

    return hr;
}



//
// Get stream by sink ID
//
HRESULT CAviSink::GetStreamSinkById(DWORD dwStreamSinkIdentifier, IMFStreamSink** ppStreamSink)
{
    HRESULT hr = S_OK;
    DWORD streamId;

    do
    {
        BREAK_ON_NULL(ppStreamSink, E_POINTER);
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        *ppStreamSink = NULL;

        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);
        
        // go through every sink in the vector until we find one with the right ID
        for(DWORD x = 0; x < m_streamSinks.size(); x++)
        {
            // get sink ID
            hr = m_streamSinks[x]->GetIdentifier(&streamId);
            BREAK_ON_FAIL(hr);

            // if this is the right sink, remove it
            if(streamId == dwStreamSinkIdentifier)
            {
                // get the IMFStreamSink interface for this sink
                hr = m_streamSinks[x]->QueryInterface(IID_IMFStreamSink, 
                    (void**)ppStreamSink);
                break;
            }
        }
        BREAK_ON_FAIL(hr);

        // if we did not find a matching sink, the ID must be wrong
        if(*ppStreamSink == NULL)
        {
            hr = MF_E_INVALIDSTREAMNUMBER;
        }
    }
    while(false);

    return hr;
}



//
// Set the presentation clock on the sink
//
HRESULT CAviSink::SetPresentationClock(IMFPresentationClock* pPresentationClock)
{
    HRESULT hr = S_OK;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // If we already have a clock, remove ourselves from that clock's
        // state notifications.
        if (m_pClock != NULL)
        {
            hr = m_pClock->RemoveClockStateSink(this);
            BREAK_ON_FAIL(hr);
        }

        // Register ourselves to get state notifications from the new clock.  Note that
        // pPresentationClock can legitimately be NULL if call is made to remove the 
        // existing presentation clock
        if (pPresentationClock != NULL)
        {
            hr = pPresentationClock->AddClockStateSink(this);
            BREAK_ON_FAIL(hr);
        }

        // Release the pointer to the old clock.
        // Store the pointer to the new clock.
        m_pClock = NULL;
        m_pClock = pPresentationClock;
    }
    while(false);

    return hr;
}


//
// Get the current presentation clock
//
HRESULT CAviSink::GetPresentationClock(IMFPresentationClock** ppPresentationClock)
{
    HRESULT hr = S_OK;

    do
    {
        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // check input parameter
        BREAK_ON_NULL(ppPresentationClock, E_POINTER);

        // see if the sink already has a clock - if it doesn't, return MF_E_NO_CLOCK error
        BREAK_ON_NULL(m_pClock, MF_E_NO_CLOCK);

        // get a pointer to the presentation clock, add ref it, and return it
        *ppPresentationClock = m_pClock;
        (*ppPresentationClock)->AddRef();
    }
    while(false);

    return hr;
}


//
// Shut down the sink
//
HRESULT CAviSink::Shutdown(void)
{
    HRESULT hr = S_OK;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);
        
        // if the sink is not shut down, shut it down
        if(m_sinkState != SinkShutdown)
        {
            // release the clock
            m_pClock = NULL;

            // release all of the sinks, since they are all just COM objects
            for(DWORD x = 0; x < m_streamSinks.size(); x++)
            {
                EXCEPTION_TO_HR( m_streamSinks[x]->Release() );
            }
            
            // clear out the internal sink pointer array
            EXCEPTION_TO_HR( m_streamSinks.clear() );

            m_sinkState = SinkShutdown;
        }
    }
    while(false);

    return hr;
}





/////////////////////////////////////////////////////////////////////////////////////////
// IMFClockStateSink interface implementation
/////////////////////////////////////////////////////////////////////////////////////////


//
// Called when the clock has been started
//
HRESULT CAviSink::OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaType> pMediaType;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // if the sink is stopped, start it
        if(m_sinkState == SinkStopped)
        {
            if(m_pFileWriter != NULL)
            {
                delete m_pFileWriter;
            }

            // create a new instance of the file writer
            m_pFileWriter = new (std::nothrow) CAviFileWriter(m_pFilename);
            BREAK_ON_NULL(m_pFileWriter, E_OUTOFMEMORY);

            // go through every stream, initialize the file writer with these streams, and 
            // send the start command to each of the streams
            for(DWORD x = 0; x < m_streamSinks.size(); x++)
            {
                pMediaType = NULL;
                CAviStream* pStream = m_streamSinks[x];

                // get the stream media type - it should be known by this point
                hr = pStream->GetCurrentMediaType(&pMediaType);
                BREAK_ON_FAIL(hr);

                // add a new stream to the file writer, initializing it with the stream ID
                // and the media type
                hr = m_pFileWriter->AddStream(pMediaType, x);
                BREAK_ON_FAIL(hr);

                // pass the start command to the stream
                hr = pStream->OnStarted();
            }
            BREAK_ON_FAIL(hr);
        }

        m_sinkState = SinkStarted;
    }
    while(false);

    return hr;
}


//
// Called when the clock has been stopped
//
HRESULT CAviSink::OnClockStop(MFTIME hnsSystemTime)
{
    HRESULT hr = S_OK;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        hr = CheckShutdown();
        BREAK_ON_FAIL(hr);

        // if the sink is stopped, do nothing - otherwise shut down
        if(m_sinkState != SinkStopped)
        {
            // Stop all of the stream sinks.
            for(DWORD x = 0; x < m_streamSinks.size(); x++)
            {
                m_streamSinks[x]->OnStopped();
            }

            // delete the file writer finalizing the AVI file
            delete m_pFileWriter;
            m_pFileWriter = NULL;

            m_sinkState = SinkStopped;
        }
    }
    while(false);

    return hr;
}


//
// Called when the clock has been paused - paused state not supported, do nothing
//
HRESULT CAviSink::OnClockPause(MFTIME hnsSystemTime)
{
    return S_OK;
}


//
// Called when the clock has been restarted
//
HRESULT CAviSink:: OnClockRestart(MFTIME hnsSystemTime)
{
    // just reuse the OnClockStart method
    return OnClockStart(hnsSystemTime, 0);
}


//
// Called when the clock rate has been changed - this is a rateless sink, so there is 
// nothing to do.
//
HRESULT CAviSink::OnClockSetRate(MFTIME hnsSystemTime, float flRate)
{
    return S_OK;
}




/////////////////////////////////////////////////////////////////////////////////////////
//
// IMFFinalizableMediaSink interface implementation
//
/////////////////////////////////////////////////////////////////////////////////////////

//
// Begin asynchronous sink finalization
//
HRESULT CAviSink::BeginFinalize(IMFAsyncCallback* pCallback, IUnknown* punkState)
{
    HRESULT hr = S_OK;

    do
    {
    }
    while(false);

    return hr;
}



//
// End asynchronous sink finalization
//
HRESULT CAviSink::EndFinalize(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;

    do
    {
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
HRESULT CAviSink::GetParameters(DWORD *pdwFlags, DWORD *pdwQueue)
{
    return E_NOTIMPL;
}


// 
// Execute an asynchronous task - process samples in the streams.
//
HRESULT CAviSink::Invoke(IMFAsyncResult* pResult)
{
    return ProcessStreamSamples();                    
}




/////////////////////////////////////////////////////////////////////////////////////////
//
// Public helper methods
//
/////////////////////////////////////////////////////////////////////////////////////////


//
// Add a work item to the thread that indicates that a new sample is available in a stream
// and needs to be processed.
//
HRESULT CAviSink::ScheduleNewSampleProcessing(void)
{
    return MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, this, NULL);
}







/////////////////////////////////////////////////////////////////////////////////////////
//
// Private helper methods
//
/////////////////////////////////////////////////////////////////////////////////////////



//
// Process samples contained in the streams, extracting the next pending sample and writing
// it to the AVI file.
//
HRESULT CAviSink::ProcessStreamSamples(void)
{
    HRESULT hr = S_OK;
    int nEarliestSampleStream = 0;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // if the sink is not started and we got here, ignore all pending messages
        if(m_sinkState != SinkStarted)
        {   
            break;
        }

        // get a stream that has the next sample to be written - it should be the stream
        // with a sample that has the earliest time stamp
        hr = GetEarliestSampleStream(&nEarliestSampleStream);

        // if not all of the streams have data, the function returns E_PENDING - in that 
        // case just exit since the function will be called again for the next sample
        if(hr == E_PENDING || nEarliestSampleStream < 0)
        {
            break;
        }
        BREAK_ON_FAIL(hr);

        // call a function to extract a sample from the stream and write it to the file
        hr = WriteSampleFromStream(nEarliestSampleStream);
    }
    while(false);

    return hr;
}


//
// Go through every stream, and figure out which stream has the next sample we want to write
// to the file, since we want to write all samples in sequential order, arranged by the time
// they need to be rendered (earliest time stamps first).
//
HRESULT CAviSink::GetEarliestSampleStream(int* pEarliestStream)
{
    HRESULT hr = S_OK;
    int nEarliestSampleStream = -1;
    LONGLONG nextSampleTime = -1;
    bool drainMode = false;

    do
    {
        // go through every stream, and figure out which stream has the next earliest sample
        for(DWORD x = 0; x < m_streamSinks.size(); x++)
        {
            CAviStream* pStream = m_streamSinks[x];
            LONGLONG currentStreamSampleTime = 0;

            // get the time stamp of the next sample in this stream
            hr = pStream->GetNextSampleTimestamp(&currentStreamSampleTime);
            
            // if a stream has reached the end, just ignore it.  However, if a stream does
            // not have any samples, but is not at the end, break out of the loop, because 
            // we need all streams to have some data
            if(hr == S_FALSE)
            {
                // store a flag indicating that we are in drain mode, and need to keep 
                // pulling samples while some of the streams are empty but others are not
                drainMode = true;

                // Stream has no data and is not expecting any more soon - ignore this error
                // and this stream, and look for data in the other streams.
                continue;
            }
            else if(hr == E_PENDING)
            {
                // a stream has indicated that it doesn't have any samples yet by returning
                // E_PENDING - just break out of the loop, and return -1 which will indicate
                // that not all streams have data.
                nEarliestSampleStream = -1;
                break;
            }
            BREAK_ON_FAIL(hr);

            // Figure out if the next sample in this stream has an earlier time stamp than
            // the other streams we have seen in this for loop
            if(currentStreamSampleTime <= nextSampleTime || nextSampleTime == -1)
            {
                nextSampleTime = currentStreamSampleTime;
                nEarliestSampleStream = x;
            }
        }

        // if we are in the drain mode - because some of the streams are already empty but
        // the others are not - schedule another sample pass to drain out the stream
        if(drainMode && nEarliestSampleStream >= 0)
        {
            hr = ScheduleNewSampleProcessing();
        }
    }
    while(false);

    // return the stream that has a sample with the earliest time stamp
    if(SUCCEEDED(hr))
    {
        *pEarliestStream = nEarliestSampleStream;
    }

    return hr;
}



//
// Extract a sample from the specified stream and write it to the file
//
HRESULT CAviSink::WriteSampleFromStream(DWORD nEarliestSampleStream)
{
    HRESULT hr = S_OK;
    DWORD requestedSampleSize = 0;
    bool isKeyFrame = false;

    do
    {
        // make sure there is enough space in the internal buffer for the next sample
        hr = CheckBufferSize(nEarliestSampleStream);
        BREAK_ON_FAIL(hr);

        // store the size of the available buffer
        requestedSampleSize = m_dwSampleData;

        // actually get the next sample from the queue of the stream selected earlier
        hr = m_streamSinks[nEarliestSampleStream]->GetNextSample(
            m_pSampleData,              // sample data buffer
            &requestedSampleSize,       // get the number of useful bytes in the buffer
            &isKeyFrame);               // a Boolean key frame flag
        BREAK_ON_FAIL(hr);

        // send the sample to the file writer
        hr = m_pFileWriter->WriteSample(
            m_pSampleData,              // data buffer to write
            requestedSampleSize,        // number of useful bytes in the buffer
            nEarliestSampleStream,      // stream ID
            isKeyFrame);                // a Boolean key frame flag
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}




//
// Check to make sure that the internal buffer in the sink has enough space for the next 
// sample in the stream with the specified ID
//
HRESULT CAviSink::CheckBufferSize(DWORD streamId)
{
    HRESULT hr = S_OK;
    DWORD requestedSampleSize = 0;

    do
    {
        // check how much space the next sample requires
        hr = m_streamSinks[streamId]->GetNextSampleLength(&requestedSampleSize);
        BREAK_ON_FAIL(hr);

        // make sure we have space in the buffer - if there is not enough space, deallocate
        // it, and allocate a new one
        if(requestedSampleSize > m_dwSampleData)
        {
            // deallocate the buffer if there was an old one
            if(m_pSampleData != NULL)
                delete m_pSampleData;

            // allocate a new buffer
            m_pSampleData = new (std::nothrow) BYTE[requestedSampleSize];
            if(m_pSampleData == NULL)
            {
                hr = E_OUTOFMEMORY;
                m_dwSampleData = 0;
                break;
            }
            m_dwSampleData = requestedSampleSize;
        }
    }
    while(false);

    return hr;
}



//
// Check whether the sink is shut down, returning the MF_E_SHUTDOWN error if it is
//
HRESULT CAviSink::CheckShutdown(void )
{
    if (m_sinkState == SinkShutdown)
    {
        return MF_E_SHUTDOWN;
    }
    else
    {
        return S_OK;
    }
}