// css_v34_external.cpp
// build: g++ css_v34_external.cpp -o css_v34_external.exe -lgdi32 -luser32 -static
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <cmath>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cstdio>

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };

static DWORD g_pid = 0;
static HANDLE g_proc = nullptr;
static uintptr_t clientBase = 0, engineBase = 0;
static DWORD clientSize = 0, engineSize = 0;
static std::atomic<bool> g_running{ true };

struct Offsets {
    uintptr_t dwLocalPlayer = 0;
    uintptr_t dwEntityList = 0;
    uintptr_t dwViewMatrix = 0;

    DWORD m_iHealth = 0x8C;
    DWORD m_iTeam = 0x9C;
    DWORD m_vecOrigin = 0x2C;
    DWORD m_lifeState = 0x8B;
    DWORD m_bDormant = 0xED;
} off;

template<typename T>
T ReadMem(uintptr_t addr) {
    T v{};
    if (!addr || !g_proc) return v;
    ReadProcessMemory(g_proc, (LPCVOID)addr, &v, sizeof(T), nullptr);
    return v;
}

template<typename T>
bool WriteMem(uintptr_t addr, T value) {
    if (!addr || !g_proc) return false;
    return WriteProcessMemory(g_proc, (LPVOID)addr, &value, sizeof(T), nullptr) != FALSE;
}

DWORD GetPID(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

bool GetModule(DWORD pid, const wchar_t* name, uintptr_t& base, DWORD& size) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);

    bool ok = false;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, name) == 0) {
                base = (uintptr_t)me.modBaseAddr;
                size = me.modBaseSize;
                ok = true;
                break;
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return ok;
}

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

uintptr_t FindPattern(uintptr_t base, DWORD size, const char* sig) {
    auto pat = ParseSig(sig);
    if (pat.empty() || size < pat.size()) return 0;

    std::vector<BYTE> buf(size);
    if (!ReadProcessMemory(g_proc, (LPCVOID)base, buf.data(), size, nullptr)) return 0;

    for (size_t i = 0; i + pat.size() <= size; i++) {
        bool ok = true;
        for (size_t j = 0; j < pat.size(); j++) {
            if (pat[j] != -1 && buf[i + j] != (BYTE)pat[j]) {
                ok = false;
                break;
            }
        }
        if (ok) return base + i;
    }
    return 0;
}

uintptr_t ResolveMovPtr(uintptr_t addr) {
    if (!addr) return 0;
    return ReadMem<uintptr_t>(addr + 2); // 8B 0D [addr]
}

uintptr_t ResolveMovupsPtr(uintptr_t addr) {
    if (!addr) return 0;
    return ReadMem<uintptr_t>(addr + 3); // 0F 10 05 [addr]
}

bool WorldToScreen(const Vec3& pos, Vec2& out, float m[16], int w, int h) {
    float clipW = m[3] * pos.x + m[7] * pos.y + m[11] * pos.z + m[15];
    if (clipW < 0.01f) return false;

    float clipX = m[0] * pos.x + m[4] * pos.y + m[8] * pos.z + m[12];
    float clipY = m[1] * pos.x + m[5] * pos.y + m[9] * pos.z + m[13];

    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;

    out.x = (w * 0.5f) * (1.0f + ndcX);
    out.y = (h * 0.5f) * (1.0f - ndcY);
    return true;
}

void DrawBox(HDC hdc, int x, int y, int w, int h, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

    Rectangle(hdc, x, y, x + w, y + h);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void DrawHealth(HDC hdc, int x, int y, int h, int hp) {
    int fill = (int)((h - 2) * (std::clamp(hp, 0, 100) / 100.0f));

    HBRUSH bg = CreateSolidBrush(RGB(20, 20, 20));
    RECT bgR{ x, y, x + 4, y + h };
    FillRect(hdc, &bgR, bg);
    DeleteObject(bg);

    HBRUSH fg = CreateSolidBrush(
        hp > 60 ? RGB(80, 220, 100) :
        hp > 30 ? RGB(240, 200, 60) :
        RGB(230, 70, 70)
    );
    RECT fgR{ x + 1, y + h - fill - 1, x + 3, y + h - 1 };
    FillRect(hdc, &fgR, fg);
    DeleteObject(fg);
}

void DrawTextA(HDC hdc, int x, int y, const char* text, COLORREF color) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    TextOutA(hdc, x, y, text, (int)strlen(text));
}

struct Target {
    Vec2 screenHead{};
    float dist = 99999.f;
    bool valid = false;
};

static void RenderThread(HWND overlay) {
    const int AIM_FOV = 90;
    const int AIM_SMOOTH = 6;
    const bool ESP_BOX = true;
    const bool ESP_HEALTH = true;
    const bool AIMBOT = true;

    while (g_running) {
        HWND game = FindWindowA("Valve001", nullptr);
        if (!game) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        RECT rc{};
        GetWindowRect(game, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) continue;

        SetWindowPos(overlay, HWND_TOPMOST, rc.left, rc.top, w, h,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);

        HDC hdc = GetDC(overlay);
        if (!hdc) continue;

        PatBlt(hdc, 0, 0, w, h, BLACKNESS);

        uintptr_t local = ReadMem<uintptr_t>(clientBase + off.dwLocalPlayer);
        if (local) {
            int myTeam = ReadMem<int>(local + off.m_iTeam);
            Vec3 myPos = ReadMem<Vec3>(local + off.m_vecOrigin);

            float matrix[16]{};
            ReadProcessMemory(g_proc, (LPCVOID)(engineBase + off.dwViewMatrix),
                matrix, sizeof(matrix), nullptr);

            uintptr_t entList = ReadMem<uintptr_t>(clientBase + off.dwEntityList);
            Target best{};
            Vec2 center{ w * 0.5f, h * 0.5f };

            for (int i = 1; i <= 64; i++) {
                uintptr_t ent = ReadMem<uintptr_t>(entList + i * 0x10);
                if (!ent || ent == local) continue;

                int hp = ReadMem<int>(ent + off.m_iHealth);
                int team = ReadMem<int>(ent + off.m_iTeam);
                int life = ReadMem<int>(ent + off.m_lifeState);
                bool dormant = ReadMem<bool>(ent + off.m_bDormant);

                if (hp <= 0 || hp > 100 || life != 0 || dormant) continue;
                if (team == myTeam) continue;

                Vec3 origin = ReadMem<Vec3>(ent + off.m_vecOrigin);
                Vec3 head = origin;
                head.z += 64.0f;

                Vec2 sHead{}, sFoot{};
                if (!WorldToScreen(head, sHead, matrix, w, h)) continue;
                if (!WorldToScreen(origin, sFoot, matrix, w, h)) continue;

                if (sHead.x < -100 || sHead.x > w + 100 ||
                    sHead.y < -100 || sHead.y > h + 100) continue;

                int boxH = (int)(sFoot.y - sHead.y);
                if (boxH < 5) continue;
                int boxW = (int)(boxH * 0.52f);
                int boxX = (int)(sHead.x - boxW * 0.5f);
                int boxY = (int)sHead.y;

                if (ESP_BOX) {
                    DrawBox(hdc, boxX, boxY, boxW, boxH, RGB(120, 90, 255));
                    DrawBox(hdc, boxX - 1, boxY - 1, boxW + 2, boxH + 2, RGB(0, 0, 0));
                }

                if (ESP_HEALTH) {
                    DrawHealth(hdc, boxX - 7, boxY, boxH, hp);
                }

                char nameBuf[64];
                snprintf(nameBuf, sizeof(nameBuf), "%d HP", hp);
                DrawTextA(hdc, boxX, boxY - 14, nameBuf, RGB(235, 235, 240));

                float d = std::sqrt(
                    (sHead.x - center.x) * (sHead.x - center.x) +
                    (sHead.y - center.y) * (sHead.y - center.y)
                );

                if (d < AIM_FOV && d < best.dist) {
                    best.dist = d;
                    best.screenHead = sHead;
                    best.valid = true;
                }
            }

            if (AIMBOT && best.valid && (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
                int dx = (int)(best.screenHead.x - center.x);
                int dy = (int)(best.screenHead.y - center.y);
                mouse_event(MOUSEEVENTF_MOVE, dx / AIM_SMOOTH, dy / AIM_SMOOTH, 0, 0);
            }
        }

        ReleaseDC(overlay, hdc);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main() {
    g_pid = GetPID(L"hl2.exe");
    if (!g_pid) g_pid = GetPID(L"cstrike.exe");
    if (!g_pid) return 1;

    g_proc = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
        FALSE, g_pid);
    if (!g_proc) return 2;

    if (!GetModule(g_pid, L"client.dll", clientBase, clientSize)) return 3;
    if (!GetModule(g_pid, L"engine.dll", engineBase, engineSize)) return 4;

    uintptr_t lpSig = FindPattern(clientBase, clientSize, "8B 0D ? ? ? ? 85 C9 74");
    uintptr_t elSig = FindPattern(clientBase, clientSize, "8B 0D ? ? ? ? 85 C9 74 ? 8B 01");
    uintptr_t vmSig = FindPattern(engineBase, engineSize, "0F 10 05 ? ? ? ? 0F 11 05");

    if (lpSig) off.dwLocalPlayer = ResolveMovPtr(lpSig) - clientBase;
    if (elSig) off.dwEntityList = ResolveMovPtr(elSig) - clientBase;
    if (vmSig) off.dwViewMatrix = ResolveMovupsPtr(vmSig) - engineBase;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"CSSV34_OVERLAY";
    RegisterClassExW(&wc);

    HWND overlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"", WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, wc.hInstance, nullptr
    );

    SetLayeredWindowAttributes(overlay, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(overlay, SW_SHOW);

    std::thread render(RenderThread, overlay);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_running = false;
    render.join();
    CloseHandle(g_proc);
    return 0;
}
