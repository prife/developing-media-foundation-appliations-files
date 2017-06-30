// TranscodeApp.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <tchar.h>

#include "TranscodeApi.h"
#include "ReaderWriterTranscoder.h"


void ShowUsage(void)
{
    wprintf(L"Usage:  TranscodeApp.exe <api|rw> <Source> <Target>\r\n");
    wprintf(L"\r\n");
    wprintf(L"  api     - use the transcode API to transcode.\r\n");
    wprintf(L"  rw      - use the reader/writer to transcode.\r\n");
    wprintf(L"  Source  - the source video file to transcode.\r\n");
    wprintf(L"  Target  - the target filename for the transcoded file.\r\n");
    wprintf(L"\r\n");
}


int wmain(int argc, WCHAR* argv[])
{
    HRESULT hr = S_OK;

    do
    {
        if(argc < 4)
        {
            ShowUsage();
            break;
        }

        // initialize COM
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if(FAILED(hr))
        {
            wprintf(L"CoInitializeEx() returned 0x%08x\r\n", hr);
            break;
        }

        // Check the type of transcode and run the appropriate transcode function
        if(_wcsicmp(argv[1], L"api") == 0)
        {
            wprintf(L"\r\nStarting transcode with the Transcode API.\r\n");
            CTranscodeApi transcode;

            hr = transcode.TranscodeFile(argv[2], argv[3]);
            if(FAILED(hr))
            {
                wprintf(L"transcode.TranscodeFile() failed, hr = 0x%08x.\r\n", hr);
            }
            else
            {
                hr = transcode.WaitUntilCompletion();
                if(FAILED(hr))
                    wprintf(L"transcode.WaitUntilCompletion() failed, hr = 0x%08x.\r\n", hr);
                else
                    wprintf(L"Transcode completed successfully.\r\n");
            }
        }
        else if(_wcsicmp(argv[1], L"rw") == 0)
        {
            wprintf(L"\r\nStarting transcode with the Source Reader and Sink Writer.\r\n");
            CReaderWriterTranscoder rwTranscoder;

            hr = rwTranscoder.Transcode(argv[2], argv[3]);

            if(FAILED(hr))
                wprintf(L"CReaderWriterTranscoder::Transcode() returned 0x%08x\r\n", hr);
            else
                wprintf(L"Transcode completed successfully.\r\n");
        }
        else
        {
            ShowUsage();
        }

        CoUninitialize();
    }
    while(false);

	return 0;
}

