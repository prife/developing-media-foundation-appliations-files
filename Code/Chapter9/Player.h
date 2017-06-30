#pragma once

#include "Common.h"

#include "TopoBuilder.h"

#include <D3D9Types.h>
#include <Dxva2api.h>

#include <D3D9.h>
#include <d3dx9tex.h>
#include <Evr9.h>

#include <Mferror.h>


enum PlayerState
{
    PlayerState_Closed = 0,     // No session.
    PlayerState_Ready,          // Session was created, ready to open a file.
    PlayerState_OpenPending,    // Session is opening a file.
    PlayerState_Started,        // Session is playing a file.
    PlayerState_Paused,         // Session is paused.
    PlayerState_Stopped,        // Session is stopped (ready to play).
    PlayerState_Closing         // Application is waiting for MESessionClosed.
};

//
// The CPlayer class wraps MediaSession functionality and hides it from a calling
// application.
//
class CPlayer : public IMFAsyncCallback
{
    public:
        CPlayer(HWND videoWindow);
        ~CPlayer(void);

        // Playback control
        HRESULT OpenURL(PCWSTR sURL, HWND renderHwnd, bool network);
        HRESULT Play(void);
        HRESULT Pause(void);
        HRESULT Stop(void);
        PlayerState   GetState() const { return m_state; }

        HRESULT IncreaseRate(void);
        HRESULT DecreaseRate(void);
        HRESULT DrawSeekbar(void);
        void SetMouseOver(bool m) { m_isSeekbarVisible = m; }

        // Video functionality
        HRESULT       Repaint();
        BOOL          HasVideo() const { return (m_pVideoDisplay != NULL);  }



        //
        // IMFAsyncCallback implementation.
        //
        // Skip the optional GetParameters() function - it is used only in advanced players.
        // Returning the E_NOTIMPL error code causes the system to use default parameters.
        STDMETHODIMP GetParameters(DWORD *pdwFlags, DWORD *pdwQueue)   { return E_NOTIMPL; }

        // Main MF event handling function
        STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult);

        //
        // IUnknown methods
        //
        STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

    protected:

        // internal initialization
        HRESULT Initialize(void);

        // private session and playback controlling functions
        HRESULT CreateSession(void);
        HRESULT CloseSession(void);
        HRESULT StartPlayback(void);

        HRESULT InitializeMixer(void);
        HRESULT InitializeSeekbarDirectX(void);
        HRESULT RenderSeekBarSurface(void);
        HRESULT DetermineDuration(void);    

        // MF event handling functionality
        HRESULT ProcessEvent(CComPtr<IMFMediaEvent>& mediaEvent);    
    
        // Media event handlers
        HRESULT OnTopologyReady(void);
        HRESULT OnPresentationEnded(void);

        void GetLocalFilePath(WCHAR* filename, WCHAR* fullPath, DWORD fullPathLength);

        volatile long                                m_nRefCount;   // COM reference count.

        CTopoBuilder                        m_topoBuilder;

        CComPtr<IMFMediaSession>            m_pSession;    
        CComPtr<IMFVideoDisplayControl>     m_pVideoDisplay;
        CComPtr<IMFRateControl>             m_pRateControl;
    
        CComPtr<IDirect3D9>                 m_pD3d9Obj;
        CComPtr<IDirect3DDevice9>           m_pD3d9Device;
        CComPtr<IDirect3DTexture9>          m_pD3d9SeekBarTexture;
        CComPtr<IDirect3DTexture9>          m_pD3d9SeekTickTexture;
        CComPtr<IDirect3DSurface9>          m_pD3d9RenderSurface;
    
        CComPtr<IDirect3DTexture9>          m_pTargetTexture;
        CComPtr<IDirect3DSurface9>          m_pTextureSurface;
    
        CComPtr<ID3DXSprite>                m_pDisplaySprite;


        CComPtr<IMFVideoMixerBitmap>        m_pMixerBitmap;
        CComPtr<IMFClock>                   m_pPresentationClock;

        LONGLONG m_duration;

        HWND                                m_hwndVideo;        // Video window.
        PlayerState                         m_state;            // Current state of the media session.

        // event fied when session close is complete
        HANDLE                              m_closeCompleteEvent;   

        bool m_isSeekbarVisible;
};