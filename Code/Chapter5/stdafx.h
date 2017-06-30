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
#include <mftransform.h>
#include <mfidl.h>
#include <mferror.h>

#include <shlwapi.h>

#include <strsafe.h>

#include <initguid.h>


#define BREAK_ON_FAIL(value)            if(FAILED(value)) break;
#define BREAK_ON_NULL(value, newHr)     if(value == NULL) { hr = newHr; break; }



extern ULONG g_dllLockCount;
extern HMODULE g_hModule;



// {B77014BF-04AC-4B0D-90BD-52CA8ADF73ED}
DEFINE_GUID(CLSID_CImageInjectorMFT, 0xb77014bf, 0x4ac, 0x4b0d, 0x90, 0xbd, 0x52, 0xca, 0x8a, 0xdf, 0x73, 0xed);

#define IMAGE_INJECTOR_MFT_CLSID_STR   L"Software\\Classes\\CLSID\\{B77014BF-04AC-4B0D-90BD-52CA8ADF73ED}"

#define BREAK_ON_FAIL(value)            if(FAILED(value)) break;
#define BREAK_ON_NULL(value, newHr)     if(value == NULL) { hr = newHr; break; }

