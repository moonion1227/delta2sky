#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    int skyKey;
    int deltaBase;
    int modifier;
    int isPressed;
} KeyMapping;

KeyMapping mappings[] = {
    { '6', 'Z', 1, 0 },
    { '7', 'X', 1, 0 },
    { '8', 'C', 1, 0 },
    { '9', 'V', 1, 0 },
    { '0', 'B', 1, 0 },
    { 'Y', 'N', 1, 0 },
    { 'U', 'M', 1, 0 },
    { 'I', 'Z', 0, 0 },
    { 'O', 'X', 0, 0 },
    { 'P', 'C', 0, 0 },
    { 'H', 'V', 0, 0 },
    { 'J', 'B', 0, 0 },
    { 'K', 'N', 0, 0 },
    { 'L', 'M', 0, 0 },
    { VK_OEM_1, 'Z', 2, 0 }
};

int mappingCount = sizeof(mappings) / sizeof(mappings[0]);
volatile int g_running = 1;
int g_topmost = 0;

HWND g_btnTopmost = NULL;
HWND g_editExitKeys = NULL;
HWND g_btnSaveExit = NULL;
HWND g_btnCapture = NULL;

volatile int g_capturing = 0;
DWORD g_captureStart = 0;
DWORD g_cooldownUntil = 0;

#define MAX_EXIT_KEYS 64
int g_exitKeys[MAX_EXIT_KEYS];
int g_exitKeyCount = 0;

char g_configPath[MAX_PATH] = {0};

#define WM_CAPTURED_KEY (WM_APP + 1)

typedef struct {
    const char* name;
    int vk;
} SpecialKey;

SpecialKey specialKeys[] = {
    { "esc", VK_ESCAPE },
    { "escape", VK_ESCAPE },
    { "enter", VK_RETURN },
    { "return", VK_RETURN },
    { "space", VK_SPACE },
    { "spacebar", VK_SPACE },
    { "tab", VK_TAB },
    { "backspace", VK_BACK },
    { "delete", VK_DELETE },
    { "del", VK_DELETE },
    { "insert", VK_INSERT },
    { "ins", VK_INSERT },
    { "home", VK_HOME },
    { "end", VK_END },
    { "pageup", VK_PRIOR },
    { "pgup", VK_PRIOR },
    { "pagedown", VK_NEXT },
    { "pgdn", VK_NEXT },
    { "up", VK_UP },
    { "down", VK_DOWN },
    { "left", VK_LEFT },
    { "right", VK_RIGHT },
    { "shift", VK_SHIFT },
    { "ctrl", VK_CONTROL },
    { "control", VK_CONTROL },
    { "alt", VK_MENU },
    { "caps", VK_CAPITAL },
    { "capslock", VK_CAPITAL },
    { "f1", VK_F1 },   { "f2", VK_F2 },   { "f3", VK_F3 },
    { "f4", VK_F4 },   { "f5", VK_F5 },   { "f6", VK_F6 },
    { "f7", VK_F7 },   { "f8", VK_F8 },   { "f9", VK_F9 },
    { "f10", VK_F10 }, { "f11", VK_F11 }, { "f12", VK_F12 },
    { "num0", VK_NUMPAD0 }, { "num1", VK_NUMPAD1 },
    { "num2", VK_NUMPAD2 }, { "num3", VK_NUMPAD3 },
    { "num4", VK_NUMPAD4 }, { "num5", VK_NUMPAD5 },
    { "num6", VK_NUMPAD6 }, { "num7", VK_NUMPAD7 },
    { "num8", VK_NUMPAD8 }, { "num9", VK_NUMPAD9 },
    { NULL, 0 }
};

void keyDown(int vk) { keybd_event(vk, 0, 0, 0); }
void keyUp(int vk) { keybd_event(vk, 0, KEYEVENTF_KEYUP, 0); }

void mouseDown(int button) {
    INPUT input;
    memset(&input, 0, sizeof(input));
    input.type = INPUT_MOUSE;
    if (button == 0) input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    else input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    SendInput(1, &input, sizeof(INPUT));
}

void mouseUp(int button) {
    INPUT input;
    memset(&input, 0, sizeof(input));
    input.type = INPUT_MOUSE;
    if (button == 0) input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    else input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    SendInput(1, &input, sizeof(INPUT));
}

void noteOn(int baseKey, int modifier) {
    if (modifier == 1) mouseDown(0);
    else if (modifier == 2) mouseDown(1);
    Sleep(20);
    keyDown(baseKey);
}

void noteOff(int baseKey, int modifier) {
    keyUp(baseKey);
    if (modifier == 1) mouseUp(0);
    else if (modifier == 2) mouseUp(1);
}

void releaseAll() {
    int i;
    for (i = 0; i < mappingCount; i++) {
        if (mappings[i].isPressed) {
            keyUp(mappings[i].deltaBase);
            if (mappings[i].modifier == 1) mouseUp(0);
            else if (mappings[i].modifier == 2) mouseUp(1);
            mappings[i].isPressed = 0;
        }
    }
}

void InitConfigPath() {
    char exePath[MAX_PATH] = {0};
    char* p;
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    p = strrchr(exePath, '\\');
    if (p) {
        *(p + 1) = 0;
        strcpy(g_configPath, exePath);
        strcat(g_configPath, "config.ini");
    }
}

void SaveConfig(const char* exitKeys) {
    if (g_configPath[0] == 0) return;
    WritePrivateProfileStringA("Settings", "ExitKeys", exitKeys, g_configPath);
}

int LoadConfig(char* buf, int bufSize) {
    if (g_configPath[0] == 0) return 0;
    return GetPrivateProfileStringA("Settings", "ExitKeys", "", buf, bufSize, g_configPath);
}

void ToLowerStr(char* lower, const char* token, int maxlen) {
    int i = 0;
    while (token[i] && i < maxlen - 1) {
        char c = token[i];
        if (c >= 'A' && c <= 'Z') c = c + 32;
        lower[i] = c;
        i++;
    }
    lower[i] = 0;
}

int GetVkFromToken(const char* token) {
    char lower[32];
    int i;
    int len;

    ToLowerStr(lower, token, 32);
    len = (int)strlen(lower);

    for (i = 0; specialKeys[i].name; i++) {
        if (strcmp(lower, specialKeys[i].name) == 0) {
            return specialKeys[i].vk;
        }
    }

    if (len == 1) {
        SHORT vk = VkKeyScanA(token[0]);
        if (vk != -1) return LOBYTE(vk);
    }

    return -1;
}

void ParseExitKeys(const char* input) {
    char token[32];
    int ti = 0;
    int i;

    g_exitKeyCount = 0;

    for (i = 0; ; i++) {
        char c = input[i];
        int isSep = (c == 0 || c == ',' || c == ' ' ||
                     c == '\t' || c == ';');

        if (isSep) {
            if (ti > 0) {
                token[ti] = 0;
                {
                    int vk = GetVkFromToken(token);
                    if (vk != -1 && g_exitKeyCount < MAX_EXIT_KEYS) {
                        g_exitKeys[g_exitKeyCount++] = vk;
                    }
                }
                ti = 0;
            }
            if (c == 0) break;
        } else {
            if (ti < 31) token[ti++] = c;
        }
    }
}

const char* VkToName(int vk) {
    int i;
    static char single[2];

    for (i = 0; specialKeys[i].name; i++) {
        if (specialKeys[i].vk == vk) {
            return specialKeys[i].name;
        }
    }

    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        single[0] = (char)vk;
        single[1] = 0;
        return single;
    }

    return NULL;
}

/* ?????????????????(????????) */
int IsKeyAlreadyExist(const char* current, const char* newKey) {
    char token[32];
    int ti = 0;
    int i;
    char lowerNew[32];
    char lowerToken[32];

    ToLowerStr(lowerNew, newKey, 32);

    for (i = 0; ; i++) {
        char c = current[i];
        int isSep = (c == 0 || c == ',' || c == ' ' ||
                     c == '\t' || c == ';');

        if (isSep) {
            if (ti > 0) {
                token[ti] = 0;
                ToLowerStr(lowerToken, token, 32);
                if (strcmp(lowerToken, lowerNew) == 0) {
                    return 1;
                }
                ti = 0;
            }
            if (c == 0) break;
        } else {
            if (ti < 31) token[ti++] = c;
        }
    }
    return 0;
}

void ToggleTopmost(HWND hwnd) {
    g_topmost = !g_topmost;
    if (g_topmost) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE);
        SetWindowTextW(g_btnTopmost,
            L"\u5df2\u7f6e\u9876\uff08\u70b9\u51fb\u53d6\u6d88\uff09");
    } else {
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE);
        SetWindowTextW(g_btnTopmost,
            L"\u70b9\u51fb\u7f6e\u9876\u7a97\u53e3");
    }
}

DWORD WINAPI ListenThread(LPVOID lpParam) {
    HWND hwnd = (HWND)lpParam;
    while (g_running) {
        int i;

        if (g_capturing) {
            DWORD elapsed = GetTickCount() - g_captureStart;
            if (elapsed > 200) {
                int vk;
                for (vk = 0x08; vk < 0xFF; vk++) {
                    if (vk == VK_LBUTTON || vk == VK_RBUTTON ||
                        vk == VK_MBUTTON || vk == VK_XBUTTON1 ||
                        vk == VK_XBUTTON2) continue;
                    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) continue;
                    if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) continue;
                    if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) continue;

                    if (GetAsyncKeyState(vk) & 0x8000) {
                        g_capturing = 0;
                        g_cooldownUntil = GetTickCount() + 2000;
                        PostMessage(hwnd, WM_CAPTURED_KEY, (WPARAM)vk, 0);
                        break;
                    }
                }
            }
        } else {
            for (i = 0; i < mappingCount; i++) {
                int skyKey = mappings[i].skyKey;
                int isDown = (GetAsyncKeyState(skyKey) & 0x8000) != 0;

                if (isDown && !mappings[i].isPressed) {
                    mappings[i].isPressed = 1;
                    noteOn(mappings[i].deltaBase, mappings[i].modifier);
                } else if (!isDown && mappings[i].isPressed) {
                    mappings[i].isPressed = 0;
                    noteOff(mappings[i].deltaBase, mappings[i].modifier);
                }
            }

            if (GetTickCount() >= g_cooldownUntil) {
                for (i = 0; i < g_exitKeyCount; i++) {
                    if (GetAsyncKeyState(g_exitKeys[i]) & 0x8000) {
                        PostMessage(hwnd, WM_CLOSE, 0, 0);
                        break;
                    }
                }
            }
        }

        Sleep(5);
    }
    return 0;
}

void DrawTextLineW(HDC hdc, int x, int y, const wchar_t* text, COLORREF color) {
    SetTextColor(hdc, color);
    TextOutW(hdc, x, y, text, wcslen(text));
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            HFONT hFont;
            char cfg[512] = {0};

            g_btnTopmost = CreateWindowW(
                L"BUTTON",
                L"\u70b9\u51fb\u7f6e\u9876\u7a97\u53e3",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                20, 220, 380, 30,
                hwnd, (HMENU)1001, hInst, NULL
            );

            g_editExitKeys = CreateWindowW(
                L"EDIT", L"esc",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                20, 260, 170, 26,
                hwnd, (HMENU)1002, hInst, NULL
            );

            g_btnCapture = CreateWindowW(
                L"BUTTON",
                L"\u6355\u83b7\u6309\u952e",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                200, 260, 100, 26,
                hwnd, (HMENU)1004, hInst, NULL
            );

            g_btnSaveExit = CreateWindowW(
                L"BUTTON",
                L"\u4fdd\u5b58\u9000\u51fa\u952e",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                310, 260, 90, 26,
                hwnd, (HMENU)1003, hInst, NULL
            );

            hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
            SendMessage(g_btnTopmost, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_editExitKeys, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_btnCapture, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_btnSaveExit, WM_SETFONT, (WPARAM)hFont, TRUE);

            InitConfigPath();

            if (LoadConfig(cfg, 511) > 0) {
                wchar_t wbuf[512] = {0};
                MultiByteToWideChar(CP_ACP, 0, cfg, -1, wbuf, 512);
                SetWindowTextW(g_editExitKeys, wbuf);
                ParseExitKeys(cfg);
            } else {
                ParseExitKeys("esc");
                SaveConfig("esc");
            }

            CreateThread(NULL, 0, ListenThread, hwnd, 0, NULL);
            break;
        }

        case WM_CAPTURED_KEY: {
            int vk = (int)wParam;
            const char* name = VkToName(vk);
            if (name) {
                char curBuf[512] = {0};
                char newBuf[512] = {0};

                GetWindowTextA(g_editExitKeys, curBuf, 511);

                if (IsKeyAlreadyExist(curBuf, name)) {
                    SetWindowTextW(g_btnCapture, L"\u6355\u83b7\u6309\u952e");
                    MessageBoxW(hwnd,
                        L"\u8be5\u6309\u952e\u5df2\u5b58\u5728\uff0c\u65e0\u9700\u91cd\u590d\u6dfb\u52a0",
                        L"\u63d0\u793a", MB_OK | MB_ICONINFORMATION);
                    break;
                }

                if (strlen(curBuf) > 0) {
                    strcpy(newBuf, curBuf);
                    strcat(newBuf, " ");
                    strcat(newBuf, name);
                } else {
                    strcpy(newBuf, name);
                }

                {
                    wchar_t wbuf[512] = {0};
                    MultiByteToWideChar(CP_ACP, 0, newBuf, -1, wbuf, 512);
                    SetWindowTextW(g_editExitKeys, wbuf);
                }

                ParseExitKeys(newBuf);
                SaveConfig(newBuf);

                SetWindowTextW(g_btnCapture, L"\u6355\u83b7\u6309\u952e");
            } else {
                MessageBoxW(hwnd, L"\u65e0\u6cd5\u8bc6\u522b\u8be5\u6309\u952e",
                    L"\u63d0\u793a", MB_OK | MB_ICONWARNING);
                SetWindowTextW(g_btnCapture, L"\u6355\u83b7\u6309\u952e");
            }
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == 1001) {
                ToggleTopmost(hwnd);
            } else if (LOWORD(wParam) == 1003) {
                wchar_t wbuf[512] = {0};
                char buf[512] = {0};
                GetWindowTextW(g_editExitKeys, wbuf, 511);
                WideCharToMultiByte(CP_ACP, 0, wbuf, -1, buf, 511, NULL, NULL);
                ParseExitKeys(buf);
                SaveConfig(buf);
                MessageBoxW(hwnd,
                    L"\u9000\u51fa\u952e\u5df2\u66f4\u65b0\uff01",
                    L"\u63d0\u793a",
                    MB_OK | MB_ICONINFORMATION);
            } else if (LOWORD(wParam) == 1004) {
                g_capturing = 1;
                g_captureStart = GetTickCount();
                SetWindowTextW(g_btnCapture, L"\u8bf7\u6309\u952e...");
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HBRUSH hBrush;
            HFONT hBig, hOld;
            HPEN hPen, hOldPen;
            RECT rc;

            hBrush = CreateSolidBrush(RGB(30, 30, 40));
            FillRect(hdc, &ps.rcPaint, hBrush);
            DeleteObject(hBrush);
            SetBkMode(hdc, TRANSPARENT);

            hBig = CreateFontW(30, 0, 0, 0, FW_BOLD, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");

            hOld = (HFONT)SelectObject(hdc, hBig);

            GetClientRect(hwnd, &rc);
            rc.top = 70;
            rc.bottom = 130;
            SetTextColor(hdc, RGB(100, 220, 255));
            DrawTextW(hdc,
                L"\u6253\u5f00\u4e09\u89d2\u6d32\u5373\u53ef\u5f39\u7434",
                -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            hPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 80));
            hOldPen = (HPEN)SelectObject(hdc, hPen);
            MoveToEx(hdc, 20, 160, NULL);
            LineTo(hdc, 400, 160);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            SelectObject(hdc, hOld);
            DeleteObject(hBig);

            hBig = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
            SelectObject(hdc, hBig);
            DrawTextLineW(hdc, 20, 180,
                L"\u25cf \u7a0b\u5e8f\u8fd0\u884c\u4e2d",
                RGB(100, 220, 100));

            SelectObject(hdc, hOld);
            DeleteObject(hBig);

            EndPaint(hwnd, &ps);
            break;
        }

        case WM_DESTROY:
            g_running = 0;
            releaseAll();
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t* CLASS_NAME = L"SkyToDeltaWindow";
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 40));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    hwnd = CreateWindowExW(
        0, CLASS_NAME,
    	L"\u4e09\u89d2\u6d32\u53e3\u7434\u2192\u5149\u9047\u952e\u4f4d",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 430, 360,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
