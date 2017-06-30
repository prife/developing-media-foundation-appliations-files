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

#include <initguid.h>


extern ULONG g_dllLockCount;
extern HMODULE g_hModule;


// {357A0C45-EAD4-4EAC-B088-54E2271E7874}
DEFINE_GUID(CLSID_AviSink, 0x357a0c45, 0xead4, 0x4eac, 0xb0, 0x88, 0x54, 0xe2, 0x27, 0x1e, 0x78, 0x74);


#define BREAK_ON_FAIL(value)            if(FAILED(value)) break;
#define BREAK_ON_NULL(value, newHr)     if(value == NULL) { hr = newHr; break; }
#define EXCEPTION_TO_HR(expression)     { try { hr = S_OK; expression; } \
catch(const CAtlException& e) { hr = e.m_hr; break; } catch(...) { hr = E_OUTOFMEMORY; break; } }