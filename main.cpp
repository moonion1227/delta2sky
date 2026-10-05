#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ROWS 3
#define COLS 5
#define MAX_MAPPINGS 15

#define GRID_X 70
#define GRID_Y 45
#define CELL_W 78
#define CELL_H 75

typedef struct {
    int skyKey;
    int deltaBase;
    int modifier;
    int isPressed;
} KeyMapping;

/* 默认映射表（重置时恢复用） */
KeyMapping defaultMappings[] = {
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

KeyMapping mappings[MAX_MAPPINGS];
int mappingCount = sizeof(defaultMappings) / sizeof(defaultMappings[0]);

/* 音符名：第 1 个字符是数字，第 2 个字符可选：'.' 表示低音（点在下方），'\'' 表示高音（点在上方） */
const wchar_t* g_noteNames[ROWS][COLS] = {
    { L"1.", L"2.", L"3.", L"4.", L"5." },
    { L"6.", L"7.", L"1",  L"2",  L"3"  },
    { L"4",  L"5",  L"6",  L"7",  L"1'" }
};

volatile int g_running = 1;
int g_topmost = 0;
volatile int g_enabled = 0;

HWND g_hwndMain = NULL;
HWND g_btnTopmost = NULL;
HWND g_btnToggle = NULL;
HWND g_btnCapture = NULL;
HWND g_btnResetExit = NULL;

volatile int g_capturing = 0;
DWORD g_captureStart = 0;
DWORD g_cooldownUntil = 0;

int g_selectedCell = -1;
int g_selectedExitKey = -1;

#define MAX_EXIT_KEYS 64
int g_exitKeys[MAX_EXIT_KEYS];
int g_exitKeyCount = 0;
int g_lastCapturedExitKey = VK_ESCAPE;

/* 退出键点击区域 */
RECT g_exitKeyRects[MAX_EXIT_KEYS];
int g_exitKeyRectIndexes[MAX_EXIT_KEYS];
int g_exitKeyRectCount = 0;

/* 退出键删除"×"点击区域 */
RECT g_exitKeyDeleteRects[MAX_EXIT_KEYS];
int g_exitKeyDeleteRectIndexes[MAX_EXIT_KEYS];
int g_exitKeyDeleteRectCount = 0;

int g_savedWindowX = -99999;
int g_savedWindowY = -99999;
int g_savedTopmost = 0;

char g_configPath[MAX_PATH] = {0};
char g_statusText[256] = "\u5df2\u6682\u505c\uff0c\u70b9\u51fb\u5f00\u59cb\u542f\u7528";

#define WM_CAPTURED_KEY       (WM_APP + 1)
#define WM_NOTE_STATE_CHANGED (WM_APP + 2)

typedef struct {
    const char* name;
    int vk;
} SpecialKey;

SpecialKey specialKeys[] = {
    { "esc", VK_ESCAPE },
    { "enter", VK_RETURN },
    { "space", VK_SPACE },
    { "tab", VK_TAB },
    { "backspace", VK_BACK },
    { "delete", VK_DELETE },
    { "insert", VK_INSERT },
    { "home", VK_HOME },
    { "end", VK_END },
    { "pageup", VK_PRIOR },
    { "pagedown", VK_NEXT },
    { "up", VK_UP },
    { "down", VK_DOWN },
    { "left", VK_LEFT },
    { "right", VK_RIGHT },
    { "f1", VK_F1 },   { "f2", VK_F2 },   { "f3", VK_F3 },
    { "f4", VK_F4 },   { "f5", VK_F5 },   { "f6", VK_F6 },
    { "f7", VK_F7 },   { "f8", VK_F8 },   { "f9", VK_F9 },
    { "f10", VK_F10 }, { "f11", VK_F11 }, { "f12", VK_F12 },
    { NULL, 0 }
};

void SetStatus(const char* text) {
    strcpy(g_statusText, text);
    if (g_hwndMain) InvalidateRect(g_hwndMain, NULL, TRUE);
}

const char* GetKeyName(int vk) {
    static char buf[32];
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) { sprintf(buf, "\u5c0f%d", vk - VK_NUMPAD0); return buf; }
    switch (vk) {
        case VK_ADD: return "\u5c0f+";
        case VK_SUBTRACT: return "\u5c0f-";
        case VK_MULTIPLY: return "\u5c0f*";
        case VK_DIVIDE: return "\u5c0f/";
        case VK_OEM_1: return ";";
        case VK_OEM_2: return "/";
        case VK_OEM_3: return "`";
        case VK_OEM_4: return "[";
        case VK_OEM_5: return "\\";
        case VK_OEM_6: return "]";
        case VK_OEM_7: return "'";
        case VK_OEM_COMMA: return ",";
        case VK_OEM_PERIOD: return ".";
        case VK_OEM_MINUS: return "-";
        case VK_OEM_PLUS: return "=";
        case VK_ESCAPE: return "ESC";
        case VK_RETURN: return "\u56de\u8f66";
        case VK_SPACE: return "\u7a7a\u683c";
        case VK_TAB: return "Tab";
        case VK_BACK: return "\u9000\u683c";
        case VK_DELETE: return "Del";
        case VK_INSERT: return "Ins";
        case VK_HOME: return "Home";
        case VK_END: return "End";
        case VK_PRIOR: return "PgUp";
        case VK_NEXT: return "PgDn";
        case VK_UP: return "\u4e0a";
        case VK_DOWN: return "\u4e0b";
        case VK_LEFT: return "\u5de6";
        case VK_RIGHT: return "\u53f3";
    }
    if (vk >= VK_F1 && vk <= VK_F12) { sprintf(buf, "F%d", vk - VK_F1 + 1); return buf; }
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) { buf[0] = (char)vk; buf[1] = 0; return buf; }
    sprintf(buf, "0x%02X", vk);
    return buf;
}

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

void releaseNote(int idx) {
    if (idx < 0 || idx >= mappingCount) return;
    if (mappings[idx].isPressed) {
        keyUp(mappings[idx].deltaBase);
        if (mappings[idx].modifier == 1) mouseUp(0);
        else if (mappings[idx].modifier == 2) mouseUp(1);
        mappings[idx].isPressed = 0;
    }
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

void SaveConfig() {
    char buf[2048];
    int i;
    if (g_configPath[0] == 0) return;

    buf[0] = 0;
    for (i = 0; i < g_exitKeyCount; i++) {
        if (i > 0) strcat(buf, " ");
        strcat(buf, GetKeyName(g_exitKeys[i]));
    }
    WritePrivateProfileStringA("Settings", "ExitKeys", buf, g_configPath);

    buf[0] = 0;
    for (i = 0; i < mappingCount; i++) {
        char tmp[16];
        if (i > 0) strcat(buf, " ");
        sprintf(tmp, "%d", mappings[i].skyKey);
        strcat(buf, tmp);
    }
    WritePrivateProfileStringA("Settings", "NoteKeys", buf, g_configPath);
}

void SaveWindowState() {
    char posbuf[64];
    RECT rc;
    if (g_configPath[0] == 0) return;

    if (g_hwndMain && !IsIconic(g_hwndMain) && GetWindowRect(g_hwndMain, &rc)) {
        sprintf(posbuf, "%ld,%ld", rc.left, rc.top);
        WritePrivateProfileStringA("Settings", "WindowPos", posbuf, g_configPath);
    }

    WritePrivateProfileStringA("Settings", "Topmost", g_topmost ? "1" : "0", g_configPath);
}

void LoadConfig() {
    char buf[2048];
    int i;

    buf[0] = 0;
    GetPrivateProfileStringA("Settings", "ExitKeys", "", buf, 2047, g_configPath);

    g_exitKeyCount = 0;
    if (buf[0] != 0) {
        char token[32];
        int ti = 0;
        for (i = 0; ; i++) {
            char c = buf[i];
            int isSep = (c == 0 || c == ' ' || c == ',' || c == '\t' || c == ';');
            if (isSep) {
                if (ti > 0) {
                    int vk = -1;
                    int j;
                    token[ti] = 0;
                    for (j = 0; specialKeys[j].name; j++) {
                        if (_stricmp(specialKeys[j].name, token) == 0) {
                            vk = specialKeys[j].vk;
                            break;
                        }
                    }
                    if (vk == -1 && token[0] == '0' &&
                        (token[1] == 'x' || token[1] == 'X')) {
                        vk = (int)strtol(token + 2, NULL, 16);
                    }
                    if (vk == -1 && strlen(token) == 1) {
                        SHORT sh = VkKeyScanA(token[0]);
                        if (sh != -1) vk = LOBYTE(sh);
                    }
                    if (vk > 0 && g_exitKeyCount < MAX_EXIT_KEYS) {
                        g_exitKeys[g_exitKeyCount++] = vk;
                    }
                    ti = 0;
                }
                if (c == 0) break;
            } else {
                if (ti < 31) token[ti++] = c;
            }
        }
    }
    if (g_exitKeyCount == 0) {
        g_exitKeys[0] = VK_ESCAPE;
        g_exitKeyCount = 1;
    }
    g_lastCapturedExitKey = g_exitKeys[g_exitKeyCount - 1];

    buf[0] = 0;
    GetPrivateProfileStringA("Settings", "NoteKeys", "", buf, 2047, g_configPath);
    if (buf[0] != 0) {
        char* p = buf;
        int idx = 0;
        while (*p && idx < mappingCount) {
            char* endp;
            long v;
            while (*p == ' ' || *p == '\t' || *p == ',') p++;
            if (!*p) break;
            v = strtol(p, &endp, 10);
            if (endp == p) { p++; continue; }
            p = endp;
            if (v > 0 && v < 256) {
                mappings[idx].skyKey = (int)v;
                mappings[idx].isPressed = 0;
                idx++;
            }
        }
    }

    buf[0] = 0;
    GetPrivateProfileStringA("Settings", "WindowPos", "", buf, 2047, g_configPath);
    if (buf[0] != 0) {
        int x, y;
        if (sscanf(buf, "%d,%d", &x, &y) == 2) {
            if (x > -10000 && x < 10000 && y > -10000 && y < 10000) {
                g_savedWindowX = x;
                g_savedWindowY = y;
            }
        }
    }

    g_savedTopmost = GetPrivateProfileIntA("Settings", "Topmost", 0, g_configPath);

    SaveConfig();
}

void ApplyTopmost(HWND hwnd, int on) {
    g_topmost = on ? 1 : 0;
    if (g_topmost) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE);
        SetWindowTextW(g_btnTopmost, L"\u53d6\u6d88\u7f6e\u9876");
    } else {
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE);
        SetWindowTextW(g_btnTopmost, L"\u7a97\u53e3\u7f6e\u9876");
    }
}

void ToggleTopmost(HWND hwnd) {
    ApplyTopmost(hwnd, !g_topmost);
    SaveWindowState();
}

void ClearSelection(HWND hwnd) {
    if (g_selectedCell != -1 || g_selectedExitKey != -1) {
        g_selectedCell = -1;
        g_selectedExitKey = -1;
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void CancelCapture() {
    if (g_capturing) {
        g_capturing = 0;
        if (g_btnCapture) {
            SetWindowTextW(g_btnCapture, L"\u6355\u83b7\u6309\u952e");
        }
    }
}

void RemoveExitKey(int idx) {
    int k;
    if (idx < 0 || idx >= g_exitKeyCount) return;
    for (k = idx; k < g_exitKeyCount - 1; k++) {
        g_exitKeys[k] = g_exitKeys[k + 1];
    }
    g_exitKeyCount--;
    if (g_exitKeyCount == 0) {
        g_exitKeys[0] = g_lastCapturedExitKey;
        g_exitKeyCount = 1;
    }
    if (g_selectedExitKey == idx) {
        g_selectedExitKey = -1;
    } else if (g_selectedExitKey > idx) {
        g_selectedExitKey--;
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
            if (g_enabled) {
                for (i = 0; i < mappingCount; i++) {
                    int skyKey = mappings[i].skyKey;
                    int isDown = (GetAsyncKeyState(skyKey) & 0x8000) != 0;

                    if (isDown && !mappings[i].isPressed) {
                        mappings[i].isPressed = 1;
                        noteOn(mappings[i].deltaBase, mappings[i].modifier);
                        PostMessage(hwnd, WM_NOTE_STATE_CHANGED, 0, 0);
                    } else if (!isDown && mappings[i].isPressed) {
                        mappings[i].isPressed = 0;
                        noteOff(mappings[i].deltaBase, mappings[i].modifier);
                        PostMessage(hwnd, WM_NOTE_STATE_CHANGED, 0, 0);
                    }
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            HFONT hFont;

            g_hwndMain = hwnd;

            g_btnToggle = CreateWindowW(L"BUTTON", L"\u5f00\u59cb",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 12, 375, 118, 45, hwnd, (HMENU)1005, hInst, NULL);
            g_btnCapture = CreateWindowW(L"BUTTON", L"\u6355\u83b7\u6309\u952e",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 134, 375, 118, 45, hwnd, (HMENU)1004, hInst, NULL);
            g_btnResetExit = CreateWindowW(L"BUTTON", L"\u91cd\u7f6e\u6309\u952e",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 256, 375, 118, 45, hwnd, (HMENU)1006, hInst, NULL);
            g_btnTopmost = CreateWindowW(L"BUTTON", L"\u7a97\u53e3\u7f6e\u9876",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 378, 375, 118, 45, hwnd, (HMENU)1001, hInst, NULL);

            hFont = CreateFontW(22, 0, 0, 0, FW_BOLD, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
            SendMessage(g_btnToggle, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_btnCapture, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_btnResetExit, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_btnTopmost, WM_SETFONT, (WPARAM)hFont, TRUE);

            memcpy(mappings, defaultMappings, sizeof(defaultMappings));
            g_selectedCell = -1;
            g_selectedExitKey = -1;

            InitConfigPath();
            LoadConfig();

            if (g_savedWindowX != -99999 && g_savedWindowY != -99999) {
                SetWindowPos(hwnd, NULL, g_savedWindowX, g_savedWindowY, 0, 0,
                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }

            if (g_savedTopmost) {
                ApplyTopmost(hwnd, 1);
            }

            {
                HANDLE hThread = CreateThread(NULL, 0, ListenThread, hwnd, 0, NULL);
                if (hThread) CloseHandle(hThread);
            }
            break;
        }

        case WM_NOTE_STATE_CHANGED: {
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }

        case WM_EXITSIZEMOVE: {
            SaveWindowState();
            break;
        }

        case WM_LBUTTONDOWN: {
            int mx = (short)LOWORD(lParam);
            int my = (short)HIWORD(lParam);
            int k;

            for (k = 0; k < g_exitKeyDeleteRectCount; k++) {
                RECT xr = g_exitKeyDeleteRects[k];
                if (mx >= xr.left && mx < xr.right && my >= xr.top && my < xr.bottom) {
                    int j = g_exitKeyDeleteRectIndexes[k];
                    RemoveExitKey(j);
                    SaveConfig();
                    SetStatus("\u5df2\u5220\u9664\u9000\u51fa\u952e");
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
            }

            for (k = 0; k < g_exitKeyRectCount; k++) {
                RECT kr = g_exitKeyRects[k];
                if (mx >= kr.left && mx < kr.right && my >= kr.top && my < kr.bottom) {
                    int j = g_exitKeyRectIndexes[k];
                    g_selectedExitKey = (g_selectedExitKey == j) ? -1 : j;
                    g_selectedCell = -1;
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
            }

            if (mx >= GRID_X && my >= GRID_Y) {
                int c = (mx - GRID_X) / CELL_W;
                int r = (my - GRID_Y) / CELL_H;
                if (c >= 0 && c < COLS && r >= 0 && r < ROWS) {
                    int idx = r * COLS + c;
                    if (idx < mappingCount) {
                        g_selectedCell = (g_selectedCell == idx) ? -1 : idx;
                        g_selectedExitKey = -1;
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                }
            }
            break;
        }

        case WM_RBUTTONDOWN: {
            if (g_selectedCell != -1 || g_selectedExitKey != -1) {
                g_selectedCell = -1;
                g_selectedExitKey = -1;
                InvalidateRect(hwnd, NULL, TRUE);
                SetStatus("\u5df2\u53d6\u6d88\u9009\u4e2d");
            }
            break;
        }

        case WM_CAPTURED_KEY: {
            int vk = (int)wParam;
            int j;
            int conflict = 0;

            SetWindowTextW(g_btnCapture, L"\u6355\u83b7\u6309\u952e");

            if (vk == 0) {
                MessageBoxW(hwnd, L"\u65e0\u6cd5\u8bc6\u522b\u8be5\u6309\u952e",
                    L"\u63d0\u793a", MB_OK | MB_ICONWARNING);
                break;
            }

            if (g_selectedCell >= 0 && g_selectedCell < mappingCount) {
                for (j = 0; j < mappingCount; j++) {
                    if (j != g_selectedCell && mappings[j].skyKey == vk) {
                        MessageBoxW(hwnd,
                            L"\u8be5\u6309\u952e\u5df2\u88ab\u5176\u4ed6\u97f3\u7b26\u952e\u5360\u7528\uff0c\u8bf7\u6362\u4e00\u4e2a",
                            L"\u51b2\u7a81\u63d0\u793a", MB_OK | MB_ICONWARNING);
                        conflict = 1;
                        break;
                    }
                }
                if (!conflict) {
                    for (j = 0; j < g_exitKeyCount; j++) {
                        if (g_exitKeys[j] == vk) {
                            MessageBoxW(hwnd,
                                L"\u8be5\u6309\u952e\u5df2\u5728\u9000\u51fa\u952e\u5217\u8868\u4e2d\uff0c\u8bf7\u6362\u4e00\u4e2a",
                                L"\u51b2\u7a81\u63d0\u793a", MB_OK | MB_ICONWARNING);
                            conflict = 1;
                            break;
                        }
                    }
                }
                if (!conflict) {
                    releaseNote(g_selectedCell);
                    mappings[g_selectedCell].skyKey = vk;
                    SaveConfig();
                    SetStatus("\u5df2\u4fee\u6539\u9009\u4e2d\u97f3\u7b26\u952e");
                }
            } else if (g_selectedExitKey >= 0 && g_selectedExitKey < g_exitKeyCount) {
                if (g_exitKeys[g_selectedExitKey] == vk) {
                    g_selectedExitKey = -1;
                } else {
                    for (j = 0; j < mappingCount; j++) {
                        if (mappings[j].skyKey == vk) {
                            MessageBoxW(hwnd,
                                L"\u8be5\u6309\u952e\u5df2\u88ab\u97f3\u7b26\u952e\u5360\u7528\uff0c\u8bf7\u6362\u4e00\u4e2a",
                                L"\u51b2\u7a81\u63d0\u793a", MB_OK | MB_ICONWARNING);
                            conflict = 1;
                            break;
                        }
                    }
                    if (!conflict) {
                        for (j = 0; j < g_exitKeyCount; j++) {
                            if (j != g_selectedExitKey && g_exitKeys[j] == vk) {
                                MessageBoxW(hwnd,
                                    L"\u8be5\u6309\u952e\u5df2\u5728\u9000\u51fa\u952e\u5217\u8868\u4e2d\uff0c\u8bf7\u6362\u4e00\u4e2a",
                                    L"\u51b2\u7a81\u63d0\u793a", MB_OK | MB_ICONWARNING);
                                conflict = 1;
                                break;
                            }
                        }
                    }
                    if (!conflict) {
                        g_exitKeys[g_selectedExitKey] = vk;
                        g_lastCapturedExitKey = vk;
                        SaveConfig();
                        SetStatus("\u5df2\u4fee\u6539\u9009\u4e2d\u9000\u51fa\u952e");
                    }
                }
            } else {
                for (j = 0; j < g_exitKeyCount; j++) {
                    if (g_exitKeys[j] == vk) {
                        MessageBoxW(hwnd,
                            L"\u8be5\u6309\u952e\u5df2\u5728\u9000\u51fa\u952e\u5217\u8868\u4e2d",
                            L"\u63d0\u793a", MB_OK | MB_ICONINFORMATION);
                        conflict = 1;
                        break;
                    }
                }
                if (!conflict) {
                    for (j = 0; j < mappingCount; j++) {
                        if (mappings[j].skyKey == vk) {
                            MessageBoxW(hwnd,
                                L"\u8be5\u6309\u952e\u5df2\u88ab\u97f3\u7b26\u952e\u5360\u7528\uff0c\u8bf7\u5148\u9009\u4e2d\u5bf9\u5e94\u683c\u5b50\u518d\u4fee\u6539",
                                L"\u51b2\u7a81\u63d0\u793a", MB_OK | MB_ICONWARNING);
                            conflict = 1;
                            break;
                        }
                    }
                }
                if (!conflict) {
                    if (g_exitKeyCount < MAX_EXIT_KEYS) {
                        g_exitKeys[g_exitKeyCount++] = vk;
                        g_lastCapturedExitKey = vk;
                        SaveConfig();
                        SetStatus("\u5df2\u6dfb\u52a0\u9000\u51fa\u952e");
                    }
                }
            }

            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == 1001) {
                if (g_capturing) { CancelCapture(); SetStatus("\u5df2\u53d6\u6d88\u6355\u83b7"); }
                ClearSelection(hwnd);
                ToggleTopmost(hwnd);
            } else if (LOWORD(wParam) == 1004) {
                if (g_capturing) {
                    CancelCapture();
                    SetStatus("\u5df2\u53d6\u6d88\u6355\u83b7");
                } else {
                    g_capturing = 1;
                    g_captureStart = GetTickCount();
                    SetWindowTextW(g_btnCapture, L"\u8bf7\u6309\u952e...");
                    SetStatus("\u7b49\u5f85\u6309\u952e\u8f93\u5165\uff0c\u518d\u6b21\u70b9\u51fb\u53d6\u6d88");
                }
                InvalidateRect(hwnd, NULL, TRUE);
            } else if (LOWORD(wParam) == 1005) {
                if (g_capturing) { CancelCapture(); SetStatus("\u5df2\u53d6\u6d88\u6355\u83b7"); }
                ClearSelection(hwnd);
                g_enabled = !g_enabled;
                if (!g_enabled) {
                    releaseAll();
                    SetWindowTextW(g_btnToggle, L"\u5f00\u59cb");
                    SetStatus("\u5df2\u6682\u505c\uff0c\u6309\u952e\u6620\u5c04\u4e0d\u751f\u6548");
                } else {
                    SetWindowTextW(g_btnToggle, L"\u6682\u505c");
                    SetStatus("\u8fd0\u884c\u4e2d");
                }
                InvalidateRect(hwnd, NULL, TRUE);
            } else if (LOWORD(wParam) == 1006) {
                if (g_capturing) { CancelCapture(); SetStatus("\u5df2\u53d6\u6d88\u6355\u83b7"); }
                if (g_selectedCell >= 0 && g_selectedCell < mappingCount) {
                    int idx = g_selectedCell;
                    releaseNote(idx);
                    mappings[idx].skyKey = defaultMappings[idx].skyKey;
                    mappings[idx].deltaBase = defaultMappings[idx].deltaBase;
                    mappings[idx].modifier = defaultMappings[idx].modifier;
                    mappings[idx].isPressed = 0;
                    SaveConfig();
                    SetStatus("\u5df2\u91cd\u7f6e\u9009\u4e2d\u97f3\u7b26\u952e");
                    g_selectedCell = -1;
                } else if (g_selectedExitKey >= 0 && g_selectedExitKey < g_exitKeyCount) {
                    RemoveExitKey(g_selectedExitKey);
                    SaveConfig();
                    SetStatus("\u5df2\u79fb\u9664\u9009\u4e2d\u9000\u51fa\u952e");
                } else {
                    int i;
                    releaseAll();
                    for (i = 0; i < mappingCount; i++) {
                        mappings[i].skyKey = defaultMappings[i].skyKey;
                        mappings[i].deltaBase = defaultMappings[i].deltaBase;
                        mappings[i].modifier = defaultMappings[i].modifier;
                        mappings[i].isPressed = 0;
                    }
                    g_exitKeys[0] = g_lastCapturedExitKey;
                    g_exitKeyCount = 1;
                    g_selectedCell = -1;
                    g_selectedExitKey = -1;
                    SaveConfig();
                    SetStatus("\u6240\u6709\u6309\u952e\u5df2\u91cd\u7f6e\u4e3a\u9ed8\u8ba4");
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 40));
            HPEN hPen, hOldPen;
            HFONT hTitle, hBody, hNote, hRow, hStatus, hHint, hOld;
            int r, c, i, j;
            wchar_t wbuf[256];
            RECT clientRc;
            const wchar_t* rowNames[] = { L"\u524d\u6392", L"\u4e2d\u6392", L"\u540e\u6392" };

            FillRect(hdc, &ps.rcPaint, hBrush);
            DeleteObject(hBrush);
            SetBkMode(hdc, TRANSPARENT);

            GetClientRect(hwnd, &clientRc);

            hTitle = CreateFontW(28, 0, 0, 0, FW_BOLD, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
            hBody = CreateFontW(22, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
            hNote = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
            hRow = CreateFontW(19, 0, 0, 0, FW_BOLD, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
            hStatus = CreateFontW(22, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
            hHint = CreateFontW(17, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");

            hOld = (HFONT)SelectObject(hdc, hTitle);

            SelectObject(hdc, hRow);
            SetTextColor(hdc, RGB(255, 180, 100));
            for (r = 0; r < ROWS; r++) {
                TextOutW(hdc, 10, GRID_Y + r * CELL_H + CELL_H / 2 - 10, rowNames[r], 2);
            }

            for (r = 0; r < ROWS; r++) {
                for (c = 0; c < COLS; c++) {
                    RECT rcc;
                    int x, y;
                    i = r * COLS + c;
                    if (i >= mappingCount) continue;
                    x = GRID_X + c * CELL_W;
                    y = GRID_Y + r * CELL_H;

                    if (mappings[i].isPressed) {
                        HBRUSH hPress = CreateSolidBrush(RGB(40, 95, 55));
                        RECT fr = { x + 1, y + 1, x + CELL_W - 1, y + CELL_H - 1 };
                        FillRect(hdc, &fr, hPress);
                        DeleteObject(hPress);
                    }

                    if (i == g_selectedCell) {
                        HBRUSH hSel = CreateSolidBrush(RGB(48, 78, 120));
                        RECT fr = { x + 1, y + 1, x + CELL_W - 1, y + CELL_H - 1 };
                        FillRect(hdc, &fr, hSel);
                        DeleteObject(hSel);
                    }

                    {
                        COLORREF borderColor;
                        int borderWidth;
                        if (i == g_selectedCell) {
                            borderColor = RGB(100, 180, 255);
                            borderWidth = 2;
                        } else if (mappings[i].isPressed) {
                            borderColor = RGB(100, 220, 100);
                            borderWidth = 2;
                        } else {
                            borderColor = RGB(60, 60, 80);
                            borderWidth = 1;
                        }
                        hPen = CreatePen(PS_SOLID, borderWidth, borderColor);
                        hOldPen = (HPEN)SelectObject(hdc, hPen);
                        {
                            HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                            Rectangle(hdc, x, y, x + CELL_W, y + CELL_H);
                            SelectObject(hdc, hOldBr);
                        }
                        SelectObject(hdc, hOldPen);
                        DeleteObject(hPen);
                    }

                    /* 上半部分：触发键 */
                    {
                        const char* name = GetKeyName(mappings[i].skyKey);
                        MultiByteToWideChar(CP_ACP, 0, name, -1, wbuf, 256);
                    }
                    SelectObject(hdc, hBody);
                    if (!g_enabled) {
                        SetTextColor(hdc, RGB(120, 120, 120));
                    } else if (mappings[i].isPressed) {
                        SetTextColor(hdc, RGB(220, 255, 220));
                    } else {
                        SetTextColor(hdc, RGB(230, 230, 230));
                    }
                    SetRect(&rcc, x, y + 2, x + CELL_W, y + CELL_H * 3 / 5);
                    DrawTextW(hdc, wbuf, -1, &rcc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                    /* 下半部分：简谱数字 + 圆点（点画在正上方或正下方） */
                    SelectObject(hdc, hNote);
                    {
                        COLORREF noteColor;
                        if (!g_enabled) noteColor = RGB(90, 90, 90);
                        else if (mappings[i].isPressed) noteColor = RGB(200, 255, 200);
                        else noteColor = RGB(180, 220, 255);
                        SetTextColor(hdc, noteColor);

                        {
                            const wchar_t* note = g_noteNames[r][c];
                            wchar_t digit = note[0];
                            int dotPos = 0;   /* 0=无，1=下方，2=上方 */
                            if (note[1] == L'.') dotPos = 1;
                            else if (note[1] == L'\'') dotPos = 2;

                            wchar_t dbuf[2] = { digit, 0 };
                            SIZE dsz;
                            GetTextExtentPoint32W(hdc, dbuf, 1, &dsz);

                            int centerX = x + CELL_W / 2;
                            int halfTop = y + CELL_H * 3 / 5;
                            int halfH = CELL_H - CELL_H * 3 / 5;

                            int digitY;
                            int dotCY = 0;

                            if (dotPos == 1) {
                                digitY = halfTop + 1;
                                dotCY = digitY + dsz.cy + 3;
                            } else if (dotPos == 2) {
                                digitY = halfTop + 7;
                                dotCY = digitY - 3;
                            } else {
                                digitY = halfTop + (halfH - dsz.cy) / 2;
                            }

                            int digitX = centerX - dsz.cx / 2;
                            TextOutW(hdc, digitX, digitY, dbuf, 1);

                            if (dotPos != 0) {
                                HBRUSH hDot = CreateSolidBrush(noteColor);
                                HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hDot);
                                HPEN hDotPen = CreatePen(PS_SOLID, 1, noteColor);
                                HPEN hOldPen2 = (HPEN)SelectObject(hdc, hDotPen);
                                Ellipse(hdc, centerX - 2, dotCY - 2, centerX + 2, dotCY + 2);
                                SelectObject(hdc, hOldBr);
                                SelectObject(hdc, hOldPen2);
                                DeleteObject(hDot);
                                DeleteObject(hDotPen);
                            }
                        }
                    }
                }
            }

            /* 状态行 */
            SelectObject(hdc, hStatus);
            SetTextColor(hdc, g_enabled ? RGB(100, 220, 100) : RGB(255, 160, 60));
            MultiByteToWideChar(CP_ACP, 0, g_statusText, -1, wbuf, 256);
            TextOutW(hdc, 25, 288, wbuf, wcslen(wbuf));

            /* 退出键标签 */
            SelectObject(hdc, hStatus);
            if (g_selectedExitKey != -1) {
                SetTextColor(hdc, RGB(255, 200, 80));
            } else {
                SetTextColor(hdc, RGB(180, 180, 180));
            }
            TextOutW(hdc, 290, 288, L"\u9000\u51fa\u952e\uff1a", 4);

            {
                int startX = 400;
                int startY = 288;
                int x = startX;
                int gapX = 12;
                int xW = 22;
                int padY = 16;
                int textH = 28;
                g_exitKeyRectCount = 0;
                g_exitKeyDeleteRectCount = 0;

                for (j = 0; j < g_exitKeyCount; j++) {
                    const char* kn = GetKeyName(g_exitKeys[j]);
                    SIZE sz;
                    RECT tr, xr;
                    int keyW, slotW, xPos;

                    MultiByteToWideChar(CP_ACP, 0, kn, -1, wbuf, 256);
                    GetTextExtentPoint32W(hdc, wbuf, wcslen(wbuf), &sz);
                    keyW = sz.cx;

                    slotW = keyW + gapX + xW + gapX;

                    if (x + slotW > clientRc.right - 4) break;

                    if (j == 0) tr.left = 285;
                    else tr.left = x - 4;
                    tr.top    = startY - padY;
                    tr.right  = x + keyW + 4;
                    tr.bottom = startY + textH + padY;
                    g_exitKeyRects[g_exitKeyRectCount] = tr;
                    g_exitKeyRectIndexes[g_exitKeyRectCount] = j;
                    g_exitKeyRectCount++;

                    if (j == g_selectedExitKey) {
                        SetTextColor(hdc, RGB(255, 200, 80));
                    } else {
                        SetTextColor(hdc, RGB(180, 180, 180));
                    }
                    TextOutW(hdc, x, startY, wbuf, wcslen(wbuf));

                    xPos = x + keyW + gapX;
                    xr.left   = xPos - 4;
                    xr.top    = startY - 4;
                    xr.right  = xPos + xW + 4;
                    xr.bottom = startY + textH + 4;
                    g_exitKeyDeleteRects[g_exitKeyDeleteRectCount] = xr;
                    g_exitKeyDeleteRectIndexes[g_exitKeyDeleteRectCount] = j;
                    g_exitKeyDeleteRectCount++;

                    SetTextColor(hdc, RGB(210, 90, 90));
                    TextOutW(hdc, xPos, startY, L"\u00D7", 1);

                    x += slotW;
                }
            }

            /* 提示行 */
            SelectObject(hdc, hHint);
            SetTextColor(hdc, RGB(140, 140, 150));
            TextOutW(hdc, 25, 340,
                L"\u70b9\u51fb\u683c\u5b50/\u9000\u51fa\u952e\u9009\u4e2d\uff0c\u518d\u70b9\u300c\u6355\u83b7\u6309\u952e\u300d\u4fee\u6539\uff1b\u53f3\u952e\u53d6\u6d88\u9009\u4e2d\uff1b\u70b9\u51fb\u300c\u00d7\u300d\u53ef\u5220\u9664\u9000\u51fa\u952e",
                -1);

            /* 捕获提示 */
            if (g_capturing) {
                RECT hintRc;
                int hintW = 380;
                int hintH = 100;
                int hintX = (clientRc.right - hintW) / 2;
                int hintY = 160;

                hintRc.left   = hintX;
                hintRc.top    = hintY;
                hintRc.right  = hintX + hintW;
                hintRc.bottom = hintY + hintH;

                {
                    HBRUSH hBg = CreateSolidBrush(RGB(20, 45, 75));
                    FillRect(hdc, &hintRc, hBg);
                    DeleteObject(hBg);
                }

                {
                    HPEN hBd = CreatePen(PS_SOLID, 2, RGB(100, 180, 255));
                    HPEN hOp = (HPEN)SelectObject(hdc, hBd);
                    HBRUSH hOb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    Rectangle(hdc, hintRc.left, hintRc.top, hintRc.right, hintRc.bottom);
                    SelectObject(hdc, hOp);
                    SelectObject(hdc, hOb);
                    DeleteObject(hBd);
                }

                {
                    HFONT hCapture = CreateFontW(24, 0, 0, 0, FW_BOLD, 0, 0, 0,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
                    HFONT hOld2 = (HFONT)SelectObject(hdc, hCapture);

                    RECT line1 = hintRc;
                    line1.bottom = hintRc.top + 55;

                    SetTextColor(hdc, RGB(255, 220, 100));
                    DrawTextW(hdc, L"\u8bf7\u6309\u4e0b\u8981\u7ed1\u5b9a\u7684\u6309\u952e\u2026",
                        -1, &line1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                    RECT line2 = hintRc;
                    line2.top = hintRc.top + 55;
                    line2.bottom = hintRc.bottom - 5;

                    HFONT hSmall = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
                    SelectObject(hdc, hSmall);

                    SetTextColor(hdc, RGB(180, 200, 230));
                    DrawTextW(hdc, L"\uff08\u70b9\u51fb\u4efb\u610f\u6309\u94ae\u53d6\u6d88\uff09",
                        -1, &line2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                    SelectObject(hdc, hOld2);
                    DeleteObject(hCapture);
                    DeleteObject(hSmall);
                }
            }

            SelectObject(hdc, hOld);
            DeleteObject(hTitle);
            DeleteObject(hBody);
            DeleteObject(hNote);
            DeleteObject(hRow);
            DeleteObject(hStatus);
            DeleteObject(hHint);
            EndPaint(hwnd, &ps);
            break;
        }

        case WM_DESTROY:
            SaveWindowState();
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
    const wchar_t* MUTEX_NAME = L"SkyToDelta_SingleInstance_Mutex_v1";
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;
    HANDLE hMutex;

    hMutex = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    if (hMutex == NULL) return 0;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hPrev = FindWindowW(CLASS_NAME, NULL);
        if (hPrev) {
            PostMessageW(hPrev, WM_CLOSE, 0, 0);
            Sleep(200);
        }
        CloseHandle(hMutex);
        hMutex = CreateMutexW(NULL, FALSE, MUTEX_NAME);
        if (hMutex == NULL) return 0;
    }

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 40));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassW(&wc)) {
        CloseHandle(hMutex);
        return 0;
    }

    /* 窗口标题改为：三角洲→光遇键位 */
    hwnd = CreateWindowExW(
        0, CLASS_NAME,
        L"\u4e09\u89d2\u6d32\u2192\u5149\u9047\u952e\u4f4d",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 490,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        CloseHandle(hMutex);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}
