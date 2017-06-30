#include "TranscodeApi.h"



CTranscodeApiTopoBuilder::CTranscodeApiTopoBuilder(void)
{
}


CTranscodeApiTopoBuilder::~CTranscodeApiTopoBuilder(void)
{
    if(m_pSource != NULL)
        m_pSource->Shutdown();
}



//
// Create a topology object for transcoding the specified input file and storing it in a
// file with the provied filename.
//
HRESULT CTranscodeApiTopoBuilder::CreateTranscodeTopology(PCWSTR pszInput, PCWSTR pszOutput)
{
    HRESULT hr = S_OK;
    
    do
    {
        // standard media source creation
        hr = CreateMediaSource(pszInput);
        BREAK_ON_FAIL(hr);

        // if the transcode profile has not been set yet, create a default transcode profile
        // for WMA v8 audio, WMV3 video, and ASF
        if(m_pTranscodeProfile == NULL)
        {
            hr = SetTranscodeProfile(
                MFAudioFormat_WMAudioV8,            // WMA v8 audio format
                MFVideoFormat_WMV3,                 // WMV9 video format
                MFTranscodeContainerType_ASF);      // ASF file container
            BREAK_ON_FAIL(hr);
        }

        // create the actual transcode topology based on the transcode profile
        hr = MFCreateTranscodeTopology(
            m_pSource,                      // the source of the content to transcode
            pszOutput,                      // output filename
            m_pTranscodeProfile,            // transcode profile to use
            &m_pTopology);                  // resulting topology
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Create a media source for the specified URL string.  The URL can be a path to a stream, 
// or it can be a path to a local file.
//
HRESULT CTranscodeApiTopoBuilder::CreateMediaSource(PCWSTR sURL)
{
    HRESULT hr = S_OK;
    MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;
    CComPtr<IMFSourceResolver> pSourceResolver = NULL;
    CComPtr<IUnknown> pSource;

    do
    {
        // Create the source resolver.
        hr = MFCreateSourceResolver(&pSourceResolver);
        BREAK_ON_FAIL(hr);

        // Use the syncrhonous source resolver to create the media source.
        hr = pSourceResolver->CreateObjectFromURL(
            sURL,                       // URL of the source.
            MF_RESOLUTION_MEDIASOURCE | 
                MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE,  
                                        // indicate that we want a source object, and 
                                        // pass in optional source search parameters
            NULL,                       // Optional property store for extra parameters
            &objectType,                // Receives the created object type.
            &pSource                    // Receives a pointer to the media source.
            );
        BREAK_ON_FAIL(hr);

        // Get the IMFMediaSource interface from the media source.
        m_pSource = pSource;
        BREAK_ON_NULL(m_pSource, E_NOINTERFACE);
    }
    while(false);

    return hr;
}


//
// Create a transcode profile for the specified target video, audio, and container types
//
HRESULT CTranscodeApiTopoBuilder::SetTranscodeProfile(
    const GUID& audioFormat,       // target audio format
    const GUID& videoFormat,       // target video format
    const GUID& containerType)     // target file container type
{
    HRESULT hr = S_OK;
    
    do
    {
        // create a new transcode profile
        hr = MFCreateTranscodeProfile(&m_pTranscodeProfile);
        BREAK_ON_FAIL(hr);
        
        // set the audio attributes on the transcode profile
        hr = SetAudioAttributes(audioFormat);
        BREAK_ON_FAIL(hr);

        // set the video attributes on the transcode profile
        hr = SetVideoAttributes(videoFormat);
        BREAK_ON_FAIL(hr);

        // set the container attributes indicating what type of file to create
        hr = SetContainerAttributes(containerType);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}



//
// Initialize target audio attributes
//
HRESULT CTranscodeApiTopoBuilder::SetAudioAttributes(const GUID& audioFormat)
{
    HRESULT hr = S_OK;
    CComPtr<IMFCollection> pAudioTypeCollection;
    CComPtr<IMFAttributes> pAudioAttrs;
    
    do
    {
        // verify that the transcode profile is available
        BREAK_ON_NULL(m_pTranscodeProfile, E_UNEXPECTED);
        
        // Construct the flags that will be used during enumeration of all the encoder MFTs
        // on the machine.  The flags object will be passed into the MFTEnumEx() function
        // internally.
        DWORD dwFlags = 
            (MFT_ENUM_FLAG_ALL & (~MFT_ENUM_FLAG_FIELDOFUSE))   
            | MFT_ENUM_FLAG_SORTANDFILTER;                      


        // enumerate all of the audio encoders that match the specified parameters and
        // find all audio types that can be generated
        hr = MFTranscodeGetAudioOutputAvailableTypes(
            audioFormat,                      // specify the requested audio format
            dwFlags,                          // get all MFTs except for the FOU, and sort
            NULL,                             // no custom attributes
            &pAudioTypeCollection);           // store result in specified collection
        BREAK_ON_FAIL(hr);

        // get the first element from the collection of media types, copy all the
        // information of the first type into a new attribute collection, and return that
        // attribute collection
        hr = GetTypeAttributesFromTypeCollection(pAudioTypeCollection, 0, pAudioAttrs);
        BREAK_ON_FAIL(hr);

        // set the audio attributes on the transcode profile
        hr = m_pTranscodeProfile->SetAudioAttributes(pAudioAttrs);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}



//
// Initialize target video attributes
//
HRESULT CTranscodeApiTopoBuilder::SetVideoAttributes(const GUID& videoFormat)
{
    HRESULT hr = S_OK;
    CComPtr<IMFCollection> pVideoTypeCollection;
    CComPtr<IMFAttributes> pVideoAttrs;

    DWORD dwFlags = 0;
    
    do
    {
        // verify that the transcode profile is available
        BREAK_ON_NULL(m_pTranscodeProfile, E_UNEXPECTED);

        // Construct the flags that will be used during enumeration of all the encoder MFTs
        // on the machine.  The flags object will be passed into the MFTEnumEx() function
        // internally.
        dwFlags =  (MFT_ENUM_FLAG_ALL & (~MFT_ENUM_FLAG_FIELDOFUSE))   
                | MFT_ENUM_FLAG_SORTANDFILTER; 

        // enumerate all of the video encoders that match the specified parameters and
        // find all video types that can be generated
        hr = GetVideoOutputAvailableTypes(
            videoFormat,                      // specify the requested video format
            dwFlags,                          // get all MFTs except for the FOU, and sort
            pVideoTypeCollection);            // return result in specified collection
        BREAK_ON_FAIL(hr);

        // get the first element from the collection, copy all the information into an
        // attribute collection, and return that attribute collection
        hr = GetTypeAttributesFromTypeCollection(pVideoTypeCollection, 0, pVideoAttrs);
        BREAK_ON_FAIL(hr); 
        
       
        // set custom MF video information specific to what we want to do
        
        // Set the frame size as two numbers stored as a packed 64-bit value
        hr = MFSetAttributeSize(
            pVideoAttrs,            // target attribute collection
            MF_MT_FRAME_SIZE,       // attribute ID GUID
            720, 576);              // two 32-bit integers to be packed as a 64-bit value
        BREAK_ON_FAIL(hr);  

        // Set the frame rate as a fraction of two numbers
        hr = MFSetAttributeRatio(
            pVideoAttrs,            // target attribute collection
            MF_MT_FRAME_RATE,       // attribute ID GUID
            30000, 1001);                 // two 32-bit integers to be packed as a 64-bit value
        BREAK_ON_FAIL(hr);  

        // set the target average bitrate of the video
        hr = pVideoAttrs->SetUINT32(MF_MT_AVG_BITRATE, 1000000);
        BREAK_ON_FAIL(hr);  

        // store the video attributes in the transcode profile
        hr = m_pTranscodeProfile->SetVideoAttributes(pVideoAttrs);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Initialize target container attributes
//
HRESULT CTranscodeApiTopoBuilder::SetContainerAttributes(const GUID& containerType)
{
    HRESULT hr = S_OK;
    CComPtr<IMFAttributes> pContainerAttributes;

    do
    {
        // verify that the transcode profile is available
        BREAK_ON_NULL(m_pTranscodeProfile, E_UNEXPECTED);

        // create an attribute collection for one element
        hr = MFCreateAttributes(&pContainerAttributes, 1);
        BREAK_ON_FAIL(hr);

        // store an attribute that indicates that we want to write to an ASF file
        hr = pContainerAttributes->SetGUID(
            MF_TRANSCODE_CONTAINERTYPE,         // attribute ID GUID - container type
            containerType);                     // generate the specified container
        BREAK_ON_FAIL(hr);

        // store the container attributes in the transcode profile
        hr = m_pTranscodeProfile->SetContainerAttributes(pContainerAttributes);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Get information about a media type with the specified index from the type colelction.
//
HRESULT CTranscodeApiTopoBuilder::GetTypeAttributesFromTypeCollection(
    CComPtr<IMFCollection>& pTypeCollection,        // collection of media types
    int typeIndex,                                  // index of the type to extract
    CComPtr<IMFAttributes>& pAttrCollection)        // return information on that type
{
    HRESULT hr = S_OK;
    CComQIPtr<IMFMediaType> pType;
    CComPtr<IUnknown> pUnknown;

    do
    {
        // Get the first IUnknown object from the collection
        hr = pTypeCollection->GetElement(typeIndex, &pUnknown);
        BREAK_ON_FAIL(hr);

        // implicitly Query pUnknown for the IMFMediaType interface during assignment of
        // the CComQIPtr object
        pType = pUnknown;
        BREAK_ON_NULL(pType, E_NOINTERFACE);

        // create a new attribute collection for the type extracted
        hr = MFCreateAttributes(&pAttrCollection, 0);       
        BREAK_ON_FAIL(hr);

        // copy the information from the extracted type into the new attribute collection
        hr = pType->CopyAllItems(pAttrCollection);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}




HRESULT CTranscodeApiTopoBuilder::GetVideoOutputAvailableTypes(
    const GUID& videoSubtypeGuid, 
    DWORD flags, 
    CComPtr<IMFCollection>& pTypeCollection)
{
    HRESULT hr = S_OK;
    IMFActivate** pActivateArray = NULL;
    MFT_REGISTER_TYPE_INFO outputType;
    UINT32 nMftsFound = 0;

    do
    {
        // create the collection in which we will return the types found
        hr = MFCreateCollection(&pTypeCollection);
        BREAK_ON_FAIL(hr);

        // initialize the structure that describes the output streams that the encoders must
        // be able to produce.  In this case we want video encoders - so major type is video, 
        // and we want the specified subtype
        outputType.guidMajorType = MFMediaType_Video;
        outputType.guidSubtype = videoSubtypeGuid;

        // get a collection of MFTs that fit the requested pattern - video encoders,
        // with the specified subtype, and using the specified search flags
        hr = MFTEnumEx(
            MFT_CATEGORY_VIDEO_ENCODER,         // type of object to find - video encoders
            flags,                              // search flags
            NULL,                               // match all input types for an encoder
            &outputType,                        // get encoders with specified output type
            &pActivateArray,
            &nMftsFound);
        BREAK_ON_FAIL(hr);

        // now that we have an array of activation objects for matching MFTs, loop through 
        // each of those MFTs, extracting all possible and available formats from each of them
        for(UINT32 x = 0; x < nMftsFound; x++)
        {
            CComPtr<IMFTransform> pEncoder;
            UINT32 typeIndex = 0;

            // activate the encoder that corresponds to the activation object
            hr = pActivateArray[x]->ActivateObject(IID_IMFTransform, 
                (void**)&pEncoder);

            // while we don't have a failure, get each available output type for the MFT 
            // encoder we keep looping until there are no more available types.  If there 
            // are no more types for the encoder, IMFTransform::GetOutputAvailableTypes[] 
            // will return MF_E_NO_MORE_TYPES
            while(SUCCEEDED(hr))
            {
                CComPtr<IMFMediaType> pType;
                
                // get the avilable type for the type index, and increment the typeIndex 
                // counter
                hr = pEncoder->GetOutputAvailableType(0, typeIndex++, &pType);
                if(SUCCEEDED(hr))
                {
                    // store the type in the IMFCollection
                    hr = pTypeCollection->AddElement(pType);
                }
            }
        }
    }
    while(false);

    // possible valid errors that may be returned after the previous for loop is done
    if(hr == MF_E_NO_MORE_TYPES  ||  hr == MF_E_TRANSFORM_TYPE_NOT_SET)
        hr = S_OK;

    // if we successfully used MFTEnumEx() to allocate an array of the MFT activation 
    // objects, then it is our responsibility to release each one and free up the memory 
    // used by the array
    if(pActivateArray != NULL)
    {
        // release the individual activation objects
        for(UINT32 x = 0; x < nMftsFound; x++)
        {
            if(pActivateArray[x] != NULL)
                pActivateArray[x]->Release();
        }

        // free the memory used by the array
        CoTaskMemFree(pActivateArray);
        pActivateArray = NULL;
    }

    return hr;
}