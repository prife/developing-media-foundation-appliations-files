#pragma once

#include "TranscodeApiTopoBuilder.h"

// Media Foundation headers
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>

class CTranscodeApi : public IMFAsyncCallback
{
    public:
        CTranscodeApi(void);
        ~CTranscodeApi(void);

        // transcode the file
        HRESULT TranscodeFile(PCWSTR pszInput, PCWSTR pszOutput);

        // IMFAsyncCallback implementation.
        STDMETHODIMP GetParameters(DWORD *pdwFlags, DWORD *pdwQueue)   { return E_NOTIMPL; }
        STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult);

        // IUnknown methods - required for IMFAsyncCallback to function
        STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        // block waiting for the transcode to complete
        HRESULT WaitUntilCompletion(void);

    private:
        volatile long m_nRefCount;                       // COM reference count.

        CTranscodeApiTopoBuilder m_topoBuilder;

        CComPtr<IMFMediaSession> m_pSession;

        HANDLE m_closeCompleteEvent;    // event fired when transcoding is complete
        HRESULT m_sessionResult;        // result of transcode process

        // helper function called from Invoke()
        HRESULT ParseMediaEvent(IMFMediaEvent* pEvt);
};

