// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#include "MftClassFactory.h"


// Handle to the DLL
HMODULE g_hModule = NULL;

// module ref count
ULONG g_dllLockCount = 0;



HRESULT RegisterCOMObject(const TCHAR* pszCOMKeyLocation, const TCHAR *pszDescription);
HRESULT UnregisterObject(const TCHAR* pszCOMKeyLocation);




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




STDAPI DllCanUnloadNow(void)
{
    if(g_dllLockCount == 0) 
        return S_OK;
    else
        return S_FALSE;
}




//
// Function exposed out of the DLL used for registration of the COM object in
// the DLL using regsvr32 (e.g. "regsvr32 filename.dll")
//
STDAPI DllRegisterServer(void)
{
    HRESULT hr = S_OK;

    do
    {
        // Register the COM object CLSID so that CoCreateInstance() can be called to 
        // instantiate the MFT object.
        hr = RegisterCOMObject(IMAGE_INJECTOR_MFT_CLSID_STR, L"Image Injector MFT");
        BREAK_ON_FAIL(hr);

        // register the COM object as an MFT - store its information in the location that 
        // the MFTEnum() and MFTEnumEx() functions search, and put it under the right MFT 
        // category.
        hr = MFTRegister(
            CLSID_CImageInjectorMFT,    // CLSID of the MFT to register
            MFT_CATEGORY_VIDEO_EFFECT,  // Category under which the MFT will appear
            L"Image Injector MFT",      // Friendly name
            MFT_ENUM_FLAG_SYNCMFT,      // this is a synchronous MFT
            0,                          // zero pre-registered input types
            NULL,                       // no pre-registered input type array
            0,                          // zero pre-registered output types
            NULL,                       // no pre-registered output type array
            NULL);                      // no custom MFT attributes (used for merit)
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
    // Unregister the MFT object so that it isn't discoverable with the MFTEnum() function
    MFTUnregister(CLSID_CImageInjectorMFT);

    // Unregister the COM object itself
    UnregisterObject(IMAGE_INJECTOR_MFT_CLSID_STR);

    return S_OK;
}




STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppObj)
{
    HRESULT hr = E_OUTOFMEMORY; 
    *ppObj = NULL; 

    //if(riid != CLSID_CImageInjectorMFT)
    //    return CLASS_E_CLASSNOTAVAILABLE;
 
    MFTClassFactory* pClassFactory = new (std::nothrow) MFTClassFactory(); 
    if (pClassFactory != NULL)   
    { 
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
        // Create a new registry key under "HKLM\\Software\\Classes\\CLSID\\" + CLSID_CImageInjectorMFT.
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
// Deletes the registry entries for the COM object
//
HRESULT UnregisterObject(const TCHAR* pszCOMKeyLocation)
{
    // Delete the reg key tree for the COM object under 
    // "HKLM\\Software\\Classes\\CLSID\\" + CLSID_CImageInjectorMFT
    LONG lRes = RegDeleteTree(
        HKEY_LOCAL_MACHINE,         // root key - HKLM
        pszCOMKeyLocation);         // key name
    
    return HRESULT_FROM_WIN32(lRes);
}