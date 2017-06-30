#include "TranscodeApi.h"


CTranscodeApi::CTranscodeApi(void)
{
    m_nRefCount = 1;

    // Start up Media Foundation platform.
    MFStartup(MF_VERSION);

    // create an event that will be fired when the asynchronous IMFMediaSession::Close() 
    // operation is complete
    m_closeCompleteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    m_sessionResult = S_OK;
}


CTranscodeApi::~CTranscodeApi(void)
{
    // Shutdown the Media Foundation platform
    MFShutdown();

    // close the event
    CloseHandle(m_closeCompleteEvent);
}



HRESULT CTranscodeApi::TranscodeFile(PCWSTR pszInput, PCWSTR pszOutput)
{
    HRESULT hr = S_OK;
    CComPtr<IMFTopology> pTopology;

    do
    {
        // Create the media session.
        hr = MFCreateMediaSession(NULL, &m_pSession);
        BREAK_ON_FAIL(hr);

        // designate this class as the one that will be handling events from the media 
        // session
        hr = m_pSession->BeginGetEvent((IMFAsyncCallback*)this, NULL);
        BREAK_ON_FAIL(hr);

        // create the transcode profile which will be used to create the transcode topology
        hr = m_topoBuilder.SetTranscodeProfile(
            MFAudioFormat_WMAudioV9,            // WMA v9 audio format
            MFVideoFormat_WMV3,                 // WMV3 video format
            MFTranscodeContainerType_ASF);      // ASF file container
        BREAK_ON_FAIL(hr);

        hr = m_topoBuilder.CreateTranscodeTopology(pszInput, pszOutput);
        BREAK_ON_FAIL(hr);


        pTopology = m_topoBuilder.GetTopology();

        hr = m_pSession->SetTopology(0, pTopology);
        if (SUCCEEDED(hr))
        {
            PROPVARIANT varStart;
            PropVariantClear(&varStart);
            hr = m_pSession->Start(&GUID_NULL, &varStart);
        }
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// The WaitUntilCompletion() function blocks and doesn't return until the session is
// complete.  This function returns the HRESULT that indicates the session transcode
// status.
//
HRESULT CTranscodeApi::WaitUntilCompletion(void)
{
    // block waiting for the session closing completed event
    WaitForSingleObject(m_closeCompleteEvent, INFINITE);
    
    // shut down the session resource - the source and the session itself
    m_topoBuilder.ShutdownSource();
    m_pSession->Close();

    // return the result of the session
    return m_sessionResult;
}





//
// IMFAsyncCallback::Invoke implementation.  This is the function called by the media
// session whenever anything of note happens or an asynchronous operation is complete.
//
HRESULT CTranscodeApi::Invoke(IMFAsyncResult* pAsyncResult)
{
    CComPtr<IMFMediaEvent> pEvent;
    HRESULT hr = S_OK;
    MediaEventType eventType;
    

    do
    {
        // Get the event from the event queue.
        hr = m_pSession->EndGetEvent(pAsyncResult, &pEvent);
        BREAK_ON_FAIL(hr);

        // Get the event type.
        hr = pEvent->GetType(&eventType);
        BREAK_ON_FAIL(hr);

        // parse the media event stored in the asynchronous result
        hr = ParseMediaEvent(pEvent);
        BREAK_ON_FAIL(hr);
                
        // If this is not the final event, tell the media session that this player is 
        // the object that will handle the next event in the queue.
        if(eventType != MESessionClosed)
        {
            hr = m_pSession->BeginGetEvent(this, NULL);
            BREAK_ON_FAIL(hr);
        }
    }
    while(false);

    return S_OK;
}



//
// Parse a media event passed in asynchronously
//
HRESULT CTranscodeApi::ParseMediaEvent(IMFMediaEvent* pEvent)
{
    HRESULT hr = S_OK;
    
    MediaEventType eventType;
    HRESULT topologySetStatusHr = S_OK;

    do
    {
        BREAK_ON_NULL(pEvent, E_UNEXPECTED);

        // Get the event type.
        hr = pEvent->GetType(&eventType);
        BREAK_ON_FAIL(hr);

        
        if(eventType == MESessionEnded)
        {
            // if the session is ended, close it
            hr = m_pSession->Close();
            BREAK_ON_FAIL(hr);
        }
        else if (eventType == MESessionClosed)
        {
            // MESessionClosed event is guaranteed to be the last event fired by the session 
            // Fire the m_closeCompleteEvent to let the player know that it can safely shut 
            // down the session and release resources associated with the session.
            SetEvent(m_closeCompleteEvent);
        }
        else if(eventType == MESessionTopologySet)
        {
            // get the result of the topology set operation
            hr = pEvent->GetStatus(&topologySetStatusHr);
            BREAK_ON_FAIL(hr);

            // if topology resolution failed, then the returned HR will indicate that
            if(FAILED(topologySetStatusHr))
            {
                // store the failure for future reference
                m_sessionResult = topologySetStatusHr;

                // close the session
                m_pSession->Close();
            }
        }
    }
    while(false);

    return hr;
}





//
// IUnknown methods
//
HRESULT CTranscodeApi::QueryInterface(REFIID riid, void** ppv)
{
    if(riid == IID_IMFAsyncCallback)
    {
        *ppv = static_cast<IMFAsyncCallback*>(this);
        return S_OK;
    }
    else if(riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(this);
        return S_OK;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

ULONG CTranscodeApi::AddRef(void)
{
    return InterlockedIncrement(&m_nRefCount);
}

ULONG CTranscodeApi::Release(void)
{
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    if (uCount == 0)
    {
        delete this;
    }
    return uCount;
}