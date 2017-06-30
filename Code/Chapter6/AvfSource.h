#pragma once

#include <atlbase.h>
#include <Mfobjects.h>
#include <mfidl.h>

#include "AviFileParser.h"
#include "SourceOperation.h"
#include "AvfStream.h"

#include "Common.h"

#include <vector>
using namespace std;

// forward declaration of the class implementing the IMFMediaStream for the AVF file. 
class AVFStream;

//
// Main source class.
//
class AVFSource : public IMFMediaSource,
                  public IMFAsyncCallback
{
    public:
        static HRESULT CreateInstance(AVFSource **ppAVFSource);
       
        // IUnknown interface implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

        //
        // IMFMediaEventGenerator interface implementation
        STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback,IUnknown* punkState);
        STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
        STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent);
        STDMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, 
            HRESULT hrStatus, const PROPVARIANT* pvValue);

        //
        // IMFMediaSource interface implementation
        STDMETHODIMP CreatePresentationDescriptor(
            IMFPresentationDescriptor** ppPresDescriptor);
        STDMETHODIMP GetCharacteristics(DWORD* pdwCharacteristics);
        STDMETHODIMP Start(IMFPresentationDescriptor* pPresentationDescriptor,
            const GUID* pguidTimeFormat,
            const PROPVARIANT* pvarStartPosition);
        STDMETHODIMP Stop(void);
        STDMETHODIMP Pause(void);
        STDMETHODIMP Shutdown(void);

        //
        // IMFAsyncCallback interface implementation
        STDMETHODIMP GetParameters(DWORD *pdwFlags, DWORD *pdwQueue);
        STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult);

        //
        // Helper methods called by the bytestream handler.
        HRESULT BeginOpen(LPCWSTR pwszURL, IMFAsyncCallback *pCallback, 
            IUnknown *pUnkState);
        HRESULT EndOpen(IMFAsyncResult *pResult);
        
        HRESULT CheckShutdown(void) const;          // Check if AVSource is shutting down.
        HRESULT IsInitialized(void) const;          // Check if AVSource is initialized


    private:
        AVFSource(HRESULT* pHr);

        // file handling methods used to parse the file and initialize the objects
        HRESULT ParseHeader(void);
        HRESULT InternalCreatePresentationDescriptor(void);
        HRESULT CreateVideoStream(IMFStreamDescriptor** pStreamDescriptor);
        HRESULT CreateAudioStream(IMFStreamDescriptor** pStreamDescriptor);
        
        HRESULT SendOperation(SourceOperationType operationType);
        
        // internal asynchronous event handler methods
        HRESULT InternalOpen(ISourceOperation* pCommand);
        HRESULT InternalStart(ISourceOperation* pCommand);
        HRESULT InternalStop(void);
        HRESULT InternalPause(void);
        HRESULT InternalRequestSample(void);
        HRESULT InternalEndOfStream(void);

        HRESULT SendSampleToStream(AVFStream* pStream);

        HRESULT SelectStreams(IMFPresentationDescriptor *pPresentationDescriptor, const 
            PROPVARIANT varStart, bool isSeek);

        ~AVFSource(void);

        friend class AVFStream;

    private:
        volatile long m_cRef;                       // reference count
        
        size_t m_pendingEndOfStream;
        AVIFileParser* m_pAVIFileParser;
        CComAutoCriticalSection m_critSec;          // critical section

        CComPtr<IMFMediaEventQueue> m_pEventQueue;
        CComPtr<IMFPresentationDescriptor> m_pPresentationDescriptor;

        // an STL vector with media stream pointers
        vector<AVFStream*> m_mediaStreams;
        
        // current state of the source
        SourceState m_state;
};

