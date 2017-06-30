#pragma once

#include <atlbase.h>
#include <atlcoll.h>

#include <Mferror.h>
#include <mfapi.h>

#include "Common.h"

#define SAMPLE_BUFFER_SIZE 2

class AVFSource;

class AVFStream : public IMFMediaStream
{
    public:
        static HRESULT CreateInstance(AVFStream** ppMediaStream, AVFSource *pMediaSource, 
            IMFStreamDescriptor *pStreamDescriptor);

        //
        // IMFMediaEventGenerator interface implementation
        STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback,IUnknown* punkState);
        STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
        STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent);
        STDMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, 
            HRESULT hrStatus, const PROPVARIANT* pvValue);

        //
        // IMFMediaStream interface implementation
        STDMETHODIMP GetMediaSource(IMFMediaSource** ppMediaSource);
        STDMETHODIMP GetStreamDescriptor(IMFStreamDescriptor** ppStreamDescriptor);
        STDMETHODIMP RequestSample(IUnknown* pToken);

        //
        // IUnknown interface implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

        //
        // Helper functions used by AVFSource
        HRESULT DeliverSample(IMFSample *pSample);
        void Activate(bool active);
        HRESULT Start(const PROPVARIANT& varStart, bool isSeek);
        HRESULT Pause(void);
        HRESULT Stop(void);
        HRESULT EndOfStream();
        bool IsActive(void) const { return m_active; }
        HRESULT Shutdown();
        void SetVideoStream(void) { m_isVideo = true; m_isAudio = false; }
        void SetAudioStream(void) { m_isVideo = false; m_isAudio = true; }
        bool IsVideoStream(void) const { return m_isVideo; }
        bool IsAudioStream(void) const { return m_isAudio; }
        bool NeedsData(void);

    private:
        AVFStream(void);
        HRESULT Init(AVFSource *pMediaSource, IMFStreamDescriptor *pStreamDescriptor);
        HRESULT CheckShutdown(void);
        HRESULT DispatchSamples(void);
        HRESULT SendSamplesOut(void);
        ~AVFStream(void);

    private:
        volatile long m_cRef;
        AVFSource* m_pMediaSource;
        bool m_active;
        bool m_endOfStream;
        bool m_isVideo;
        bool m_isAudio;
        SourceState m_state;

        volatile int m_nSamplesRequested;

        CComAutoCriticalSection m_critSec;          // critical section

        CComPtr<IMFStreamDescriptor> m_pStreamDescriptor;
        CComPtr<IMFMediaEventQueue>  m_pEventQueue;
        CInterfaceList<IMFSample> m_pSampleList;
        CInterfaceList<IUnknown, &IID_IUnknown> m_pTokenList;
};

