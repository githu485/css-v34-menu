#include <windows.h>
#include <commctrl.h>
#include <string>

#pragma comment(lib, "comctl32.lib")

bool g_bhop = false;
bool g_wallhack = false;
bool g_norecoil = false;

#define ID_CHECK_BHOP 1001
#define ID_CHECK_WALL 1002
#define ID_CHECK_RECOIL 1003
#define ID_BTN_HIDE 1004

HWND g_mainWnd = nullptr;
HWND g_hCheckBhop = nullptr;
HWND g_hCheckWall = nullptr;
HWND g_hCheckRecoil = nullptr;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            g_hCheckBhop = CreateWindowExA(0, "BUTTON", "Bhop (Auto Jump)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 20, 200, 25, hWnd, (HMENU)ID_CHECK_BHOP, NULL, NULL);
            g_hCheckWall = CreateWindowExA(0, "BUTTON", "Wallhack (Glow)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 50, 200, 25, hWnd, (HMENU)ID_CHECK_WALL, NULL, NULL);
            g_hCheckRecoil = CreateWindowExA(0, "BUTTON", "No Recoil", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 80, 200, 25, hWnd, (HMENU)ID_CHECK_RECOIL, NULL, NULL);
            
            CreateWindowExA(0, "BUTTON", "Hide (INSERT)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 120, 100, 30, hWnd, (HMENU)ID_BTN_HIDE, NULL, NULL);

            SendMessage(g_hCheckBhop, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_hCheckWall, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_hCheckRecoil, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_CHECK_BHOP) {
                g_bhop = (SendMessage(g_hCheckBhop, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            if (LOWORD(wParam) == ID_CHECK_WALL) {
                g_wallhack = (SendMessage(g_hCheckWall, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            if (LOWORD(wParam) == ID_CHECK_RECOIL) {
                g_norecoil = (SendMessage(g_hCheckRecoil, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            if (LOWORD(wParam) == ID_BTN_HIDE) {
                ShowWindow(hWnd, SW_HIDE); // Прячем по кнопке
            }
            break;

        case WM_CLOSE:
            // УНИЧТОЖАЕМ окно (полный выход)
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            // Обнуляем указатель, чтобы хоткей не крашился
            g_mainWnd = nullptr; 
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

DWORD WINAPI HotkeyThread(LPVOID lpParam) {
    bool wasPressed = false;
    while (true) {
        bool isPressed = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        
        // Проверяем, живо ли еще окно (если его закрыли крестиком, указатель станет nullptr)
        if (isPressed && !wasPressed && g_mainWnd && IsWindow(g_mainWnd)) {
            if (IsWindowVisible(g_mainWnd)) {
                ShowWindow(g_mainWnd, SW_HIDE);
            } else {
                ShowWindow(g_mainWnd, SW_SHOW);
                SetForegroundWindow(g_mainWnd);
            }
        }
        wasPressed = isPressed;
        Sleep(50);
    }
    return 0;
}

DWORD WINAPI GuiThread(LPVOID lpParam) {
    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName = "CommandvsMenu";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassExA(&wc)) return 1;

    g_mainWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "CommandvsMenu", "commandvs // css v34",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 280, 220,
        NULL, NULL, wc.hInstance, NULL
    );

    if (!g_mainWnd) return 1;

    CreateThread(nullptr, 0, HotkeyThread, NULL, 0, nullptr);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    
    // Когда окно уничтожено, поток завершает работу
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, GuiThread, hMod, 0, nullptr);
    }
    return TRUE;
}
