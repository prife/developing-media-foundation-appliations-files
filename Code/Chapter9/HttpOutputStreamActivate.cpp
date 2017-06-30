#include "HttpOutputStreamActivate.h"


CHttpOutputStreamActivate::CHttpOutputStreamActivate(DWORD requestPort) :
    m_cRef(0)
{
    m_requestPort = requestPort;
}


CHttpOutputStreamActivate::~CHttpOutputStreamActivate(void)
{
}


//////////////////////////////////////////////////////////////////////////////////////////
//
//  IMFActivate interface implementation
//
/////////////////////////////////////////////////////////////////////////////////////////

//
// Activate the CHttpOutputByteStream object
//
HRESULT CHttpOutputStreamActivate::ActivateObject(REFIID riid, void **ppv)
{
    HRESULT hr = S_OK;
    CComPtr<IMFByteStream> pByteStream;

    do
    {
        hr = CHttpOutputByteStream::CreateInstance(m_requestPort, &pByteStream);
        BREAK_ON_FAIL(hr);

        hr = pByteStream->QueryInterface(riid, ppv);
    }
    while(false);

    return hr;
}


//////////////////////////////////////////////////////////////////////////////////////////
//
//  IUnknown interface implementation
//
/////////////////////////////////////////////////////////////////////////////////////////
ULONG CHttpOutputStreamActivate::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG CHttpOutputStreamActivate::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT CHttpOutputStreamActivate::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(static_cast<IMFActivate*>(this));
    }
    else if (riid == IID_IMFActivate)
    {
        *ppv = static_cast<IMFActivate*>(this);
    }
    else if (riid == IID_IMFAttributes)
    {
        *ppv = static_cast<IMFAttributes*>(this);
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