#include "ReaderWriterTranscoder.h"



// All video decoders and encoders support at least one of these video formats - basically
// decoded frames (in some cases variants of bitmaps)
static GUID intermediateVideoFormats[] =
{
    MFVideoFormat_NV12,
    MFVideoFormat_YV12,
    MFVideoFormat_YUY2,
    MFVideoFormat_RGB32
};
int nIntermediateVideoFormats = 4;


// audio stream formats that every audio decoder and encoder should
// be able to agree on - uncompressed audio data
static GUID intermediateAudioFormats[] =
{
    MFAudioFormat_Float,
    MFAudioFormat_PCM,
};
int nIntermediateAudioFormats = 2;



CReaderWriterTranscoder::CReaderWriterTranscoder(void)
{
    // Start up Media Foundation platform.
    MFStartup(MF_VERSION);
}


CReaderWriterTranscoder::~CReaderWriterTranscoder(void)
{
    // release any COM objects before calling MFShutdown() - otherwise MFShutdown() will cause them
    // to go away, and the CComPtr destructor will AV
    m_pSourceReader = NULL;
    m_pSinkWriter = NULL;

    // Shutdown the Media Foundation platform
    MFShutdown();
}



//
// Main transcoding function that triggers the transcode process:
// 1.  Create stream reader and sink writer objects.
// 2.  Map the streams found in the source file to the sink.
// 3.  Run the transcode operation.
//
HRESULT CReaderWriterTranscoder::Transcode(LPCWSTR source, LPCWSTR target)
{
    HRESULT hr = S_OK;
    CComPtr<IMFAttributes> pConfigAttrs;

    do
    {
        // you want to set only a single attribute - you want to make sure that both the
        // source reader and the sink writer load any hardware transforms that they can,
        // since that will greatly speed up the transcoding process.  Therefore create
        // an attribute store and add the MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS 
        // property to it.
        
        // create an attribute store
        hr = MFCreateAttributes(&pConfigAttrs, 1);
        BREAK_ON_FAIL(hr);

        // set MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS property in the store
        hr = pConfigAttrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        BREAK_ON_FAIL(hr);

        // create a source reader
        hr = MFCreateSourceReaderFromURL(source, pConfigAttrs, &m_pSourceReader);
        BREAK_ON_FAIL(hr);

        // create a sink writer
        hr = MFCreateSinkWriterFromURL(target, NULL, pConfigAttrs, &m_pSinkWriter);
        BREAK_ON_FAIL(hr);
        
        // map the streams found in the source file from the source reader to the
        // sink writer, while negotiating media types
        hr = MapStreams();
        BREAK_ON_FAIL(hr);

        // run the transcode loop
        hr = RunTranscode();
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}




//
// Map each source reader output stream to an input stream of the sink writer, deciding
// on the target format.  Audio and video stream formats are modified to AAC and H.264 
// respectively, but other target formats remain unchanged.
//
HRESULT CReaderWriterTranscoder::MapStreams(void)
{
    HRESULT hr = S_OK;
    BOOL isStreamSelected = FALSE;
    DWORD sourceStreamIndex = 0;
    DWORD sinkStreamIndex = 0;
    GUID streamMajorType;
    CComPtr<IMFMediaType> pStreamMediaType;
   

    do
    {
        m_nStreams = 0;
        
        while(SUCCEEDED(hr))
        {
            // check whether you have a stream with the right index - if you don't, the 
            // IMFSourceReader::GetStreamSelection() function will fail, and you will drop
            // out of the while loop
            hr = m_pSourceReader->GetStreamSelection(sourceStreamIndex, &isStreamSelected);
            if(FAILED(hr))
            {
                hr = S_OK;
                break;
            }

            // count the total number of streams for later
            m_nStreams++;

            // get the source media type of the stream
            hr = m_pSourceReader->GetNativeMediaType(
                sourceStreamIndex,           // index of the stream you are interested in
                0,                           // index of the media type exposed by the 
                                             //    stream decoder
                &pStreamMediaType);          // media type
            BREAK_ON_FAIL(hr);

            // extract the major type of the source stream from the media type
            hr = pStreamMediaType->GetMajorType(&streamMajorType);
            BREAK_ON_FAIL(hr);

            // select a stream, indicating that the source should send out its data instead
            // of dropping all of the samples
            hr = m_pSourceReader->SetStreamSelection(sourceStreamIndex, TRUE);
            BREAK_ON_FAIL(hr);

            // if this is a video or audio stream, transcode it and negotiate the media type
            // between the source reader stream and the corresponding sink writer stream.  
            // If this is a some other stream format (e.g. subtitles), just pass the media 
            // type unchanged.
            if(streamMajorType == MFMediaType_Audio || streamMajorType == MFMediaType_Video)
            {
                // get the target media type - the media type into which you will transcode
                // the data of the current source stream
                hr = GetTranscodeMediaType(pStreamMediaType);
                BREAK_ON_FAIL(hr);

                // add the stream to the sink writer - i.e. tell the sink writer that a 
                // stream with the specified index will have the target media type
                hr = m_pSinkWriter->AddStream(pStreamMediaType, &sinkStreamIndex);
                BREAK_ON_FAIL(hr);

                // hook up the source and sink streams - i.e. get them to agree on an
                // intermediate media type that will be used to pass data between source 
                // and sink
                hr = ConnectStream(sourceStreamIndex, streamMajorType);
                BREAK_ON_FAIL(hr);
            }
            else
            {
                // add the stream to the sink writer with the exact same media type as the
                // source stream
                hr = m_pSinkWriter->AddStream(pStreamMediaType, &sinkStreamIndex);
                BREAK_ON_FAIL(hr);
            }

            // make sure that the source stream index is equal to the sink stream index
            if(sourceStreamIndex != sinkStreamIndex)
            {
                hr = E_UNEXPECTED;
                break;
            }

            // increment the source stream index, so that on the next loop you are analyzing
            // the next stream
            sourceStreamIndex++;

            // release the media type
            pStreamMediaType = NULL;
        }
    
        BREAK_ON_FAIL(hr);

    }
    while(false);
    
    return hr;
}






//
// Set the target target audio and video media types to hard-coded values.  In this case you
// are setting audio to AAC, and video to 720p H.264
//
HRESULT CReaderWriterTranscoder::GetTranscodeMediaType(
    CComPtr<IMFMediaType>& pStreamMediaType)
{
    HRESULT hr = S_OK;
    GUID streamMajorType;

    do
    {
        // extract the major type of the source stream from the media type
        hr = pStreamMediaType->GetMajorType(&streamMajorType);
        BREAK_ON_FAIL(hr);

        // if this is an audio stream, configure a hard-coded AAC profile.  If this is a
        // video stream, configure an H.264 profile
        if(streamMajorType == MFMediaType_Audio)
        {
            hr = GetTranscodeAudioType(pStreamMediaType);
        }
        else if(streamMajorType == MFMediaType_Video)
        {
            hr = GetTranscodeVideoType(pStreamMediaType);
        }
    }
    while(false);

    return hr;
}



//
// Get the target audio media type - use the AAC media format.
//
HRESULT CReaderWriterTranscoder::GetTranscodeAudioType(
    CComPtr<IMFMediaType>& pStreamMediaType)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(pStreamMediaType, E_POINTER);

        // wipe out existing data from the media type
        hr = pStreamMediaType->DeleteAllItems();
        BREAK_ON_FAIL(hr);

        // reset the major type to audio since we just wiped everything out
        pStreamMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        BREAK_ON_FAIL(hr);

        // set the audio subtype
        hr = pStreamMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
        BREAK_ON_FAIL(hr);

        // set the number of audio bits per sample
        hr = pStreamMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        BREAK_ON_FAIL(hr);

        // set the number of audio samples per second
        hr = pStreamMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
        BREAK_ON_FAIL(hr);

        // set the number of audio channels
        hr = pStreamMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
        BREAK_ON_FAIL(hr);

        // set the Bps of the audio stream
        hr = pStreamMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000);
        BREAK_ON_FAIL(hr);

        // set the block alignment of the samples
        hr = pStreamMediaType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}



//
// Get the target video media type - use the H.264 media format.
//
HRESULT CReaderWriterTranscoder::GetTranscodeVideoType(
    CComPtr<IMFMediaType>& pStreamMediaType)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(pStreamMediaType, E_POINTER);

        // wipe out existing data from the media type
        hr = pStreamMediaType->DeleteAllItems();
        BREAK_ON_FAIL(hr);

        // reset the major type to video since we just wiped everything out
        pStreamMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        BREAK_ON_FAIL(hr);

        // set the video subtype
        hr = pStreamMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        BREAK_ON_FAIL(hr);

        // set the frame size to 720p as a 64-bit packed value
        hr = MFSetAttributeSize(
            pStreamMediaType,           // attribute store on which to set the value
            MF_MT_FRAME_SIZE,           // value ID GUID
            1280, 720);                 // frame width and height
        BREAK_ON_FAIL(hr);

        // Set the frame rate to 30/1.001 - the standard frame rate of NTSC television - as 
        // a 64-bit packed value consisting of a fraction of two integers
        hr = MFSetAttributeRatio(
            pStreamMediaType,           // attribute store on which to set the value
            MF_MT_FRAME_RATE,           // value
            30000, 1001);               // frame rate ratio
        BREAK_ON_FAIL(hr);

        // set the average bitrate of the video in bits per second - in this case 10 Mbps
        hr = pStreamMediaType->SetUINT32(MF_MT_AVG_BITRATE, 10000000 );
        BREAK_ON_FAIL(hr);

        // set the interlace mode to progressive
        hr = pStreamMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 
            MFVideoInterlace_Progressive );
        BREAK_ON_FAIL(hr);

        // set the pixel aspect ratio to 1x1 - square pixels
        hr = MFSetAttributeSize(pStreamMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}



//
// Attempt to find an uncompressed media type for the specified stream that both the source 
// and sink can agree on
//
HRESULT CReaderWriterTranscoder::ConnectStream(DWORD dwStreamIndex, 
    const GUID& streamMajorType)
{
    HRESULT hr = S_OK;

    CComPtr<IMFMediaType> pPartialMediaType;
    CComPtr<IMFMediaType> pFullMediaType;

    BOOL fConfigured = FALSE;
    GUID* intermediateFormats = NULL;
    int nFormats = 0;

    do
    {
        // create a media type container object that will be used to match stream input
        // and output media types
        hr = MFCreateMediaType( &pPartialMediaType );
        BREAK_ON_FAIL(hr);

        // set the major type of the partial match media type container
        hr = pPartialMediaType->SetGUID( MF_MT_MAJOR_TYPE, streamMajorType );
        BREAK_ON_FAIL(hr);

        // Get the appropriate list of intermediate formats - formats that every decoder and
        // encoder of that type should agree on.  Essentially these are the uncompressed 
        // formats that correspond to decoded frames for video, and uncompressed audio 
        // formats
        if(streamMajorType == MFMediaType_Video)
        {
            intermediateFormats = intermediateVideoFormats;
            nFormats = nIntermediateVideoFormats;
        }
        else if(streamMajorType == MFMediaType_Audio)
        {
            intermediateFormats = intermediateAudioFormats;
            nFormats = nIntermediateAudioFormats;
        } 
        else
        {
            hr = E_UNEXPECTED;
            break;
        }

        
        // loop through every intermediate format that you have for this major type, and
        // try to find one on which both the source stream and sink stream can agree on
        for( int x = 0; x < nFormats; x++ )
        {
            // set the format of the partial media type
            hr = pPartialMediaType->SetGUID( MF_MT_SUBTYPE, intermediateFormats[x] );
            BREAK_ON_FAIL(hr);

            // set the partial media type on the source stream
            hr = m_pSourceReader->SetCurrentMediaType( 
                dwStreamIndex,                      // stream index
                NULL,                               // reserved - always NULL
                pPartialMediaType );                // media type to try to set

            // if the source stream (i.e. the decoder) is not happy with this media type -
            // if it cannot decode the data into this media type, restart the loop in order 
            // to try the next format on the list
            if( FAILED(hr) )
            {
                hr = S_OK;
                continue;
            }

            pFullMediaType = NULL;

            // if you got here, the source stream is happy with the partial media type you set
            // - extract the full media type for this stream (with all internal fields 
            // filled in)
            hr = m_pSourceReader->GetCurrentMediaType( dwStreamIndex, &pFullMediaType );

            // Now try to match the full media type to the corresponding sink stream
            hr = m_pSinkWriter->SetInputMediaType( 
                dwStreamIndex,             // stream index
                pFullMediaType,            // media type to match
                NULL );                    // configuration attributes for the encoder

            // if the sink stream cannot accept this media type - i.e. if no encoder was
            // found that would accept this media type - restart the loop and try the next
            // format on the list
            if( FAILED(hr) )
            {
                hr = S_OK;
                continue;
            }

            // you found a media type that both the source and sink could agree on - no need
            // to try any other formats
            fConfigured = TRUE;
            break;
        }
        BREAK_ON_FAIL(hr);

        // if you didn't match any formats return an error code
        if( !fConfigured )
        {
            hr = MF_E_INVALIDMEDIATYPE;
            break;
        }

    }
    while(false);

    return hr;
}



//
// Main transcoding loop.  The loop pulls a sample from the source, pushes
// it into the sink, and repeats until all of the data is sent through.
//
HRESULT CReaderWriterTranscoder::RunTranscode(void)
{
    HRESULT hr = S_OK;

    DWORD streamIndex;
    DWORD flags = 0;
    LONGLONG timestamp = 0;
    int nFinishedStreams = 0;
    CComPtr<IMFSample> pSample;

    do
    {
        // initialize target file and prepare for writing
        hr = m_pSinkWriter->BeginWriting();
        BREAK_ON_FAIL(hr);

        // loop while there is any data in the source streams
        while( nFinishedStreams < m_nStreams )
        {
            // pull a sample out of the source reader
            hr = m_pSourceReader->ReadSample( 
                (DWORD)MF_SOURCE_READER_ANY_STREAM,     // get a sample from any stream
                0,                                      // no source reader controller flags
                &streamIndex,                         // get index of the stream
                &flags,                               // get flags for this sample
                &timestamp,                           // get the timestamp for this sample
                &pSample );                             // get the actual sample
            BREAK_ON_FAIL(hr);

            // The sample can be null if you've reached the end of stream or encountered a
            // data gap (AKA a stream tick).  If you got a sample, send it on.  Otherwise, 
            // if you got a stream gap, send information about it to the sink.
            if( pSample != NULL )
            {
                // push the sample to the sink writer
                hr = m_pSinkWriter->WriteSample( streamIndex, pSample );
                BREAK_ON_FAIL(hr);
            }
            else if( flags & MF_SOURCE_READERF_STREAMTICK )
            {
                // signal a stream tick
                hr = m_pSinkWriter->SendStreamTick( streamIndex, timestamp );
                BREAK_ON_FAIL(hr);
            }

            // if a stream reached the end, notify the sink, and increment the number of 
            // finished streams
            if(flags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                hr = m_pSinkWriter->NotifyEndOfSegment(streamIndex);
                BREAK_ON_FAIL(hr);

                nFinishedStreams++;
            }

            // release sample
            pSample = NULL;
        }

        // check if you have encountered a failure in the loop
        BREAK_ON_FAIL(hr);

        // flush all the samples in the writer, and close the file container
        hr = m_pSinkWriter->Finalize();
        BREAK_ON_FAIL(hr);
    }
    while(false);


    return hr;
}


