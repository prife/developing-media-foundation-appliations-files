// AVFByteStreamHandler.cpp : Implementation of AVFByteStreamHandler

#include "stdafx.h"
#include "AVFByteStreamHandler.h"


AVFByteStreamHandler::AVFByteStreamHandler(void) :
    m_cRef(1),
    m_pAVFSource(NULL),
    m_objectCreationCanceled(false)
{    
}



AVFByteStreamHandler::~AVFByteStreamHandler(void)
{
    if (m_pAVFSource != NULL)
    {
        m_pAVFSource->Release();
        m_pAVFSource = NULL;
    }
}


//
// IUnknown interface implementation
//
ULONG AVFByteStreamHandler::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG AVFByteStreamHandler::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT AVFByteStreamHandler::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(static_cast<IMFByteStreamHandler*>(this));
    }
    else if (riid == IID_IMFByteStreamHandler)
    {
        *ppv = static_cast<IMFByteStreamHandler*>(this);
    }
    else if (riid == IID_IMFAsyncCallback)
    {
        *ppv = static_cast<IMFAsyncCallback*>(this);
    }
    else if (riid == IID_IInitializeWithFile)
    {
        *ppv = static_cast<IInitializeWithFile*>(this);
    }
    else if (riid == IID_IPropertyStore)
    {
        *ppv = static_cast<IPropertyStore*>(this);
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


/////////////////////////////////////////////////////////////////////////
// IMFByteStreamHandler
/////////////////////////////////////////////////////////////////////////

//
// Begin asynchronously creating and initializing the IMFByteStreamHandler object.
//
HRESULT AVFByteStreamHandler::BeginCreateObject(IMFByteStream *pByteStream, LPCWSTR pwszURL, 
    DWORD dwFlags, IPropertyStore *pProps, IUnknown **ppIUnknownCancelCookie, 
    IMFAsyncCallback *pCallback, IUnknown *punkState)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        // Sanity check input arguments.
        BREAK_ON_NULL (pByteStream, E_POINTER);
        BREAK_ON_NULL (pCallback, E_POINTER);
        BREAK_ON_NULL (pwszURL, E_POINTER);

        // At this point the source should be NULL - otherwise multiple clients are trying
        // to create an AVFSource concurrently with the same byte stream handler - that is
        // not supported.
        if( m_pAVFSource != NULL)
        {
            hr = E_UNEXPECTED;
            break;
        }

        // Verify that the caller is requesting the creation of a MediaSource object -
        // the AVFByteStreamHandler doesn't support other object types.
        if ((dwFlags & MF_RESOLUTION_MEDIASOURCE) == 0)
        {
            hr = E_INVALIDARG;
            break;
        }
        
        // Create an asynchronous result that will be used to indicate to the caller that
        // the source has been created.  The result is stored in a class member variable.
        hr = MFCreateAsyncResult(NULL, pCallback, punkState, &m_pResult);
        BREAK_ON_FAIL(hr);
        
        // New object - creation was not canceled, reset the cancel flag.
        m_objectCreationCanceled = false;

        // just return a pointer to the byte stream handler as the cancel cookie object
        if (ppIUnknownCancelCookie != NULL)
        {
            hr = this->QueryInterface(IID_IUnknown, (void**)ppIUnknownCancelCookie);
            BREAK_ON_FAIL(hr);
        }        

        // create the main source worker object - the AVFSource
        hr = AVFSource::CreateInstance(&m_pAVFSource);
        BREAK_ON_FAIL(hr);

        // Begin source asynchronous open operation - tell the source to call this object 
        // when it is done
        hr = m_pAVFSource->BeginOpen(pwszURL, this, NULL);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    // if something failed, release all internal variables
    if(FAILED(hr))
    {
        SafeRelease(m_pAVFSource);
        m_pResult = NULL;
    }

    return hr;
}
      


//
// End asynchronously creating and initializing the IMFByteStreamHandler object
//
HRESULT AVFByteStreamHandler::EndCreateObject(IMFAsyncResult* pResult,
                                              MF_OBJECT_TYPE* pObjectType,
                                              IUnknown** ppObject)
{
    HRESULT hr = S_OK;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // Sanity checks input arguments.
        BREAK_ON_NULL (pResult, E_POINTER); 
        BREAK_ON_NULL (pObjectType, E_POINTER); 
        BREAK_ON_NULL (ppObject, E_POINTER);
        BREAK_ON_NULL (m_pAVFSource, E_UNEXPECTED);

        // initialize output parameters - the object has not been created yet, so if there
        // is an error these output parameters will contain the right values
        *pObjectType = MF_OBJECT_INVALID;
        *ppObject = NULL;

        // Check to see if there is an error.
        hr = pResult->GetStatus();
        
        if(SUCCEEDED(hr))
        {
            // if we got here, result indicated success in creating the source - therefore
            // we can return a flag indicating that we created a source
            *pObjectType = MF_OBJECT_MEDIASOURCE;

            // Since the handler just created a media source, get the media source interface
            // from the underlying AVFSource helper object.
            hr = m_pAVFSource->QueryInterface(IID_IMFMediaSource, (void**)ppObject);
        }

        // whatever happens, make sure the source is in a good state by resetting internal
        // variables
        SafeRelease(m_pAVFSource);
        m_pResult = NULL;
        m_objectCreationCanceled = false;
    }
    while(false);

    return hr;
}
        

//
// Cancel the asynchronous object creation operation.
//
HRESULT AVFByteStreamHandler::CancelObjectCreation(IUnknown* pIUnknownCancelCookie)
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    // if m_pResult is NULL, nobody is trying to create an object, and there is nothing to
    // cancel - return an error.  Otherwise, store the cancellation command.
    if(m_pResult == NULL)
    {
        return E_UNEXPECTED;
    }
    else
    {
        m_objectCreationCanceled = true;
        return S_OK;
    }
}

//
// Determine the maximum number of bytes the source needs to parse before it can determine 
// if the file format is something that it can read.
//
HRESULT AVFByteStreamHandler::GetMaxNumberOfBytesRequiredForResolution(QWORD* pqwBytes)
{
    if(pqwBytes == NULL)
        return E_INVALIDARG;

    // Just return some value - this functionality is implemented in the open operation of 
    // the source and AviFileParser, and does not depend on the byte stream.
    *pqwBytes = 1024;

    return S_OK;
}




/////////////////////////////////////////////////////////////////////////
//
// IMFAsyncCallback implementation
//
/////////////////////////////////////////////////////////////////////////

//
// Get the behavior information (duration, etc.) of the asynchronous callback operation - 
// not implemented.
//
HRESULT AVFByteStreamHandler::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue)
{
    return E_NOTIMPL;
}


// 
// Asynchronous worker function - called when the source has finished initializing.
//
HRESULT AVFByteStreamHandler::Invoke(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    do
    {
        BREAK_ON_NULL(m_pResult, E_UNEXPECTED);

        // If object creation was canceled, just delete the AVFSource, and return E_ABORT.
        if(m_objectCreationCanceled)
        {
            m_objectCreationCanceled = false;
            hr = E_ABORT;

            // release the source - it will not be needed now
            m_pAVFSource->Release();
            m_pAVFSource = NULL;
        }
        else
        {
            // Call EndOpen to finish asynchronous open operation, and check for errors
            // during parsing.  If this failed, the HR will be stored in the result.
            hr = m_pAVFSource->EndOpen(pResult);
        }

        // Store the result of the operation.
        m_pResult->SetStatus(hr);

        // Call back the caller with the result.
        hr = MFInvokeCallback(m_pResult);

        // Release the result for the client - it has been used and is no longer needed.
        m_pResult = NULL;
    }
    while(false);

    return hr;
}








////////////////////////////////////////////////////////////////////////////////////////
//
// IInitializeWithFile interface implementation
//
////////////////////////////////////////////////////////////////////////////////////////

//
// Initialize the IPropertyStore interface components of the AVFByteStreamHandler
//
HRESULT AVFByteStreamHandler::Initialize( LPCWSTR pszFilePath, DWORD grfMode )
{
    HRESULT hr = S_OK;
    AVIFileParser* pParser = NULL;

    do
    {
        BREAK_ON_NULL(pszFilePath, E_POINTER);

        // If the mode is trying to modify the properties, return access denied - 
        // read-write properties are not supported.
        if ((grfMode & (STGM_READWRITE | STGM_WRITE)) != 0 ) 
        {
            hr = STG_E_ACCESSDENIED;
            break;
        }

        // Create the AVI parser
        hr = AVIFileParser::CreateInstance(pszFilePath, &pParser);
        BREAK_ON_FAIL(hr);

        // Parse the AVI file header.
        hr = pParser->ParseHeader();
        BREAK_ON_FAIL(hr);

        hr = pParser->GetPropertyStore(&m_pPropertyStore);
        BREAK_ON_FAIL(hr);

        if(pParser != NULL)
        {
            delete pParser;
        }
    }
    while(false);

    return hr;
}



////////////////////////////////////////////////////////////////////////////////////////
// IPropertyStore interface implementation
////////////////////////////////////////////////////////////////////////////////////////
HRESULT AVFByteStreamHandler::Commit()
{
    if(m_pPropertyStore == NULL)
        return E_UNEXPECTED;

    return m_pPropertyStore->Commit();
}


HRESULT AVFByteStreamHandler::GetAt( DWORD iProp, PROPERTYKEY *pkey)
{
    if(m_pPropertyStore == NULL)
        return E_UNEXPECTED;

    return m_pPropertyStore->GetAt(iProp, pkey);
}


HRESULT AVFByteStreamHandler::GetCount( DWORD *cProps )
{
    if(m_pPropertyStore == NULL)
        return E_UNEXPECTED;

    return m_pPropertyStore->GetCount(cProps);
}


HRESULT AVFByteStreamHandler::GetValue( REFPROPERTYKEY key, PROPVARIANT *pv)
{
    if(m_pPropertyStore == NULL)
        return E_UNEXPECTED;

    return m_pPropertyStore->GetValue(key, pv);
}


HRESULT AVFByteStreamHandler::SetValue( REFPROPERTYKEY key, REFPROPVARIANT propvar)
{
    if(m_pPropertyStore == NULL)
        return E_UNEXPECTED;

    return m_pPropertyStore->SetValue(key, propvar);
}

