#pragma once

#include "Common.h"

#include "TopoBuilder.h"
#include "MP3Session.h"

enum PlayerState
{
    Closed = 0,     // No session.
    Ready,          // Session was created, ready to open a file.
    OpenPending,    // Session is opening a file.
    Started,        // Session is playing a file.
    Paused,         // Session is paused.
    Stopped,        // Session is stopped (ready to play).
    Closing         // Application has closed the session, but is waiting for MESessionClosed.
};

//
//  The CPlayer class wraps MediaSession functionality and hides it from a calling application.
//
class CPlayer : public IMFAsyncCallback
{
public:
    CPlayer(HWND videoWindow);
    ~CPlayer();

    // Playback control
    HRESULT       OpenURL(PCWSTR sURL);
    HRESULT       Play();
    HRESULT       Pause();
    PlayerState   GetState() const { return m_state; }

    HRESULT       IncreaseRate(void);
    HRESULT       DecreaseRate(void);

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
    HRESULT Initialize();

    // private session and playback controlling functions
    HRESULT CreateSession();
    HRESULT CloseSession();
    HRESULT StartPlayback();

    // MF event handling functionality
    HRESULT ProcessEvent(CComPtr<IMFMediaEvent>& mediaEvent);    
    
    // Media event handlers
    HRESULT OnTopologyReady(void);
    HRESULT OnPresentationEnded(void);

    long                                m_nRefCount;        // COM reference count.

    CTopoBuilder                        m_topoBuilder;

    CComPtr<IMFMediaSession>            m_pSession;    
    CComPtr<IMFVideoDisplayControl>     m_pVideoDisplay;
    CComPtr<IMFRateControl>             m_pRateControl;

    HWND                                m_hwndVideo;        // Video window.
    PlayerState                         m_state;            // Current state of the media session.

    HANDLE                              m_closeCompleteEvent;   // event fied when session colse is complete
};