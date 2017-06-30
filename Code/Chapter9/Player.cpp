
#include "Player.h"
#include <strsafe.h>
#include <Mferror.h>


//
//  CPlayer constructor - instantiates internal objects and initializes MF
//
CPlayer::CPlayer(HWND videoWindow) :
    m_pSession(NULL),
    m_hwndVideo(videoWindow),
    m_state(PlayerState_Closed),
    m_nRefCount(1),
    m_duration(0),
    m_isSeekbarVisible(false)
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
        if (m_state != PlayerState_Closing)
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

    if(FAILED(hr))
    {
        WCHAR msg[MAX_PATH] = {'\0'};
        StringCchPrintf(msg, MAX_PATH, L"CPlayer::ProcessEvent(): An error has occured: 0x%08x", hr);

        MessageBoxW(NULL, msg, L"Error", MB_OK | MB_ICONEXCLAMATION);
    }

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
HRESULT CPlayer::OpenURL(PCWSTR sURL, HWND renderHwnd, bool network)
{
    CComPtr<IMFTopology> pTopology = NULL;
    HRESULT hr = S_OK;
    bool isMp3 = false;
    DWORD extensionStart = 0;

    do
    {
        extensionStart = (DWORD)(wcslen(sURL) - 4);
        if((DWORD)_wcsicmp(sURL + extensionStart, L".mp3") == 0)
        {
            isMp3 = true;
        }

        // Step 1: create a media session if one doesn't exist already
        if(m_pSession == NULL)
        {
            hr = CreateSession();
            BREAK_ON_FAIL(hr);
        }

        m_hwndVideo = renderHwnd;

        // Step 2: build the topology.  Here we are using the TopoBuilder helper class.
        if(renderHwnd != NULL && network)
        {
            hr = m_topoBuilder.RenderURL(sURL, m_hwndVideo, true);
        }
        else if(renderHwnd != NULL && !network)
        {
            hr = m_topoBuilder.RenderURL(sURL, m_hwndVideo, false);
        }
        else if(renderHwnd == NULL && network)
        {
            hr = m_topoBuilder.RenderURL(sURL, NULL, true);
        }
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
        if(m_state == PlayerState_Ready)
        {
            m_state = PlayerState_OpenPending;
        }
    }
    while(false);

    if (FAILED(hr))
    {
        m_state = PlayerState_Closed;
    }

    return hr;
}


//
//  Starts playback from paused or stopped state.
//
HRESULT CPlayer::Play(void)
{
    if (m_state != PlayerState_Paused && m_state != PlayerState_Stopped)
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
    if (m_state != PlayerState_Started)
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
        m_state = PlayerState_Paused;
    }

    return hr;
}


//
//  Stop playback.
//
HRESULT CPlayer::Stop(void)
{
    HRESULT hr = S_OK;

    do
    {
        if (m_state != PlayerState_Started)
        {
            hr = MF_E_INVALIDREQUEST;
            break;
        }

        BREAK_ON_NULL(m_pSession, E_UNEXPECTED);

        hr = m_pSession->Stop();
        BREAK_ON_FAIL(hr);

        m_state = PlayerState_Stopped;        
    }
    while(false);

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
        if (m_state != PlayerState_Started)
        {
            hr = MF_E_INVALIDREQUEST;
            break;
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
        if (m_state != PlayerState_Started)
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
    
    // release any previous instance of the m_pVideoDisplay interface
    m_pVideoDisplay.Release();

    // Ask the session for the IMFVideoDisplayControl interface. 
    MFGetService(m_pSession, MR_VIDEO_RENDER_SERVICE,  IID_PPV_ARGS(&m_pVideoDisplay));

    // initialize the seek bar variables and ignore any errors in case this is an audio 
    // stream
    InitializeMixer();

    // since the topology is ready, start playback
    hr = StartPlayback();

    m_pPresentationClock = NULL;
    m_pSession->GetClock(&m_pPresentationClock);

    DetermineDuration();
    DrawSeekbar();

    // get the rate control service that can be used to change the playback rate of the service
    m_pRateControl = NULL;
    MFGetService(m_pSession, MF_RATE_CONTROL_SERVICE, IID_IMFRateControl, (void**)&m_pRateControl);

    return hr;
}


//
//  Handler for MEEndOfPresentation event, fired when playback has stopped.
//
HRESULT CPlayer::OnPresentationEnded(void)
{
    // The session puts itself into the stopped state automatically.
    m_state = PlayerState_Stopped;
    m_pSession->Stop();
    m_pSession->Shutdown();
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

        assert(m_state == PlayerState_Closed);

        // Create the media session.
        hr = MFCreateMediaSession(NULL, &m_pSession);
        BREAK_ON_FAIL(hr);

        m_state = PlayerState_Ready;

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

    m_state = PlayerState_Closing;

    // release the video display object
    m_pVideoDisplay = NULL;

    // Call the asynchronous Close() method and then wait for the close
    // operation to complete on another thread
    if (m_pSession != NULL)
    {
        m_state = PlayerState_Closing;

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

    m_state = PlayerState_Closed;

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
        m_state = PlayerState_Started;
    }

    PropVariantClear(&varStart);
    return hr;
}



HRESULT CPlayer::DetermineDuration(void)
{
    HRESULT hr = S_OK;
    CComPtr<IMFTopology> pTopology;
    CComPtr<IMFTopologyNode> pTopoNode;
    CComPtr<IMFMediaSource> pSource;
    CComPtr<IMFPresentationDescriptor> pPresDescriptor;
    WORD nodeIndex = 0;
    MF_TOPOLOGY_TYPE nodeType = MF_TOPOLOGY_MAX;

    do
    {
        BREAK_ON_NULL(m_pSession, E_UNEXPECTED);

        hr = m_pSession->GetFullTopology(
                MFSESSION_GETFULLTOPOLOGY_CURRENT,
                0,
                &pTopology);
        BREAK_ON_FAIL(hr);

        while(true)
        {
            hr = pTopology->GetNode(nodeIndex++, &pTopoNode);
            BREAK_ON_FAIL(hr);

            hr = pTopoNode->GetNodeType(&nodeType);
            BREAK_ON_FAIL(hr);

            if(nodeType == MF_TOPOLOGY_SOURCESTREAM_NODE)
            {
                hr = pTopoNode->GetUnknown(
                        MF_TOPONODE_SOURCE, 
                        IID_IMFMediaSource, 
                        (void**) &pSource);
                BREAK_ON_FAIL(hr);

                hr = pSource->CreatePresentationDescriptor(&pPresDescriptor);
                BREAK_ON_FAIL(hr);

                hr = pPresDescriptor->GetUINT64( MF_PD_DURATION, (UINT64*)&m_duration );
                BREAK_ON_FAIL(hr);

                break;
            }
        }
    }
    while(false);

    return hr;
}


//
// Initialize the mixer pointer and DirectX objects necessary to draw the seek bar
//
HRESULT CPlayer::InitializeMixer(void)
{
    HRESULT hr = S_OK;

    do
    {
        m_pMixerBitmap = NULL;

        // get the mixer pointer from the session
        hr = MFGetService(m_pSession, MR_VIDEO_MIXER_SERVICE, IID_IMFVideoMixerBitmap, 
            (void**)&m_pMixerBitmap);
        BREAK_ON_FAIL(hr);

        // load the various seek bar required objects if they have not been loaded
        if(m_pD3d9Obj == NULL)
        {
            hr = InitializeSeekbarDirectX();
        }
    }
    while(false);

    return hr;
}


//
// Create and initialize the DirectX objects needed to draw the seek bar.
//
HRESULT CPlayer::InitializeSeekbarDirectX(void)
{
    HRESULT hr = S_OK;
    D3DDISPLAYMODE dispMode;
    D3DPRESENT_PARAMETERS d3dParams;
    WCHAR fullPath[MAX_PATH];

    do
    {
        // create the DX9 object
        m_pD3d9Obj = Direct3DCreate9(D3D_SDK_VERSION);
        BREAK_ON_NULL(m_pD3d9Obj, E_UNEXPECTED);

        // get the current adapter display mode        
        hr = m_pD3d9Obj->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &dispMode );

        // initialize the d3d parameter structure 
        ZeroMemory( &d3dParams, sizeof(D3DPRESENT_PARAMETERS) );
        d3dParams.Windowed = TRUE;                  // windowed mode
        d3dParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dParams.BackBufferFormat = dispMode.Format;
        d3dParams.hDeviceWindow = m_hwndVideo;

        // create a D3D9 device 
        hr = m_pD3d9Obj->CreateDevice(
            D3DADAPTER_DEFAULT,             // use the primary display adapter
            D3DDEVTYPE_HAL,
            NULL,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING     // use sw processing
                | D3DCREATE_FPU_PRESERVE            // use double floating point
                | D3DCREATE_MULTITHREADED,          // multiple threads will access device
            &d3dParams,
            &m_pD3d9Device);
        BREAK_ON_FAIL(hr);

        // create the surface that will hold the render surface in system memory - that is
        // required by the mixer
        hr = m_pD3d9Device->CreateOffscreenPlainSurface(
                800, 50,                          // surface dimensions
                D3DFMT_A8R8G8B8,                  // make a surface with alpha
                D3DPOOL_SYSTEMMEM,                // place surface in system memory
                &m_pD3d9RenderSurface,            // output pointer to the new surface
                NULL);                            // reserved
        BREAK_ON_FAIL(hr);

        // create a texture that will be used to blend the seek bar images
        hr = D3DXCreateTexture(m_pD3d9Device, 
                1024, 64,                       // texture dimensions - factor of 2        
                1,                              // MIP levels
                D3DUSAGE_RENDERTARGET,          // this texture will be a render target
                D3DFMT_A8R8G8B8,                // texture format - will have alpha
                D3DPOOL_DEFAULT,                // create texture in default pool
                &m_pTargetTexture);             // output texture ptr
        BREAK_ON_FAIL(hr);

        // get the surface behind the target texture - the render target     
        hr = m_pTargetTexture->GetSurfaceLevel(0, &m_pTextureSurface);
        BREAK_ON_FAIL(hr);

        // create the sprite that will be used to blend the individual seek bar images
        hr = D3DXCreateSprite(m_pD3d9Device, &m_pDisplaySprite);
        BREAK_ON_FAIL(hr);

        // create a texture of the seek bar image
        GetLocalFilePath(L"\\seekbar.png", fullPath, MAX_PATH);
        hr = D3DXCreateTextureFromFile(m_pD3d9Device, fullPath, &m_pD3d9SeekBarTexture);
        BREAK_ON_FAIL(hr);

        // create a texture of the current location indicator image
        GetLocalFilePath(L"\\seektick.png", fullPath, MAX_PATH);
        hr = D3DXCreateTextureFromFile(m_pD3d9Device, fullPath, &m_pD3d9SeekTickTexture);
        BREAK_ON_FAIL(hr);

        // indicate that all render commands to this device will now go to the specified 
        // surface
        hr = m_pD3d9Device->SetRenderTarget(0, m_pTextureSurface);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}



//
// Draw the seek bar and the current position indicator on the UI
//
HRESULT CPlayer::DrawSeekbar(void)
{
    HRESULT hr = S_OK;
    MFVideoAlphaBitmap alphaBmp;

    do
    {
        // audio playback may not have initialized seek bar - just exit in that case
        BREAK_ON_NULL(m_pD3d9Device, S_OK);

        // if the seek bar is invisible just clear out the alpha bitmap from the mixer
        if(!m_isSeekbarVisible)
        {
            hr = m_pMixerBitmap->ClearAlphaBitmap();
            break;
        }

        // clear the texture of the old seek bar image
        hr = m_pD3d9Device->Clear(
            0, NULL,                    // clear the entire surface
            D3DCLEAR_TARGET,            // clear the surface set as the render target
            D3DCOLOR_RGBA(0,0,0,0),     // set to the transparancey color
            1, 0 );                     // z and stencil
        BREAK_ON_FAIL(hr);

        // blend the seek bar textures together on the target surface only if the seek bar
        // is visible
        hr = RenderSeekBarSurface();
        BREAK_ON_FAIL(hr);

        // overall transparency of the seek bar image
        alphaBmp.params.fAlpha = 0.7f;

        // Initialize the structure with the alpha bitmap info
        alphaBmp.GetBitmapFromDC = FALSE;               // this is not a GDI DC
        alphaBmp.bitmap.pDDS = m_pD3d9RenderSurface;    // specify the surface
        alphaBmp.params.dwFlags = 
            MFVideoAlphaBitmap_EntireDDS |              // apply alpha to entire surface
            MFVideoAlphaBitmap_Alpha |                  // use per-pixel alpha
            MFVideoAlphaBitmap_DestRect;                // specify a custom destination rect
        
        // specify the destination rectangle on the screen in relative coordinates
        alphaBmp.params.nrcDest.left = 0.1f;             
        alphaBmp.params.nrcDest.top = 0.8f;
        alphaBmp.params.nrcDest.bottom = 0.85f;
        alphaBmp.params.nrcDest.right = 0.9f;
        
        // send the alpha bitmap to the mixer
        hr = m_pMixerBitmap->SetAlphaBitmap(&alphaBmp);
    }
    while(false);

    return hr;
}


//
// Blend seek bar textures together on the intermediate surface
//
HRESULT CPlayer::RenderSeekBarSurface(void)
{
    HRESULT hr = S_OK;
    D3DXVECTOR3 tickPosition(0, 0, 0);
    LONGLONG clockTime = 0;
    MFTIME systemTime = 0;

    do
    {
        // get the current time from the presentation clock
        hr = m_pPresentationClock->GetCorrelatedTime(0, &clockTime, &systemTime);
        BREAK_ON_FAIL(hr);

        // calculate how far from the beginning of the seek bar the seek indicator should be
        tickPosition.x = (float)(960 * ((double)clockTime / (double)m_duration));

        // begin drawing scene
        hr = m_pD3d9Device->BeginScene();
        BREAK_ON_FAIL(hr);

        // begin drawing the textures
        hr = m_pDisplaySprite->Begin(D3DXSPRITE_ALPHABLEND);
        BREAK_ON_FAIL(hr);

        // draw the seek bar texture on the render target of the device to which the 
        // sprite object belongs to
        hr = m_pDisplaySprite->Draw(
            m_pD3d9SeekBarTexture,              // draw this texture on render target
            NULL, NULL,                         // source recgangle and center of sprite
            NULL,                               // position - use 0,0 coordinates
            D3DCOLOR_RGBA(255, 255, 255, 255)); // color modulation value - 0xffffffff
        BREAK_ON_FAIL(hr);

        // draw the current location indicator (tick) on the seek bar
        hr = m_pDisplaySprite->Draw(
            m_pD3d9SeekTickTexture,             // draw this texture on render target
            NULL, NULL,                         // source recgangle and center of sprite
            &tickPosition,                      // position - use tickPosition coords
            D3DCOLOR_RGBA(255, 255, 255, 255)); // color modulation value - 0xffffffff
        BREAK_ON_FAIL(hr);

        // end drawing the textures
        hr = m_pDisplaySprite->End();
        BREAK_ON_FAIL(hr);

        // end drawing the scene - actually draws the scene on the surface
        hr = m_pD3d9Device->EndScene();
        BREAK_ON_FAIL(hr);

        // copy the contents of the target texture's surface to the render surface - the 
        // render surface is in the system memory, which is what the mixer requires
        hr = D3DXLoadSurfaceFromSurface(
                m_pD3d9RenderSurface,       // target surface
                NULL, NULL,                 // use the entire surface as the target rect
                m_pTextureSurface,          // source surface
                NULL, NULL,                 // use the entire surface as the source rect
                D3DX_DEFAULT, 0);           // image filter and special transparent color
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}




void CPlayer::GetLocalFilePath(WCHAR* filename, WCHAR* fullPath, DWORD fullPathLength)
{
    DWORD pathEnds = 0;

    // get the filename and full path of this DLL and store it in the tempStr object        
    GetModuleFileName(NULL, fullPath, fullPathLength);

    // find and isolate the name of the DLL so that we can replace it with the name of the image
    for(DWORD x = (DWORD)wcslen(fullPath) - 1; x > 0; x--)
    {
        if(fullPath[x] == L'\\')
        {
            fullPath[x] = L'\0';
            break;
        }
    }

    // append the image name to the filename
    wcscat_s(fullPath, MAX_PATH, filename);
}