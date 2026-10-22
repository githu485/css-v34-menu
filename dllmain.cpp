// css_v34_fixed.cpp
// build (MinGW): g++ css_v34_fixed.cpp -o css_v34_fixed.exe -lgdi32 -luser32 -static -municode
// build (MSVC):  cl /O2 /EHsc css_v34_fixed.cpp user32.lib gdi32.lib
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <cmath>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdarg>

using uptr = std::uintptr_t;

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };

static HANDLE  g_proc = nullptr;
static DWORD   g_pid  = 0;
static uptr    clientBase = 0, engineBase = 0;
static DWORD   clientSize = 0, engineSize = 0;
static std::atomic<bool> g_running{ true };
static char    g_logPath[MAX_PATH]{};

// ---------- CONFIG ----------
static const bool  ESP_ENEMY_RED   = true;   // враги — красные
static const bool  ESP_TEAM_BLUE   = true;   // свои — синие
static const bool  ESP_HEALTH      = true;
static const bool  ESP_NAME        = true;
static const bool  AIMBOT          = true;
static const int   AIM_FOV         = 90;
static const int   AIM_SMOOTH      = 6;
static const bool  FOV_CIRCLE      = true;
// ----------------------------

struct Offsets {
    uptr dwLocalPlayer = 0;
    uptr dwEntityList  = 0;
    uptr dwViewMatrix  = 0;

    DWORD m_iHealth   = 0x8C;
    DWORD m_iTeam     = 0x9C;
    DWORD m_vecOrigin = 0x2C;
    DWORD m_lifeState = 0x8B;
    DWORD m_bDormant  = 0xED;
} off;

// ---------- LOG ----------
static void Log(const char* fmt, ...) {
    if (!g_logPath[0]) {
        char exe[MAX_PATH]{};
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        char* dot = strrchr(exe, '.');
        if (dot) *dot = 0;
        snprintf(g_logPath, sizeof(g_logPath), "%s.log", exe);
    }
    FILE* f = fopen(g_logPath, "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static void FailBox(const char* title, const char* text) {
    Log("[FAIL] %s: %s", title, text);
    MessageBoxA(nullptr, text, title, MB_OK | MB_ICONERROR);
}

// ---------- MEMORY ----------
template<typename T>
T ReadMem(uptr addr) {
    T v{};
    if (!addr || !g_proc) return v;
    ReadProcessMemory(g_proc, (LPCVOID)addr, &v, sizeof(T), nullptr);
    return v;
}

// ---------- PROCESS / MODULES ----------
static DWORD GetPIDByName(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static DWORD FindGamePID() {
    const wchar_t* names[] = { L"hl2.exe", L"cstrike.exe", L"hl2_linux.exe" };
    for (auto n : names) {
        DWORD p = GetPIDByName(n);
        if (p) return p;
    }
    return 0;
}

static bool GetModule(DWORD pid, const wchar_t* name, uptr& base, DWORD& size) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32W me{}; me.dwSize = sizeof(me);
    bool ok = false;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, name) == 0) {
                base = (uptr)me.modBaseAddr;
                size = me.modBaseSize;
                ok = true;
                break;
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return ok;
}

// ---------- PATTERN ----------
static std::vector<int> ParseSig(const char* sig) {
    std::vector<int> out;
    const char* p = sig;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '?') {
            out.push_back(-1);
            p++;
            if (*p == '?') p++;
        } else {
            char* end = nullptr;
            long v = strtol(p, &end, 16);
            out.push_back((int)v);
            p = end;
        }
    }
    return out;
}

static uptr FindPattern(uptr base, DWORD size, const char* sig) {
    auto pat = ParseSig(sig);
    if (pat.empty() || size < pat.size()) return 0;

    std::vector<BYTE> buf(size);
    SIZE_T read = 0;
    if (!ReadProcessMemory(g_proc, (LPCVOID)base, buf.data(), size, &read)) return 0;
    if (read < pat.size()) return 0;

    size_t limit = read - pat.size();
    for (size_t i = 0; i <= limit; i++) {
        bool ok = true;
        for (size_t j = 0; j < pat.size(); j++) {
            if (pat[j] != -1 && buf[i + j] != (BYTE)pat[j]) { ok = false; break; }
        }
        if (ok) return base + i;
    }
    return 0;
}

static uptr ResolveMovPtr(uptr addr)     { return addr ? ReadMem<uptr>(addr + 2) : 0; }  // 8B 0D
static uptr ResolveMovupsPtr(uptr addr)  { return addr ? ReadMem<uptr>(addr + 3) : 0; }  // 0F 10 05

// ---------- MATH ----------
static bool WorldToScreen(const Vec3& pos, Vec2& out, float m[16], int w, int h) {
    float clipW = m[3] * pos.x + m[7] * pos.y + m[11] * pos.z + m[15];
    if (clipW < 0.01f) return false;

    float clipX = m[0] * pos.x + m[4] * pos.y + m[8]  * pos.z + m[12];
    float clipY = m[1] * pos.x + m[5] * pos.y + m[9]  * pos.z + m[13];

    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;

    out.x = (w * 0.5f) * (1.0f + ndcX);
    out.y = (h * 0.5f) * (1.0f - ndcY);
    return true;
}

// ---------- DRAW ----------
static void DrawBox(HDC hdc, int x, int y, int w, int h, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void DrawHealth(HDC hdc, int x, int y, int h, int hp) {
    int fill = (int)((h - 2) * (std::clamp(hp, 0, 100) / 100.0f));

    HBRUSH bg = CreateSolidBrush(RGB(20, 20, 20));
    RECT bgR{ x, y, x + 4, y + h };
    FillRect(hdc, &bgR, bg);
    DeleteObject(bg);

    HBRUSH fg = CreateSolidBrush(
        hp > 60 ? RGB(80, 220, 100) :
        hp > 30 ? RGB(240, 200, 60) : RGB(230, 70, 70));
    RECT fgR{ x + 1, y + h - fill - 1, x + 3, y + h - 1 };
    FillRect(hdc, &fgR, fg);
    DeleteObject(fg);
}

static void DrawTextA_(HDC hdc, int x, int y, const char* text, COLORREF color) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    TextOutA(hdc, x, y, text, (int)strlen(text));
}

static void DrawFovCircle(HDC hdc, int w, int h, int radius, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Ellipse(hdc, w / 2 - radius, h / 2 - radius, w / 2 + radius, h / 2 + radius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

// ---------- RENDER ----------
struct Target { Vec2 head{}; float dist = 1e9f; bool valid = false; };

static void RenderThread(HWND overlay) {
    while (g_running) {
        // END — выход
        if (GetAsyncKeyState(VK_END) & 0x8000) { PostMessage(overlay, WM_CLOSE, 0, 0); break; }

        HWND game = FindWindowA("Valve001", nullptr);
        if (!game) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }
        if (IsIconic(game)) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }

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

        uptr local = ReadMem<uptr>(clientBase + off.dwLocalPlayer);
        if (local) {
            int  myTeam = ReadMem<int>(local + off.m_iTeam);
            Vec3 myPos  = ReadMem<Vec3>(local + off.m_vecOrigin);

            float matrix[16]{};
            ReadProcessMemory(g_proc, (LPCVOID)(engineBase + off.dwViewMatrix),
                matrix, sizeof(matrix), nullptr);

            uptr entList = ReadMem<uptr>(clientBase + off.dwEntityList);
            Target best{};
            Vec2 center{ w * 0.5f, h * 0.5f };

            for (int i = 1; i <= 64; i++) {
                uptr ent = ReadMem<uptr>(entList + i * 0x10);
                if (!ent || ent == local) continue;

                int  hp      = ReadMem<int>(ent + off.m_iHealth);
                int  team    = ReadMem<int>(ent + off.m_iTeam);
                int  life    = ReadMem<int>(ent + off.m_lifeState);
                bool dormant = ReadMem<BYTE>(ent + off.m_bDormant) != 0;

                if (hp <= 0 || hp > 100 || life != 0 || dormant) continue;

                bool isTeam = (team == myTeam);
                if (isTeam && !ESP_TEAM_BLUE) continue;
                if (!isTeam && !ESP_ENEMY_RED) continue;

                Vec3 origin = ReadMem<Vec3>(ent + off.m_vecOrigin);
                Vec3 head   = origin; head.z += 64.0f;

                Vec2 sHead{}, sFoot{};
                if (!WorldToScreen(head, sHead, matrix, w, h)) continue;
                if (!WorldToScreen(origin, sFoot, matrix, w, h)) continue;

                if (sHead.x < -200 || sHead.x > w + 200 ||
                    sHead.y < -200 || sHead.y > h + 200) continue;

                int boxH = (int)(sFoot.y - sHead.y);
                if (boxH < 6) continue;
                int boxW = (int)(boxH * 0.52f);
                int boxX = (int)(sHead.x - boxW * 0.5f);
                int boxY = (int)sHead.y;

                // Синие = свои, красные = враги
                COLORREF col = isTeam ? RGB(70, 140, 255) : RGB(230, 70, 70);

                DrawBox(hdc, boxX, boxY, boxW, boxH, RGB(0, 0, 0));           // обводка
                DrawBox(hdc, boxX - 1, boxY - 1, boxW + 2, boxH + 2, col);    // цвет

                if (ESP_HEALTH) DrawHealth(hdc, boxX - 7, boxY, boxH, hp);

                if (ESP_NAME) {
                    char nameBuf[64];
                    float d3 = std::sqrt(
                        (origin.x - myPos.x) * (origin.x - myPos.x) +
                        (origin.y - myPos.y) * (origin.y - myPos.y) +
                        (origin.z - myPos.z) * (origin.z - myPos.z));
                    snprintf(nameBuf, sizeof(nameBuf), "%s | %d HP | %dm",
                        isTeam ? "TEAM" : "ENEMY", hp, (int)(d3 / 32.0f));
                    DrawTextA_(hdc, boxX, boxY - 14, nameBuf, col);
                }

                // выбор цели — только враги
                if (!isTeam) {
                    float d = std::sqrt(
                        (sHead.x - center.x) * (sHead.x - center.x) +
                        (sHead.y - center.y) * (sHead.y - center.y));
                    if (d < AIM_FOV && d < best.dist) {
                        best.dist = d;
                        best.head = sHead;
                        best.valid = true;
                    }
                }
            }

            if (FOV_CIRCLE) DrawFovCircle(hdc, w, h, AIM_FOV, RGB(120, 90, 255));

            if (AIMBOT && best.valid && (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
                int dx = (int)(best.head.x - center.x);
                int dy = (int)(best.head.y - center.y);
                if (AIM_SMOOTH > 1) { dx /= AIM_SMOOTH; dy /= AIM_SMOOTH; }
                mouse_event(MOUSEEVENTF_MOVE, dx, dy, 0, 0);
            }
        }

        ReleaseDC(overlay, hdc);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ---------- WND PROC ----------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) { g_running = false; PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------- MAIN ----------
int main() {
    if (HWND c = GetConsoleWindow()) ShowWindow(c, SW_HIDE);
    SetProcessDPIAware();
    Log("=== start ===");

    // 1) игра
    for (;;) {
        g_pid = FindGamePID();
        if (g_pid) break;
        int r = MessageBoxA(nullptr,
            "Игра не найдена (hl2.exe / cstrike.exe).\n\nЗапусти CS:S v34, потом нажми OK.",
            "commandvs — старт", MB_RETRYCANCEL | MB_ICONWARNING);
        if (r != IDRETRY) { Log("no game, exit"); return 1; }
    }
    Log("pid=%lu", g_pid);

    // 2) процесс
    g_proc = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
        FALSE, g_pid);
    if (!g_proc) {
        DWORD e = GetLastError();
        char buf[256];
        snprintf(buf, sizeof(buf),
            "OpenProcess упал (err=%lu).\nЗапусти чит от имени администратора.", e);
        FailBox("commandvs — ошибка", buf);
        return 2;
    }

    // 3) модули
    if (!GetModule(g_pid, L"client.dll", clientBase, clientSize)) {
        FailBox("commandvs — ошибка", "client.dll не найден. Игра полностью загружена?");
        return 3;
    }
    if (!GetModule(g_pid, L"engine.dll", engineBase, engineSize)) {
        FailBox("commandvs — ошибка", "engine.dll не найден.");
        return 4;
    }
    Log("client=0x%llX size=%lu", (unsigned long long)clientBase, clientSize);
    Log("engine=0x%llX size=%lu", (unsigned long long)engineBase, engineSize);

    // 4) сигнатуры
    uptr lpSig = FindPattern(clientBase, clientSize, "8B 0D ? ? ? ? 85 C9 74");
    uptr elSig = FindPattern(clientBase, clientSize, "8B 0D ? ? ? ? 85 C9 74 ? 8B 01");
    uptr vmSig = FindPattern(engineBase, engineSize, "0F 10 05 ? ? ? ? 0F 11 05");

    if (lpSig) off.dwLocalPlayer = ResolveMovPtr(lpSig) - clientBase;
    if (elSig) off.dwEntityList  = ResolveMovPtr(elSig) - clientBase;
    if (vmSig) off.dwViewMatrix  = ResolveMovupsPtr(vmSig) - engineBase;

    Log("sigs: lp=%llX el=%llX vm=%llX",
        (unsigned long long)lpSig, (unsigned long long)elSig, (unsigned long long)vmSig);
    Log("offs: lp=0x%llX el=0x%llX vm=0x%llX",
        (unsigned long long)off.dwLocalPlayer,
        (unsigned long long)off.dwEntityList,
        (unsigned long long)off.dwViewMatrix);

    if (!lpSig || !elSig || !vmSig) {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "Одна из сигнатур не найдена:\n"
            "localplayer=%llX\nentitylist=%llX\nviewmatrix=%llX\n\n"
            "Открой лог рядом с exe. Возможно, у тебя другая сборка v34.",
            (unsigned long long)lpSig, (unsigned long long)elSig, (unsigned long long)vmSig);
        FailBox("commandvs — сигнатуры", buf);
        // не выходим — можешь всё равно глянуть оверлей
    }

    // 5) окно оверлея
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"CSSV34_OVERLAY_FIXED";
    if (!RegisterClassExW(&wc)) {
        FailBox("commandvs — ошибка", "RegisterClassExW упал.");
        return 5;
    }

    HWND overlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"", WS_POPUP,
        0, 0, 200, 200, nullptr, nullptr, wc.hInstance, nullptr);

    if (!overlay) {
        FailBox("commandvs — ошибка", "CreateWindowExW упал.");
        return 6;
    }

    // чёрный = прозрачный
    SetLayeredWindowAttributes(overlay, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(overlay, SW_SHOW);
    UpdateWindow(overlay);

    Log("overlay ready. END — выход.");
    std::thread render(RenderThread, overlay);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_running = false;
    if (render.joinable()) render.join();
    if (g_proc) CloseHandle(g_proc);
    Log("=== exit ===");
    return 0;
}
