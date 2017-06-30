// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit

#include <atlbase.h>
#include <atlstr.h>

// TODO: reference additional headers your program requires here

#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>

#include <shlwapi.h>

#include <initguid.h>



extern ULONG g_dllLockCount;
extern HMODULE g_hModule;

#define AVF_BYTE_STREAM_HANDLER_CLSID_STR   L"Software\\Classes\\CLSID\\{65085DC5-EB10-421B-943A-425F0CADBD0B}"

// {65085DC5-EB10-421B-943A-425F0CADBD0B}
DEFINE_GUID(CLSID_AVFByteStreamHandler, 0x65085dc5, 0xeb10, 0x421b, 0x94, 0x3a, 0x42, 0x5f, 0xc, 0xad, 0xbd, 0xb);


#define BREAK_ON_FAIL(value)            if(FAILED(value)) break;
#define BREAK_ON_NULL(value, newHr)     if(value == NULL) { hr = newHr; break; }
#define EXCEPTION_TO_HR(expression) { try { hr = S_OK; expression; } catch(const CAtlException& e) { hr = e.m_hr; } catch(...) { hr = E_OUTOFMEMORY; } }


template <typename T>
inline void SafeRelease(T*& p)
{
    if (p)
    {
        p->Release();
        p = NULL;
    }
}

template <typename T>
inline void SafeAddRef(T* p)
{
    if (p)
    {
        p->AddRef();
    }
}


class PropVariantGeneric : public PROPVARIANT
{
public:
    PropVariantGeneric()
    {
        PropVariantInit(this);
    }

    ~PropVariantGeneric()
    {
        PropVariantClear(this);
    }
};