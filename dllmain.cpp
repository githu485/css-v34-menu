// overlay_test.cpp
// g++ overlay_test.cpp -o overlay_test.exe -lgdi32 -luser32 -static -O2 -s
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thread>
#include <atomic>
#include <cstdio>

static std::atomic<bool> g_run{ true };
static char g_logPath[MAX_PATH]{};

static void Log(const char* msg) {
    if (!g_logPath[0]) {
        char exe[MAX_PATH]{};
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        char* dot = strrchr(exe, '.');
        if (dot) *dot = 0;
        snprintf(g_logPath, sizeof(g_logPath), "%s.log", exe);
    }
    FILE* f = fopen(g_logPath, "a");
    if (!f) return;
    fprintf(f, "%s\n", msg);
    fclose(f);
}

static void Fail(const char* title, const char* text) {
    Log(title); Log(text);
    MessageBoxA(nullptr, text, title, MB_OK | MB_ICONERROR);
}

static void DrawBox(HDC hdc, int x, int y, int w, int h, COLORREF color, int thick) {
    HPEN pen = CreatePen(PS_SOLID, thick, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void DrawTextAt(HDC hdc, int x, int y, const char* t, COLORREF c) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, c);
    TextOutA(hdc, x, y, t, (int)strlen(t));
}

static void DrawCrosshair(HDC hdc, int w, int h, COLORREF c) {
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ old = SelectObject(hdc, pen);
    int cx = w / 2, cy = h / 2;
    MoveToEx(hdc, cx - 20, cy, nullptr); LineTo(hdc, cx + 20, cy);
    MoveToEx(hdc, cx, cy - 20, nullptr); LineTo(hdc, cx, cy + 20);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void RenderLoop(HWND overlay) {
    while (g_run) {
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            PostMessage(overlay, WM_CLOSE, 0, 0);
            break;
        }

        HWND game = FindWindowA("Valve001", nullptr);
        if (!game) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if (IsIconic(game)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        RECT rc{};
        GetClientRect(game, &rc);
        POINT pt{ 0, 0 };
        ClientToScreen(game, &pt);

        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) continue;

        SetWindowPos(overlay, HWND_TOPMOST, pt.x, pt.y, w, h,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);

        HDC hdc = GetDC(overlay);
        if (!hdc) continue;

        PatBlt(hdc, 0, 0, w, h, BLACKNESS);

        // Синий кубик — слева
        DrawBox(hdc, 79, 79, 122, 162, RGB(0, 0, 0), 3);
        DrawBox(hdc, 80, 80, 120, 160, RGB(70, 140, 255), 2);
        DrawTextAt(hdc, 80, 60, "BLUE OK", RGB(70, 140, 255));

        // Красный кубик — справа
        DrawBox(hdc, w - 201, 79, 122, 162, RGB(0, 0, 0), 3);
        DrawBox(hdc, w - 200, 80, 120, 160, RGB(230, 70, 70), 2);
        DrawTextAt(hdc, w - 200, 60, "RED OK", RGB(230, 70, 70));

        // Крестик в центре
        DrawCrosshair(hdc, w, h, RGB(120, 90, 255));

        // Инфо
        char info[128];
        snprintf(info, sizeof(info), "OVERLAY %dx%d | END to exit", w, h);
        DrawTextAt(hdc, 20, 20, info, RGB(235, 235, 240));

        ReleaseDC(overlay, hdc);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) {
        g_run = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main() {
    if (HWND c = GetConsoleWindow()) ShowWindow(c, SW_HIDE);
    SetProcessDPIAware();
    Log("=== overlay_test start ===");

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"OVERLAY_TEST_WND";
    if (!RegisterClassExW(&wc)) {
        Fail("overlay_test", "RegisterClassExW упал.");
        return 1;
    }

    HWND overlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"", WS_POPUP,
        0, 0, 400, 300, nullptr, nullptr, wc.hInstance, nullptr);

    if (!overlay) {
        Fail("overlay_test", "CreateWindowExW упал.");
        return 2;
    }

    SetLayeredWindowAttributes(overlay, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(overlay, SW_SHOW);
    UpdateWindow(overlay);

    Log("overlay ready");
    std::thread t(RenderLoop, overlay);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_run = false;
    if (t.joinable()) t.join();
    Log("=== exit ===");
    return 0;
}
