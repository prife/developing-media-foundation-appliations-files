#pragma once

#include <Vfw.h>
#include <Mfobjects.h>
#include <mfidl.h>
#include <Mferror.h>
#include <mfapi.h>

#include <hash_map>
using namespace std;

#ifndef MF_MT_BITCOUNT
DEFINE_GUID(MF_MT_BITCOUNT, 0xc496f370, 0x2f8b, 0x4f51, 0xae, 0x46, 0x9c, 0xfc, 0x1b, 0xc8, 0x2a, 0x47);
#endif

class CAviFileWriter
{
    public:
        CAviFileWriter(const WCHAR* pFilename);
        ~CAviFileWriter(void);

        HRESULT AddStream(IMFMediaType* pMediaType, DWORD id);
        HRESULT WriteSample(BYTE* pData, DWORD dataLength, DWORD streamId, bool isKeyframe = false);

    private:
        struct AviStreamData
        {
            IAVIStream* pStream;
            ULONG nNextSample;
            bool isAudio;
        };

        IAVIFile* m_pAviFile;        
        hash_map<DWORD, AviStreamData*> m_streamHash;
        WAVEFORMATEX*  m_pAudioFormat;

        HRESULT AddAudioStream(IMFMediaType* pMT, IAVIStream** pStream);
        HRESULT AddVideoStream(IMFMediaType* pMT, IAVIStream** pStream);
};

