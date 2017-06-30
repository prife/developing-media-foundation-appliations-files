#pragma once
#include "unknwn.h"
#include "AviSink.h"




class ClassFactory : public IClassFactory
{
    public:
        ClassFactory(void);
        ~ClassFactory(void);

        // IClassFactory interface implementation
        virtual HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
        virtual HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock);

        // IUnknown interface implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);

    private:

        long m_cRef;
};

