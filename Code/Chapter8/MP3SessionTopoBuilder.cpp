#include "MP3SessionTopoBuilder.h"


CMP3SessionTopoBuilder::CMP3SessionTopoBuilder(PCWSTR pUrl) :
    m_cRef(1)
{
    // allocate a space for and store the path passed in
    if(wcslen(pUrl) > 0)
    {
        m_pFileUrl = new (std::nothrow) WCHAR[wcslen(pUrl) + 1];

        if(m_pFileUrl != NULL)
        {
            wcscpy_s(m_pFileUrl, wcslen(pUrl) + 1, pUrl);
        }
    }
}


CMP3SessionTopoBuilder::~CMP3SessionTopoBuilder(void)
{
    if(m_pSource != NULL)
    {
        m_pSource->Shutdown();
        m_pSource.Release();
    }

    if(m_pSink != NULL)
    {
        m_pSink->Shutdown();
        m_pSink.Release();
    }

    if(m_pEventQueue != NULL)
    {
        m_pEventQueue->Shutdown();
        m_pEventQueue.Release();
    }

    if(m_pFileUrl != NULL)
    {
        delete m_pFileUrl;
    }
}





//////////////////////////////////////////////////////////////////////////////////////////
//
//  IUnknown interface implementation
//
/////////////////////////////////////////////////////////////////////////////////////////

ULONG CMP3SessionTopoBuilder::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG CMP3SessionTopoBuilder::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT CMP3SessionTopoBuilder::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(static_cast<IMFMediaEventGenerator*>(this));
    }
    else if (riid == IID_IMFMediaEventGenerator)
    {
        *ppv = static_cast<IMFMediaEventGenerator*>(this);
    }
    else if (riid == IID_IMFAsyncCallback)
    {
        *ppv = static_cast<IMFAsyncCallback*>(this);
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




//
// Load the custom topology for decoding MP3 files
//
HRESULT CMP3SessionTopoBuilder::LoadCustomTopology(void)
{
    HRESULT hr = S_OK;

    do
    {
        // directly instantiate the MP3 decoder MFT
        hr = m_pDecoder.CoCreateInstance(CLSID_CMP3DecMediaObject);
        BREAK_ON_FAIL(hr);

        // instantiate the helper audio resampler MFT
        hr = m_pResampler.CoCreateInstance(CLSID_CResamplerMediaObject);
        BREAK_ON_FAIL(hr);

        // create the audio renderer
        hr = MFCreateAudioRenderer(NULL, &m_pSink);
        BREAK_ON_FAIL(hr);

        // as the last step begin asynchronously creating the media source through its
        // IMFByteStreamHandler - do this last so that after the source is created we can
        // negotiate the media types between the rest of the components
        hr = BeginCreateMediaSource();
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Negotiate media types between MF components after the IMFByteStreamHandler has created 
// the source.
//
HRESULT CMP3SessionTopoBuilder::HandleByteStreamHandlerEvent(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;
    MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;
    CComPtr<IUnknown> pUnkSource;

    do
    {
        // get the actual source by calling IMFByteStreamHandler::EndCreateObject()
        hr = m_pByteStreamHandler->EndCreateObject(pResult, &objectType, &pUnkSource);
        BREAK_ON_FAIL(hr);

        // make sure that what was created was the media source
        if(objectType != MF_OBJECT_MEDIASOURCE)
        {
            hr = E_UNEXPECTED;
            break;
        }

        // get the IMFMediaSource pointer from the IUnknown we got from EndCreateObject
        m_pSource = pUnkSource;
        BREAK_ON_NULL(m_pSource, E_UNEXPECTED);

        // call a function to negotiate the media types between each MF component in the
        // topology
        hr = NegotiateMediaTypes();
        BREAK_ON_FAIL(hr);

        // fire the MESessionTopologyStatus event with a pointer to the topology
        hr = FireTopologyReadyEvent();
    }
    while(false);

    return hr;
}

//
// Fire the MESessionTopologyStatus event signaling that the topology is ready
//
HRESULT CMP3SessionTopoBuilder::FireTopologyReadyEvent(void)
{
    HRESULT hr = S_OK;
    PROPVARIANT variantStatus;
    CComPtr<IMFMediaEvent> pEvent;

    do
    {
        // initialize the structure
        PropVariantInit(&variantStatus);

        // initialize the PROPVARIANT with the pointer to the topology
        variantStatus.vt = VT_UNKNOWN;
        variantStatus.punkVal = m_pTopology.p;

        // create an IMFMediaEvent object that will hold MESessionTopologyStatus event 
        // and a pointer to the new topology
        hr = MFCreateMediaEvent(
            MESessionTopologyStatus,    // event type
            GUID_NULL,                  // no extended event type
            hr,                         // result of operations
            &variantStatus,             // pointer to the topology
            &pEvent);
        BREAK_ON_FAIL(hr);

        // set the topology status to indicate that the topology is ready
        hr = pEvent->SetUINT32(MF_EVENT_TOPOLOGY_STATUS, MF_TOPOSTATUS_READY);
        BREAK_ON_FAIL(hr);

        // queue the event
        hr = m_pEventQueue->QueueEvent(pEvent);
    }
    while(false);

    // free any internal variables that can be freed in the PROPVARIANT
    PropVariantClear(&variantStatus);

    return hr;
}





//
// Find matching media types between each MF component, and set them
//
HRESULT CMP3SessionTopoBuilder::NegotiateMediaTypes(void)
{
    HRESULT hr = S_OK;

    do
    {
        // find matching type between source and the decoder
        hr = ConnectSourceToMft(m_pDecoder);
        BREAK_ON_FAIL(hr);

        // set the media type between the decoder and the resampler
        hr = ConnectMftToMft(m_pDecoder, m_pResampler);
        BREAK_ON_FAIL(hr);

        // set the media type between the resampler and the audio sink
        hr = ConnectMftToSink(m_pResampler);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}



//
// Negotiate media type between source and the specified MFT.
//
HRESULT CMP3SessionTopoBuilder::ConnectSourceToMft(IMFTransform* pMft)
{
    HRESULT hr = S_OK;
    CComPtr<IMFStreamDescriptor> pStreamDescriptor;
    CComPtr<IMFMediaTypeHandler> pMediaTypeHandler;
    CComPtr<IMFMediaType> pMediaType;
    BOOL streamSelected = FALSE;
    DWORD decoderInputStreamId = 0;

    DWORD sourceTypesCount = 0;

    do
    {
        BREAK_ON_NULL(pMft, E_UNEXPECTED);

        // get the presentation descriptor for the source
        hr = m_pSource->CreatePresentationDescriptor(&m_pPresentation);
        BREAK_ON_FAIL(hr);

        // get the stream descriptor for the first stream
        hr = m_pPresentation->GetStreamDescriptorByIndex(0, &streamSelected, 
            &pStreamDescriptor);
        BREAK_ON_FAIL(hr);

        // get the media type handler for the source
        hr = pStreamDescriptor->GetMediaTypeHandler(&pMediaTypeHandler);
        BREAK_ON_FAIL(hr);

        // get the number of media types that are exposed by the source stream
        hr = pMediaTypeHandler->GetMediaTypeCount(&sourceTypesCount);
        BREAK_ON_FAIL(hr);        

        // go through every media type exposed by the source, and try each one with the sink
        for(DWORD x = 0; x < sourceTypesCount; x++)
        {
            pMediaType = NULL;

            // get a media type from the source by index
            hr = pMediaTypeHandler->GetMediaTypeByIndex(x, &pMediaType);
            BREAK_ON_FAIL(hr);

            // try to set the input media type on the decoder - assume that the input stream
            // ID is 0, since this is a well-known MFT
            hr = pMft->SetInputType(0, pMediaType, 0);
            if(SUCCEEDED(hr))
            {
                // if the decoder accepted the input media type, set it on the source
                hr = pMediaTypeHandler->SetCurrentMediaType(pMediaType);
                BREAK_ON_FAIL(hr);
                break;
            }
        }
        
        // if the type was found, hr will be S_OK - otherwise hr will indicate a failure to
        // either get the media type by index, set it on the decoder, or set it on the 
        // media type handler
        BREAK_ON_FAIL(hr);

        // if the source stream is not activated, activate it
        if(!streamSelected)
        {
            hr = m_pPresentation->SelectStream(0);
        }
    }
    while(false);

    return hr;
}


//
// Find matching media type for two MFTs, and set it
//
HRESULT CMP3SessionTopoBuilder::ConnectMftToMft(IMFTransform* pMft1, IMFTransform* pMft2)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaType> pMediaType;
    DWORD mft1OutputStreamId = 0;
    DWORD mft2InputStreamId = 0;

    DWORD mft1TypeIndex = 0;

    do
    {
        BREAK_ON_NULL(pMft1, E_UNEXPECTED);
        BREAK_ON_NULL(pMft2, E_UNEXPECTED);
        
        // loop through all of the avialable output types exposed by the upstream MFT, and
        // try each of them as the input type of the downstream MFT.
        while(true)
        {
            pMediaType = NULL;

            // get the type with the mftTypeIndex index from the upstream MFT
            hr = pMft1->GetOutputAvailableType(mft1OutputStreamId, mft1TypeIndex++, 
                &pMediaType);
            BREAK_ON_FAIL(hr);

            // try to set the input type on the downstream MFT
            hr = pMft2->SetInputType(mft2InputStreamId, pMediaType, 0);
            if(SUCCEEDED(hr))
            {
                // if we succeeded, set the output type on the upstream component
                hr = pMft1->SetOutputType(mft1OutputStreamId, pMediaType, 0);
                break;
            }
        }
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}



//
// Find and set the media type between the specified MFT and the audio sink
//
HRESULT CMP3SessionTopoBuilder::ConnectMftToSink(IMFTransform* pMFTransform)
{
    HRESULT hr = S_OK;
    CComPtr<IMFTransform> pMft = pMFTransform;
    CComPtr<IMFMediaTypeHandler> pMediaTypeHandler;
    CComPtr<IMFMediaType> pMediaType;

    // assume that the decoder output stream ID is 0, since it's a well-known object
    DWORD mftOutputStreamId = 0;
    DWORD mftTypeIndex = 0;

    do
    {
        BREAK_ON_NULL(pMft, E_UNEXPECTED);
        BREAK_ON_NULL(m_pSink, E_UNEXPECTED);

        // get the first available stream from the sink
        hr = m_pSink->GetStreamSinkByIndex(0, &m_pStreamSink);
        BREAK_ON_FAIL(hr);

        // get the media type handler for the sink
        hr = m_pStreamSink->GetMediaTypeHandler(&pMediaTypeHandler);
        BREAK_ON_FAIL(hr);

        // loop through all of the avialable types exposed by the decoder, and try each of
        // them on the sink
        while(true)
        {
            pMediaType = NULL;

            // get the type with the mftTypeIndex index from the decoder
            hr = pMft->GetOutputAvailableType(mftOutputStreamId, mftTypeIndex++, 
                    &pMediaType);
            BREAK_ON_FAIL(hr);

            // try to set the media type on the media type handler for the sink
            hr = pMediaTypeHandler->SetCurrentMediaType(pMediaType);
            if(SUCCEEDED(hr))
            {
                // set the output media type on the MFT
                hr = pMft->SetOutputType(mftOutputStreamId, pMediaType, 0);
                break;
            }
        }
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}




//
// Create a media source for the cached file URL.
//
HRESULT CMP3SessionTopoBuilder::BeginCreateMediaSource(void)
{
    HRESULT hr = S_OK;
    CComPtr<IMFByteStream> pByteStream;
    CComPtr<IAsyncState> pState;

    do
    {
        // create an IMFByteStreamHandler object for the MP3 file
        hr = m_pByteStreamHandler.CoCreateInstance(CLSID_MP3ByteStreamPlugin);
        BREAK_ON_FAIL(hr);

        // open the file and get its IMFByteStream object
        hr = MFCreateFile(
                MF_ACCESSMODE_READ,                 // open the file for reading
                MF_OPENMODE_FAIL_IF_NOT_EXIST,      // fail if file does not exist
                MF_FILEFLAGS_NONE,                  // default behavior
                m_pFileUrl,                         // URL to the file
                &pByteStream);                      // get result here
        BREAK_ON_FAIL(hr);

        // create a state object that will identify the asynchronous operation to the Invoke
        pState = new (std::nothrow) CAsyncState(AsyncEventType_ByteStreamHandlerEvent);
        BREAK_ON_NULL(pState, E_OUTOFMEMORY);

        // create the media source from the IMFByteStreamHandler for the MP3 file
        hr = m_pByteStreamHandler->BeginCreateObject(
                pByteStream,                    // byte stream to the file
                m_pFileUrl,                     // URL-style path to the file 
                MF_RESOLUTION_MEDIASOURCE,      // create a media source
                NULL,                           // no custom configuration properties
                NULL,                           // no cancel cookie
                this,                           // IMFAsyncCallback that will be called
                pState);                        // custom state object indicating event type
    }
    while(false);

    return hr;
}
