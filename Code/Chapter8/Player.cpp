
#include "Player.h"
#include <strsafe.h>


//
//  CPlayer constructor - instantiates internal objects and initializes MF
//
CPlayer::CPlayer(HWND videoWindow) :
    m_pSession(NULL),
    m_hwndVideo(videoWindow),
    m_state(Closed),
    m_nRefCount(1)
{
    // Start up Media Foundation platform.
    MFStartup(MF_VERSION);

    // create an event that will be fired when the asynchronous IMFMediaSession::Close() 
    // operation is complete
    m_closeCompleteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}



CPlayer::~CPlayer(void)
{
    CloseSession();

    // Shutdown the Media Foundation platform
    MFShutdown();

    // close the event
    CloseHandle(m_closeCompleteEvent);
}


//
// IMFAsyncCallback::Invoke implementation.  This is the function called by media session
// whenever anything of note happens or an asynchronous operation is complete.
//
// pAsyncResult - a pointer to the asynchronous result object which references the event 
// itself in the IMFMediaEventGenerator's event queue.  (The media session is the object
// that implements the IMFMediaEventGenerator interface.)
//
HRESULT CPlayer::Invoke(IMFAsyncResult* pAsyncResult)
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

        // MESessionClosed event is guaranteed to be the last event fired by the session. 
        // Fire the m_closeCompleteEvent to let the player know that it can safely shut 
        // down the session and release resources associated with the session.
        if (eventType == MESessionClosed)
        {
            SetEvent(m_closeCompleteEvent);
        }
        else
        {
            // If this is not the final event, tell the Media Session that this player is 
            // the object that will handle the next event in the queue.
            hr = m_pSession->BeginGetEvent(this, NULL);
            BREAK_ON_FAIL(hr);
        }

        // If we are in a normal state, handle the event by passing it to the HandleEvent()
        // function.  Otherwise, if we are in the closing state, do nothing with the event.
        if (m_state != Closing)
        {
            ProcessEvent(pEvent);
        }
    }
    while(false);

    return S_OK;
}



//
//  Called by Invoke() to do the actual event processing, and determine what, if anything
//  needs to be done.
//
HRESULT CPlayer::ProcessEvent(CComPtr<IMFMediaEvent>& mediaEvent)
{
    HRESULT hrStatus = S_OK;            // Event status
    HRESULT hr = S_OK;
    MF_TOPOSTATUS TopoStatus = MF_TOPOSTATUS_INVALID; 
    MediaEventType eventType;

    do
    {
        BREAK_ON_NULL( mediaEvent, E_POINTER );

        // Get the event type.
        hr = mediaEvent->GetType(&eventType);
        BREAK_ON_FAIL(hr);

        // Get the event status. If the operation that triggered the event did
        // not succeed, the status is a failure code.
        hr = mediaEvent->GetStatus(&hrStatus);
        BREAK_ON_FAIL(hr);

        // Check if the async operation succeeded.
        if (FAILED(hrStatus))
        {
            hr = hrStatus;
            break;
        }

        // Switch on the event type. Update the internal state of the CPlayer as needed.
        if(eventType == MESessionTopologyStatus)
        {
            // Get the status code.
            hr = mediaEvent->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, (UINT32*)&TopoStatus);
            BREAK_ON_FAIL(hr);

            if (TopoStatus == MF_TOPOSTATUS_READY)
            {
                hr = OnTopologyReady();
            }
        }
        else if(eventType == MEEndOfPresentation)
        {
            hr = OnPresentationEnded();
        }
    }
    while(false);

    return hr;
}




//
// IUnknown methods
//
HRESULT CPlayer::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if(ppv == NULL)
    {
        return E_POINTER;
    }

    if(riid == IID_IMFAsyncCallback)
    {
        *ppv = static_cast<IMFAsyncCallback*>(this);
    }
    else if(riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(this);
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

ULONG CPlayer::AddRef(void)
{
    return InterlockedIncrement(&m_nRefCount);
}

ULONG CPlayer::Release(void)
{
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    if (uCount == 0)
    {
        delete this;
    }
    return uCount;
}



//
// OpenURL is the main initialization function that triggers bulding of the core
// MF components.
//
HRESULT CPlayer::OpenURL(PCWSTR sURL)
{
    CComPtr<IMFTopology> pTopology = NULL;
    HRESULT hr = S_OK;
    bool isMp3 = false;
    DWORD extensionStart = 0;

    do
    {
        extensionStart = wcslen(sURL) - 4;
        if((DWORD)_wcsicmp(sURL + extensionStart, L".mp3") == 0)
        {
            isMp3 = true;
        }

        // Step 1: create a media session if one doesn't exist already
        if(m_pSession == NULL)
        {
            if(isMp3)
            {
                CMP3Session* pMp3Session = new (std::nothrow) CMP3Session(sURL);
                BREAK_ON_NULL(pMp3Session, E_OUTOFMEMORY);

                hr = pMp3Session->Init();
                BREAK_ON_FAIL(hr);

                m_pSession = pMp3Session;
                BREAK_ON_NULL(m_pSession, E_UNEXPECTED);

                // designate this class as the one that will be handling events from the media 
                // session
                hr = m_pSession->BeginGetEvent((IMFAsyncCallback*)this, NULL);
                BREAK_ON_FAIL(hr);
            }
            else
            {
                hr = CreateSession();
                BREAK_ON_FAIL(hr);
            }
        }

        // Step 2: build the topology.  Here we are using the TopoBuilder helper class.
        hr = m_topoBuilder.RenderURL(sURL, m_hwndVideo);
        BREAK_ON_FAIL(hr);

        // get the topology from the TopoBuilder
        pTopology = m_topoBuilder.GetTopology();
        BREAK_ON_NULL(pTopology, E_UNEXPECTED);

        // Step 3: add the topology to the internal queue of topologies associated with this
        // media session
        if(pTopology != NULL)
        {
            hr = m_pSession->SetTopology(0, pTopology);
            BREAK_ON_FAIL(hr);
        }

        // If we've just initialized a brand new topology in step 1, set the player state 
        // to "open pending" - not playing yet, but ready to begin.
        if(m_state == Ready)
        {
            m_state = OpenPending;
        }
    }
    while(false);

    if (FAILED(hr))
    {
        m_state = Closed;
    }

    return hr;
}


//
//  Starts playback from paused or stopped state.
//
HRESULT CPlayer::Play(void)
{
    if (m_state != Paused && m_state != Stopped)
    {
        return MF_E_INVALIDREQUEST;
    }
    
    if (m_pSession == NULL)
    {
        return E_UNEXPECTED;
    }

    return StartPlayback();
}


//
//  Pauses playback.
//
HRESULT CPlayer::Pause(void)
{
    if (m_state != Started)
    {
        return MF_E_INVALIDREQUEST;
    }

    if (m_pSession == NULL)
    {
        return E_UNEXPECTED;
    }

    HRESULT hr = m_pSession->Pause();

    if (SUCCEEDED(hr))
    {
        m_state = Paused;
    }

    return hr;
}


//
// Increase the playback rate by 0.5
//
HRESULT CPlayer::IncreaseRate(void)
{
    HRESULT hr = S_OK;
    float rate = 0;
    BOOL thin = FALSE;
    
    do
    {
        if (m_state != Started)
        {
            return MF_E_INVALIDREQUEST;
        }

        BREAK_ON_NULL(m_pRateControl, E_UNEXPECTED);

        // get the current rate
        hr = m_pRateControl->GetRate(&thin, &rate);
        BREAK_ON_FAIL(hr);

        // increase the current rate by 0.5
        rate += 0.5;

        // set the rate
        hr = m_pRateControl->SetRate(thin, rate);
    }
    while(false);

    return hr;
}


HRESULT CPlayer::DecreaseRate(void)
{
    HRESULT hr = S_OK;
    float rate = 0;
    BOOL thin = FALSE;
    
    do
    {
        if (m_state != Started)
        {
            return MF_E_INVALIDREQUEST;
        }

        BREAK_ON_NULL(m_pRateControl, E_UNEXPECTED);

        // get the current rate
        hr = m_pRateControl->GetRate(NULL, &rate);
        BREAK_ON_FAIL(hr);

        // decrease the current rate by 0.5
        rate -= 0.5;

        hr = m_pRateControl->SetRate(thin, rate);
    }
    while(false);

    return hr;
}



//
//  Repaints the video window.
//
//  The application calls this method when it receives a WM_PAINT message.
//
HRESULT CPlayer::Repaint(void)
{
    HRESULT hr = S_OK;

    if (m_pVideoDisplay)
    {
        hr = m_pVideoDisplay->RepaintVideo();
    }

    return hr;
}






//
// Handler for MESessionTopologyReady event - starts session playback.
//
HRESULT CPlayer::OnTopologyReady(void)
{
    HRESULT hr = S_OK;

    do
    {
        // release any previous instance of the m_pVideoDisplay interface
        m_pVideoDisplay.Release();

        // Ask the session for the IMFVideoDisplayControl interface - ignore the returned 
        // HRESULT in case this is an MP3 session without a video renderer.
        MFGetService(m_pSession, MR_VIDEO_RENDER_SERVICE,  IID_IMFVideoDisplayControl,
            (void**)&m_pVideoDisplay);

        // since the topology is ready, start playback
        hr = StartPlayback();
        BREAK_ON_FAIL(hr);

        // get the rate control service that can be used to change the playback rate of the
        // service 
        hr = MFGetService(m_pSession, MF_RATE_CONTROL_SERVICE, IID_IMFRateControl, 
            (void**)&m_pRateControl);
    }
    while(false);

    return hr;
}


//
//  Handler for MEEndOfPresentation event, fired when playback has stopped.
//
HRESULT CPlayer::OnPresentationEnded(void)
{
    // The session puts itself into the stopped state automatically.
    m_state = Stopped;
    return S_OK;
}


//
//  Creates a new instance of the media session.
//
HRESULT CPlayer::CreateSession(void)
{
    // Close the old session, if any.
    HRESULT hr = S_OK;
    HRESULT hr2 = S_OK;
    MF_TOPOSTATUS topoStatus = MF_TOPOSTATUS_INVALID;
    CComQIPtr<IMFMediaEvent> mfEvent;
    
    do
    {
        // close the session if one is already created
        hr = CloseSession();
        BREAK_ON_FAIL(hr);

        assert(m_state == Closed);

        // Create the media session.
        hr = MFCreateMediaSession(NULL, &m_pSession);
        BREAK_ON_FAIL(hr);

        m_state = Ready;

        // designate this class as the one that will be handling events from the media 
        // session
        hr = m_pSession->BeginGetEvent((IMFAsyncCallback*)this, NULL);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
//  Closes the media session.
//
//  The IMFMediaSession::Close method is asynchronous, so the CloseSession 
//  method waits for the MESessionClosed event. The MESessionClosed event is 
//  guaranteed to be the last event that the media session fires.
//
HRESULT CPlayer::CloseSession(void)
{
    HRESULT hr = S_OK;
    DWORD dwWaitResult = 0;

    m_state = Closing;

    // release the video display object
    m_pVideoDisplay = NULL;

    // Call the asynchronous Close() method and then wait for the close
    // operation to complete on another thread
    if (m_pSession != NULL)
    {
        m_state = Closing;

        hr = m_pSession->Close();
        if (SUCCEEDED(hr))
        {
            // Begin waiting for the Win32 close event, fired in CPlayer::Invoke(). The 
            // close event will indicate that the close operation is finished, and the 
            // session can be shut down.
            dwWaitResult = WaitForSingleObject(m_closeCompleteEvent, 5000);
            if (dwWaitResult == WAIT_TIMEOUT)
            {
                assert(FALSE);
            }
        }
    }

    // Shut down the media session. (Synchronous operation, no events.)  Releases all of the
    // internal session resources.
    if (m_pSession != NULL)
    {
        m_pSession->Shutdown();
    }

    // release the session
    m_pSession = NULL;

    m_state = Closed;

    return hr;
}


//
//  Starts playback from the current position.
//
HRESULT CPlayer::StartPlayback(void)
{
    assert(m_pSession != NULL);

    PROPVARIANT varStart;
    PropVariantInit(&varStart);

    varStart.vt = VT_EMPTY;

    // If Start fails later, we will get an MESessionStarted event with an error code, 
    // and will update our state. Passing in GUID_NULL and VT_EMPTY indicates that
    // playback should start from the current position.
    HRESULT hr = m_pSession->Start(&GUID_NULL, &varStart);
    if (SUCCEEDED(hr))
    {
        m_state = Started;
    }

    // free any values that can be freed in the PROPVARIANT
    PropVariantClear(&varStart);

    return hr;
}
