#pragma once

#include "common.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <Ks.h>
#include <Codecapi.h>

#include <Mfreadwrite.h>

class CReaderWriterTranscoder
{
    public:
        CReaderWriterTranscoder(void);
        ~CReaderWriterTranscoder(void);

        // Start the transcode
        HRESULT Transcode(LPCWSTR source, LPCWSTR sink);

    private:
        CComPtr<IMFSourceReader> m_pSourceReader;
        CComPtr<IMFSinkWriter> m_pSinkWriter;
        int* m_streamMap;
        int m_nStreams;

        // map individual streams
        HRESULT MapStreams(void);

        // figure out the target media types
        HRESULT GetTranscodeMediaType(CComPtr<IMFMediaType>& pStreamMediaType);
        HRESULT GetTranscodeVideoType(CComPtr<IMFMediaType>& pStreamMediaType);
        HRESULT GetTranscodeAudioType(CComPtr<IMFMediaType>& pStreamMediaType);

        // connect the streams
        HRESULT ConnectStream(DWORD dwStreamIndex, const GUID& streamMajorType);

        // do the actual transcode.
        HRESULT RunTranscode(void);
};

