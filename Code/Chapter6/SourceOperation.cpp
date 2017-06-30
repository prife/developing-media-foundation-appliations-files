#include "StdAfx.h"
#include "SourceOperation.h"


SourceOperation::SourceOperation(SourceOperationType operation) :
    m_cRef(0),
    m_pUrl(NULL)
{
    Init(operation, NULL);
}


SourceOperation::SourceOperation(SourceOperationType operation, LPCWSTR pUrl, IMFAsyncResult* pCallerResult) :
    m_cRef(0),
    m_pUrl(NULL)
    
{
    if(pUrl == NULL)
        return;

    m_pUrl = new WCHAR[wcslen(pUrl) + 1];
    if(m_pUrl == NULL)
        return;

    m_operationType = operation;

    wcscpy_s(m_pUrl, wcslen(pUrl) + 1, pUrl);

    m_pCallerResult = pCallerResult;
}

SourceOperation::SourceOperation(SourceOperationType operation, IMFPresentationDescriptor* pPresentationDescriptor) :
    m_cRef(0),
    m_pUrl(NULL)
{
    Init(operation, pPresentationDescriptor);
}

SourceOperation::SourceOperation(const SourceOperation& operation) :
    m_cRef(0),
    m_pUrl(NULL)
{
    Init(operation.m_operationType, operation.m_pPresentationDescriptor);
}

void SourceOperation::Init(SourceOperationType operation, IMFPresentationDescriptor* pPresentationDescriptor)
{
    m_isSeek = false;
    m_operationType = operation;
    if (pPresentationDescriptor)
    {
        m_pPresentationDescriptor = pPresentationDescriptor;
    }
}

HRESULT SourceOperation::GetPresentationDescriptor(IMFPresentationDescriptor** ppPresentationDescriptor)
{
    CComPtr<IMFPresentationDescriptor> spPresentationDescriptor = m_pPresentationDescriptor;
    if (spPresentationDescriptor)
    {
        *ppPresentationDescriptor = spPresentationDescriptor.Detach();
        return S_OK;
    }
    return MF_E_INVALIDREQUEST;
}

HRESULT SourceOperation::SetData(const PROPVARIANT& data, bool isSeek)
{
    m_isSeek = isSeek;
    return PropVariantCopy(&m_data, &data);
}

PROPVARIANT& SourceOperation::GetData(void)
{ 
    return m_data;
}

HRESULT SourceOperation::GetCallerAsyncResult(IMFAsyncResult** ppCallerResult)
{
    if(ppCallerResult == NULL)
        return E_POINTER;

    *ppCallerResult = m_pCallerResult;
    (*ppCallerResult)->AddRef();
    return S_OK;
}

SourceOperation::~SourceOperation(void)
{
    if(m_pUrl != NULL)
        delete m_pUrl;
}



//
// Standard IUnknown interface implementation
//
ULONG SourceOperation::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG SourceOperation::Release()
{
    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT SourceOperation::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(this);
    }
    else if (riid == IID_ISourceOperation)
    {
        *ppv = static_cast<ISourceOperation*>(this);
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