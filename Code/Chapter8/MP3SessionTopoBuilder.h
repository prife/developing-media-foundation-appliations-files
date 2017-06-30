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

#include <new>


#include <InitGuid.h>

// MP3 ByteStreamHandler CLSID 
// {A82E50BA-8E92-41eb-9DF2-433F50EC2993}
DEFINE_GUID(CLSID_MP3ByteStreamPlugin, 0xa82e50ba, 0x8e92, 0x41eb, 0x9d, 0xf2, 0x43, 0x3f, 0x50, 0xec, 0x29, 0x93);

// MP3 Audio Decoder CLSID
// {bbeea841-0a63-4f52-a7ab-a9b3a84ed38a}
DEFINE_GUID(CLSID_CMP3DecMediaObject, 0xbbeea841, 0x0a63, 0x4f52, 0xa7, 0xab, 0xa9, 0xb3, 0xa8, 0x4e, 0xd3, 0x8a);

// Audio resampler CLSID
// {f447b69e-1884-4a7e-8055-346f74d6edb3}
DEFINE_GUID(CLSID_CResamplerMediaObject, 0xf447b69e, 0x1884, 0x4a7e, 0x80, 0x55, 0x34, 0x6f, 0x74, 0xd6, 0xed, 0xb3);

//
// Builds the MP3 topology
//
class CMP3SessionTopoBuilder :
    public IMFAsyncCallback,
    public IMFMediaEventGenerator
{
    public:
        CMP3SessionTopoBuilder(PCWSTR pUrl);
        ~CMP3SessionTopoBuilder(void);

        // IUnknown interface implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

    protected:
        volatile long m_cRef;
        CComAutoCriticalSection m_critSec;

        WCHAR* m_pFileUrl;

        CComPtr<IMFMediaEventQueue>  m_pEventQueue;
        CComPtr<IMFTopology> m_pTopology;

        CComPtr<IMFByteStreamHandler> m_pByteStreamHandler;
        CComPtr<IMFPresentationDescriptor> m_pPresentation;
        CComQIPtr<IMFMediaSource> m_pSource;
        CComPtr<IMFTransform> m_pDecoder;
        CComPtr<IMFTransform> m_pResampler;
        CComPtr<IMFMediaSink> m_pSink;
        
        CComQIPtr<IMFMediaStream> m_pSourceStream;
        CComPtr<IMFStreamSink> m_pStreamSink;

        HRESULT HandleByteStreamHandlerEvent(IMFAsyncResult* pResult);
        HRESULT FireTopologyReadyEvent(void);

        HRESULT LoadCustomTopology(void);
        HRESULT BeginCreateMediaSource(void);
        HRESULT NegotiateMediaTypes(void);
        HRESULT ConnectSourceToMft(IMFTransform* pMFTransform);
        HRESULT ConnectMftToSink(IMFTransform* pMFTransform);
        HRESULT ConnectMftToMft(IMFTransform* pMFTransform1, IMFTransform* pMFTransform2);  
};

