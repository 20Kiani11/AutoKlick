// Simple C AutoClicker, i dont have the patience to put a license on this so do whatever you want with it (preferably refrence me, but who actually cares?)
// Should take around ~1.1MB to 3MB
// creator: 20KIANI11


#include <windows.h>
#include <stdbool.h>
#include <stdio.h>

#define ID_HOLD_CHECK 101
#define ID_APPLY_BTN  102
#define HOTKEY_ID     1

HWND hInterval, hButton, hKey, hMod, hStatus, hHoldCheck;
HFONT hFont;
volatile bool clicking = false;
bool holdMode = false;
UINT hotkeyVK = VK_F6;
UINT hotkeyMod = MOD_CONTROL;
HANDLE hClickThread = NULL;

INPUT clickInput[2];

void SetupClickInput(int button) {
    DWORD down, up;
    switch (button) {
        case 1: down = MOUSEEVENTF_RIGHTDOWN; up = MOUSEEVENTF_RIGHTUP; break;
        case 2: down = MOUSEEVENTF_MIDDLEDOWN; up = MOUSEEVENTF_MIDDLEUP; break;
        default: down = MOUSEEVENTF_LEFTDOWN; up = MOUSEEVENTF_LEFTUP; break;
    }
    clickInput[0].type = INPUT_MOUSE;
    clickInput[0].mi.dwFlags = down;
    clickInput[1].type = INPUT_MOUSE;
    clickInput[1].mi.dwFlags = up;
}

UINT ParseModifiers(const char* str) {
    UINT mod = 0;
    if (strstr(str, "Ctrl"))  mod |= MOD_CONTROL;
    if (strstr(str, "Alt"))   mod |= MOD_ALT;
    if (strstr(str, "Shift")) mod |= MOD_SHIFT;
    if (strstr(str, "Win"))   mod |= MOD_WIN;
    return mod;
}

UINT ParseKey(const char* str) {
    if (strlen(str) == 1) {
        SHORT vk = VkKeyScan(str[0]);
        return LOBYTE(vk);
    }
    if (str[0] == 'F') {
        int f = atoi(str+1);
        if (f >= 1 && f <= 24) return VK_F1 + f - 1;
    }
    return VK_F6;
}

DWORD WINAPI ClickLoop(LPVOID lpParam) {
    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    char buf[16];
    int interval = 1;
    while (clicking) {
        if (holdMode && !(GetAsyncKeyState(hotkeyVK) & 0x8000)) {
            clicking = false;
            SetWindowText(hStatus, "Status: Idle");
            break;
        }

        GetWindowText(hInterval, buf, sizeof(buf));
        interval = atoi(buf);
        if (interval < 0) interval = 0;

        QueryPerformanceCounter(&now);
        double elapsed = (double)(now.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
        if (elapsed >= interval) {
            SendInput(2, clickInput, sizeof(INPUT));
            QueryPerformanceCounter(&start);
        }
        if (interval == 0) Sleep(0); else Sleep(1);
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        hFont = CreateFont(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                           ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY,FF_MODERN,"Consolas");

        CreateWindow("STATIC","Interval (ms):",WS_VISIBLE|WS_CHILD,
                     10,10,100,25,hwnd,NULL,NULL,NULL);
        hInterval = CreateWindow("EDIT","1",WS_VISIBLE|WS_CHILD|WS_BORDER|ES_NUMBER,
                                 120,10,100,25,hwnd,NULL,NULL,NULL);

        CreateWindow("STATIC","Button:",WS_VISIBLE|WS_CHILD,
                     10,45,100,25,hwnd,NULL,NULL,NULL);
        hButton = CreateWindow("COMBOBOX",NULL,WS_VISIBLE|WS_CHILD|CBS_DROPDOWNLIST,
                               120,45,100,100,hwnd,NULL,NULL,NULL);
        SendMessage(hButton,CB_ADDSTRING,0,(LPARAM)"Left");
        SendMessage(hButton,CB_ADDSTRING,0,(LPARAM)"Right");
        SendMessage(hButton,CB_ADDSTRING,0,(LPARAM)"Middle");
        SendMessage(hButton,CB_SETCURSEL,0,0);

        CreateWindow("STATIC","Modifiers:",WS_VISIBLE|WS_CHILD,
                     10,80,100,25,hwnd,NULL,NULL,NULL);
        hMod = CreateWindow("EDIT","Ctrl",WS_VISIBLE|WS_CHILD|WS_BORDER,
                            120,80,100,25,hwnd,NULL,NULL,NULL);

        CreateWindow("STATIC","Key:",WS_VISIBLE|WS_CHILD,
                     10,115,100,25,hwnd,NULL,NULL,NULL);
        hKey = CreateWindow("EDIT","F6",WS_VISIBLE|WS_CHILD|WS_BORDER,
                            120,115,100,25,hwnd,NULL,NULL,NULL);

        hHoldCheck = CreateWindow("BUTTON","Hold to click",WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX,
                                  10,150,210,25,hwnd,(HMENU)ID_HOLD_CHECK,NULL,NULL);

        CreateWindow("BUTTON","Apply Hotkey",WS_VISIBLE|WS_CHILD,
                     10,185,210,30,hwnd,(HMENU)ID_APPLY_BTN,NULL,NULL);

        hStatus = CreateWindow("STATIC","Status: Idle",WS_VISIBLE|WS_CHILD,
                               10,220,210,25,hwnd,NULL,NULL,NULL);

        HWND ctrls[] = {hInterval,hButton,hMod,hKey,hHoldCheck,hStatus};
        for (int i = 0; i < sizeof(ctrls)/sizeof(HWND); i++)
            SendMessage(ctrls[i],WM_SETFONT,(WPARAM)hFont,TRUE);

        RegisterHotKey(hwnd,HOTKEY_ID,hotkeyMod,hotkeyVK);
        SetupClickInput(0);
        return 0;
    }

    case WM_HOTKEY:
        if (clicking) {
            clicking = false;
            SetWindowText(hStatus,"Status: Idle");
        } else {
            holdMode = SendMessage(hHoldCheck,BM_GETCHECK,0,0) == BST_CHECKED;
            clicking = true;
            SetWindowText(hStatus, holdMode ? "Status: Holding" : "Status: Clicking");
            if (hClickThread) CloseHandle(hClickThread);
            hClickThread = CreateThread(NULL,0,ClickLoop,NULL,0,NULL);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_APPLY_BTN) {
            UnregisterHotKey(hwnd,HOTKEY_ID);
            char modStr[64], keyStr[64];
            GetWindowText(hMod,modStr,sizeof(modStr));
            GetWindowText(hKey,keyStr,sizeof(keyStr));
            hotkeyMod = ParseModifiers(modStr);
            hotkeyVK  = ParseKey(keyStr);
            RegisterHotKey(hwnd,HOTKEY_ID,hotkeyMod,hotkeyVK);

            int btn = SendMessage(hButton,CB_GETCURSEL,0,0);
            SetupClickInput(btn);
            SetWindowText(hStatus,"Status: Idle");
        }
        return 0;

    case WM_DESTROY:
        UnregisterHotKey(hwnd,HOTKEY_ID);
        DeleteObject(hFont);
        clicking = false;
        if (hClickThread) {
            WaitForSingleObject(hClickThread, INFINITE);
            CloseHandle(hClickThread);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd,msg,wParam,lParam);
}

int WINAPI WinMain(HINSTANCE hInst,HINSTANCE hPrev,LPSTR lpCmd,int nCmd) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "AutoClicker";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow("AutoClicker","AutoKlick",
                             WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
                             CW_USEDEFAULT,CW_USEDEFAULT,240,300,
                             NULL,NULL,hInst,NULL);
    ShowWindow(hwnd,nCmd);

    MSG msg;
    while (GetMessage(&msg,NULL,0,0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
