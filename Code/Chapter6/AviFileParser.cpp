#include "StdAfx.h"
#include "AviFileParser.h"

//
// Create a new instance of the AVI file parser for the specified file
//
HRESULT AVIFileParser::CreateInstance(const WCHAR* url, AVIFileParser **ppParser)
{
    HRESULT hr = S_OK;
    AVIFileParser* pParser = NULL;

    do
    {
        BREAK_ON_NULL (ppParser, E_POINTER);

        DWORD pathCount = MAX_PATH;
        wchar_t temp[MAX_PATH];

        // if it's necessary, convert the URL into an MS-DOS format path - if not, just 
        // copy the path to the temp string
        if(PathIsURL(url))
        {
            // convert a URL-style path to an MS-DOS style path string
            hr = PathCreateFromUrl(url, temp, &pathCount, 0);
            BREAK_ON_FAIL(hr);
        }
        else
        {
            wcscpy_s(temp, MAX_PATH, url);
        }

        // handle a case where the file either doesn't exist or is inaccessable
        if (!PathFileExists(temp))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

        // create a new parser
        pParser = new (std::nothrow) AVIFileParser(temp);
        BREAK_ON_NULL (pParser, E_OUTOFMEMORY);

        // initialize the parser
        hr = pParser->Init();
        BREAK_ON_FAIL(hr);

        *ppParser = pParser;
    }
    while(false);


    if (FAILED(hr) && pParser != NULL)
    {
        delete pParser;
    }

    return hr;
}

AVIFileParser::AVIFileParser(const WCHAR* url) : m_pAviFile(NULL),
                                                   m_pVideoStream(NULL),
                                                   m_pAudioStream(NULL),
                                                   m_currentVideoSample(0),
                                                   m_currentAudioSample(0),
                                                   m_duration(0),
                                                   m_audioSampleTime(0),
                                                   m_url(NULL),
                                                   m_nextKeyframe(0)
{
    // allocate a space for and store the path passed in
    if(url != NULL && wcslen(url) > 0)
    {
        m_url = new (std::nothrow) WCHAR[wcslen(url) + 1];
        wcscpy_s(m_url, wcslen(url) + 1, url);
    }

    // zero out internal structures
    ZeroMemory(&m_aviInfo, sizeof(m_aviInfo));
    ZeroMemory(&m_videoStreamInfo, sizeof(m_videoStreamInfo));
    ZeroMemory(&m_audioStreamInfo, sizeof(m_audioStreamInfo));
    ZeroMemory(&m_videoFormat, sizeof(m_videoFormat));
    ZeroMemory(&m_audioFormat, sizeof(m_audioFormat));

    // initialize the AVI file library
    AVIFileInit();
}

//
// Initialize the parser and open the AVI file
//
HRESULT AVIFileParser::Init()
{
    HRESULT hr = S_OK;

    if (m_url == NULL || wcslen(m_url) == 0)
    {
        return E_FAIL;
    }

    // use the AVIFileOpen function to create an IAVIFile object
    hr = AVIFileOpen(&m_pAviFile, m_url, OF_READ, NULL);

    return hr;
}

//
// Parse the AVI file header
//
HRESULT AVIFileParser::ParseHeader()
{
    HRESULT hr = S_OK;

    do
    {
        // open the AVI file and read the header information
        AVIFileInfo(m_pAviFile, &m_aviInfo, sizeof(m_aviInfo));
        
        // open the audio & video streams and build up an index of AVI file chunks
        AVIFileGetStream(m_pAviFile, &m_pVideoStream, streamtypeVIDEO, 0);
        AVIFileGetStream(m_pAviFile, &m_pAudioStream, streamtypeAUDIO, 0);

        // parse the video stream information and construct the video media type
        hr = ParseVideoStreamHeader();
        BREAK_ON_FAIL(hr);

        // parse audio stream information and construct the audio media type
        hr = ParseAudioStreamHeader();
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Parse the video stream header and construct the video media type
//
HRESULT AVIFileParser::ParseVideoStreamHeader(void)
{
    HRESULT hr = S_OK;
    LONG tempFormatSize = 0;
    BYTE* tempFormatBuffer = NULL;
    LONG cbUserData = 0;
    BYTE* pUserData = NULL;

    do
    {
        // just return success if there is no video stream - this may happen if there is only
        // audio in the AVI file
        BREAK_ON_NULL(m_pVideoStream, S_OK);

        // get an information structure describing the stream
        hr = m_pVideoStream->Info(&m_videoStreamInfo, sizeof(m_videoStreamInfo));
        BREAK_ON_FAIL(hr);

        // get first video sample number
        m_currentVideoSample = m_videoStreamInfo.dwStart;

        // get the size of the format structure for the stream - pass in NULL as the buffer            
        hr = m_pVideoStream->ReadFormat(0, NULL, &tempFormatSize);
        BREAK_ON_FAIL(hr);

        // allocate some space for the format block
        tempFormatBuffer = new (std::nothrow) BYTE[tempFormatSize];

        // read the data into the allocated byte array
        hr = m_pVideoStream->ReadFormat(0, tempFormatBuffer, &tempFormatSize);
        BREAK_ON_FAIL(hr);

        // copy information from the temp format buffer into the BITMAPINFOHEADER structure
        // for later use
        memcpy_s(&m_videoFormat, sizeof(m_videoFormat), tempFormatBuffer, 
            min(sizeof(m_videoFormat), tempFormatSize));

        // figure out how much user data we have
        if(tempFormatSize > sizeof(m_videoFormat))
        {
            cbUserData = tempFormatSize - sizeof(m_videoFormat);
            pUserData = tempFormatBuffer + cbUserData;
        }

        // use a helper function to create the actual video media type
        hr = CreateVideoMediaType(pUserData, cbUserData);
        BREAK_ON_FAIL(hr);

        // calculate the file duration by looking at the number of video samples, and 
        // the duration of each sample.  The frames per second rate is calculated by
        // dividing dwRate by dwScale.  Then we take the number of samples from dwLength,
        // and divide that by the number of frames per second, to get the total number of
        // seconds.
        double nSeconds = (double)m_videoStreamInfo.dwLength / ((double)m_videoStreamInfo.dwRate / m_videoStreamInfo.dwScale);

        // The duration is stored in 100 nanosecond units - therefore multiply the number
        // of seconds by 10^7.
        m_duration = (LONGLONG)( nSeconds * 10000000);


        // Find the location of the first keyframe
        m_nextKeyframe = m_pVideoStream->FindSample(0, FIND_KEY);
    }
    while(false);

    // delete the temporary buffer if it has been allocated
    if(tempFormatBuffer != NULL)
    {
        delete tempFormatBuffer;
    }

    return hr;
}


//
// Parse the audio stream header and construct the audio media type
//
HRESULT AVIFileParser::ParseAudioStreamHeader(void)
{
    HRESULT hr = S_OK;
    LONG tempFormatSize = 0;
    BYTE* tempFormatBuffer = NULL;
    LONG cbUserData = 0;
    BYTE* pUserData = NULL;

    do
    {
        // just return success if there is no audio stream - this may happen if there is only
        // video in the AVI file
        BREAK_ON_NULL(m_pAudioStream, S_OK);

        // get an information structure describing the stream
        hr = m_pAudioStream->Info(&m_audioStreamInfo, sizeof(m_audioStreamInfo));
        BREAK_ON_FAIL(hr);

        // get first audio sample number
        m_currentAudioSample = m_audioStreamInfo.dwStart;

        // first find out how large a buffer we need to allocate for the audio 
        // format structure - pass in NULL as the buffer
        hr = m_pAudioStream->ReadFormat(0, NULL, &tempFormatSize);
        if(hr != AVIERR_BUFFERTOOSMALL)
        {
            BREAK_ON_FAIL(hr);
        }

        // allocate the audio format structure
        tempFormatBuffer = new (std::nothrow) BYTE[tempFormatSize];

        // get the actual format buffer
        hr = m_pAudioStream->ReadFormat(0, tempFormatBuffer, &tempFormatSize);
        BREAK_ON_FAIL(hr);

        // copy information from the temp format buffer into the WAVEFORMATEX object for later
        memcpy_s(&m_audioFormat, sizeof(m_audioFormat), tempFormatBuffer, 
            min(sizeof(m_audioFormat), tempFormatSize));

        if(tempFormatSize > sizeof(m_audioFormat))
        {
            cbUserData = tempFormatSize - sizeof(m_audioFormat);
            pUserData = tempFormatBuffer + cbUserData;
        }            

        // construct the actual media type out of the cached m_audioFormat structure, as well
        // as any additional data that may be present in the audio info structure
        //hr = CreateAudioMediaType(pUserData, cbUserData);
        hr = CreateAudioMediaType(tempFormatBuffer, tempFormatSize);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    // delete the temporary buffer if it has been allocated
    if(tempFormatBuffer != NULL)
    {
        delete tempFormatBuffer;
    }

    return hr;
}



//
// Get a copy of the video media type
//
HRESULT AVIFileParser::GetVideoMediaType(IMFMediaType** ppMediaType)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(ppMediaType, E_POINTER);
        BREAK_ON_NULL(m_pVideoType, E_UNEXPECTED);

        hr = m_pVideoType.CopyTo(ppMediaType);
    }
    while(false);

    return hr;
}


//
// Get a copy of the audio media type
//
HRESULT AVIFileParser::GetAudioMediaType(IMFMediaType** ppMediaType)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(ppMediaType, E_POINTER);
        BREAK_ON_NULL(m_pVideoType, E_UNEXPECTED);
    
        hr = m_pAudioType.CopyTo(ppMediaType);
    }
    while(false);

    return hr;
}


//
//  Get the next video sample from the underlying AVI file
//
HRESULT AVIFileParser::GetNextVideoSample(IMFSample** ppSample)
{
    HRESULT hr = S_OK;

    long bufferSize = 0;
    BYTE* pBuffer = NULL;
    LONGLONG sampleTime = 0;
    CComPtr<IMFMediaBuffer> pMediaBuffer;
    CComPtr<IMFSample> pSample;

    do
    {
        BREAK_ON_NULL (ppSample, E_POINTER);
        BREAK_ON_NULL (m_pVideoStream, E_UNEXPECTED);

        // figure out the required length of the stream by passing in NULL as the buffer
        hr = m_pVideoStream->Read(m_currentVideoSample, 1, NULL, 0, &bufferSize, NULL);
        BREAK_ON_FAIL(hr);

        // create an IMFMediaBuffer object with the required size
        hr = MFCreateMemoryBuffer(bufferSize, &pMediaBuffer);
        BREAK_ON_FAIL(hr);

        // lock the IMFMediaBuffer object to get a pointer to its underlyng buffer
        hr = pMediaBuffer->Lock(&pBuffer, NULL, NULL);
        BREAK_ON_FAIL(hr);

        // read the data from the stream into the buffer
        hr = m_pVideoStream->Read(
                m_currentVideoSample,           // number of the sample to read
                1,                              // the number of samples requested
                pBuffer,                        // pointer to the buffer for the sample
                bufferSize,                     // size of the sample buffer
                &bufferSize,                    // pointer to the number of bytes actually read
                NULL);                          // pointer to the number of samples read
        BREAK_ON_FAIL(hr);

        // unlock the IMFMediaBuffer
        hr = pMediaBuffer->Unlock();
        BREAK_ON_FAIL(hr);

        // store the number of bytes read in the IMFMediaBuffer object
        hr = pMediaBuffer->SetCurrentLength(bufferSize);
        BREAK_ON_FAIL(hr);

        // create the actual IMFSample object
        hr = MFCreateSample(&pSample);
        BREAK_ON_FAIL(hr);

        // store the buffer in the sample
        hr = pSample->AddBuffer(pMediaBuffer);
        BREAK_ON_FAIL(hr);

        // calculate and set the time when the sample is displayed relative to the beginning of
        // the stream (time 0) - cast to LONGLONG vefore multiplication to avoid overflow
        sampleTime = ((LONGLONG)AVIStreamSampleToTime(m_pVideoStream, m_currentVideoSample)) * 10000;
        hr = pSample->SetSampleTime(sampleTime);
        BREAK_ON_FAIL(hr);
        
        // If this frame is a keyframe that we previously found, put a flag in the sample to indicate that.
        if(m_currentVideoSample == m_nextKeyframe)
        {
            hr = pSample->SetUINT32(MFSampleExtension_CleanPoint, 1);
            BREAK_ON_FAIL(hr);

            // since we found the current keyframe, find the index of the next one in the file
            m_nextKeyframe = m_pVideoStream->FindSample(m_currentVideoSample+1, FIND_KEY | FIND_NEXT);
        }

        // detach the sample from the object and store it in the passed-in pointer
        *ppSample = pSample.Detach();

        // get the index of the next video sample in the stream
        m_currentVideoSample = AVIStreamNextSample(m_pVideoStream, m_currentVideoSample);
    }
    while(false);

    return hr;
}


//
// Read the next audio sample from the AVI file
//
HRESULT AVIFileParser::GetNextAudioSample(IMFSample** ppSample)
{
    HRESULT hr = S_OK;

    long numOfSamples = 0;
    long bufferSize = 0;
    BYTE* pBuffer = NULL;
    LONGLONG duration = 0;
    CComPtr<IMFMediaBuffer> pMediaBuffer;
    CComPtr<IMFSample> pSample;

    do
    {
        BREAK_ON_NULL (ppSample, E_POINTER);
        BREAK_ON_NULL (m_pAudioStream, E_UNEXPECTED);

        // One second buffer size aligned to format block - use enough buffer for 1 second
        // of audio, and add some padding for the block alighnment
        bufferSize = (m_audioFormat.nAvgBytesPerSec / 10) + m_audioFormat.nBlockAlign - 1;
        
        // adjust buffer size to fit an integer (whole) number of samples - no fractions
        bufferSize -= bufferSize % m_audioFormat.nBlockAlign;

        // Get the duration of the audio sample in 100 nanosecond units - use the average
        // bytes per second and the buffer size to figure out how much audio data will be
        // approximately in the buffer.
        duration = ((LONGLONG)bufferSize * 10000000) / m_audioFormat.nAvgBytesPerSec;

        // figure out the number of audio samples that will fit into each buffer
        numOfSamples = bufferSize / m_audioFormat.nBlockAlign;

        // create the IMFMediaBuffer object for the sample
        hr = MFCreateMemoryBuffer(bufferSize, &pMediaBuffer);
        BREAK_ON_FAIL(hr);

        // lock the IMFMediaBuffer object to get the pointer to its internal buffer
        hr = pMediaBuffer->Lock(&pBuffer, NULL, NULL);
        BREAK_ON_FAIL(hr);

        // Read the audio sample from the AVI file.  Note that since the default AVI file header parser
        // on Windows is rather fragile, this function may return AVIERR_FILEREAD, stalling the reader.
        // This error is not recoverable, and to fix it you would need to implement our own AVI file
        // parser.
        hr = m_pAudioStream->Read(
                m_currentAudioSample,           // number of the sample to read
                numOfSamples,                   // the number of samples requested
                pBuffer,                        // pointer to the buffer for the sample
                bufferSize,                     // size of the sample buffer
                &bufferSize,                    // pointer to the number of bytes actually read
                NULL);                          // pointer to the number of samples read
        
        // do not fail here in case we get AVIERR_FILEREAD error - this way the video will still flow
        // without audio
        //BREAK_ON_FAIL(hr);

        // unlock the IMFMediaBuffer object
        hr = pMediaBuffer->Unlock();
        BREAK_ON_FAIL(hr);

        // set the length of data in the buffer
        hr = pMediaBuffer->SetCurrentLength(bufferSize);
        BREAK_ON_FAIL(hr);

        // create the actual sample
        hr = MFCreateSample(&pSample);
        BREAK_ON_FAIL(hr);

        // add the IMFMediaBuffer that we just filled with data to the sample
        hr = pSample->AddBuffer(pMediaBuffer);
        BREAK_ON_FAIL(hr);

        // set the time when the sample should be rendered
        hr = pSample->SetSampleTime(m_audioSampleTime);
        BREAK_ON_FAIL(hr);

        // set the duration of the sample (how long it should be "displayed")
        hr = pSample->SetSampleDuration(duration);
        BREAK_ON_FAIL(hr);

        // detach the sample so that we can return it
        *ppSample = pSample.Detach();

        // Figure out the next audio sample to display
        m_currentAudioSample += numOfSamples;
        
        // figure out when the next sample should play
        m_audioSampleTime += duration;
    }
    while(false);

    return hr;
}


//
// Create the video media type
//
HRESULT AVIFileParser::CreateVideoMediaType(BYTE* pUserData, DWORD dwUserData)
{
    HRESULT hr = S_OK;
    CComPtr<IMFVideoMediaType> pType;

    do
    {
        DWORD original4CC = m_videoFormat.biCompression;

        // use a special case to handle custom 4CC types.  For example variations of the DivX
        // decoder - DIV3 and DIVX - can be handled by MS decoders MP43 and MP4V.  Therefore
        // modify the 4CC value to match the decoders that will handle the data
        if(original4CC == 0x33564944)       // special case - "DIV3" handled by "MP43" decoder
        {
            m_videoFormat.biCompression = '34PM';
        }
        else if(original4CC == 0x44495658)  // special case - "DIVX" handled by "MP4V" decoder
        {
            m_videoFormat.biCompression = 'V4PM';
        }
        
        // construct the media type from the 
        hr = MFCreateVideoMediaTypeFromBitMapInfoHeaderEx(
                    &m_videoFormat,                     // video info header to convert
                    m_videoFormat.biSize,               // size of the header structure
                    1,                                  // pixel aspect ratio X
                    1,                                  // pixel aspect ratio Y
                    MFVideoInterlace_Progressive,       // interlace mode 
                    0,                                  // video flags
                    m_videoStreamInfo.dwRate,           // FPS numerator
                    m_videoStreamInfo.dwScale,          // FPS denominator
                    m_videoStreamInfo.dwScale,          // max bitrate
                    &pType);                           // result - out
        BREAK_ON_FAIL(hr);

        // store the original 4CC value
        hr = pType->SetUINT32(MF_MT_ORIGINAL_4CC, original4CC);
        BREAK_ON_FAIL(hr);

        // if there is any extra video information data, store it in the media type
        if(pUserData != NULL && dwUserData > 0)
        {
            hr = pType->SetBlob(MF_MT_USER_DATA, pUserData, dwUserData);
            BREAK_ON_FAIL(hr);
        }

        m_pVideoType = pType;
    }
    while(false);

    return hr;
}

HRESULT AVIFileParser::SetOffset(const PROPVARIANT& varStart)
{
    HRESULT hr = S_OK;

    // VT_EMPTY mean current position, so mke sure that it is a seek.
    if (varStart.vt == VT_I8)
    {
        m_currentAudioSample = (DWORD)((m_audioStreamInfo.dwLength * varStart.hVal.QuadPart) / m_duration);
        m_audioSampleTime = varStart.hVal.QuadPart;
    }

    return hr;
}


//
// Create the audio media type
//
HRESULT AVIFileParser::CreateAudioMediaType(BYTE* pData, DWORD dwDataSize)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaType> spType;
    WAVEFORMATEX formatEx;
    WAVEFORMATEX* pFormatEx = (WAVEFORMATEX*)pData;
    DWORD dwStructureLength = dwDataSize;

    do
    {
        // create the media type object
        hr = MFCreateMediaType(&spType);
        BREAK_ON_FAIL(hr);

        // if this is not a real WAVEFORMATEX structure but a smaller object (WAVEFORMAT?),
        // then copy it into a new WAVEFORMATEX structure, and fake it in order to make the
        //  MFInitMediaTypeFromWaveFormatEx function happy
        if(dwDataSize < sizeof(WAVEFORMATEX))
        {
            ZeroMemory(&formatEx, sizeof(WAVEFORMATEX));
            memcpy_s(&formatEx, sizeof(WAVEFORMATEX), pData, dwDataSize);

            pFormatEx = &formatEx;
            dwStructureLength = sizeof(WAVEFORMATEX);
        }

        // use a library function to initialize an audio media type from a WAVEFORMATEX structure
        hr = MFInitMediaTypeFromWaveFormatEx(spType, pFormatEx, dwStructureLength);
        BREAK_ON_FAIL(hr);

        // if we got here, then everything succeeded, set the internal pointer to the media type
        m_pAudioType = spType; 
    }
    while(false);

    return hr;
}


//
// Create the audio media type
//
/*HRESULT AVIFileParser::CreateAudioMediaTypeFromWaveFormat(IMFMediaType* pType, BYTE* pWaveFormat, DWORD dwLength)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaType> spType;

    do
    {
        // create the media type object
        hr = MFCreateMediaType(&spType);
        BREAK_ON_FAIL(hr);
    
        // set the major type on the media type
        hr = spType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        BREAK_ON_FAIL(hr);

        // decode the audio format id and store it as a subtype GUID - this function handles 
        // only MP3 and PCM audio types.
        if(m_audioFormat.wFormatTag == WAVE_FORMAT_MPEGLAYER3)
        {
            // MP3 subtype
            hr = spType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_MP3);
            BREAK_ON_FAIL(hr);
        }
        else
        {
            // uncompressed PCM subtype
            hr = spType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
            BREAK_ON_FAIL(hr);

            // since this is an uncompressed audio format, samples are not dependent on each other
            hr = spType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
            BREAK_ON_FAIL(hr);
        }

        // set the number of audio channels
        hr = spType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, m_audioFormat.nChannels);
        BREAK_ON_FAIL(hr);

        // set the number of audio samples per second
        hr = spType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, m_audioFormat.nSamplesPerSec);
        BREAK_ON_FAIL(hr);

        // set block alignment
        hr = spType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, m_audioFormat.nBlockAlign);
        BREAK_ON_FAIL(hr);

        // set average bytes per second
        hr = spType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, m_audioFormat.nAvgBytesPerSec);
        BREAK_ON_FAIL(hr);

        // set number of bits in each sample
        hr = spType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, m_audioFormat.wBitsPerSample);
        BREAK_ON_FAIL(hr);

        // If there is additional data in the structure, then add it to the media type.
        if(pUserData != NULL && dwUserData > 0)
        {
            hr = spType->SetBlob(MF_MT_USER_DATA, pUserData, dwUserData);
            BREAK_ON_FAIL(hr);
        }

        // if we got here, then everything succeeded, set the internal pointer to the media type
        m_pAudioType = spType;
    }
    while(false);

    return hr;
}     */




AVIFileParser::~AVIFileParser(void)
{
    if (m_pVideoStream != NULL)
    {
        AVIStreamRelease(m_pVideoStream);
    }

    if (m_pAudioStream != NULL)
    {    
        AVIStreamRelease(m_pAudioStream);
    }

    if (m_pAviFile != NULL)
    {
        AVIFileRelease(m_pAviFile);
    }

    if(m_url != NULL)
    {
        delete m_url;
    }

    AVIFileExit();
}



//
// Create and return a new property store describing the file
//
HRESULT AVIFileParser::GetPropertyStore(IPropertyStore** ppPropertyStore)
{
    HRESULT hr = S_OK;
    CComPtr<IPropertyStore> pPropStore;
    PROPVARIANT propValue;

    do
    {
        BREAK_ON_NULL(ppPropertyStore, E_POINTER);
        BREAK_ON_NULL(m_pAviFile, E_UNEXPECTED);

        // create a new property store
        hr = PSCreateMemoryPropertyStore(IID_IPropertyStore, (void**)&pPropStore);
        BREAK_ON_FAIL(hr);

        // set the duration property
        InitPropVariantFromInt64(m_duration, &propValue);
        hr = pPropStore->SetValue(PKEY_Media_Duration, propValue);
        BREAK_ON_FAIL(hr);

        // if we got here, everything succeeded - detach and return the property store
        *ppPropertyStore = pPropStore.Detach();
    }
    while(false);

    return hr;
}