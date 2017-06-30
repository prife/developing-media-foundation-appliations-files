#pragma once
#include "mftransform.h"
#include <MMSystem.h>

#include "BmpFile.h"
#include "FrameParser.h"

class CImageInjectorMFT :
    public IMFTransform
{
    public:
        CImageInjectorMFT(void);
        ~CImageInjectorMFT(void);

        //
        // IMFTransform stream handling functions
        STDMETHODIMP GetStreamLimits(  DWORD* pdwInputMinimum, DWORD* pdwInputMaximum,
            DWORD* pdwOutputMinimum, DWORD* pdwOutputMaximum );

        STDMETHODIMP GetStreamIDs( DWORD dwInputIDArraySize, DWORD* pdwInputIDs,
            DWORD dwOutputIDArraySize, DWORD* pdwOutputIDs );

        STDMETHODIMP GetStreamCount( DWORD* pcInputStreams, DWORD* pcOutputStreams );
        STDMETHODIMP GetInputStreamInfo( DWORD dwInputStreamID, 
            MFT_INPUT_STREAM_INFO* pStreamInfo );
        STDMETHODIMP GetOutputStreamInfo( DWORD dwOutputStreamID, 
            MFT_OUTPUT_STREAM_INFO* pStreamInfo );
        STDMETHODIMP GetInputStreamAttributes( DWORD dwInputStreamID, 
            IMFAttributes** pAttributes );
        STDMETHODIMP GetOutputStreamAttributes( DWORD dwOutputStreamID, 
            IMFAttributes** pAttributes );
        STDMETHODIMP DeleteInputStream( DWORD dwStreamID );
        STDMETHODIMP AddInputStreams( DWORD cStreams, DWORD* adwStreamIDs );

        //
        // IMFTransform mediatype handling functions
        STDMETHODIMP GetInputAvailableType( DWORD dwInputStreamID, DWORD dwTypeIndex, 
            IMFMediaType** ppType );
        STDMETHODIMP GetOutputAvailableType( DWORD dwOutputStreamID, DWORD dwTypeIndex, 
            IMFMediaType** ppType );
        STDMETHODIMP SetInputType( DWORD dwInputStreamID, IMFMediaType* pType, 
            DWORD dwFlags );
        STDMETHODIMP SetOutputType( DWORD dwOutputStreamID, IMFMediaType* pType, 
            DWORD dwFlags );
        STDMETHODIMP GetInputCurrentType( DWORD dwInputStreamID, IMFMediaType** ppType );
        STDMETHODIMP GetOutputCurrentType( DWORD dwOutputStreamID, IMFMediaType** ppType );

        //
        // IMFTransform status and eventing functions
        STDMETHODIMP GetInputStatus( DWORD dwInputStreamID, DWORD* pdwFlags );
        STDMETHODIMP GetOutputStatus( DWORD* pdwFlags );
        STDMETHODIMP SetOutputBounds( LONGLONG hnsLowerBound, LONGLONG hnsUpperBound);
        STDMETHODIMP ProcessEvent( DWORD dwInputStreamID, IMFMediaEvent* pEvent );
        STDMETHODIMP GetAttributes( IMFAttributes** pAttributes );


        //
        // IMFTransform main data processing and command functions
        STDMETHODIMP ProcessMessage( MFT_MESSAGE_TYPE eMessage, ULONG_PTR ulParam );
        STDMETHODIMP ProcessInput( DWORD dwInputStreamID, IMFSample* pSample, 
            DWORD dwFlags);

        STDMETHODIMP ProcessOutput( DWORD dwFlags, DWORD cOutputBufferCount, 
            MFT_OUTPUT_DATA_BUFFER* pOutputSamples, DWORD* pdwStatus);

        //
        // IUnknown interface implementation
        //
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

    
    private:

        volatile long m_cRef;                             // ref count
        CComAutoCriticalSection m_critSec;       // critical section for the MFT

        CComPtr<IMFSample>  m_pSample;           // Input sample.
        CComPtr<IMFMediaType> m_pInputType;      // Input media type.
        CComPtr<IMFMediaType> m_pOutputType;     // Output media type.

        CFrameParser m_frameParser;              // frame parsing and image injection object

        // private helper functions
        HRESULT GetSupportedMediaType(DWORD dwTypeIndex, IMFMediaType** ppmt);
        HRESULT CheckMediaType(IMFMediaType *pmt);
};

