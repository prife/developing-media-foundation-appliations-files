// Note: Most of the interesting code is in Player.cpp

#include "Common.h"
#include "Player.h"
#include "resource.h"
#include <new>

const wchar_t szTitle[] = L"BasicPlayback";
const wchar_t szWindowClass[] = L"MFBASICPLAYBACK";

BOOL        g_bRepaintClient = TRUE;            // Repaint the application client area?
CPlayer     *g_pPlayer = NULL;                  // Global player object.

// Note: After WM_CREATE is processed, g_pPlayer remains valid until the
// window is destroyed.

BOOL                CreateApplicationWindow(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

// Message handlers
LRESULT             OnCreateWindow(HWND hwnd);
void                OnPaint(HWND hwnd);
void                OnKeyPress(WPARAM key);
void                OnOpenFile(HWND parent);


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    MSG msg;

    ZeroMemory(&msg, sizeof(msg));

    // Perform application initialization.
    if (!CreateApplicationWindow(hInstance, nCmdShow))
    {
        return FALSE;
    }

    // main message pump for the application
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

BOOL CreateApplicationWindow(HINSTANCE hInst, int nCmdShow)
{
    HWND hwnd;
    WNDCLASSEX wcex = { 0 };

    // initialize the structure describing the window
    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.hCursor        = LoadCursor(hInst, MAKEINTRESOURCE(IDC_POINTER));
    wcex.hInstance      = hInst;
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_FILEMENU);
    wcex.lpszClassName  = szWindowClass;

    // register the class for the window
    if (RegisterClassEx(&wcex) == 0)
    {
        return FALSE;
    }

    // Create the application window.
    hwnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInst, NULL);
    if (hwnd == 0)
    {
        return FALSE;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    return TRUE;
}


//
// Main application message handling procedure - called by Windows to pass window-related
// messages to the application.
//
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HRESULT hr = S_OK;

    switch (message)
    {
    case WM_CREATE:
        return OnCreateWindow(hwnd);

    case WM_COMMAND:
        if(LOWORD(wParam) == ID_FILE_EXIT)
        {
            DestroyWindow(hwnd);
        }
        else if(LOWORD(wParam) == ID_FILE_OPENFILE)
        {
            OnOpenFile(hwnd);
        }
        else if(LOWORD(wParam) == ID_CONTROL_PLAY)
        {
            if(g_pPlayer != NULL)
            {
                g_pPlayer->Play();
            }
        }
        else if(LOWORD(wParam) == ID_CONTROL_PAUSE)
        {
            if(g_pPlayer != NULL)
            {
                g_pPlayer->Pause();
            }
        }
        else
        {   
            return DefWindowProc(hwnd, message, wParam, lParam);
        }
        break;

    case WM_PAINT:
        OnPaint(hwnd);
        break;

    case WM_ERASEBKGND:
        // Suppress window erasing, to reduce flickering while the video is playing.
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_CHAR:
        OnKeyPress(wParam);
        break;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}


//  Handles the WM_CREATE message.
//
//  hwnd: Handle to the video clipping window. (For this sample, the
//        video window is just the main application window.)

LRESULT OnCreateWindow(HWND hwnd)
{   
    HRESULT hr = S_OK;

    // Initialize the player object.
    g_pPlayer = new (std::nothrow) CPlayer(hwnd, &hr);

    // if player creation failed, 
    if(FAILED(hr))
    {
        delete g_pPlayer;
        g_pPlayer = NULL;
    }

    return 0;
}




//
//  Description: Handles WM_PAINT messages.  This has special behavior in order to handle cases where the
// video is paused and resized.
//
void OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    if (g_pPlayer && g_pPlayer->HasVideo())
    {
        // We have a player with an active topology and a video renderer that can paint the
        // window surface - ask the videor renderer (through the player) to redraw the surface.
        g_pPlayer->Repaint();
    }
    else
    {
        // The player topology hasn't been activated, which means there is no video renderer that 
        // repaint the surface.  This means we must do it ourselves.
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH) COLOR_WINDOW);
    }
    EndPaint(hwnd, &ps);
}

void OnKeyPress(WPARAM key)
{
    if (key == VK_SPACE)
    {
        // Space key toggles between running and paused
        if (g_pPlayer->GetState() == PlayerState_Started)
        {
            g_pPlayer->Pause();
        }
        else if (g_pPlayer->GetState() == PlayerState_Paused)
        {
            g_pPlayer->Play();
        }
    }
}



void OnOpenFile(HWND parent)
{
    OPENFILENAME ofn;       // common dialog box structure
    WCHAR  szFile[260];       // buffer for file name
//    HWND hwnd;              // owner window

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFile = szFile;
    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
    // use the contents of szFile to initialize itself.
    ofn.lpstrFile[0] = '\0\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"WMV\0*.wmv\0ASF\0*.asf\0AVI\0*.avi\0All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // Display the Open dialog box. 

    if (GetOpenFileName(&ofn)==TRUE) 
    {
        if(g_pPlayer != NULL)
        {
            g_pPlayer->OpenURL(ofn.lpstrFile);
        }
    }
}
