#pragma once
#include "unknwn.h"

#include <atlbase.h>
#include <Mfidl.h>
#include <Mferror.h>
#include <InitGuid.h>


enum AsyncEventType
{
    AsyncEventType_SourceEvent,
    AsyncEventType_SourceStreamEvent,
    AsyncEventType_StreamSinkEvent,
    AsyncEventType_ByteStreamHandlerEvent,
    AsyncEventType_SyncMftSampleRequest
};


// {88ACF5E6-2ED1-4780-87B1-D71814C2D42A}
DEFINE_GUID(IID_IAsyncState, 0x88acf5e6, 0x2ed1, 0x4780, 0x87, 0xb1, 0xd7, 0x18, 0x14, 0xc2, 0xd4, 0x2a);

// Microsoft-specific extension necessary to support the __uuidof(IAsyncState) notation.
class __declspec(uuid("88ACF5E6-2ED1-4780-87B1-D71814C2D42A")) IAsyncState;


class IAsyncState : public IUnknown
{
    public:
        virtual AsyncEventType EventType(void) = 0;
};


class CAsyncState :
    public IAsyncState
{
    public:
        CAsyncState(AsyncEventType type);
        ~CAsyncState(void) {};

        // IUnknown interface implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

        virtual AsyncEventType EventType(void) { return m_eventType; }

    private:
        volatile long m_cRef;                       // reference count

        AsyncEventType m_eventType;
};
