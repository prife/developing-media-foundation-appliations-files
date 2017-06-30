// AVFByteStreamHandler.h : Declaration of the AVFByteStreamHandler

#pragma once
#include "AVFSource.h"
#include "AviFileParser.h"

#include <mfapi.h>



class AVFByteStreamHandler :
    public IMFByteStreamHandler,
    public IMFAsyncCallback,
    public IInitializeWithFile,
    public IPropertyStore
{
    public:
	    AVFByteStreamHandler(void);
        ~AVFByteStreamHandler(void);

        // IMFByteStreamHandler interface implementation
        STDMETHODIMP BeginCreateObject(IMFByteStream* pByteStream,
                                       LPCWSTR pwszURL,
                                       DWORD dwFlags,
                                       IPropertyStore* pProps,
                                       IUnknown** ppIUnknownCancelCookie,
                                       IMFAsyncCallback* pCallback,
                                       IUnknown *punkState);
        
        STDMETHODIMP EndCreateObject(IMFAsyncResult* pResult,
                                     MF_OBJECT_TYPE* pObjectType,
                                     IUnknown** ppObject);
        
        STDMETHODIMP CancelObjectCreation(IUnknown* pIUnknownCancelCookie);
        STDMETHODIMP GetMaxNumberOfBytesRequiredForResolution(QWORD* pqwBytes);

        //
        // IMFAsyncCallback interface implementation
        STDMETHODIMP GetParameters(DWORD* pdwFlags, DWORD* pdwQueue);
        STDMETHODIMP Invoke(IMFAsyncResult* pResult);

        //
        // IUnknown interface implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

        //
        // IInitializeWithFile interface implementation
        STDMETHODIMP Initialize( LPCWSTR pszFilePath, DWORD grfMode );

        //
        // IPropertyStore interface implementation
        STDMETHODIMP Commit();
        STDMETHODIMP GetAt( DWORD iProp, PROPERTYKEY* pkey);
        STDMETHODIMP GetCount( DWORD* cProps );
        STDMETHODIMP GetValue( REFPROPERTYKEY key, PROPVARIANT* pv);
        STDMETHODIMP SetValue( REFPROPERTYKEY key, REFPROPVARIANT propvar);

    private:
        volatile long m_cRef;                                    // ref count
        CComAutoCriticalSection m_critSec;

        AVFSource* m_pAVFSource;
        CComPtr<IMFAsyncResult> m_pResult;
        CComPtr<IPropertyStore> m_pPropertyStore;

        // holds a value indicating that the creation is being canceled
        bool m_objectCreationCanceled;
};

