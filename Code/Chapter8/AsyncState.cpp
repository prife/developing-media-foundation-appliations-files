#include "AsyncState.h"


CAsyncState::CAsyncState(AsyncEventType type) :
    m_cRef(0),
    m_eventType(type)
{
}



//
// Standard IUnknown interface implementation
//

//
// Increment reference count of the object.
//
ULONG CAsyncState::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

//
// Decrement the reference count of the object.
//
ULONG CAsyncState::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

//
// Get the interface specified by the riid from the class
//
HRESULT CAsyncState::QueryInterface(REFIID riid, void** ppv)
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
    else if (riid == IID_IAsyncState)
    {
        *ppv = static_cast<IAsyncState*>(this);
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