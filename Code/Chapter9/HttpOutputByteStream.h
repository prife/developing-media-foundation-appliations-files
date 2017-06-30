#pragma once


#include <mfidl.h>
#include "Common.h"
#include "ByteStreamWriteData.h"
#include <new>

class CHttpOutputByteStream : 
    public IMFByteStream,
    public IMFAsyncCallback
{
    public:
        static HRESULT CreateInstance(DWORD requestPort, IMFByteStream** ppByteStream);
        
        // IUnknown interface implementation
        STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

        // IMFByteStream interface implementation
        STDMETHODIMP  GetCapabilities(DWORD *pdwCapabilities);
        STDMETHODIMP  GetLength(QWORD *pqwLength);
        STDMETHODIMP  SetLength(QWORD qwLength);
        STDMETHODIMP  GetCurrentPosition(QWORD *pqwPosition);
        STDMETHODIMP  SetCurrentPosition(QWORD qwPosition);
        STDMETHODIMP  IsEndOfStream(BOOL *pfEndOfStream);
        STDMETHODIMP  Read(BYTE *pb, ULONG cb, ULONG *pcbRead);
        STDMETHODIMP  BeginRead(BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, 
            IUnknown *punkState);
        STDMETHODIMP  EndRead(IMFAsyncResult *pResult, ULONG *pcbRead);
        STDMETHODIMP  Write(const BYTE *pb, ULONG cb, ULONG *pcbWritten);
        STDMETHODIMP  BeginWrite(const BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, 
            IUnknown *punkState);
        STDMETHODIMP  EndWrite(IMFAsyncResult *pResult, ULONG *pcbWritten);
        STDMETHODIMP  Seek(MFBYTESTREAM_SEEK_ORIGIN SeekOrigin, LONGLONG llSeekOffset, 
            DWORD dwSeekFlags, QWORD *pqwCurrentPosition);
        STDMETHODIMP  Flush(void);
        STDMETHODIMP  Close(void);

        // IMFAsyncCallback interface implementation
        STDMETHODIMP GetParameters(DWORD* pdwFlags, DWORD* pdwQueue);
        STDMETHODIMP Invoke(IMFAsyncResult* pResult);

    private:
        volatile long m_cRef;
        CComAutoCriticalSection m_critSec;          // critical section

        WSADATA m_wsaData;
        SOCKET m_clientSocket;
        SOCKET m_listenSocket;
        DWORD m_outputPort;

        HANDLE m_testThread;
        
        DWORD m_netWorkQueue;

        BYTE* m_pOutputBuffer;
        DWORD m_dwOutputBuffer;
        volatile DWORD m_dwOutputBufferNewSize;
        DWORD m_dwOutputDataCollected;
        bool m_bIsStopping;

        CHttpOutputByteStream(void);
        ~CHttpOutputByteStream(void);

        HRESULT InitSocket(DWORD port);
        HRESULT SendData(const BYTE* data, DWORD dataLength);
        HRESULT SendAsyncWriteResult(IAsyncWriteData* pWriteData, HRESULT sendResult);
};

