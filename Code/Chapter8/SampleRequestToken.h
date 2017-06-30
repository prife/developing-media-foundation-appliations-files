#pragma once
#include "unknwn.h"
class CSampleRequestToken :
    public IUnknown
{
    public:
        CSampleRequestToken(void);
        ~CSampleRequestToken(void);
        
        // IUnknown interface implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

        long RefCount(void) { m_cRef; }

    private:
        volatile long m_cRef;                       // reference count
};

