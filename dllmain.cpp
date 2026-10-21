// commandvs.cpp — css v34 (VEngineClient011) BLUE BOX TEST
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "d3d9.lib")

// ═══════════════════════════════════════════
//   SDK & Math
// ═══════════════════════════════════════════
struct Vector {
    float x, y, z;
    Vector() : x(0), y(0), z(0) {}
    Vector(float a, float b, float c) : x(a), y(b), z(c) {}
    Vector operator+(const Vector& v) const { return { x+v.x, y+v.y, z+v.z }; }
    Vector operator-(const Vector& v) const { return { x-v.x, y-v.y, z-v.z }; }
    float Length() const { return sqrtf(x*x + y*y + z*z); }
};
struct matrix3x4_t { float m[3][4]; };
struct VMatrix { float m[4][4]; float* operator[](int i){ return m[i]; } const float* operator[](int i) const { return m[i]; } };

// ═══════════════════════════════════════════
//   Interfaces & Offsets
// ═══════════════════════════════════════════
typedef void* (*CreateInterfaceFn)(const char*, int*);
void* g_engine = nullptr;
void* g_entityList = nullptr;

template<typename T> T vf(void* inst, int idx) {
    return (T)(*(void***)inst)[idx];
}

#define VT_ENGINE_LOCALPLAYER   12
#define VT_ENGINE_SCREENSIZE     5
#define VT_ENGINE_ISINGAME      26
#define VT_ENTITY_GET            3
#define VT_ENTITY_HIGHEST        6

// ОФФСЕТЫ
namespace Off {
    constexpr uintptr_t Health    = 0xDA4;
    constexpr uintptr_t Origin    = 0x1b18; 
    constexpr uintptr_t BoneMat   = 0x4E4;
    constexpr uintptr_t W2SMatrix = 0x3094; 
}

template<typename T>
bool SafeRead(uintptr_t addr, T& out) {
    if (addr < 0x10000) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
            out = *(T*)addr;
            return true;
        }
    }
    return false;
}

int  Engine_LocalPlayer()             { typedef int(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_LOCALPLAYER)(g_engine); }
void Engine_ScreenSize(int& w, int& h){ typedef void(__thiscall* F)(void*,int&,int&); vf<F>(g_engine, VT_ENGINE_SCREENSIZE)(g_engine,w,h); }
bool Engine_InGame()                  { typedef bool(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_ISINGAME)(g_engine); }
void* Entity_Get(int i)               { typedef void*(__thiscall* F)(void*,int); return vf<F>(g_entityList, VT_ENTITY_GET)(g_entityList, i); }
int   Entity_Highest()                { typedef int(__thiscall* F)(void*); return vf<F>(g_entityList, VT_ENTITY_HIGHEST)(g_entityList); }

bool IsPlayerEntitySafe(void* ent) {
    if (!ent) return false;
    int hp = 0;
    if (!SafeRead((uintptr_t)ent + Off::Health, hp)) return false;
    if (hp <= 0 || hp > 100) return false;
    uintptr_t bonePtr = 0;
    if (!SafeRead((uintptr_t)ent + Off::BoneMat, bonePtr)) return false;
    if (bonePtr < 0x10000000 || bonePtr > 0x7FFFFFFF) return false;
    return true;
}

Vector Ent_Origin(void* e) { Vector v; SafeRead((uintptr_t)e + Off::Origin, v); return v; }

bool WorldToScreen(const Vector& w, Vector& out) {
    VMatrix* m = (VMatrix*)((uintptr_t)g_engine + Off::W2SMatrix);
    float cw = (*m)[3][0]*w.x + (*m)[3][1]*w.y + (*m)[3][2]*w.z + (*m)[3][3];
    if (cw < 0.001f) return false;
    float iw = 1.0f / cw;
    float sx = ((*m)[0][0]*w.x + (*m)[0][1]*w.y + (*m)[0][2]*w.z + (*m)[0][3]) * iw;
    float sy = ((*m)[1][0]*w.x + (*m)[1][1]*w.y + (*m)[1][2]*w.z + (*m)[1][3]) * iw;
    
    int sw, sh; Engine_ScreenSize(sw, sh);
    out.x = (sw * 0.5f) + (sx * sw) * 0.5f;
    out.y = (sh * 0.5f) - (sy * sh) * 0.5f;
    return true;
}

// ═══════════════════════════════════════════
//   Фокус и Мышка
// ═══════════════════════════════════════════
HWND g_gameWindow = nullptr;
void FocusGame() {
    if (!g_gameWindow) g_gameWindow = FindWindowA("Valve001", NULL);
    if (g_gameWindow) { SetForegroundWindow(g_gameWindow); ClipCursor(NULL); }
}

// ═══════════════════════════════════════════
//   DirectX 9 Render
// ═══════════════════════════════════════════
IDirect3DDevice9* g_pDevice = nullptr;

void DrawRect(float x, float y, float w, float h, D3DCOLOR color) {
    if (!g_pDevice) return;
    struct Vertex { float x, y, z, rhw; D3DCOLOR color; };
    Vertex v[4] = {
        { x,     y,     0.0f, 1.0f, color },
        { x + w, y,     0.0f, 1.0f, color },
        { x,     y + h, 0.0f, 1.0f, color },
        { x + w, y + h, 0.0f, 1.0f, color }
    };
    g_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_pDevice->SetTexture(0, NULL);
    g_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vertex));
}

void DrawLine(float x1, float y1, float x2, float y2, D3DCOLOR color) {
    if (!g_pDevice) return;
    struct Vertex { float x, y, z, rhw; D3DCOLOR color; };
    Vertex v[2] = { { x1, y1, 0.0f, 1.0f, color }, { x2, y2, 0.0f, 1.0f, color } };
    g_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_pDevice->SetTexture(0, NULL);
    g_pDevice->DrawPrimitiveUP(D3DPT_LINELIST, 1, v, sizeof(Vertex));
}

// ═══════════════════════════════════════════
//   ESP Логика
// ═══════════════════════════════════════════
struct ESP {
    bool enabled = true; 
    bool blueBox = false; // Состояние тумблера Blue Box
} g_esp;

void DrawESP() {
    if (!g_pDevice) return;

    int sw, sh; 
    if (Engine_ScreenSize) Engine_ScreenSize(sw, sh); else return;
    int centerX = sw / 2;
    int centerY = sh / 2;

    // Рисуем синий квадрат по центру, если включен тумблер
    if (g_esp.blueBox) {
        DrawRect(centerX - 50, centerY - 50, 100, 100, D3DCOLOR_ARGB(255, 0, 0, 255));
    }

    if (!Engine_InGame() || !g_esp.enabled) return;

    int lp = Engine_LocalPlayer();
    if (lp <= 0) return;

    int hi = Entity_Highest();
    for (int i = 1; i <= hi; i++) {
        if (i == lp) continue;
        void* e = Entity_Get(i);
        if (!e || !IsPlayerEntitySafe(e)) continue;

        Vector feet = Ent_Origin(e);
        Vector head = feet + Vector(0, 0, 72);

        Vector sH, sF;
        if (WorldToScreen(head, sH) && WorldToScreen(feet, sF)) {
            // Линии к врагам
            DrawLine(centerX, centerY, sH.x, sH.y, D3DCOLOR_ARGB(255, 0, 255, 0)); // К голове
            DrawLine(centerX, centerY, sF.x, sF.y, D3DCOLOR_ARGB(255, 255, 0, 0)); // К ногам
        }
    }
}

// ═══════════════════════════════════════════
//   Меню (Win32)
// ═══════════════════════════════════════════
namespace P {
    const COLORREF BG = RGB(18, 19, 23), HEAD_A = RGB(30, 32, 40), HEAD_B = RGB(22, 24, 30);
    const COLORREF STROKE = RGB(46, 48, 58), TEXT = RGB(232, 233, 240), SUBTEXT = RGB(135, 137, 148);
    const COLORREF ACCENT = RGB(232, 168, 56), ON = RGB(232, 168, 56), OFF = RGB(48, 50, 58);
    const COLORREF KNOB = RGB(240, 241, 246), CLOSE_H = RGB(210, 70, 70), ROW_HOV = RGB(24, 26, 32);
}

const int WW = 320, WH = 220, HH = 44, TBH = 34;
HWND g_wnd = nullptr;
HFONT g_fBold = nullptr, g_fB = nullptr, g_fSm = nullptr;
volatile bool g_closing = false;
std::vector<RECT> g_ctrls;

void fillRect(HDC dc, RECT r, COLORREF c) { HBRUSH br = CreateSolidBrush(c); FillRect(dc, &r, br); DeleteObject(br); }
void fillGrad(HDC dc, RECT r, COLORREF top, COLORREF bot) {
    TRIVERTEX v[2] = { { r.left, r.top, (COLOR16)(GetRValue(top)<<8), (COLOR16)(GetGValue(top)<<8), (COLOR16)(GetBValue(top)<<8), 0xFF00 },
                       { r.right, r.bottom,(COLOR16)(GetRValue(bot)<<8), (COLOR16)(GetGValue(bot)<<8), (COLOR16)(GetBValue(bot)<<8), 0xFF00 } };
    GRADIENT_RECT gr = { 0, 1 }; GradientFill(dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V);
}
void fillRound(HDC dc, RECT r, COLORREF c, int rad) {
    HBRUSH br = CreateSolidBrush(c); HPEN pen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ ob = SelectObject(dc, br); HGDIOBJ op = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(br); DeleteObject(pen);
}
void strokeRound(HDC dc, RECT r, COLORREF c, int rad) {
    HPEN pen = CreatePen(PS_SOLID, 1, c); HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(pen);
}
void txt(HDC dc, HFONT f, COLORREF c, int x, int y, const char* s) {
    SelectObject(dc, f); SetTextColor(dc, c); SetBkMode(dc, TRANSPARENT); TextOutA(dc, x, y, s, (int)strlen(s));
}
void drawToggle(HDC dc, RECT row, bool on) {
    int h = 18, w = 34;
    RECT t = { row.right - w - 4, row.top + (row.bottom - row.top - h) / 2, row.right - 4, row.top + (row.bottom - row.top - h) / 2 + h };
    fillRound(dc, t, on ? P::ON : P::OFF, h);
    int ks = h - 6;
    RECT k = { on ? (t.right - ks - 3) : (t.left + 3), t.top + 3, on ? (t.right - 3) : (t.left + 3 + ks), t.top + 3 + ks };
    fillRound(dc, k, P::KNOB, ks);
}

void LayoutUI() {
    g_ctrls.clear();
    int cx = 16, cw = WW - 32, y = HH + TBH + 16;
    // Тумблер 1: Enable ESP
    g_ctrls.push_back({ cx, y, cx + cw, y + 26 });
    y += 34;
    // Тумблер 2: Blue Box
    g_ctrls.push_back({ cx, y, cx + cw, y + 26 });
}

void DrawUI(HDC dc) {
    RECT cr; GetClientRect(g_wnd, &cr);
    fillRect(dc, cr, P::BG);
    RECT hr = { 0, 0, cr.right, HH };
    fillGrad(dc, hr, P::HEAD_A, P::HEAD_B);
    fillRect(dc, { 0, 0, 3, HH }, P::ACCENT);
    txt(dc, g_fBold, P::TEXT, 16, 8, "commandvs");
    txt(dc, g_fSm, P::SUBTEXT, 16, 24, "css v34 // TEST MENU");

    RECT closeR = { cr.right - 36, 10, cr.right - 12, 34 };
    POINT mp; GetCursorPos(&mp); ScreenToClient(g_wnd, &mp);
    if (PtInRect(&closeR, mp)) fillRound(dc, closeR, P::CLOSE_H, 6);
    SelectObject(dc, g_fBold); SetTextColor(dc, P::TEXT); SetBkMode(dc, TRANSPARENT);
    DrawTextA(dc, "×", -1, &closeR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    fillRect(dc, { 0, HH, cr.right, HH + 1 }, P::STROKE);

    if (g_ctrls.size() >= 2) {
        // Рисуем Enable ESP
        RECT r1 = g_ctrls[0];
        if (PtInRect(&r1, mp)) fillRound(dc, r1, P::ROW_HOV, 4);
        txt(dc, g_fB, P::TEXT, r1.left + 6, r1.top + 5, "Enable ESP");
        drawToggle(dc, r1, g_esp.enabled);

        // Рисуем Blue Box
        RECT r2 = g_ctrls[1];
        if (PtInRect(&r2, mp)) fillRound(dc, r2, P::ROW_HOV, 4);
        txt(dc, g_fB, P::TEXT, r2.left + 6, r2.top + 5, "Blue Box");
        drawToggle(dc, r2, g_esp.blueBox);
    }
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            g_fBold = CreateFontA(13, 0,0,0, FW_BOLD,   0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_fB    = CreateFontA(13, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_fSm   = CreateFontA(11, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            LayoutUI(); return 0;
        case WM_MOUSEMOVE:
            InvalidateRect(h, NULL, FALSE);
            { TRACKMOUSEEVENT tme = { sizeof tme, TME_LEAVE, h, 0 }; TrackMouseEvent(&tme); }
            return 0;
        case WM_MOUSELEAVE:
            InvalidateRect(h, NULL, FALSE); return 0;
        case WM_LBUTTONDOWN: {
            POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            RECT closeR = { WW - 36, 10, WW - 12, 34 };
            if (PtInRect(&closeR, p)) { ShowWindow(h, SW_HIDE); FocusGame(); return 0; }
            
            if (g_ctrls.size() >= 2) {
                if (PtInRect(&g_ctrls[0], p)) { g_esp.enabled = !g_esp.enabled; InvalidateRect(h, NULL, FALSE); }
                if (PtInRect(&g_ctrls[1], p)) { g_esp.blueBox = !g_esp.blueBox; InvalidateRect(h, NULL, FALSE); }
            }
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
            HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, WW, WH);
            HBITMAP old = (HBITMAP)SelectObject(mem, bmp); DrawUI(mem);
            BitBlt(dc, 0, 0, WW, WH, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0;
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(h, msg, wp, lp);
            if (hit == HTCLIENT) {
                POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }; ScreenToClient(h, &p);
                RECT closeR = { WW - 36, 10, WW - 12, 34 };
                if (PtInRect(&closeR, p)) return HTCLIENT;
                if (g_ctrls.size() >= 2) {
                    if (PtInRect(&g_ctrls[0], p)) return HTCLIENT;
                    if (PtInRect(&g_ctrls[1], p)) return HTCLIENT;
                }
                return HTCAPTION;
            }
            return hit;
        }
        case WM_CLOSE: ShowWindow(h, SW_HIDE); FocusGame(); return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

DWORD WINAPI MenuThread(LPVOID) {
    WNDCLASSEXA wc = { sizeof wc };
    wc.lpfnWndProc = WndProc; wc.hInstance = GetModuleHandleA(NULL);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); wc.lpszClassName = "CommandvsMenu"; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExA(&wc);
    g_wnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "CommandvsMenu", "commandvs", WS_POPUP, 
        (GetSystemMetrics(SM_CXSCREEN) - WW) / 2, (GetSystemMetrics(SM_CYSCREEN) - WH) / 2, WW, WH, NULL, NULL, wc.hInstance, NULL);
    if (!g_wnd) return 1;
    HRGN rgn = CreateRoundRectRgn(0, 0, WW + 1, WH + 1, 12, 12); SetWindowRgn(g_wnd, rgn, TRUE);
    RegisterHotKey(g_wnd, 1, MOD_NOREPEAT, VK_INSERT);
    ShowWindow(g_wnd, SW_SHOWNORMAL); SetForegroundWindow(g_wnd);

    MSG msg;
    while (!g_closing && GetMessageA(&msg, NULL, 0, 0)) {
        if (msg.message == WM_HOTKEY && msg.wParam == 1) {
            if (IsWindowVisible(g_wnd)) { ShowWindow(g_wnd, SW_HIDE); FocusGame(); }
            else { ShowWindow(g_wnd, SW_SHOWNORMAL); SetForegroundWindow(g_wnd); }
            continue;
        }
        TranslateMessage(&msg); DispatchMessageA(&msg);
    }
    return 0;
}

// ═══════════════════════════════════════════
//   D3D9 Хук (Present)
// ═══════════════════════════════════════════
typedef HRESULT(__stdcall* Present_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
Present_t g_origPresent = nullptr;

HRESULT __stdcall hkPresent(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    g_pDevice = pDevice;

    // Сохраняем настройки
    DWORD oldZ, oldAlpha, oldLighting, oldCull, oldScissor, oldBlendSrc, oldBlendDest;
    g_pDevice->GetRenderState(D3DRS_ZENABLE, &oldZ);
    g_pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlpha);
    g_pDevice->GetRenderState(D3DRS_LIGHTING, &oldLighting);
    g_pDevice->GetRenderState(D3DRS_CULLMODE, &oldCull);
    g_pDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &oldScissor);
    g_pDevice->GetRenderState(D3DRS_SRCBLEND, &oldBlendSrc);
    g_pDevice->GetRenderState(D3DRS_DESTBLEND, &oldBlendDest);

    // Ставим свои
    g_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    // Рисуем
    DrawESP();

    // Возвращаем
    g_pDevice->SetRenderState(D3DRS_ZENABLE, oldZ);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlpha);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, oldLighting);
    g_pDevice->SetRenderState(D3DRS_CULLMODE, oldCull);
    g_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, oldScissor);
    g_pDevice->SetRenderState(D3DRS_SRCBLEND, oldBlendSrc);
    g_pDevice->SetRenderState(D3DRS_DESTBLEND, oldBlendDest);

    return g_origPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

void HookD3D9() {
    HWND hwnd = CreateWindowA("STATIC", "dummy", WS_POPUP, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) return;
    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    d3dpp.Windowed = TRUE; d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hwnd; d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    IDirect3DDevice9* pDevice = nullptr;
    HRESULT hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice);
    if (pDevice) {
        void** vTable = *(void***)pDevice;
        g_origPresent = (Present_t)vTable[17];
        DWORD old;
        VirtualProtect(&vTable[17], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
        vTable[17] = (void*)hkPresent;
        VirtualProtect(&vTable[17], sizeof(void*), old, &old);
        pDevice->Release();
    }
    pD3D->Release();
    DestroyWindow(hwnd);
}

// ═══════════════════════════════════════════
//   Init + DllMain
// ═══════════════════════════════════════════
bool Init() {
    HMODULE eng = GetModuleHandleA("engine.dll");
    HMODULE cli = GetModuleHandleA("client.dll");
    if (!eng || !cli) return false;
    auto cifE = (CreateInterfaceFn)GetProcAddress(eng, "CreateInterface");
    auto cifC = (CreateInterfaceFn)GetProcAddress(cli, "CreateInterface");
    if (!cifE || !cifC) return false;
    g_engine     = cifE("VEngineClient011", nullptr);
    g_entityList = cifC("VClientEntityList003", nullptr);
    return g_engine && g_entityList;
}

DWORD WINAPI InitThread(LPVOID) {
    Sleep(2000); 
    if (Init()) HookD3D9(); 
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
        CreateThread(NULL, 0, MenuThread, NULL, 0, NULL);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        g_closing = true; if (g_wnd) PostMessageA(g_wnd, WM_QUIT, 0, 0);
    }
    return TRUE;
}
