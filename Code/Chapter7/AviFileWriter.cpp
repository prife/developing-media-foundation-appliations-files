#include "StdAfx.h"
#include "AviFileWriter.h"


CAviFileWriter::CAviFileWriter(const WCHAR* pFilename)
{
    HRESULT hr = S_OK;
    
    do
    {
        hr = AVIFileOpen(&m_pAviFile, pFilename, OF_CREATE | OF_WRITE, NULL);
        BREAK_ON_FAIL(hr);
    }
    while(false);
}


CAviFileWriter::~CAviFileWriter(void)
{
    if(m_pAviFile != NULL)
    {
        AVIStreamRelease(m_streamHash[1]->pStream);
        AVIStreamRelease(m_streamHash[0]->pStream);

        AVIFileRelease(m_pAviFile);
    }
}




HRESULT CAviFileWriter::AddStream(IMFMediaType* pMT, DWORD id)
{
    HRESULT hr = S_OK;
    AviStreamData* pNewStreamData;
    GUID majorType = GUID_NULL;
    CComPtr<IMFMediaType> pMediaType = pMT;

    do
    {
        hr = pMediaType->GetMajorType(&majorType);
        BREAK_ON_FAIL(hr);

        pNewStreamData = new (std::nothrow) AviStreamData;
        if(pNewStreamData == NULL)
        {
            hr = E_OUTOFMEMORY;
            break;
        }

        if(majorType == MFMediaType_Video)
        {
            hr = AddVideoStream(pMediaType, &(pNewStreamData->pStream));
            pNewStreamData->isAudio = false;
        }
        else if(majorType == MFMediaType_Audio)
        {
            hr = AddAudioStream(pMediaType, &(pNewStreamData->pStream));
            pNewStreamData->isAudio = true;
        }
        else
        {
            hr = MF_E_INVALIDMEDIATYPE;
        }
        BREAK_ON_FAIL(hr);

        pNewStreamData->nNextSample = 0;
        
        EXCEPTION_TO_HR( m_streamHash[id] = pNewStreamData );
    }
    while(false);

    if(FAILED(hr) && pNewStreamData != NULL)
    {
        delete pNewStreamData;
    }

    return hr;
}





HRESULT CAviFileWriter::AddAudioStream(IMFMediaType* pMT, IAVIStream** ppStream)
{
    HRESULT hr = S_OK;
    AVISTREAMINFOW streamInfo;
    GUID majorType = GUID_NULL;
    GUID subtype = GUID_NULL;
    CComPtr<IMFMediaType> pMediaType = pMT;
    UINT32 waveFormatExSize = 0;

    do
    {
        hr = pMediaType->GetMajorType(&majorType);
        BREAK_ON_FAIL(hr);

        hr = pMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
        BREAK_ON_FAIL(hr);

        ZeroMemory(&streamInfo, sizeof(streamInfo));
        

        // set major type
        streamInfo.fccType = streamtypeAUDIO;

        // get the WAVEFORMATEX structure from media type
        hr = MFCreateWaveFormatExFromMFMediaType(pMediaType, &m_pAudioFormat, &waveFormatExSize);
        BREAK_ON_FAIL(hr);
        
        streamInfo.fccHandler = 0;
        streamInfo.dwScale = m_pAudioFormat->nBlockAlign;
        streamInfo.dwRate = m_pAudioFormat->nAvgBytesPerSec;//m_pAudioFormat->nSamplesPerSec * m_pAudioFormat->nBlockAlign;
        streamInfo.dwSampleSize = m_pAudioFormat->nBlockAlign;
        streamInfo.dwQuality = (DWORD)-1;
        streamInfo.dwInitialFrames = 1;

        // create the audio stream
        hr = m_pAviFile->CreateStream(ppStream, &streamInfo);
        BREAK_ON_FAIL(hr);

        // set the format of the stream
        hr = (*ppStream)->SetFormat(0, m_pAudioFormat, waveFormatExSize);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}




HRESULT CAviFileWriter::AddVideoStream(IMFMediaType* pMT, IAVIStream** ppStream)
{
    HRESULT hr = S_OK;
    IAVIStream* pStream = NULL;
    AVISTREAMINFOW streamInfo;
    GUID majorType = GUID_NULL;
    GUID subtype = GUID_NULL;
    CComPtr<IMFMediaType> pMediaType = pMT;

    BITMAPINFOHEADER bmpHeader;
    UINT32 fpsNumerator = 0;
    UINT32 fpsDenominator = 0;        
    UINT32 sampleSize = 0;
    UINT32 frameWidth = 0;
    UINT32 frameHeight = 0;
    UINT32 bitCount = 0;
    UINT32 original4cc = 0;

    do
    {
        hr = pMediaType->GetMajorType(&majorType);
        BREAK_ON_FAIL(hr);

        hr = pMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
        BREAK_ON_FAIL(hr);

        // Get the original 4CC value if there was one - if the value was not stored in the 
        // media type, then just use the virst DWORD of the subtype
        hr = pMediaType->GetUINT32(MF_MT_ORIGINAL_4CC, &original4cc);
        if(FAILED(hr))
        {
            original4cc = subtype.Data1;
        }

        hr = MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &frameWidth, &frameHeight);
        BREAK_ON_FAIL(hr);

        hr = pMediaType->GetUINT32(MF_MT_BITCOUNT, &bitCount);
        BREAK_ON_FAIL(hr);

        // this information could be unavailable
        hr = pMediaType->GetUINT32(MF_MT_SAMPLE_SIZE, &sampleSize);
        if(hr != MF_E_ATTRIBUTENOTFOUND)
        {
            BREAK_ON_FAIL(hr);
        }

        hr = MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, &fpsNumerator, &fpsDenominator);
        BREAK_ON_FAIL(hr);

        ZeroMemory(&streamInfo, sizeof(streamInfo));
        ZeroMemory(&bmpHeader, sizeof(BITMAPINFOHEADER));

        streamInfo.fccType = streamtypeVIDEO;
        streamInfo.fccHandler = original4cc;
        streamInfo.dwScale = fpsDenominator;
        streamInfo.dwRate = fpsNumerator;
        streamInfo.dwSuggestedBufferSize  = sampleSize;
        streamInfo.rcFrame.top = 0;
        streamInfo.rcFrame.left = 0;
        streamInfo.rcFrame.bottom = frameHeight;
        streamInfo.rcFrame.right = frameWidth;

        bmpHeader.biSize = sizeof(bmpHeader);
        bmpHeader.biWidth = frameWidth;
        bmpHeader.biHeight = frameHeight;
        bmpHeader.biPlanes = 1;
        bmpHeader.biCompression = original4cc;
        bmpHeader.biSizeImage = sampleSize;
        bmpHeader.biBitCount = bitCount;

        // create the stream
        hr = m_pAviFile->CreateStream(ppStream, &streamInfo);
        BREAK_ON_FAIL(hr);

        // set the BITMAPINFOHEADER as the format of the stream
        hr = (*ppStream)->SetFormat(0, &bmpHeader, sizeof(bmpHeader));
        BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}



//
// Write a sample for the stream with the specified stream ID
//
HRESULT CAviFileWriter::WriteSample(BYTE* pBuffer, DWORD bufferLength, DWORD streamId, bool isKeyframe)
{
    HRESULT hr = S_OK;
    LONG nSamplesToWrite = 1;
    LONG nSamplesWritten = 0;
    LONG nBytesWritten = 0;
    DWORD dwFlags = 0;

    do
    {
        BREAK_ON_NULL(pBuffer, E_POINTER);

        // check to see if a stream with the specified ID exists
        if(m_streamHash.find(streamId) == m_streamHash.end())
        {
            hr = MF_E_INVALIDSTREAMNUMBER;
            break;
        }

        // extract the data about the stream to which we are trying to write
        AviStreamData* pData = m_streamHash[streamId];

        // if this is an audio data block, it may have more than one audio samples in it - 
        // therefore use the block alignment value to figure out how many actual samples
        // are in the data blob
        if(m_streamHash[streamId]->isAudio)
        {
            nSamplesToWrite = bufferLength / m_pAudioFormat->nBlockAlign;            
        }

        // if we got a flag indicating that this is a keyframe, pass that flag on into the 
        // VFW AVI writer, so that it can be written in the AVI file index
        if(isKeyframe)
        {
            dwFlags = AVIIF_KEYFRAME;
        }

        // write the data to the stream
        hr = pData->pStream->Write(
                pData->nNextSample,         // number of the first sample to write
                nSamplesToWrite,            // number of samples to write
                pBuffer,                    // pointer to the data blob
                bufferLength,               // length of the data blob
                dwFlags,                    // flag indicating whether this is a keyframe
                &nSamplesWritten,           // number of samples written
                &nBytesWritten);            // number of bytes written
        BREAK_ON_FAIL(hr);

        // increment the counter of samples written
        pData->nNextSample += nSamplesWritten;
    }
    while(false);

    return hr;
}










