#pragma once

#include <atlbase.h>
#include <Vfw.h>
#include <Shlwapi.h>

#include <mfapi.h>
#include <mfobjects.h>
#include <mferror.h>
#include <propkey.h>
#include <propvarutil.h>


// Parse the AVI file with the VFW interfaces.
class AVIFileParser
{
    public:
        static HRESULT CreateInstance(const WCHAR* url, AVIFileParser **ppParser);

        HRESULT ParseHeader(void);
        HRESULT GetVideoMediaType(IMFMediaType** ppMediaType);
        HRESULT GetAudioMediaType(IMFMediaType** ppMediaType);
        HRESULT GetNextVideoSample(IMFSample** ppSample);
        HRESULT GetNextAudioSample(IMFSample** ppSample);
        HRESULT SetOffset(const PROPVARIANT& varStart);

        DWORD StreamCount(void) const           { return m_aviInfo.dwStreams; };
        bool IsSupportedFormat(void) const      { return true; };
        bool HasVideo(void) const               { return (m_pVideoStream != NULL); };
        bool HasAudio(void) const               { return (m_pAudioStream != NULL); };
        bool IsVideoEndOfStream(void) const     
        { return (m_currentVideoSample >= m_videoStreamInfo.dwLength); };
        bool IsAudioEndOfStream(void) const     
        { return (m_currentAudioSample >= m_audioStreamInfo.dwLength); };
        LONGLONG Duration(void) const           { return m_duration; };

        ~AVIFileParser(void);

        HRESULT GetPropertyStore(IPropertyStore** ppPropertyStore);

    protected:
        AVIFileParser(const WCHAR* url);
        HRESULT Init();
        HRESULT CreateVideoMediaType(BYTE* pUserData, DWORD dwUserData);
        HRESULT CreateAudioMediaType(BYTE* pUserData, DWORD dwUserData);
        HRESULT ParseVideoStreamHeader(void);
        HRESULT ParseAudioStreamHeader(void);

    private:
        WCHAR* m_url;
        AVIFILEINFO m_aviInfo;
        AVISTREAMINFO m_videoStreamInfo;
        AVISTREAMINFO m_audioStreamInfo;

        BITMAPINFOHEADER m_videoFormat;
        WAVEFORMATEX m_audioFormat;

        IAVIFile* m_pAviFile;
        IAVIStream* m_pVideoStream;
        IAVIStream* m_pAudioStream;

        DWORD m_currentVideoSample;
        DWORD m_currentAudioSample;
        LONGLONG m_audioSampleTime;
        LONGLONG m_duration;

        LONG m_nextKeyframe;

        CComPtr<IMFMediaType> m_pVideoType;
        CComPtr<IMFMediaType> m_pAudioType;
};