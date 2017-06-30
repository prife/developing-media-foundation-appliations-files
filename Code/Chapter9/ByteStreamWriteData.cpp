#include "ByteStreamWriteData.h"


CByteStreamWriteData::CByteStreamWriteData(BYTE* pData, DWORD dwData, IMFAsyncCallback* pCallback, IUnknown* pState) :
    m_cRef(0)
{
    m_pData = pData;
    m_dataLength = dwData;
    m_pCallback = pCallback;
    m_pStateObj = pState;
}


CByteStreamWriteData::~CByteStreamWriteData(void)
{
}


HRESULT CByteStreamWriteData::GetWriteData(BYTE** pWriteData, DWORD* dataLength)
{
    if(pWriteData == NULL || dataLength == NULL)
        return E_POINTER;

    *pWriteData = m_pData;
    *dataLength = m_dataLength;

    return S_OK;
}


HRESULT CByteStreamWriteData::SendCallback(HRESULT status)
{
    CComPtr<IMFAsyncResult> pResult;

    HRESULT hr = MFCreateAsyncResult(this, m_pCallback, m_pStateObj, &pResult);
    if(SUCCEEDED(hr))
    {
        pResult->SetStatus(status);
        hr = MFPutWorkItemEx(MFASYNC_CALLBACK_QUEUE_STANDARD, pResult);
    }

    return hr;
}


HRESULT CByteStreamWriteData::GetStateObject(IUnknown** ppState)
{
    if(ppState == NULL)
        return E_POINTER;

    *ppState = m_pStateObj.Detach();
    return S_OK;
}

HRESULT CByteStreamWriteData::GetCallback(IMFAsyncCallback** ppCallback)
{
    if(ppCallback == NULL)
        return E_POINTER;

    *ppCallback = m_pCallback.Detach();
    return S_OK;
}



//
// Standard IUnknown interface implementation
//
ULONG CByteStreamWriteData::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG CByteStreamWriteData::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT CByteStreamWriteData::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(this);
    }
    else if (riid == IID_IAsyncWriteData)
    {
        *ppv = static_cast<IAsyncWriteData*>(this);
    }
    else
    {
        *ppv = NULL;
        hr = E_NOINTERFACE;
    }

    if(SUCCEEDED(hr))
        AddRef();

    return hr;
}