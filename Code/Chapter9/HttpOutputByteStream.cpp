#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include "HttpOutputByteStream.h"



#define RECEIVE_BUFFER_SIZE  262144
#define SEND_BUFFER_SIZE 65536


char HttpResponseHeader[] = 
    "HTTP/1.1 200 OK\r\n\
Content-Type: video/x-ms-asf\r\n\
Server: Microsoft-HTTPAPI/2.0\r\n\
Accept-Ranges: none\r\n\
TransferMode.DLNA.ORG: Streaming\r\n\
Connection: open\r\n\r\n";


HRESULT CHttpOutputByteStream::CreateInstance(DWORD requestPort, IMFByteStream** ppByteStream)
{
    HRESULT hr = S_OK;
    CHttpOutputByteStream* pByteStream = NULL;

    do
    {
        BREAK_ON_NULL(ppByteStream, E_POINTER);

        pByteStream = new (std::nothrow) CHttpOutputByteStream();
        BREAK_ON_NULL(pByteStream, E_OUTOFMEMORY);

        // initialize the socket system
        int result = WSAStartup(0x202, &(pByteStream->m_wsaData));
        if (result != 0) 
        {
            hr = HRESULT_FROM_WIN32(WSAGetLastError());
            break;
        }

        pByteStream->m_dwOutputBuffer = SEND_BUFFER_SIZE;
        pByteStream->m_dwOutputBufferNewSize = SEND_BUFFER_SIZE;
        pByteStream->m_dwOutputDataCollected = 0;
        
        pByteStream->m_pOutputBuffer = new (std::nothrow) BYTE[SEND_BUFFER_SIZE];
        BREAK_ON_NULL(pByteStream->m_pOutputBuffer, E_OUTOFMEMORY);

        pByteStream->m_outputPort = requestPort;

        // allocate a special worker thread for the blocking synchronous MFT operations
        hr = MFAllocateWorkQueue(&(pByteStream->m_netWorkQueue));
        BREAK_ON_FAIL(hr);

        *ppByteStream = pByteStream;
    }
    while(false);

    if(FAILED(hr) && pByteStream != NULL)
    {
        delete pByteStream;
    }

    return hr;
}



CHttpOutputByteStream::CHttpOutputByteStream(void) :
    m_pOutputBuffer(NULL),
    m_cRef(1),
    m_bIsStopping(false),
    m_clientSocket(INVALID_SOCKET),
    m_listenSocket(INVALID_SOCKET)
{
}


CHttpOutputByteStream::~CHttpOutputByteStream(void)
{
    if(m_pOutputBuffer != NULL)
    {
        delete m_pOutputBuffer;
    }

    if(m_clientSocket != INVALID_SOCKET)
    {
        closesocket(m_clientSocket);
    }

    // release the private worker queue
    MFUnlockWorkQueue(m_netWorkQueue);
}


//////////////////////////////////////////////////////////////////////////////////////////
//
//  IUnknown interface implementation
//
/////////////////////////////////////////////////////////////////////////////////////////
ULONG CHttpOutputByteStream::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG CHttpOutputByteStream::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }

    return refCount;
}

HRESULT CHttpOutputByteStream::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(static_cast<IMFByteStream*>(this));
    }
    else if (riid == IID_IMFByteStream)
    {
        *ppv = static_cast<IMFByteStream*>(this);
    }
    else if (riid == IID_IMFAsyncCallback)
    {
        *ppv = static_cast<IMFAsyncCallback*>(this);
    }
    else
    {
        *ppv = NULL;
        hr = E_NOINTERFACE;
    }

    if(SUCCEEDED(hr))
        AddRef();

    return hr;
}



/////////////////////////////////////////////////////////////////////////
//
// IMFAsyncCallback implementation
//
/////////////////////////////////////////////////////////////////////////

//
// Get the behavior information (duration, etc.) of the asynchronous callback operation - 
// not implemented.
//
HRESULT CHttpOutputByteStream::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue)
{
    return E_NOTIMPL;
}


// 
// Asynchronous worker function - called when an asynchronous write has been scheduled.
//
HRESULT CHttpOutputByteStream::Invoke(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;
    HRESULT sendResult = S_OK;
    CComPtr<IUnknown> pUnkState;
    CComQIPtr<IAsyncWriteData> pWriteData;
    BYTE* pData = NULL;
    DWORD dataLength = 0;

    do
    {
        // get the state object from the result
        hr = pResult->GetState(&pUnkState);
        BREAK_ON_FAIL(hr);

        // cast the IUnknown state object to IAsyncWriteData to extract the write data
        pWriteData = pUnkState;
        BREAK_ON_NULL(pWriteData, E_UNEXPECTED);

        // make sure the output port has been initialized
        if(m_clientSocket == INVALID_SOCKET)
        {
            hr = InitSocket(m_outputPort);
            BREAK_ON_FAIL(hr);
        }
        
        // get the data that will be sent to the client from the state object
        hr = pWriteData->GetWriteData(&pData, &dataLength);
        BREAK_ON_FAIL(hr);

        // send the data to the internal buffer, and eventually to the client
        sendResult = SendData(pData, dataLength);
    }
    while(false);

    // always call a helper function to send the result of the write operation to the caller
    SendAsyncWriteResult(pWriteData, sendResult);

    return hr;
}


//
// Send the result of the asynchronous write operation to the caller
//
HRESULT CHttpOutputByteStream::SendAsyncWriteResult(IAsyncWriteData* pWriteData, 
    HRESULT sendResult)
{
    HRESULT hr = S_OK;
    
    CComPtr<IUnknown> pBeginWriteState;
    CComPtr<IMFAsyncCallback> pBeginWriteCallback;
    CComPtr<IMFAsyncResult> pBeginWriteResult;

    do
    {
        // get the callback object passed in during BeginWrite call
        hr = pWriteData->GetCallback(&pBeginWriteCallback);
        BREAK_ON_FAIL(hr);

        // get the status object passed in during BeginWrite call
        hr = pWriteData->GetStateObject(&pBeginWriteState);
        BREAK_ON_FAIL(hr);

        // create an IMFAsyncResult object that will be used to report the operation status 
        // to the caller
        hr = MFCreateAsyncResult(pWriteData, pBeginWriteCallback, pBeginWriteState, 
            &pBeginWriteResult);
        BREAK_ON_FAIL(hr);

        // store the send operation result in the IMFAsyncResult for the caller
        pBeginWriteResult->SetStatus(sendResult);

        // schedule a callback to the caller
        MFInvokeCallback(pBeginWriteResult);
    }
    while(false);

    return hr;
}



/////////////////////////////////////////////////////////////////////////
//
// IMFByteStream implementation
//
/////////////////////////////////////////////////////////////////////////

//
// Query the capabilities of this IMFByteStream object
//
HRESULT CHttpOutputByteStream::GetCapabilities(DWORD *pdwCapabilities)
{
    if(pdwCapabilities == NULL)
        return E_POINTER;

    *pdwCapabilities = MFBYTESTREAM_IS_WRITABLE;

    return S_OK;
}


HRESULT CHttpOutputByteStream::GetLength(QWORD *pqwLength)
{
    return E_NOTIMPL;
}


HRESULT CHttpOutputByteStream::SetLength(QWORD qwLength)
{
    return E_NOTIMPL;
}


HRESULT CHttpOutputByteStream::GetCurrentPosition(QWORD *pqwPosition)
{
    return E_NOTIMPL;
}


HRESULT CHttpOutputByteStream::SetCurrentPosition(QWORD qwPosition)
{
    return E_NOTIMPL;
}


HRESULT CHttpOutputByteStream::IsEndOfStream(BOOL *pfEndOfStream)
{
    return E_NOTIMPL;
}


HRESULT CHttpOutputByteStream::Read(BYTE *pb, ULONG cb, ULONG *pcbRead)
{
    return E_NOTIMPL;
}


HRESULT CHttpOutputByteStream::BeginRead(BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, IUnknown *punkState)
{
    return E_NOTIMPL;
}


HRESULT CHttpOutputByteStream::EndRead(IMFAsyncResult *pResult, ULONG *pcbRead)
{
    return E_NOTIMPL;
}


//
// Synchronously write the data to the network
//
HRESULT CHttpOutputByteStream::Write(const BYTE *pb, ULONG cb, ULONG *pcbWritten)
{
    HRESULT hr = S_OK;

    do
    {
        BREAK_ON_NULL(pb, E_POINTER);
        BREAK_ON_NULL(pcbWritten, E_POINTER);

        // clear the written counter - if the write fails, this parameter will return 0
        *pcbWritten = 0;

        // if the output socket has not been initialized yet, intialize it - note that
        // this call will block until a network client connects
        if(m_clientSocket == INVALID_SOCKET)
        {
            hr = InitSocket(m_outputPort);
            BREAK_ON_FAIL(hr);
        }

        // send the data to the internal buffer, and eventually out to the network
        hr = SendData(pb, cb);
        BREAK_ON_FAIL(hr);

        // if we got here, the write succeeded - set the number of bytes written
        *pcbWritten = cb;
    }
    while(false);

    return hr;
}

//
// Begin asynchronous write operation
//
HRESULT CHttpOutputByteStream::BeginWrite(const BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, IUnknown *punkState)
{
    HRESULT hr = S_OK;
    CComPtr<IAsyncWriteData> pWriteData;

    do
    {
        BREAK_ON_NULL(pb, E_POINTER);
        BREAK_ON_NULL(pCallback, E_POINTER);        
  
        // create an async call state object
        pWriteData = new (std::nothrow) CByteStreamWriteData((BYTE*)pb, cb, pCallback, 
            punkState);
        BREAK_ON_NULL(pWriteData, E_OUTOFMEMORY);

        // schedule the asynchronous operation on the private worker queue
        hr = MFPutWorkItem(m_netWorkQueue, this, pWriteData);
    }
    while(false);

    return hr;
}


//
// End the asynchronous write operation and return the number of bytes written
//
HRESULT CHttpOutputByteStream::EndWrite(IMFAsyncResult* pResult, ULONG* pcbWritten)
{
    HRESULT hr = S_OK;
    CComPtr<IUnknown> pUnkState;
    CComQIPtr<IAsyncWriteData> pWriteData;
    BYTE* pData = NULL;
    DWORD dataLength = 0;

    do
    {
        BREAK_ON_NULL(pcbWritten, E_POINTER);
        BREAK_ON_NULL(pResult, E_POINTER);
        
        // see if this asynchronous call failed - if it did set the number of bytes written
        // to zero
        hr = pResult->GetStatus();
        if(FAILED(hr))
        {
            *pcbWritten = 0;
        }
        BREAK_ON_FAIL(hr);

        // get the IAsyncWriteData state object from the result
        hr = pResult->GetObject(&pUnkState);
        BREAK_ON_FAIL(hr);

        // Get the IAsyncWriteData pointer from the IUnknown pointer of the state object
        pWriteData = pUnkState;
        BREAK_ON_NULL(pWriteData, E_UNEXPECTED);

        // get the number of bytes that were stored in this state object, and return it
        pWriteData->GetWriteData(&pData, &dataLength);
        *pcbWritten = dataLength;
    }
    while(false);

    return hr;
}


HRESULT CHttpOutputByteStream::Seek(MFBYTESTREAM_SEEK_ORIGIN SeekOrigin, LONGLONG llSeekOffset, DWORD dwSeekFlags, QWORD *pqwCurrentPosition)
{
    return E_NOTIMPL;
}


//
// Flush the byte stream, emptying the internal buffer and sending the data to the client
//
HRESULT CHttpOutputByteStream::Flush(void)
{
    HRESULT hr = S_OK;
    int sendResult = 0;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // if there is data, send it out
        if(m_dwOutputDataCollected > 0)
        {
            // send whatever is collected in the buffer
            sendResult = send( m_clientSocket, (const char*)m_pOutputBuffer, 
                m_dwOutputDataCollected, 0 );
            if (sendResult == SOCKET_ERROR) 
            {
                hr = HRESULT_FROM_WIN32(WSAGetLastError());
                break;
            }

            // reset the counter indicating that the buffer is now empty
            m_dwOutputDataCollected = 0;
        }
    }
    while(false);

    return hr;
}


HRESULT CHttpOutputByteStream::Close(void)
{
    HRESULT hr = S_OK;
    int result = 0;

    do
    {
        m_bIsStopping = true;
        {
            CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

            // reset the buffer on stop
            m_dwOutputDataCollected = 0;
        
            if(m_clientSocket != INVALID_SOCKET)
            {
                result = shutdown(m_clientSocket, SD_SEND);
                if (result == SOCKET_ERROR) 
                {
                    hr = HRESULT_FROM_WIN32(WSAGetLastError());
                    break;
                }
            }
        }
    }
    while(false);

    return hr;
}






HRESULT CHttpOutputByteStream::SendData(const BYTE* data, DWORD dataLength)
{
    HRESULT hr = S_OK;
    int sendResult = 0;
    int sendError = 0;
    DWORD bytesCopied = 0;
    DWORD bytesToCopy = 0;

    // loop while there is still unsent data in the sample buffer
    while((dataLength - bytesCopied) > 0)
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // if there is space left in the buffer, copy data into it
        if(m_dwOutputDataCollected < m_dwOutputBuffer)
        {
            // figure out how many bytes we need to copy - it will be the min of the two numbers:
            bytesToCopy = min( 
                (dataLength - bytesCopied),                     // number of bytes that haven't been copied yet
                (m_dwOutputBuffer - m_dwOutputDataCollected) );   // amount of space left in the buffer

            // copy the data from the sample into the output buffer
            memcpy_s(m_pOutputBuffer + m_dwOutputDataCollected,         
                    (m_dwOutputBuffer - m_dwOutputDataCollected), 
                    data + bytesCopied, 
                    bytesToCopy);
            
            m_dwOutputDataCollected += bytesToCopy;     // adjust amount of data in the buffer that we've collected
            bytesCopied += bytesToCopy;                 // adjust amount of data in the sample that hasn't been collected yet
        }

        // if the output buffer is full, then send its contents and reset the counters
        if(m_dwOutputDataCollected == m_dwOutputBuffer)
        {
            if(m_clientSocket != INVALID_SOCKET)
            {
                sendResult = send( m_clientSocket, (const char*)m_pOutputBuffer, m_dwOutputDataCollected, 0 );

                if (sendResult == SOCKET_ERROR) 
                {
                    hr = HRESULT_FROM_WIN32(WSAGetLastError());
                    break;
                }

                // if we got here, we must have successfully sent the buffer.  Reset the buffer.
                m_dwOutputDataCollected = 0;

                // if we were asked to resize the output buffer at some point, do so
                if(m_dwOutputBuffer != m_dwOutputBufferNewSize)
                {
                    m_dwOutputBuffer = m_dwOutputBufferNewSize;
                    delete m_pOutputBuffer;
                    m_pOutputBuffer = new (std::nothrow) BYTE[m_dwOutputBuffer];
                    BREAK_ON_NULL(m_pOutputBuffer, E_OUTOFMEMORY);
                }
            }
            else
            {
                hr = E_UNEXPECTED;
                break;
            }
        }
    }

    return hr;
}



HRESULT CHttpOutputByteStream::InitSocket(DWORD port)
{
    HRESULT hr = S_OK;
    addrinfo* resultAddress = NULL;
    addrinfo hints;
    int result = 0;
    char recvbuf[RECEIVE_BUFFER_SIZE] = {'\0'};
    char portStr[8] = {'\0'};

    do
    {
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_IP;
        hints.ai_flags = AI_PASSIVE;

        sprintf_s(portStr, 8, "%d", port);

        // Resolve the server address and port
        result = getaddrinfo(NULL, portStr, &hints, &resultAddress);
        if ( result != 0 )
        {
            hr = HRESULT_FROM_WIN32(WSAGetLastError());
            break;
        }

        // Create a SOCKET for connecting to server
        m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, 
            WSA_FLAG_OVERLAPPED);
        if (m_listenSocket == INVALID_SOCKET)
        {
            hr = HRESULT_FROM_WIN32(WSAGetLastError());
            break;
        }

        // configure the socket
        int nZero = 0;
	    result = setsockopt(m_listenSocket, SOL_SOCKET, SO_SNDBUF, (char *)&nZero, 
            sizeof(nZero));
        if (result == SOCKET_ERROR)
        {
            hr = HRESULT_FROM_WIN32(WSAGetLastError());
            break;
        }

        // Setup the TCP listening socket
        result = bind( m_listenSocket, resultAddress->ai_addr, 
            (int)(resultAddress->ai_addrlen));
        if (result == SOCKET_ERROR) 
        {
            hr = HRESULT_FROM_WIN32(WSAGetLastError());
            break;
        }

        freeaddrinfo(resultAddress);
        resultAddress = NULL;

        // start listening on the input socket
        result = listen(m_listenSocket, SOMAXCONN);
        if (result == SOCKET_ERROR) 
        {
            hr = HRESULT_FROM_WIN32(WSAGetLastError());
            break;
        }

        // Accept a client socket connection
        m_clientSocket = accept(m_listenSocket, NULL, NULL);
        if (m_clientSocket == INVALID_SOCKET) 
        {
            hr = HRESULT_FROM_WIN32(WSAGetLastError());
            break;
        }

        // Receive all the data - it should be a short HTTP GET message requesting data.
        // Ignore the message contents - we will send out data after any request on port
        result = recv(m_clientSocket, recvbuf, RECEIVE_BUFFER_SIZE, 0);
        
        // send the HTTP response header to notify the client that data is forthcoming
        result = send( m_clientSocket, (const char*)HttpResponseHeader, 
            (int)strlen(HttpResponseHeader), 0 );
        if(result == SOCKET_ERROR)
        {
            hr = HRESULT_FROM_WIN32(WSAGetLastError());
            break;
        }
    }
    while(FALSE);

    if(FAILED(hr))
    {
        if(m_clientSocket != INVALID_SOCKET)
        {
            closesocket(m_clientSocket);
            m_clientSocket = INVALID_SOCKET;
        }

        if(m_listenSocket != INVALID_SOCKET)
        {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
        }

        if(resultAddress != NULL)
        {
            freeaddrinfo(resultAddress);
        }
    }

    return hr;
}


