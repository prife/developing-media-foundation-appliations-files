#pragma once

#include <atlbase.h>
#include "mfobjects.h"
#include "HttpOutputByteStream.h"


class CHttpOutputStreamActivate :
    public IMFActivate
{
    public:
        CHttpOutputStreamActivate(DWORD requestPort);
        ~CHttpOutputStreamActivate(void);

        // IUnknown interface implementation
        STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

        // IMFActivate interface implementation
        STDMETHODIMP  ActivateObject(REFIID riid, void **ppv);
        STDMETHODIMP  ShutdownObject(void) { return S_OK; }
        STDMETHODIMP  DetachObject(void) { return S_OK; }

        // IMFAttributes interface implementation - all not implemented
        STDMETHODIMP  GetItem(REFGUID guidKey, PROPVARIANT *pValue) { return E_NOTIMPL; }
        STDMETHODIMP  GetItemType(REFGUID guidKey, MF_ATTRIBUTE_TYPE *pType) { return E_NOTIMPL; }
        STDMETHODIMP  CompareItem(REFGUID guidKey, REFPROPVARIANT Value, BOOL *pbResult) 
        { return E_NOTIMPL; }
        STDMETHODIMP  Compare(IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, 
            BOOL *pbResult) { return E_NOTIMPL; }
        STDMETHODIMP  GetUINT32(REFGUID guidKey, UINT32 *punValue) { return E_NOTIMPL; }
        STDMETHODIMP  GetUINT64(REFGUID guidKey, UINT64 *punValue) { return E_NOTIMPL; }
        STDMETHODIMP  GetDouble(REFGUID guidKey, double *pfValue) { return E_NOTIMPL; }
        STDMETHODIMP  GetGUID(REFGUID guidKey, GUID *pguidValue) { return E_NOTIMPL; }
        STDMETHODIMP  GetStringLength(REFGUID guidKey, UINT32 *pcchLength) 
        { return E_NOTIMPL; }
        STDMETHODIMP  GetString(REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, 
            UINT32 *pcchLength) { return E_NOTIMPL; }
        STDMETHODIMP  GetAllocatedString(REFGUID guidKey, LPWSTR *ppwszValue, 
            UINT32 *pcchLength) { return E_NOTIMPL; }
        STDMETHODIMP  GetBlobSize(REFGUID guidKey, UINT32 *pcbBlobSize) 
        { return E_NOTIMPL; }
        STDMETHODIMP  GetBlob(REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize, 
            UINT32 *pcbBlobSize) { return E_NOTIMPL; }
        STDMETHODIMP  GetAllocatedBlob(REFGUID guidKey, UINT8 **ppBuf, UINT32 *pcbSize) 
        { return E_NOTIMPL; }
        STDMETHODIMP  GetUnknown(REFGUID guidKey, REFIID riid, LPVOID *ppv) 
        { return E_NOTIMPL; }
        STDMETHODIMP  SetItem(REFGUID guidKey, REFPROPVARIANT Value) { return E_NOTIMPL; }
        STDMETHODIMP  DeleteItem(REFGUID guidKey) { return E_NOTIMPL; }
        STDMETHODIMP  DeleteAllItems(void) { return E_NOTIMPL; }
        STDMETHODIMP  SetUINT32(REFGUID guidKey, UINT32 unValue) { return E_NOTIMPL; }
        STDMETHODIMP  SetUINT64(REFGUID guidKey, UINT64 unValue) { return E_NOTIMPL; }
        STDMETHODIMP  SetDouble(REFGUID guidKey, double fValue) { return E_NOTIMPL; }
        STDMETHODIMP  SetGUID(REFGUID guidKey, REFGUID guidValue) { return E_NOTIMPL; }
        STDMETHODIMP  SetString(REFGUID guidKey, LPCWSTR wszValue) { return E_NOTIMPL; }
        STDMETHODIMP  SetBlob(REFGUID guidKey, const UINT8 *pBuf, UINT32 cbBufSize) 
        { return E_NOTIMPL; }
        STDMETHODIMP  SetUnknown(REFGUID guidKey, IUnknown *pUnknown) { return E_NOTIMPL; }
        STDMETHODIMP  LockStore(void) { return E_NOTIMPL; }
        STDMETHODIMP  UnlockStore(void) { return E_NOTIMPL; }
        STDMETHODIMP  GetCount(UINT32 *pcItems) { return E_NOTIMPL; }
        STDMETHODIMP  GetItemByIndex(UINT32 unIndex, GUID *pguidKey, PROPVARIANT *pValue) 
        { return E_NOTIMPL; }
        STDMETHODIMP  CopyAllItems(IMFAttributes *pDest) { return E_NOTIMPL; }

    private:
        volatile long m_cRef;        

        DWORD m_requestPort;
};

