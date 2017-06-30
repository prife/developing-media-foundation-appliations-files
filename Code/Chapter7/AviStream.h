#pragma once

#include <atlbase.h>
#include <atlcoll.h>
#include <Mfobjects.h>
#include <mfidl.h>
#include <Mfapi.h>

#include <Propvarutil.h>    


class CAviSink;

// {6DBB0806-D2F5-41A2-8CC2-32923CFE5BDA}
DEFINE_GUID(MFSTREAMSINK_MARKER_FLAG, 0x6dbb0806, 0xd2f5, 0x41a2, 0x8c, 0xc2, 0x32, 0x92, 0x3c, 0xfe, 0x5b, 0xda);

// {C61841B8-9A1B-4845-A860-8086DB0C3F3A}
DEFINE_GUID(MFSTREAMSINK_MARKER_CONTEXT_BLOB, 0xc61841b8, 0x9a1b, 0x4845, 0xa8, 0x60, 0x80, 0x86, 0xdb, 0xc, 0x3f, 0x3a);


class CAviStream :
    public IMFStreamSink,
    public IMFMediaTypeHandler
{
    public:

        static HRESULT CreateInstance(DWORD id, IMFMediaType* pMediaType, CAviSink* pSink, CAviStream** ppStream);

        CAviStream(DWORD id, IMFMediaType* pMediaType, CAviSink* pSink);
        ~CAviStream(void);

        // IUnknown interface implementation
        STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

        // IMFStreamSink interface implementation
        STDMETHODIMP GetMediaSink(IMFMediaSink** ppMediaSink);
        STDMETHODIMP GetIdentifier(DWORD* pdwIdentifier);
        STDMETHODIMP GetMediaTypeHandler(IMFMediaTypeHandler** ppHandler);
        STDMETHODIMP ProcessSample(IMFSample* pSample);
        STDMETHODIMP PlaceMarker(MFSTREAMSINK_MARKER_TYPE eMarkerType, const PROPVARIANT* pvarMarkerValue, const PROPVARIANT* pvarContextValue);        
        STDMETHODIMP Flush(void);

        // IMFMediaEventGenerator interface implementation
        STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback,IUnknown* punkState);
        STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
        STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent);
        STDMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue);

        // IMFMediaTypeHandler interface implementation
        STDMETHODIMP IsMediaTypeSupported(IMFMediaType* pMediaType, IMFMediaType** ppMediaType);
        STDMETHODIMP GetMediaTypeCount(DWORD* pdwTypeCount);
        STDMETHODIMP GetMediaTypeByIndex(DWORD dwIndex, IMFMediaType** ppType);
        STDMETHODIMP SetCurrentMediaType(IMFMediaType* pMediaType);
        STDMETHODIMP GetCurrentMediaType(IMFMediaType** ppMediaType);
        STDMETHODIMP GetMajorType(GUID* pguidMajorType);

        HRESULT GetNextSampleTimestamp(LONGLONG* pTimestamp);
        HRESULT GetNextSampleLength(DWORD* pSize);
        HRESULT GetNextSample(BYTE* pBuffer, DWORD* pBufferSize, bool* pIsKeyFrame);
        
        HRESULT OnStarted(void);
        HRESULT OnPaused(void);
        HRESULT OnStopped(void);

    private:
        volatile long m_cRef;                       // reference count
        CComAutoCriticalSection m_critSec;          // critical section

        DWORD m_streamId;

        CAviSink* m_pSink;
        CComPtr<IMFMediaType> m_pMediaType;
        CComPtr<IMFMediaEventQueue> m_pEventQueue;
        
        CInterfaceList<IMFSample> m_sampleQueue;
        CComPtr<IMFAsyncCallback> m_pSinkCallback;

        bool m_endOfSegmentEncountered;

        HRESULT CopyNextSampleData(BYTE* pBuffer, DWORD* pBufferSize);
        HRESULT TryFireMarkerEvent(HRESULT markerResult);
};

