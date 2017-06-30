#pragma once

#include <atlbase.h>
#include <Mfidl.h>
#include <Mferror.h>
#include <Mfapi.h>

#include <InitGuid.h>

struct __declspec(uuid("BF1998FC-9CFE-4336-BF07-50B10F9A9AFE")) IAsyncWriteData;

DEFINE_GUID(IID_IAsyncWriteData, 0xBF1998FC, 0x9CFE, 0x4336, 0xbf, 0x07, 0x50, 0xb1, 0x0f, 
    0x9a, 0x9a, 0xfe);

struct IAsyncWriteData : public IUnknown
{
    public:
        virtual HRESULT GetWriteData(BYTE** pWriteData, DWORD* dataLength) = 0;
        virtual HRESULT SendCallback(HRESULT status) = 0;
        virtual HRESULT GetStateObject(IUnknown** ppState) = 0;
        virtual HRESULT GetCallback(IMFAsyncCallback** ppCallback) = 0;
};



class CByteStreamWriteData :
    public IAsyncWriteData
{
    public:
        CByteStreamWriteData(BYTE* pData, DWORD dwData, IMFAsyncCallback* callback, 
            IUnknown* pState);

        // IUnknown interface implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

        virtual HRESULT GetWriteData(BYTE** pWriteData, DWORD* dataLength);
        virtual HRESULT SendCallback(HRESULT status);
        virtual HRESULT GetStateObject(IUnknown** ppState);
        virtual HRESULT GetCallback(IMFAsyncCallback** ppCallback);

    private:
        ~CByteStreamWriteData(void);

        volatile long m_cRef;

        BYTE* m_pData;
        DWORD m_dataLength;
        CComPtr<IMFAsyncCallback> m_pCallback;
        CComPtr<IUnknown> m_pStateObj;
};

