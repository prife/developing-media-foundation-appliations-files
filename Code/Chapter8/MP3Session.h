#pragma once

#include <atlbase.h>

// Media Foundation headers
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfobjects.h>
#include <evr.h>

#include <Wmcodecdsp.h>

#include "Common.h"
#include "AsyncState.h"
#include "SampleRequestToken.h"
#include "MP3SessionTopoBuilder.h"
#include <new>


//
// Main MP3 session class - receives component events and passes data through the topology
//
class CMP3Session :
    public CMP3SessionTopoBuilder,
    public IMFMediaSession,
    public IMFGetService
{
    public:
        CMP3Session(PCWSTR pUrl);
        ~CMP3Session(void);

        HRESULT Init(void);

        // IUnknown interface implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

        // IMFMediaSession interface implementation
        STDMETHODIMP SetTopology(DWORD dwSetTopologyFlags, IMFTopology *pTopology);
        STDMETHODIMP ClearTopologies(void);
        STDMETHODIMP Start(const GUID *pguidTimeFormat, 
            const PROPVARIANT *pvarStartPosition);
        STDMETHODIMP Pause(void);
        STDMETHODIMP Stop(void);
        STDMETHODIMP Close(void);
        STDMETHODIMP Shutdown(void);
        STDMETHODIMP GetClock(IMFClock** ppClock);
        STDMETHODIMP GetSessionCapabilities(DWORD* pdwCaps);
        STDMETHODIMP GetFullTopology(DWORD dwGetFullTopologyFlags, TOPOID TopoId, 
            IMFTopology **ppFullTopology);

        // IMFAsyncCallback interface implementation
        STDMETHODIMP GetParameters(DWORD* pdwFlags, DWORD* pdwQueue);
        STDMETHODIMP Invoke(IMFAsyncResult* pResult);

        // IMFGetService interface implementation
        STDMETHODIMP GetService(REFGUID guidService, REFIID riid, LPVOID *ppvObject);

        // IMFMediaEventGenerator interface implementation
        STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback,IUnknown* punkState);
        STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
        STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent);
        STDMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, 
            HRESULT hrStatus, const PROPVARIANT* pvValue);

    private:
        DWORD m_syncMftWorkerQueue;
        bool m_sessionStarted;
        
        CComQIPtr<IMFSample> m_pReadySourceSample;
        HANDLE m_sourceSampleReadyEvent;

        CComPtr<IMFPresentationClock> m_pClock;  
        
        HRESULT HandleStreamSinkEvent(IMFAsyncResult* pResult);
        HRESULT HandleSourceEvent(IMFAsyncResult* pResult);
        HRESULT HandleSourceStreamEvent(IMFAsyncResult* pResult);
        HRESULT HandleSynchronousMftRequest(IMFAsyncResult* pResult);
        
        HRESULT PullDataFromMFT(IMFTransform* pMFTransform, IMFSample** ppNewSample);
        HRESULT InitOutputDataBuffer(IMFTransform* pMFTransform, 
            MFT_OUTPUT_DATA_BUFFER* pBuffer);
        HRESULT PullDataFromSource(IMFSample** ppNewSample);
};

