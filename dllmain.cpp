// commandvs.cpp — css v34 (build 4044) clean esp
// cl /EHsc /O2 /MD /D_USRDLL /D_WINDLL commandvs.cpp /link /DLL /OUT:commandvs.dll

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")

// ═══════════════════════════════════════════
//   SDK
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
//   Interfaces
// ═══════════════════════════════════════════
typedef void* (*CreateInterfaceFn)(const char*, int*);
void* g_engine = nullptr;
void* g_entityList = nullptr;
void* g_surface = nullptr;

template<typename T> T vf(void* inst, int idx) {
    return (T)(*(void***)inst)[idx];
}

// ═══════════════════════════════════════════
//   Vtable indexes — build 4044
// ═══════════════════════════════════════════
#define VT_ENGINE_LOCALPLAYER   12
#define VT_ENGINE_SCREENSIZE     5
#define VT_ENGINE_W2SMATRIX     37
#define VT_ENGINE_ISINGAME      26
#define VT_ENTITY_GET            3
#define VT_ENTITY_HIGHEST        6
#define VT_SURF_SETCOLOR        15
#define VT_SURF_OUTLINE         19
#define VT_SURF_PAINTTRAVERSE   41

// ═══════════════════════════════════════════
//   Offsets — CSS v34 (build 4044)
// ═══════════════════════════════════════════
namespace Off {
    constexpr uintptr_t Health    = 0xFC;   // m_iHealth
    constexpr uintptr_t Origin    = 0x128;  // m_vecOrigin (позиция ног)
    constexpr uintptr_t BoneMat   = 0x4E4;  // m_dwBoneMatrix
}

int  Engine_LocalPlayer()             { typedef int(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_LOCALPLAYER)(g_engine); }
void Engine_ScreenSize(int& w, int& h){ typedef void(__thiscall* F)(void*,int&,int&); vf<F>(g_engine, VT_ENGINE_SCREENSIZE)(g_engine,w,h); }
VMatrix* Engine_W2SM()                { typedef VMatrix*(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_W2SMATRIX)(g_engine); }
bool Engine_InGame()                  { typedef bool(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_ISINGAME)(g_engine); }
void* Entity_Get(int i)               { typedef void*(__thiscall* F)(void*,int); return vf<F>(g_entityList, VT_ENTITY_GET)(g_entityList, i); }
int   Entity_Highest()                { typedef int(__thiscall* F)(void*); return vf<F>(g_entityList, VT_ENTITY_HIGHEST)(g_entityList); }

// ═══════════════════════════════════════════
//   Защита от вылетов (фильтр сущностей)
// ═══════════════════════════════════════════
bool IsPlayerEntity(void* ent) {
    if (!ent || (uintptr_t)ent < 0x10000) return false;
    // Vtable index 2 = GetClientClass()
    typedef void* (__thiscall* GetClientClassFn)(void*);
    void* cc = vf<GetClientClassFn>(ent, 2)(ent);
    if (!cc) return false;
    const char* name = *(const char**)((uintptr_t)cc + 0x8);
    if (!name) return false;
    // В CSS игроки имеют NetworkName "C_CSPlayer"
    return (strstr(name, "Player") != nullptr || strstr(name, "player") != nullptr);
}

int  Ent_Health(void* e) { return *(int*)((uintptr_t)e + Off::Health); }
Vector Ent_Origin(void* e) { return *(Vector*)((uintptr_t)e + Off::Origin); }
matrix3x4_t* Ent_Bones(void* e) {
    uintptr_t p = *(uintptr_t*)((uintptr_t)e + Off::BoneMat);
    return p ? (matrix3x4_t*)p : nullptr;
}
Vector BonePos(matrix3x4_t* b, int i) {
    if (!b) return {};
    return { b[i].m[0][3], b[i].m[1][3], b[i].m[2][3] };
}

// ═══════════════════════════════════════════
//   World to screen
// ═══════════════════════════════════════════
bool WorldToScreen(const Vector& w, Vector& out) {
    VMatrix* m = Engine_W2SM();
    if (!m) return false;
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
//   ISurface wrappers
// ═══════════════════════════════════════════
void S_SetColor(int r,int g,int b,int a){ typedef void(__thiscall* F)(void*,int,int,int,int); vf<F>(g_surface, VT_SURF_SETCOLOR)(g_surface,r,g,b,a); }
void S_Outline(int x0,int y0,int x1,int y1){ typedef void(__thiscall* F)(void*,int,int,int,int); vf<F>(g_surface, VT_SURF_OUTLINE)(g_surface,x0,y0,x1,y1); }

// ═══════════════════════════════════════════
//   ESP
// ═══════════════════════════════════════════
struct ESP {
    bool enabled = false;
    bool box = true;
    COLORREF boneColor = RGB(232, 168, 56);
} g_esp;

void DrawESP() {
    if (!g_esp.enabled || !Engine_InGame()) return;

    int lp = Engine_LocalPlayer();
    if (lp <= 0) return;

    int r = GetRValue(g_esp.boneColor);
    int g = GetGValue(g_esp.boneColor);
    int bl = GetBValue(g_esp.boneColor);
    S_SetColor(r, g, bl, 255);

    int hi = Entity_Highest();
    for (int i = 1; i <= hi; i++) {
        if (i == lp) continue;
        
        void* e = Entity_Get(i);
        if (!e) continue;
        
        // Защита 1: Это игрок? (Отсекает оружие, пропы, декали)
        if (!IsPlayerEntity(e)) continue;

        // Защита 2: Живой и валидный HP?
        int hp = Ent_Health(e);
        if (hp <= 0 || hp > 1000) continue;

        // Берем позицию ног (Origin) и голову (Bone 8)
        Vector feet = Ent_Origin(e);
        matrix3x4_t* b = Ent_Bones(e);
        if (!b) continue;
        
        Vector head = BonePos(b, 8); // head
        // Если кость головы уехала в 0,0,0 (баг), берем origin + 72 юнита вверх
        if (head.Length() < 0.1f) {
            head = feet + Vector(0, 0, 72);
        }

        Vector sH, sF;
        if (!WorldToScreen(head, sH) || !WorldToScreen(feet, sF)) continue;
        
        float h = sF.y - sH.y;
        if (h < 4 || h > 1800) continue; // Защита от гигантских/микроскопических боксов
        float w = h * 0.42f;

        int x0 = (int)(sH.x - w * 0.5f);
        int x1 = (int)(sH.x + w * 0.5f);
        int y0 = (int)sH.y;
        int y1 = (int)sF.y;

        if (g_esp.box) {
            S_Outline(x0, y0, x1, y1);
        }
    }
}

// ═══════════════════════════════════════════
//   Menu palette
// ═══════════════════════════════════════════
namespace P {
    const COLORREF BG        = RGB(18, 19, 23);
    const COLORREF HEAD_A    = RGB(30, 32, 40);
    const COLORREF HEAD_B    = RGB(22, 24, 30);
    const COLORREF STROKE    = RGB(46, 48, 58);
    const COLORREF TEXT      = RGB(232, 233, 240);
    const COLORREF SUBTEXT   = RGB(135, 137, 148);
    const COLORREF ACCENT    = RGB(232, 168, 56);
    const COLORREF ACCENT_HI = RGB(248, 200, 104);
    const COLORREF ON        = RGB(232, 168, 56);
    const COLORREF OFF       = RGB(48, 50, 58);
    const COLORREF KNOB      = RGB(240, 241, 246);
    const COLORREF CLOSE_H   = RGB(210, 70, 70);
    const COLORREF ROW_HOV   = RGB(24, 26, 32);
    const COLORREF SW_ACTIVE = RGB(240, 241, 246);
}

const int WW = 320, WH = 280, HH = 44, TBH = 34;

HWND g_wnd = nullptr;
HFONT g_fBold = nullptr, g_fB = nullptr, g_fSm = nullptr;
volatile bool g_closing = false;

COLORREF g_swatches[] = {
    RGB(232, 168, 56), RGB(120, 90, 255), RGB(80, 220, 100), RGB(230, 70, 70),
    RGB(100, 180, 255), RGB(255, 255, 255), RGB(255, 120, 200), RGB(80, 200, 200),
};
int g_swatchCount = 8;

enum CtrlType { CTRL_TOGGLE, CTRL_COLOR };
struct Ctrl {
    RECT r;
    int type;
    void* data;
    std::string label;
    int id;
};
std::vector<Ctrl> g_ctrls;

// ═══════════════════════════════════════════
//   GDI helpers
// ═══════════════════════════════════════════
void fillRect(HDC dc, RECT r, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c);
    FillRect(dc, &r, br);
    DeleteObject(br);
}

void fillGrad(HDC dc, RECT r, COLORREF top, COLORREF bot) {
    TRIVERTEX v[2] = {
        { r.left, r.top,    (COLOR16)(GetRValue(top)<<8), (COLOR16)(GetGValue(top)<<8), (COLOR16)(GetBValue(top)<<8), 0xFF00 },
        { r.right, r.bottom,(COLOR16)(GetRValue(bot)<<8), (COLOR16)(GetGValue(bot)<<8), (COLOR16)(GetBValue(bot)<<8), 0xFF00 }
    };
    GRADIENT_RECT gr = { 0, 1 };
    GradientFill(dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V);
}

void fillRound(HDC dc, RECT r, COLORREF c, int rad) {
    HBRUSH br = CreateSolidBrush(c);
    HPEN pen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ ob = SelectObject(dc, br);
    HGDIOBJ op = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
    DeleteObject(pen);
}

void strokeRound(HDC dc, RECT r, COLORREF c, int rad) {
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
}

void txt(HDC dc, HFONT f, COLORREF c, int x, int y, const char* s) {
    SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    TextOutA(dc, x, y, s, (int)strlen(s));
}

void drawToggle(HDC dc, RECT row, bool on) {
    int h = 18, w = 34;
    RECT t = { row.right - w - 4, row.top + (row.bottom - row.top - h) / 2,
               row.right - 4, row.top + (row.bottom - row.top - h) / 2 + h };
    fillRound(dc, t, on ? P::ON : P::OFF, h);
    int ks = h - 6;
    RECT k = { on ? (t.right - ks - 3) : (t.left + 3), t.top + 3,
               on ? (t.right - 3) : (t.left + 3 + ks), t.top + 3 + ks };
    fillRound(dc, k, P::KNOB, ks);
}

// ═══════════════════════════════════════════
//   Layout
// ═══════════════════════════════════════════
void LayoutUI() {
    g_ctrls.clear();
    int cx = 16;
    int cw = WW - 32;
    int y = HH + TBH + 16;

    const char* lbl[] = { "Enable ESP", "Box" };
    bool* vals[] = { &g_esp.enabled, &g_esp.box };
    for (int i = 0; i < 2; i++) {
        Ctrl c;
        c.r = { cx, y, cx + cw, y + 26 };
        c.type = CTRL_TOGGLE;
        c.data = vals[i];
        c.label = lbl[i];
        c.id = i;
        g_ctrls.push_back(c);
        y += 34;
    }

    y += 10;
    for (int i = 0; i < g_swatchCount; i++) {
        Ctrl sw;
        sw.r = { cx + i*32, y, cx + i*32 + 24, y + 24 };
        sw.type = CTRL_COLOR;
        sw.data = (void*)(uintptr_t)i;
        sw.id = i;
        g_ctrls.push_back(sw);
    }
}

// ═══════════════════════════════════════════
//   Draw
// ═══════════════════════════════════════════
void DrawUI(HDC dc) {
    RECT cr; GetClientRect(g_wnd, &cr);
    fillRect(dc, cr, P::BG);

    RECT hr = { 0, 0, cr.right, HH };
    fillGrad(dc, hr, P::HEAD_A, P::HEAD_B);
    fillRect(dc, { 0, 0, 3, HH }, P::ACCENT);

    txt(dc, g_fBold, P::TEXT, 16, 8, "commandvs");
    txt(dc, g_fSm, P::SUBTEXT, 16, 24, "css v34 // clean");

    RECT closeR = { cr.right - 36, 10, cr.right - 12, 34 };
    POINT mp; GetCursorPos(&mp); ScreenToClient(g_wnd, &mp);
    bool closeHover = PtInRect(&closeR, mp);
    if (closeHover) fillRound(dc, closeR, P::CLOSE_H, 6);
    SelectObject(dc, g_fBold);
    SetTextColor(dc, P::TEXT);
    SetBkMode(dc, TRANSPARENT);
    DrawTextA(dc, "×", -1, &closeR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    fillRect(dc, { 0, HH, cr.right, HH + 1 }, P::STROKE);

    for (auto& c : g_ctrls) {
        if (c.type == CTRL_TOGGLE) {
            bool hover = PtInRect(&c.r, mp);
            if (hover) fillRound(dc, c.r, P::ROW_HOV, 4);
            txt(dc, g_fB, P::TEXT, c.r.left + 6, c.r.top + 5, c.label.c_str());
            drawToggle(dc, c.r, *(bool*)c.data);
        }
        else if (c.type == CTRL_COLOR) {
            int idx = c.id;
            COLORREF col = g_swatches[idx];
            bool sel = (g_esp.boneColor == col);
            RECT r = c.r;
            if (sel) {
                RECT ring = { r.left - 2, r.top - 2, r.right + 2, r.bottom + 2 };
                strokeRound(dc, ring, P::SW_ACTIVE, 14);
            }
            fillRound(dc, r, col, 12);
            bool hover = PtInRect(&r, mp);
            if (hover) strokeRound(dc, r, P::ACCENT_HI, 12);
        }
    }
}

// ═══════════════════════════════════════════
//   Window proc
// ═══════════════════════════════════════════
LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_fBold = CreateFontA(13, 0,0,0, FW_BOLD,   0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_fB    = CreateFontA(13, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_fSm   = CreateFontA(11, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            LayoutUI();
            return 0;
        }

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_MOUSEMOVE: {
            InvalidateRect(h, NULL, FALSE);
            TRACKMOUSEEVENT tme = { sizeof tme, TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE: {
            InvalidateRect(h, NULL, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            RECT closeR = { WW - 36, 10, WW - 12, 34 };
            if (PtInRect(&closeR, p)) { ShowWindow(h, SW_HIDE); return 0; }
            for (auto& c : g_ctrls) {
                if (!PtInRect(&c.r, p)) continue;
                if (c.type == CTRL_TOGGLE) {
                    *(bool*)c.data = !*(bool*)c.data;
                    InvalidateRect(h, NULL, FALSE);
                } else if (c.type == CTRL_COLOR) {
                    g_esp.boneColor = g_swatches[c.id];
                    InvalidateRect(h, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, WW, WH);
            HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
            DrawUI(mem);
            BitBlt(dc, 0, 0, WW, WH, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(h, msg, wp, lp);
            if (hit == HTCLIENT) {
                POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ScreenToClient(h, &p);
                RECT closeR = { WW - 36, 10, WW - 12, 34 };
                if (PtInRect(&closeR, p)) return HTCLIENT;
                for (auto& c : g_ctrls) if (PtInRect(&c.r, p)) return HTCLIENT;
                return HTCAPTION;
            }
            return hit;
        }

        case WM_CLOSE:
            ShowWindow(h, SW_HIDE);
            return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

// ═══════════════════════════════════════════
//   Menu thread
// ═══════════════════════════════════════════
DWORD WINAPI MenuThread(LPVOID) {
    WNDCLASSEXA wc = { sizeof wc };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "CommandvsMenu";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExA(&wc);

    g_wnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        "CommandvsMenu", "commandvs",
        WS_POPUP,
        (GetSystemMetrics(SM_CXSCREEN) - WW) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - WH) / 2,
        WW, WH, NULL, NULL, wc.hInstance, NULL);
    if (!g_wnd) return 1;

    HRGN rgn = CreateRoundRectRgn(0, 0, WW + 1, WH + 1, 12, 12);
    SetWindowRgn(g_wnd, rgn, TRUE);

    RegisterHotKey(g_wnd, 1, MOD_NOREPEAT, VK_INSERT);
    ShowWindow(g_wnd, SW_SHOW);

    MSG msg;
    while (!g_closing && GetMessageA(&msg, NULL, 0, 0)) {
        if (msg.message == WM_HOTKEY && msg.wParam == 1) {
            if (IsWindowVisible(g_wnd)) ShowWindow(g_wnd, SW_HIDE);
            else { ShowWindow(g_wnd, SW_SHOW); }
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}

// ═══════════════════════════════════════════
//   PaintTraverse hook
// ═══════════════════════════════════════════
typedef void(__thiscall* PaintTraverseFn)(void*, unsigned int, bool, bool);
PaintTraverseFn g_origPaint = nullptr;

void __fastcall hkPaintTraverse(void* self, void*, unsigned int panel, bool a, bool b) {
    g_origPaint(self, panel, a, b);
    DrawESP();
}

bool HookPaintTraverse() {
    if (!g_surface) return false;
    void** vt = *(void***)g_surface;
    g_origPaint = (PaintTraverseFn)vt[VT_SURF_PAINTTRAVERSE];
    DWORD old;
    VirtualProtect(&vt[VT_SURF_PAINTTRAVERSE], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
    vt[VT_SURF_PAINTTRAVERSE] = (void*)hkPaintTraverse;
    VirtualProtect(&vt[VT_SURF_PAINTTRAVERSE], sizeof(void*), old, &old);
    return true;
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

    g_engine     = cifE("VEngineClient014", nullptr);
    if (!g_engine) g_engine = cifE("VEngineClient013", nullptr);
    g_entityList = cifC("VClientEntityList003", nullptr);
    g_surface    = cifC("VGUI_Surface031", nullptr);
    if (!g_surface) g_surface = cifC("VGUI_Surface030", nullptr);

    return g_engine && g_entityList && g_surface;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        if (!Init()) return FALSE;
        HookPaintTraverse();
        CreateThread(NULL, 0, MenuThread, NULL, 0, NULL);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        g_closing = true;
        if (g_wnd) PostMessageA(g_wnd, WM_QUIT, 0, 0);
    }
    return TRUE;
}
