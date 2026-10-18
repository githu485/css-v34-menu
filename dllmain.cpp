#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>

#pragma comment(lib, "comctl32.lib")

// Состояния наших элементов
bool g_toggleState = false;
bool g_testPressed = false;
bool g_testHover = false;
bool g_xHover = false;

// Координаты элементов
RECT g_toggleRect = { 15, 45, 75, 65 };
RECT g_testBtnRect = { 15, 80, 115, 110 };
RECT g_closeBtnRect = { 245, 10, 265, 30 };

HWND g_mainWnd = nullptr;
HFONT g_hFont = nullptr;
HFONT g_hFontBold = nullptr;

void DrawCustomUI(HDC hdc) {
    RECT clientRect;
    GetClientRect(g_mainWnd, &clientRect);
    
    // Двойная буферизация (чтобы не мерцало)
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // 1. Заливаем основной фон
    HBRUSH bgBrush = CreateSolidBrush(RGB(25, 25, 30));
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // 2. Рисуем шапку
    RECT headerRect = { 0, 0, clientRect.right, 40 };
    HBRUSH headerBrush = CreateSolidBrush(RGB(35, 35, 45));
    FillRect(memDC, &headerRect, headerBrush);
    DeleteObject(headerBrush);

    // Текст в шапке
    SetTextColor(memDC, RGB(255, 255, 255));
    SetBkMode(memDC, TRANSPARENT);
    SelectObject(memDC, g_hFontBold);
    TextOutA(memDC, 15, 10, "commandvs // css v34", 19);

    // Крестик
    if (g_xHover) {
        HBRUSH xHoverBrush = CreateSolidBrush(RGB(200, 60, 60));
        FillRect(memDC, &g_closeBtnRect, xHoverBrush);
        DeleteObject(xHoverBrush);
    }
    SetTextColor(memDC, RGB(255, 255, 255));
    DrawTextA(memDC, "X", -1, &g_closeBtnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Разделитель под шапкой
    RECT lineRect = { 0, 40, clientRect.right, 41 };
    HBRUSH lineBrush = CreateSolidBrush(RGB(60, 60, 70));
    FillRect(memDC, &lineRect, lineBrush);
    DeleteObject(lineBrush);

    // 3. Тумблер
    HBRUSH toggleBgBrush = CreateSolidBrush(g_toggleState ? RGB(80, 200, 100) : RGB(60, 60, 70));
    SelectObject(memDC, toggleBgBrush);
    SelectObject(memDC, GetStockObject(NULL_PEN));
    RoundRect(memDC, g_toggleRect.left, g_toggleRect.top, g_toggleRect.right, g_toggleRect.bottom, 20, 20);
    DeleteObject(toggleBgBrush);

    // Кружок внутри тумблера
    HBRUSH knobBrush = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(memDC, knobBrush);
    int knobX = g_toggleState ? 55 : 17;
    Ellipse(memDC, knobX, 47, knobX + 16, 63);
    DeleteObject(knobBrush);

    // Текст рядом с тумблером
    SetTextColor(memDC, RGB(200, 200, 200));
    SelectObject(memDC, g_hFont);
    TextOutA(memDC, 85, 46, "Custom Toggle", 13);

    // 4. Кнопка Test
    HBRUSH btnBgBrush;
    if (g_testPressed) btnBgBrush = CreateSolidBrush(RGB(40, 40, 50));
    else if (g_testHover) btnBgBrush = CreateSolidBrush(RGB(70, 70, 80));
    else btnBgBrush = CreateSolidBrush(RGB(50, 50, 60));

    SelectObject(memDC, btnBgBrush);
    RoundRect(memDC, g_testBtnRect.left, g_testBtnRect.top, g_testBtnRect.right, g_testBtnRect.bottom, 10, 10);
    DeleteObject(btnBgBrush);

    SetTextColor(memDC, RGB(255, 255, 255));
    SelectObject(memDC, g_hFontBold);
    DrawTextA(memDC, "Test", -1, &g_testBtnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Копируем на экран
    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);
    
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hFont = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            g_hFontBold = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            break;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            POINT pt = { x, y };

            bool newTestHover = PtInRect(&g_testBtnRect, pt);
            bool newXHover = PtInRect(&g_closeBtnRect, pt);

            if (newTestHover != g_testHover || newXHover != g_xHover) {
                g_testHover = newTestHover;
                g_xHover = newXHover;
                InvalidateRect(hWnd, NULL, FALSE);
            }

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            break;
        }

        case WM_MOUSELEAVE: {
            if (g_testHover || g_xHover) {
                g_testHover = false;
                g_xHover = false;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            if (PtInRect(&g_toggleRect, pt)) {
                g_toggleState = !g_toggleState;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (PtInRect(&g_testBtnRect, pt)) {
                g_testPressed = true;
                SetCapture(hWnd);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (PtInRect(&g_closeBtnRect, pt)) {
                DestroyWindow(hWnd);
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (g_testPressed) {
                g_testPressed = false;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
                
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (PtInRect(&g_testBtnRect, pt)) {
                    MessageBoxA(hWnd, "Test Button Clicked!", "Info", MB_OK | MB_ICONINFORMATION);
                }
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            DrawCustomUI(hdc);
            EndPaint(hWnd, &ps);
            break;
        }

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
            if (hit == HTCLIENT) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(hWnd, &pt);
                if (PtInRect(&g_toggleRect, pt) || PtInRect(&g_testBtnRect, pt) || PtInRect(&g_closeBtnRect, pt)) {
                    return HTCLIENT;
                }
                return HTCAPTION;
            }
            return hit;
        }

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            DeleteObject(g_hFont);
            DeleteObject(g_hFontBold);
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
        if (isPressed && !wasPressed && g_mainWnd && IsWindow(g_mainWnd)) {
            if (IsWindowVisible(g_mainWnd)) ShowWindow(g_mainWnd, SW_HIDE);
            else { ShowWindow(g_mainWnd, SW_SHOW); SetForegroundWindow(g_mainWnd); }
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
    wc.hbrBackground = CreateSolidBrush(RGB(25, 25, 30));
    wc.lpszClassName = "CommandvsMenu";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassExA(&wc);

    // Убрали WS_EX_LAYERED, теперь окно непрозрачное и не лагает
    g_mainWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, 
        "CommandvsMenu", "commandvs",
        WS_POPUP | WS_VISIBLE, 
        100, 100, 280, 130,
        NULL, NULL, wc.hInstance, NULL
    );

    if (!g_mainWnd) return 1;

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
        CreateThread(nullptr, 0, GuiThread, hMod, 0, nullptr);
    }
    return TRUE;
}
