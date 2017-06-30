#include "SampleRequestToken.h"


CSampleRequestToken::CSampleRequestToken(void) :
    m_cRef(1)
{
}


CSampleRequestToken::~CSampleRequestToken(void)
{
}


//
// Fake IUnknown interface implementation - does not delete self when refcount falls to zero
//
ULONG CSampleRequestToken::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG CSampleRequestToken::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT CSampleRequestToken::QueryInterface(REFIID riid, void** ppv)
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
    else
    {
        *ppv = NULL;
        hr = E_NOINTERFACE;
    }

    if(SUCCEEDED(hr))
        AddRef();

    return hr;
}