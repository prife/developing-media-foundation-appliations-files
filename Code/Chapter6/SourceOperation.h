#pragma once


#include <atlbase.h>
#include <Mfidl.h>
#include <Mferror.h>


// Operation type.
enum SourceOperationType
{
    SourceOperationOpen,
    SourceOperationStart,
    SourceOperationPause,
    SourceOperationStop,
    SourceOperationStreamNeedData,
    SourceOperationEndOfStream
};


// ISourceOperation COM IID.
// {35D8883D-3239-4ABE-84BD-43EAC5ED2304}
DEFINE_GUID(IID_ISourceOperation, 0x35d8883d, 0x3239, 0x4abe, 0x84, 0xbd, 0x43, 0xea, 0xc5, 
    0xed, 0x23, 0x4);

// ISourceOperation COM interface
struct ISourceOperation : public IUnknown
{
    public:
        virtual HRESULT GetPresentationDescriptor(
            IMFPresentationDescriptor** ppPresentationDescriptor) = 0;
        virtual HRESULT SetData(const PROPVARIANT& data, bool isSeek) = 0;
        virtual PROPVARIANT& GetData() = 0;
        virtual bool IsSeek(void) = 0;
        virtual SourceOperationType Type(void) = 0;
        virtual WCHAR* GetUrl(void) = 0;
        virtual HRESULT GetCallerAsyncResult(IMFAsyncResult** pCallerResult) = 0;
};

// COM object used to pass commands between threads.
class SourceOperation : public ISourceOperation
{
    public:
        SourceOperation(SourceOperationType operation);
        SourceOperation(SourceOperationType operation, LPCWSTR pUrl, 
            IMFAsyncResult* pCallerResult);
        SourceOperation(SourceOperationType operation, 
            IMFPresentationDescriptor* pPresentationDescriptor);
        SourceOperation(const SourceOperation& operation);

        // ISourceOperation interface implementation
        virtual HRESULT GetPresentationDescriptor(
            IMFPresentationDescriptor** ppPresentationDescriptor);
        virtual PROPVARIANT& GetData();
        virtual bool IsSeek(void)  { return m_isSeek; }
        virtual SourceOperationType Type(void) { return m_operationType; }
        virtual WCHAR* GetUrl(void) { return m_pUrl; };
        virtual HRESULT GetCallerAsyncResult(IMFAsyncResult** ppCallerResult);

        // IUnknown interface implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);


        HRESULT SetData(const PROPVARIANT& data, bool isSeek);


    private:
        ~SourceOperation();
        void Init(SourceOperationType operation, 
            IMFPresentationDescriptor* pPresentationDescriptor);

        volatile long m_cRef;                       // reference count
        bool m_isSeek;
        SourceOperationType m_operationType;
        PropVariantGeneric m_data;
        CComPtr<IMFPresentationDescriptor> m_pPresentationDescriptor;
        
        // variables used during BeginOpen operation - URL of file to open and client's
        // result that will be invoked when open is complete
        CComPtr<IMFAsyncResult> m_pCallerResult;
        WCHAR* m_pUrl;
};

