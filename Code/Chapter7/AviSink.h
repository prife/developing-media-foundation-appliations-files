#pragma once

#include <atlbase.h>

#include <Mfobjects.h>
#include <mfidl.h>
#include <Mferror.h>

#include <vector>
using namespace std;

#include <strsafe.h>


#include "AviStream.h"
#include "AviFileWriter.h"




class CAviSink :
    public IMFFinalizableMediaSink,
    public IMFClockStateSink,
    public IMFAsyncCallback
{
    public:

        //CAviSink(void);
        CAviSink(const WCHAR* pFilename, HRESULT* pHr);
        ~CAviSink(void);

        // IUnknown interface implementation
        STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);


        // IMFMediaSink interface implementation
        STDMETHODIMP GetCharacteristics(DWORD *pdwCharacteristics);
        STDMETHODIMP AddStreamSink(DWORD dwStreamSinkIdentifier, IMFMediaType* pMediaType, IMFStreamSink** ppStreamSink);
        STDMETHODIMP RemoveStreamSink(DWORD dwStreamSinkIdentifier);
        STDMETHODIMP GetStreamSinkCount(DWORD* pcStreamSinkCount);
        STDMETHODIMP GetStreamSinkByIndex(DWORD dwIndex, IMFStreamSink** ppStreamSink);
        STDMETHODIMP GetStreamSinkById(DWORD dwStreamSinkIdentifier, IMFStreamSink** ppStreamSink);
        STDMETHODIMP SetPresentationClock(IMFPresentationClock* pPresentationClock);
        STDMETHODIMP GetPresentationClock(IMFPresentationClock** ppPresentationClock);
        STDMETHODIMP Shutdown(void);

        // IMFClockStateSink interface implementation
        STDMETHODIMP OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset);
        STDMETHODIMP OnClockStop(MFTIME hnsSystemTime);
        STDMETHODIMP OnClockPause(MFTIME hnsSystemTime);
        STDMETHODIMP OnClockRestart(MFTIME hnsSystemTime);
        STDMETHODIMP OnClockSetRate(MFTIME hnsSystemTime, float flRate);

        // IMFFinalizableMediaSink interface implementation
        STDMETHODIMP BeginFinalize(IMFAsyncCallback* pCallback, IUnknown* punkState);
        STDMETHODIMP EndFinalize(IMFAsyncResult* pResult);

        // IMFAsyncCallback interface implementation
        STDMETHODIMP GetParameters(DWORD *pdwFlags, DWORD *pdwQueue);
        STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult);

        HRESULT ScheduleNewSampleProcessing(void);

    private:

        enum SinkState
        {
            SinkStarted,
            SinkPaused,
            SinkStopped,
            SinkShutdown
        };


        volatile long m_cRef;                       // reference count
        CComAutoCriticalSection m_critSec;          // critical section

        WCHAR* m_pFilename;
        BYTE* m_pSampleData;
        DWORD m_dwSampleData;

        SinkState m_sinkState;
        HANDLE m_unpauseEvent;

        CComPtr<IMFPresentationClock> m_pClock;     // pointer to the presentation clock

        vector<CAviStream*> m_streamSinks;

        CAviFileWriter* m_pFileWriter;

        HRESULT ProcessStreamSamples(void);
        HRESULT GetEarliestSampleStream(int* pEarliestStream);
        HRESULT WriteSampleFromStream(DWORD nEarliestSampleStream);
                
        HRESULT CheckBufferSize(DWORD streamId);
        HRESULT CheckShutdown(void);
};

