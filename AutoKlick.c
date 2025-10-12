#include <windows.h>
#include <mmsystem.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#pragma comment(lib, "winmm.lib")

#define ID_HOLD_CHECK       101
#define ID_MAX_CLICK_CHECK  102
#define ID_SEND_KEYS_CHECK  103
#define ID_APPLY_BTN        104
#define ID_HELP_BTN         105
#define HOTKEY_ID           1

HWND hInterval, hButton, hKey, hMod, hStatus, hHoldCheck, hMaxClickCheck, hSendKeysCheck;
HWND hKeySeq, hCPSLabel;
HFONT hFont;
volatile bool clicking = false;
UINT hotkeyVK = VK_F6;
UINT hotkeyMod = MOD_CONTROL;
HANDLE hClickThread = NULL;
INPUT clickInput[2];
int cps = 0;

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

UINT ResolveVK(const char* key) {
    if (strlen(key) == 1) return VkKeyScan(key[0]) & 0xFF;
    if (_stricmp(key, "Enter") == 0) return VK_RETURN;
    if (_stricmp(key, "Tab") == 0) return VK_TAB;
    if (_stricmp(key, "Esc") == 0 || _stricmp(key, "Escape") == 0) return VK_ESCAPE;
    if (_stricmp(key, "Space") == 0) return VK_SPACE;
    if (_stricmp(key, "Backspace") == 0) return VK_BACK;
    if (_stricmp(key, "Delete") == 0 || _stricmp(key, "Del") == 0) return VK_DELETE;
    if (_stricmp(key, "Up") == 0) return VK_UP;
    if (_stricmp(key, "Down") == 0) return VK_DOWN;
    if (_stricmp(key, "Left") == 0) return VK_LEFT;
    if (_stricmp(key, "Right") == 0) return VK_RIGHT;
    if (_strnicmp(key, "F", 1) == 0 && isdigit(key[1])) {
        int f = atoi(key+1);
        if (f >= 1 && f <= 24) return VK_F1 + f - 1;
    }
    return 0;
}

void SendKeyCombo(const char* combo) {
    INPUT inputs[10];
    int count = 0;

    if (strstr(combo, "Ctrl")) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_CONTROL;
        inputs[count].ki.dwFlags = 0;
        count++;
    }
    if (strstr(combo, "Alt")) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_MENU;
        inputs[count].ki.dwFlags = 0;
        count++;
    }
    if (strstr(combo, "Shift")) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_SHIFT;
        inputs[count].ki.dwFlags = 0;
        count++;
    }

    const char* key = strrchr(combo, '+');
    if (key) key++;
    else key = combo;

    UINT vk = ResolveVK(key);
    if (vk == 0) return;

    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wVk = vk;
    inputs[count].ki.dwFlags = 0;
    count++;

    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wVk = vk;
    inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
    count++;

    for (int i = count - 3; i >= 0; i--) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = inputs[i].ki.wVk;
        inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
        count++;
    }

    SendInput(count, inputs, sizeof(INPUT));
}

DWORD WINAPI MouseClickLoop(LPVOID lpParam) {
    char buf[32];
    int interval = 1;
    GetWindowText(hInterval, buf, sizeof(buf));
    interval = atoi(buf);
    if (interval < 0) interval = 0;

    cps = 0;
    DWORD lastCPS = GetTickCount();

    while (clicking) {
        if (SendMessage(hHoldCheck, BM_GETCHECK, 0, 0) == BST_CHECKED &&
            !(GetAsyncKeyState(hotkeyVK) & 0x8000)) {
            clicking = false;
            SetWindowText(hStatus, "Status: Idle");
            break;
        }

        SendInput(2, clickInput, sizeof(INPUT));
        cps++;

        DWORD nowTick = GetTickCount();
        if (nowTick - lastCPS >= 1000) {
            char cpsText[32];
            sprintf(cpsText, "CPS: %d", cps);
            SetWindowText(hCPSLabel, cpsText);
            cps = 0;
            lastCPS = nowTick;
        }

        Sleep(SendMessage(hMaxClickCheck, BM_GETCHECK, 0, 0) == BST_CHECKED ? 0 : interval);
    }
    return 0;
}

DWORD WINAPI KeySequenceLoop(LPVOID lpParam) {
    char buf[128];
    int interval = 1;
    GetWindowText(hInterval, buf, sizeof(buf));
    interval = atoi(buf);
    if (interval < 0) interval = 0;

    cps = 0;
    DWORD lastCPS = GetTickCount();

    while (clicking) {
        if (SendMessage(hHoldCheck, BM_GETCHECK, 0, 0) == BST_CHECKED &&
            !(GetAsyncKeyState(hotkeyVK) & 0x8000)) {
            clicking = false;
            SetWindowText(hStatus, "Status: Idle");
            break;
        }

        char seq[256];
        GetWindowText(hKeySeq, seq, sizeof(seq));
        char* token = strtok(seq, "/");
        while (token && clicking) {
            SendKeyCombo(token);
            Sleep(interval);
            cps++;
            token = strtok(NULL, "/");
        }

        DWORD nowTick = GetTickCount();
        if (nowTick - lastCPS >= 1000) {
            char cpsText[32];
            sprintf(cpsText, "CPS: %d", cps);
            SetWindowText(hCPSLabel, cpsText);
            cps = 0;
            lastCPS = nowTick;
        }

        Sleep(1);
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        hFont = CreateFont(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                           ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY,FF_MODERN,"Consolas");

        hInterval = CreateWindow("EDIT","1",WS_VISIBLE|WS_CHILD|WS_BORDER|ES_NUMBER,
                                 120,10,100,25,hwnd,NULL,NULL,NULL);
        CreateWindow("STATIC","Interval (ms):",WS_VISIBLE|WS_CHILD,
                     10,10,100,25,hwnd,NULL,NULL,NULL);

        hButton = CreateWindow("COMBOBOX",NULL,WS_VISIBLE|WS_CHILD|CBS_DROPDOWNLIST,
                               120,45,100,100,hwnd,NULL,NULL,NULL);
        CreateWindow("STATIC","Button:",WS_VISIBLE|WS_CHILD,
                     10,45,100,25,hwnd,NULL,NULL,NULL);
        SendMessage(hButton,CB_ADDSTRING,0,(LPARAM)"Left");
        SendMessage(hButton,CB_ADDSTRING,0,(LPARAM)"Right");
        SendMessage(hButton,CB_ADDSTRING,0,(LPARAM)"Middle");
        SendMessage(hButton,CB_SETCURSEL,0,0);

        hMod = CreateWindow("EDIT","Ctrl",WS_VISIBLE|WS_CHILD|WS_BORDER,
                            120,80,100,25,hwnd,NULL,NULL,NULL);
        CreateWindow("STATIC","Modifiers:",WS_VISIBLE|WS_CHILD,
                     10,80,100,25,hwnd,NULL,NULL,NULL);

        hKey = CreateWindow("EDIT","F6",WS_VISIBLE|WS_CHILD|WS_BORDER,
                            120,115,100,25,hwnd,NULL,NULL,NULL);
        CreateWindow("STATIC","Key:",WS_VISIBLE|WS_CHILD,
                     10,115,100,25,hwnd,NULL,NULL,NULL);

        hKeySeq = CreateWindow("EDIT","Ctrl+C/Alt+V",WS_VISIBLE|WS_CHILD|WS_BORDER,
                               10,150,210,25,hwnd,NULL,NULL,NULL);
        CreateWindow("STATIC","Key Sequence:",WS_VISIBLE|WS_CHILD,
                     10,130,210,20,hwnd,NULL,NULL,NULL);

        hHoldCheck = CreateWindow("BUTTON","Hold to click",WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX,
                                  10,180,210,25,hwnd,(HMENU)ID_HOLD_CHECK,NULL,NULL);
        hMaxClickCheck = CreateWindow("BUTTON","Max Click (no delay)",WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX,
                                      10,210,210,25,hwnd,(HMENU)ID_MAX_CLICK_CHECK,NULL,NULL);
        hSendKeysCheck = CreateWindow("BUTTON","Send Keys",WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX,
                                      10,240,210,25,hwnd,(HMENU)ID_SEND_KEYS_CHECK,NULL,NULL);

        CreateWindow("BUTTON","Apply Hotkey",WS_VISIBLE|WS_CHILD,
                     10,270,210,30,hwnd,(HMENU)ID_APPLY_BTN,NULL,NULL);

        CreateWindow("BUTTON","?",WS_VISIBLE|WS_CHILD,
                     210,5,20,20,hwnd,(HMENU)ID_HELP_BTN,NULL,NULL);

        hStatus = CreateWindow("STATIC","Status: Idle",WS_VISIBLE|WS_CHILD,
                               10,310,210,25,hwnd,NULL,NULL,NULL);
        hCPSLabel = CreateWindow("STATIC","CPS: 0",WS_VISIBLE|WS_CHILD,
                                 10,340,210,25,hwnd,NULL,NULL,NULL);

        HWND ctrls[] = {hInterval,hButton,hMod,hKey,hKeySeq,hHoldCheck,hMaxClickCheck,hSendKeysCheck,hStatus,hCPSLabel};
        for (int i = 0; i < sizeof(ctrls)/sizeof(HWND); i++)
            SendMessage(ctrls[i],WM_SETFONT,(WPARAM)hFont,TRUE);

        RegisterHotKey(hwnd,HOTKEY_ID,hotkeyMod,hotkeyVK);
        int btn = SendMessage(hButton,CB_GETCURSEL,0,0);
        SetupClickInput(btn);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_APPLY_BTN) {
            UnregisterHotKey(hwnd, HOTKEY_ID);
            char modStr[64], keyStr[64];
            GetWindowText(hMod, modStr, sizeof(modStr));
            GetWindowText(hKey, keyStr, sizeof(keyStr));
            hotkeyMod = 0;
            if (strstr(modStr, "Ctrl"))  hotkeyMod |= MOD_CONTROL;
            if (strstr(modStr, "Alt"))   hotkeyMod |= MOD_ALT;
            if (strstr(modStr, "Shift")) hotkeyMod |= MOD_SHIFT;
            if (strstr(modStr, "Win"))   hotkeyMod |= MOD_WIN;
            hotkeyVK = ResolveVK(keyStr);
            RegisterHotKey(hwnd, HOTKEY_ID, hotkeyMod, hotkeyVK);

            int btn = SendMessage(hButton, CB_GETCURSEL, 0, 0);
            SetupClickInput(btn);

            BOOL sendKeysMode = SendMessage(hSendKeysCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            BOOL maxMode = SendMessage(hMaxClickCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            EnableWindow(hInterval, !maxMode);
            EnableWindow(hButton, !sendKeysMode);
            EnableWindow(hKeySeq, sendKeysMode);

            SetWindowText(hStatus, "Status: Idle");
        } else if (LOWORD(wParam) == ID_HELP_BTN) {
            MessageBox(hwnd,
                "AutoKlick Help:\n\n"
                "- Use 'Interval' to set delay in ms.\n"
                "- Choose mouse button or enter key sequences.\n"
                "- Use '/' to separate key combos (e.g., Ctrl+C/Alt+V).\n"
                "- 'Max Click' disables interval for fastest speed.\n"
                "- 'Hold to click' only clicks while hotkey is held.\n"
                "- 'Send Keys' enables keyboard emulation.\n"
                "- Click 'Apply Hotkey' to activate settings.",
                "AutoKlick Help", MB_OK | MB_ICONINFORMATION);
        }
        return 0;

    case WM_HOTKEY:
        if (clicking) {
            clicking = false;
            SetWindowText(hStatus, "Status: Idle");
        } else {
            clicking = true;
            BOOL sendKeysMode = SendMessage(hSendKeysCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SetWindowText(hStatus, sendKeysMode ? "Status: Sending Keys" : "Status: Clicking");
            if (hClickThread) CloseHandle(hClickThread);
            hClickThread = CreateThread(NULL, 0,
                sendKeysMode ? KeySequenceLoop : MouseClickLoop,
                NULL, 0, NULL);
        }
        return 0;

    case WM_DESTROY:
        UnregisterHotKey(hwnd, HOTKEY_ID);
        DeleteObject(hFont);
        clicking = false;
        if (hClickThread) {
            WaitForSingleObject(hClickThread, INFINITE);
            CloseHandle(hClickThread);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nCmd) {
    timeBeginPeriod(1); // Improve Sleep() resolution

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "AutoClicker";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow("AutoClicker", "AutoKlick",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 240, 410,
        NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nCmd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    timeEndPeriod(1); // Restore timer resolution
    return 0;
}

