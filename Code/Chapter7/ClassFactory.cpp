#include "StdAfx.h"
#include "ClassFactory.h"



ClassFactory::ClassFactory(void)
{
    InterlockedIncrement(&g_dllLockCount);
    m_cRef = 1;
}



ClassFactory::~ClassFactory(void)
{
    InterlockedDecrement(&g_dllLockCount);
}




// 
// IClassFactory::CreateInstance implementation.  Attempts to create an instance of the 
// specified COM object, and return the object in the ppvObject pointer.
//
HRESULT STDMETHODCALLTYPE ClassFactory::CreateInstance(
    IUnknown *pUnkOuter,      // aggregation object - used only for for aggregation
    REFIID riid,              // IID of the object to create
    void **ppvObject)         // on return contains pointer to the new object
{
    HRESULT hr = S_OK;
    CAviSink* pSink;
    WCHAR moduleFilenameStr[MAX_PATH];
    WCHAR targetFilenameStr[MAX_PATH];

    // this is a non-aggregating COM object - return a failure if we are asked to
    // aggregate
    if ( pUnkOuter != NULL )
        return CLASS_E_NOAGGREGATION;

    if(ppvObject == NULL)
        return E_POINTER;

    // get the path to the DLL
    GetModuleFileName(g_hModule, moduleFilenameStr, MAX_PATH);

    // isolate just the path, removing the DLL filename
    for(DWORD x = (DWORD)wcslen(moduleFilenameStr); x >= 0; x--)
    {
        if(moduleFilenameStr[x] == L'\\')
        {
            moduleFilenameStr[x] = L'\0';
            break;
        }
    }

    // construct the target AVI file path by appending the "\test.avi" string to the path
    StringCchPrintf(targetFilenameStr, MAX_PATH, L"%s\\test.avi", moduleFilenameStr);

    do
    {
        // create a new instance of the AVI sink object, passing in the target AVI file
        pSink = new (std::nothrow) CAviSink(targetFilenameStr, &hr);
        BREAK_ON_FAIL(hr);
        BREAK_ON_NULL(pSink, E_OUTOFMEMORY);

        // Attempt to QI the new object for the requested interface
        hr = pSink->QueryInterface(riid, ppvObject);
    }
    while(false);

    // if we failed to QI for the interface for any reason, then this must be the wrong object,
    // delete it and make sure the ppvObject pointer contains NULL.
    if(FAILED(hr) && pSink != NULL)
    {
        delete pSink;
        *ppvObject = NULL;
    }

    return hr;
}



// 
// IClassFactory::LockServer implementation.  This function is used to lock
// the object in memory in order to improve performance and reduce unloading and
// reloading of the DLL.
//
HRESULT STDMETHODCALLTYPE ClassFactory::LockServer(BOOL fLock)
{
    if(fLock)
    {
        InterlockedIncrement(&g_dllLockCount);
    }
    else
    {
        InterlockedDecrement(&g_dllLockCount);
    }

    return S_OK;
}



//
// IUnknown methods
//
HRESULT ClassFactory::QueryInterface(REFIID riid, void** ppv)
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
    else if(riid == IID_IClassFactory)
    {
        *ppv = static_cast<IClassFactory*>(this);
    }
    else
    {
        *ppv = NULL;
        hr = E_NOINTERFACE;
    }

    if(SUCCEEDED(hr))
    {
        AddRef();
    }

    return hr;
}

ULONG ClassFactory::AddRef(void)  
{ 
    return InterlockedIncrement(&m_cRef); 
}
        
ULONG ClassFactory::Release(void) 
{ 
    long cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}