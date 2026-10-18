#include <windows.h>
#include <commctrl.h>
#include <string>

#pragma comment(lib, "comctl32.lib")

// Тумблеры
bool g_bhop = false;
bool g_wallhack = false;
bool g_norecoil = false;
float g_speed = 1.0f;

// Идентификаторы для элементов управления
#define ID_CHECK_BHOP 1001
#define ID_CHECK_WALL 1002
#define ID_CHECK_RECOIL 1003

HWND g_mainWnd = nullptr;
HWND g_hCheckBhop = nullptr;
HWND g_hCheckWall = nullptr;
HWND g_hCheckRecoil = nullptr;

// Процедура обработки сообщений окна
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            // Создаем шрифт покрасивее
            HFONT hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            // Чекбоксы
            g_hCheckBhop = CreateWindowExA(0, "BUTTON", "Bhop (Auto Jump)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 20, 200, 25, hWnd, (HMENU)ID_CHECK_BHOP, NULL, NULL);
            g_hCheckWall = CreateWindowExA(0, "BUTTON", "Wallhack (Glow)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 50, 200, 25, hWnd, (HMENU)ID_CHECK_WALL, NULL, NULL);
            g_hCheckRecoil = CreateWindowExA(0, "BUTTON", "No Recoil", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 80, 200, 25, hWnd, (HMENU)ID_CHECK_RECOIL, NULL, NULL);

            // Применяем шрифт
            SendMessage(g_hCheckBhop, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_hCheckWall, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_hCheckRecoil, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            // Делаем фон темным
            SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case WM_COMMAND:
            // Обработка нажатий на чекбоксы
            if (LOWORD(wParam) == ID_CHECK_BHOP) {
                g_bhop = (SendMessage(g_hCheckBhop, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            if (LOWORD(wParam) == ID_CHECK_WALL) {
                g_wallhack = (SendMessage(g_hCheckWall, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            if (LOWORD(wParam) == ID_CHECK_RECOIL) {
                g_norecoil = (SendMessage(g_hCheckRecoil, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            break;

        case WM_CLOSE:
            // Вместо закрытия просто прячем окно
            ShowWindow(hWnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// Поток для отслеживания нажатия INSERT (Скрыть/Показать)
DWORD WINAPI HotkeyThread(LPVOID lpParam) {
    bool wasPressed = false;
    while (true) {
        // Проверяем нажатие клавиши INSERT (VK_INSERT)
        bool isPressed = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        
        // Если клавиша только что была нажата
        if (isPressed && !wasPressed) {
            if (g_mainWnd) {
                if (IsWindowVisible(g_mainWnd)) {
                    ShowWindow(g_mainWnd, SW_HIDE); // Скрыть
                } else {
                    ShowWindow(g_mainWnd, SW_SHOW); // Показать
                    SetForegroundWindow(g_mainWnd); // Вывести на передний план
                }
            }
        }
        wasPressed = isPressed;
        Sleep(50); // Проверяем каждые 50 мс, чтобы не грузить процессор
    }
    return 0;
}

// Поток для создания окна
DWORD WINAPI GuiThread(LPVOID lpParam) {
    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30)); // Темный фон
    wc.lpszClassName = "CommandvsMenu";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassExA(&wc)) return 1;

    g_mainWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "CommandvsMenu", "commandvs // css v34",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 280, 200,
        NULL, NULL, wc.hInstance, NULL
    );

    if (!g_mainWnd) return 1;

    // Запускаем поток для хоткея
    CreateThread(nullptr, 0, HotkeyThread, NULL, 0, nullptr);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        // Запускаем GUI в отдельном потоке
        CreateThread(nullptr, 0, GuiThread, hMod, 0, nullptr);
    }
    return TRUE;
}
