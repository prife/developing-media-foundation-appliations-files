// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <tchar.h>

#include "ClassFactory.h"


// Handle to the DLL
HMODULE g_hModule = NULL;

// module ref count
ULONG g_dllLockCount = 0;

// Description string for the bytestream handler.
const wchar_t* sByteStreamHandlerDescription = L"AVF Source ByteStreamHandler";

// File extension for AVF files.
const wchar_t* sFileExtension = L".avf";

// Registry location for bytestream handlers.
const wchar_t* REGKEY_MF_BYTESTREAM_HANDLERS = L"Software\\Microsoft\\Windows Media Foundation\\ByteStreamHandlers";



HRESULT RegisterCOMObject(const TCHAR* pszCOMKeyLocation, const TCHAR *pszDescription);
HRESULT RegisterByteStreamHandler(const GUID& guid, const wchar_t *sFileExtension, const wchar_t *sDescription);
HRESULT UnregisterObject(const TCHAR* pszCOMKeyLocation);
HRESULT UnregisterByteStreamHandler(const GUID& guid, const wchar_t *sFileExtension);

HRESULT RegisterPropertyHandler(const GUID& guid, const wchar_t *sFileExtension);
HRESULT UnregisterPropertyHandler(void);



BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
        g_hModule = (HMODULE)hModule;
        break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}




STDAPI DllCanUnloadNow()
{
    if(g_dllLockCount == 0) 
        return S_OK;
    else
        return S_FALSE;
}




//
// Function exposed out of the dll used for registration of the COM object in
// the DLL using regsvr32 (e.g. "regsvr32 filename.dll")
//
STDAPI DllRegisterServer()
{
    HRESULT hr = S_OK;

    do
    {
        // Register the COM object CLSID so that CoCreateInstance() can be called to instantiate the
        // byte stream handler object.
        hr = RegisterCOMObject(AVF_BYTE_STREAM_HANDLER_CLSID_STR, L"AVF Source");
        BREAK_ON_FAIL(hr);

        // register the COM object as a byte stream handler - store its information in the location 
        // that the MFCreateSource*() functions search
        hr = RegisterByteStreamHandler(CLSID_AVFByteStreamHandler, 
                                       L".avf",
                                       L"AVF Byte Stream Handler");
        BREAK_ON_FAIL(hr);

        hr = RegisterPropertyHandler(CLSID_AVFByteStreamHandler, L".avf");
    }
    while(false);

    return hr;
}


//
// Function exposed out of the dll used for unregistration of the COM object in
// the DLL using regsvr32 (e.g. "regsvr32 /u filename.dll")
//
STDAPI DllUnregisterServer()
{
    UnregisterByteStreamHandler(CLSID_AVFByteStreamHandler, sFileExtension);

    // Unregister the COM object itself
    UnregisterObject(AVF_BYTE_STREAM_HANDLER_CLSID_STR);

    UnregisterPropertyHandler();

    return S_OK;
}



//
// Get the object with the specified IID
//
STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppObj)
{
    HRESULT hr = E_OUTOFMEMORY; 
    *ppObj = NULL; 

    // create a helper class factory object
    ClassFactory* pClassFactory = new (std::nothrow) ClassFactory(); 
    if (pClassFactory != NULL)   
    { 
        // get the object that implements the requested interface from the class factory
        hr = pClassFactory->QueryInterface(riid, ppObj); 

        // release the class factory
        pClassFactory->Release(); 
    } 
    
    return hr;
}







//
// Create a registry key if one doesn't exist, and set a string value under that key.
// Return a handle to this key.
//
HRESULT CreateRegKeyAndStringValue(
    HKEY hKey,                  // the root of the registry key
    PCWSTR pszSubKeyName,       // name of the subkey
    PCWSTR pszValueName,        // name of a value to create (or NULL if set the default value)
    PCWSTR pszData,             // the data in the value (string)
    HKEY* pResultKey            // return a pointer to the handle of the key
    )
{
    *pResultKey = NULL;
    LONG result = ERROR_SUCCESS;

    // create the registry key requested - if the key already exists, just open it
    result = RegCreateKeyEx(
        hKey,                       // handle to the root key
        pszSubKeyName,              // name of the subkey
        0,                          // reserved - must be zero
        NULL,                       // class - can be ignored
        REG_OPTION_NON_VOLATILE,    // non-volatile - preserve between reboots
        KEY_ALL_ACCESS,             // security - allow all access
        NULL,                       // security attributes - none
        pResultKey,                 // pointer ot the result key
        NULL);                      // pValue indicating whether key was created or opened

    // if we have successfully created or opened the reg key, set the specified value
    // if the value name specified was NULL, then we are setting the "(Default)" value
    // of the key
    if (result == ERROR_SUCCESS)
    {
        result = RegSetValueExW(
            (*pResultKey),              // root key for the value - use the one just opened
            pszValueName,               // value name, or NULL for "(Default)" value
            0,                          // reserved - set to 0
            REG_SZ,                     // value type is string
            (LPBYTE) pszData,           // data itself
            ((DWORD) wcslen(pszData) + 1) * sizeof(WCHAR)  // data buffer length
            );

        // if the operation failed, just close the result key
        if (result != ERROR_SUCCESS)
        {
            RegCloseKey(*pResultKey);
        }
    }

    return HRESULT_FROM_WIN32(result);
}



//
// Create the registry entries for a COM object.
//
HRESULT RegisterCOMObject(const TCHAR* pszCOMKeyLocation, const TCHAR* pszDescription)
{
    HRESULT hr = S_OK;
    HKEY hKey = NULL;
    HKEY hSubkey = NULL;

    WCHAR tempStr[MAX_PATH];

    do
    {
        // Create a new registry key under "HKLM\\Software\\Classes\\CLSID\\" + AVF_BYTE_STREAM_HANDLER_CLSID_STR.
        hr = CreateRegKeyAndStringValue(
            HKEY_LOCAL_MACHINE,               // start in HKLM
            pszCOMKeyLocation,                // name + path of the key ("HKLM\\Software...")
            NULL,                             // name of a value under the key (none)
            pszDescription,                   // description of the filter
            &hKey);                           // returned handle of the key
        BREAK_ON_FAIL(hr);

        // get the filename and full path of this DLL and store it in the tempStr object        
        GetModuleFileName(g_hModule, tempStr, MAX_PATH);
        hr = HRESULT_FROM_WIN32(GetLastError());
        BREAK_ON_FAIL(hr);


        // Create the "InprocServer32" subkey and set it to the name of the
        // DLL file
        hr = CreateRegKeyAndStringValue(
            hKey,                   // root key
            L"InProcServer32",      // name of the key
            NULL,                   // name of a value under the key (none)
            tempStr,                // write in the key the full path to the DLL
            &hSubkey);              // get a handle to the new key
        
        // close the InProcServer32 subkey immediately since we don't need anything else from it
        RegCloseKey(hSubkey);
        BREAK_ON_FAIL(hr);
    
        // Add the "ThreadingModel" subkey and set it to "Both"
        hr = CreateRegKeyAndStringValue(
            hKey,                   // root of the key
            L"InProcServer32",      // name of the key 
            L"ThreadingModel",      // name of the value which will be modified/added
            L"Both",                // value data - we support free and apartment thead model
            &hSubkey);              // get a handle to the new key
        
        // close the subkey handle immediately since we don't need it
        RegCloseKey(hSubkey);        
        BREAK_ON_FAIL(hr);

        // close hkeys since we are done
        RegCloseKey(hKey);
    }
    while(false);

    return hr;
}


//
// Delete the registry entries for the COM object
//
HRESULT UnregisterObject(const TCHAR* pszCOMKeyLocation)
{
    // Delete the reg key tree for the COM object under 
    // "HKLM\\Software\\Classes\\CLSID\\" + CLSID
    LONG lRes = RegDeleteTree(
        HKEY_LOCAL_MACHINE,         // root key - HKLM
        pszCOMKeyLocation);         // key name
    
    return HRESULT_FROM_WIN32(lRes);
}



///////////////////////////////////////////////////////////////////////
// Name: CreateRegistryKey
// Desc: Creates a new registry key. (Thin wrapper just to encapsulate
//       all of the default options.)
///////////////////////////////////////////////////////////////////////

HRESULT CreateRegistryKey(HKEY hKey, LPCTSTR subkey, HKEY *phKey)
{
    LONG lreturn = RegCreateKeyEx(
        hKey,                 // parent key
        subkey,               // name of subkey
        0,                    // reserved
        NULL,                 // class string (can be NULL)
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        NULL,                 // security attributes
        phKey,
        NULL                  // receives the "disposition" (is it a new or existing key)
        );

    return HRESULT_FROM_WIN32(lreturn);
}




///////////////////////////////////////////////////////////////////////
// Name: RegisterByteStreamHandler
// Desc: Register a bytestream handler for the Media Foundation
//       source resolver.
//
// guid:            CLSID of the bytestream handler.
// sFileExtension:  File extension.
// sDescription:    Description.
//
// Note: sFileExtension can also be a MIME type although that is not
//       illustrated in this sample.
///////////////////////////////////////////////////////////////////////

HRESULT RegisterByteStreamHandler(const GUID& guid, const wchar_t *sFileExtension, const wchar_t *sDescription)
{
    HRESULT hr = S_OK;

    HKEY    hKey = NULL;
    HKEY    hSubKey = NULL;

    WCHAR szCLSID[256];
    size_t  cchDescription = 0;

    do
    {
        cchDescription = wcslen(sDescription);

        hr = StringFromGUID2(guid, szCLSID, 256);
        BREAK_ON_FAIL(hr);
        
        hr = CreateRegKeyAndStringValue(HKEY_LOCAL_MACHINE, 
            L"Software\\Microsoft\\Windows Media Foundation\\ByteStreamHandlers",
            NULL,
            sFileExtension,
            &hKey);
        BREAK_ON_FAIL(hr);
        
        hr = CreateRegistryKey(hKey, sFileExtension, &hSubKey);
        BREAK_ON_FAIL(hr);

        hr = RegSetValueEx(
                hSubKey,
                szCLSID,
                0,
                REG_SZ,
                (BYTE*)sDescription,
                static_cast<DWORD>((cchDescription + 1) * sizeof(wchar_t))
                );
        BREAK_ON_FAIL(hr);
    }
    while(false);

    if (hSubKey != NULL)
    {
        RegCloseKey( hSubKey );
    }

    if (hKey != NULL)
    {
        RegCloseKey( hKey );
    }

    return hr;
}


HRESULT RegisterPropertyHandler(const GUID& guid, const wchar_t *sFileExtension)
{
    HRESULT hr = S_OK;

    HKEY    hKey = NULL;
    DWORD   dwVal;

    WCHAR szCLSID[256];

    do
    {
        hr = StringFromGUID2(guid, szCLSID, 256);
        BREAK_ON_FAIL(hr);
        
        // set the property handler CLSID - this is the same as the byte stream handler
        hr = CreateRegKeyAndStringValue(HKEY_LOCAL_MACHINE, 
            L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\.avf",
            NULL,
            szCLSID,
            &hKey);
        BREAK_ON_FAIL(hr);

        if (hKey != NULL)
        {
            RegCloseKey( hKey );
        }

        
        // Disable the process isolation to allow the component to be loaded into shell
        dwVal = 1;
        
        hr = CreateRegistryKey(HKEY_LOCAL_MACHINE, AVF_BYTE_STREAM_HANDLER_CLSID_STR, &hKey);
        BREAK_ON_FAIL(hr);

        hr = RegSetValueEx(
                hKey,
                L"DisableProcessIsolation",
                0,
                REG_DWORD,
                (BYTE*)&dwVal,
                sizeof(dwVal)
                );
        BREAK_ON_FAIL(hr);

        if (hKey != NULL)
        {
            RegCloseKey( hKey );
        }
         
        // Specify which properties to display
        hr = CreateRegKeyAndStringValue(HKEY_CLASSES_ROOT, 
            L"SystemFileAssociations\\.avf",
            L"FullDetails",
            L"prop:System.Media.Duration",
            &hKey);
        BREAK_ON_FAIL(hr);
    }
    while(false);

    if (hKey != NULL)
    {
        RegCloseKey( hKey );
    }

    return hr;
}


HRESULT UnregisterByteStreamHandler(const GUID& guid, const wchar_t *sFileExtension)
{
    TCHAR szKey[MAX_PATH];
    OLECHAR szCLSID[256];

    DWORD result = 0;
    HRESULT hr = S_OK;

    do
    {
        // construct the subkey name.
        hr = swprintf_s(szKey, MAX_PATH, L"%s\\%s", REGKEY_MF_BYTESTREAM_HANDLERS, sFileExtension);
        BREAK_ON_FAIL(hr);

        // construct the CLSID name in canonical form.
        hr = StringFromGUID2(guid, szCLSID, 256);
        BREAK_ON_FAIL(hr);

        // Delete the CLSID entry under the subkey. 
        // Note: There might be multiple entries for this file extension, so we should not delete 
        // the entire subkey, just the entry for this CLSID.
        result = RegDeleteKeyValue(HKEY_LOCAL_MACHINE, szKey, szCLSID);
        if (result != ERROR_SUCCESS)
        {
            hr = HRESULT_FROM_WIN32(result);
            break;
        }
    }
    while(false);

    return hr;
}


HRESULT UnregisterPropertyHandler(void)
 {
    DWORD result = 0;
    HRESULT hr = S_OK;

    do
    {
        result = RegDeleteKey(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\.avf");
        if (result != ERROR_SUCCESS)
        {
            hr = HRESULT_FROM_WIN32(result);
            break;
        }

        result = RegDeleteKey(HKEY_CLASSES_ROOT, L"SystemFileAssociations\\.avf");
        if (result != ERROR_SUCCESS)
        {
            hr = HRESULT_FROM_WIN32(result);
            break;
        }
    }
    while(false);

    return hr;
}