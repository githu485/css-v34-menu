// commandvs.cpp — css v34 (VEngineClient011) W2S SCANNER
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <sstream>

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

// ТВОИ КАСТОМНЫЕ ОФФСЕТЫ (НАЙДЕНЫ СКАНЕРОМ)
namespace Off {
    constexpr uintptr_t Health    = 0xDA4;
    constexpr uintptr_t Team      = 0x90;
    constexpr uintptr_t Origin    = 0x1D4;
    constexpr uintptr_t BoneMat   = 0x4E4;
}

// ═══════════════════════════════════════════
//   Безопасное чтение памяти (Защита от крашей)
// ═══════════════════════════════════════════
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

int  Ent_Health(void* e) { int hp = 0; SafeRead((uintptr_t)e + Off::Health, hp); return hp; }
Vector Ent_Origin(void* e) { Vector v; SafeRead((uintptr_t)e + Off::Origin, v); return v; }

// ═══════════════════════════════════════════
//   Фокус и Мышка
// ═══════════════════════════════════════════
HWND g_gameWindow = nullptr;

void FocusGame() {
    if (!g_gameWindow) g_gameWindow = FindWindowA("Valve001", NULL);
    if (g_gameWindow) {
        SetForegroundWindow(g_gameWindow);
        ClipCursor(NULL); 
    }
}

// ═══════════════════════════════════════════
//   W2S INDEX SCANNER (F10)
// ═══════════════════════════════════════════
std::string GenerateDiagnostics() {
    std::stringstream ss;
    ss << "=== COMMANDVS W2S SCANNER ===\r\n\r\n";
    
    int lp = Engine_LocalPlayer();
    if (lp <= 0) return "Not in game!\r\n";

    int sw, sh;
    Engine_ScreenSize(sw, sh);
    ss << "Screen Size: " << sw << " x " << sh << "\r\n";

    // Найдем первого попавшегося врага, чтобы на нем тестить матрицу
    int hi = Entity_Highest();
    void* targetEnt = nullptr;
    for (int i = 1; i <= hi; i++) {
        if (i == lp) continue;
        void* e = Entity_Get(i);
        if (e && IsPlayerEntitySafe(e)) { targetEnt = e; break; }
    }

    if (!targetEnt) return "No target found to test W2S!\r\n";

    Vector feet = Ent_Origin(targetEnt);
    Vector head = feet + Vector(0, 0, 72);
    ss << "Target Origin: X:" << feet.x << " Y:" << feet.y << " Z:" << feet.z << "\r\n\r\n";

    ss << "[ TESTING W2S MATRIX INDEXES 30-45 ]\r\n";

    for (int idx = 30; idx <= 45; idx++) {
        typedef VMatrix*(__thiscall* F)(void*);
        VMatrix* m = vf<F>(g_engine, idx)(g_engine);
        
        if (!m) {
            ss << "Index " << idx << ": NULL pointer\r\n";
            continue;
        }

        // Пробуем перевести координаты головы
        float cw = (*m)[3][0]*head.x + (*m)[3][1]*head.y + (*m)[3][2]*head.z + (*m)[3][3];
        
        if (cw > 0.001f) {
            float iw = 1.0f / cw;
            float sx = ((*m)[0][0]*head.x + (*m)[0][1]*head.y + (*m)[0][2]*head.z + (*m)[0][3]) * iw;
            float sy = ((*m)[1][0]*head.x + (*m)[1][1]*head.y + (*m)[1][2]*head.z + (*m)[1][3]) * iw;
            
            float screenX = (sw * 0.5f) + (sx * sw) * 0.5f;
            float screenY = (sh * 0.5f) - (sy * sh) * 0.5f;

            // Если координаты адекватные
            if (screenX > -2000 && screenX < 2000 && screenY > -2000 && screenY < 2000) {
                ss << "-> Index " << idx << ": VALID! (X:" << screenX << " Y:" << screenY << ")\r\n";
            } else {
                ss << "Index " << idx << ": out of bounds (X:" << screenX << " Y:" << screenY << ")\r\n";
            }
        } else {
            ss << "Index " << idx << ": behind camera\r\n";
        }
    }

    return ss.str();
}

static HWND g_diagWnd = nullptr;

void ToggleDiagWindow() {
    if (IsWindow(g_diagWnd) && IsWindowVisible(g_diagWnd)) {
        ShowWindow(g_diagWnd, SW_HIDE);
        FocusGame();
        return;
    }

    if (!IsWindow(g_diagWnd)) {
        WNDCLASSEXA wc = { sizeof(wc) };
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = "CommandvsDiag";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        RegisterClassExA(&wc);

        g_diagWnd = CreateWindowExA(
            WS_EX_TOPMOST, "CommandvsDiag", "commandvs // W2S SCANNER (F10 to close)",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 600, 500,
            NULL, NULL, wc.hInstance, NULL
        );
    }

    if (g_diagWnd) {
        std::string text = GenerateDiagnostics();
        HWND edit = FindWindowExA(g_diagWnd, NULL, "EDIT", NULL);
        if (edit) {
            SetWindowTextA(edit, text.c_str());
        } else {
            edit = CreateWindowExA(
                0, "EDIT", text.c_str(),
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                10, 10, 560, 440, g_diagWnd, NULL, GetModuleHandleA(NULL), NULL
            );
            HFONT hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
            SendMessageA(edit, WM_SETFONT, (WPARAM)hFont, TRUE);
        }

        ShowWindow(g_diagWnd, SW_SHOW);
        SetForegroundWindow(g_diagWnd);
    }
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

void DrawBox(float x, float y, float w, float h, float thickness, D3DCOLOR color) {
    DrawRect(x, y, w, thickness, color);
    DrawRect(x, y + h - thickness, w, thickness, color);
    DrawRect(x, y, thickness, h, color);
    DrawRect(x + w - thickness, y, thickness, h, color);
}

// ═══════════════════════════════════════════
//   ESP Логика
// ═══════════════════════════════════════════
struct ESP {
    bool enabled = false;
    bool box = true;
    D3DCOLOR color = D3DCOLOR_ARGB(255, 232, 168, 56); 
    D3DCOLOR fillColor = D3DCOLOR_ARGB(40, 232, 168, 56);
} g_esp;

void DrawESP() {
    if (!g_esp.enabled || !g_pDevice) return;

    DWORD oldZ, oldAlpha, oldLighting;
    g_pDevice->GetRenderState(D3DRS_ZENABLE, &oldZ);
    g_pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlpha);
    g_pDevice->GetRenderState(D3DRS_LIGHTING, &oldLighting);

    g_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    DrawRect(10, 10, 20, 20, D3DCOLOR_ARGB(255, 255, 0, 0)); // Хук работает

    if (!Engine_InGame()) return;
    int lp = Engine_LocalPlayer();
    if (lp <= 0) return;

    // Временно используем 37 индекс, чтобы просто рисовать. 
    // Если боксы будут в углу экрана, мы поменяем этот индекс на тот, что найдем.
    typedef VMatrix*(__thiscall* F)(void*);
    VMatrix* m = vf<F>(g_engine, 37)(g_engine);
    if (!m) return;

    int sw, sh;
    Engine_ScreenSize(sw, sh);
    int hi = Entity_Highest();

    for (int i = 1; i <= hi; i++) {
        if (i == lp) continue;
        void* e = Entity_Get(i);
        if (!e || !IsPlayerEntitySafe(e)) continue;

        Vector feet = Ent_Origin(e);
        Vector head = feet + Vector(0, 0, 72);

        float cwH = (*m)[3][0]*head.x + (*m)[3][1]*head.y + (*m)[3][2]*head.z + (*m)[3][3];
        float cwF = (*m)[3][0]*feet.x + (*m)[3][1]*feet.y + (*m)[3][2]*feet.z + (*m)[3][3];

        if (cwH > 0.001f && cwF > 0.001f) {
            float iwH = 1.0f / cwH;
            float sxH = ((*m)[0][0]*head.x + (*m)[0][1]*head.y + (*m)[0][2]*head.z + (*m)[0][3]) * iwH;
            float syH = ((*m)[1][0]*head.x + (*m)[1][1]*head.y + (*m)[1][2]*head.z + (*m)[1][3]) * iwH;
            float sXH = (sw * 0.5f) + (sxH * sw) * 0.5f;
            float sYH = (sh * 0.5f) - (syH * sh) * 0.5f;

            float iwF = 1.0f / cwF;
            float sxF = ((*m)[0][0]*feet.x + (*m)[0][1]*feet.y + (*m)[0][2]*feet.z + (*m)[0][3]) * iwF;
            float syF = ((*m)[1][0]*feet.x + (*m)[1][1]*feet.y + (*m)[1][2]*feet.z + (*m)[1][3]) * iwF;
            float sXF = (sw * 0.5f) + (sxF * sw) * 0.5f;
            float sYF = (sh * 0.5f) - (syF * sh) * 0.5f;

            float h = sYF - sYH;
            if (h > 4 && h < 1800) {
                float w = h * 0.42f;
                float x0 = sXH - w * 0.5f;
                float y0 = sYH;

                DrawRect(x0, y0, w, h, g_esp.fillColor);
                if (g_esp.box) DrawBox(x0, y0, w, h, 1.5f, g_esp.color);
            }
        }
    }

    g_pDevice->SetRenderState(D3DRS_ZENABLE, oldZ);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlpha);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, oldLighting);
}

// ═══════════════════════════════════════════
//   Меню (Win32)
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

D3DCOLOR g_colors[] = {
    D3DCOLOR_ARGB(255, 232, 168, 56), D3DCOLOR_ARGB(255, 120, 90, 255), 
    D3DCOLOR_ARGB(255, 80, 220, 100), D3DCOLOR_ARGB(255, 230, 70, 70),
    D3DCOLOR_ARGB(255, 100, 180, 255), D3DCOLOR_ARGB(255, 255, 255, 255),
    D3DCOLOR_ARGB(255, 255, 120, 200), D3DCOLOR_ARGB(255, 80, 200, 200),
};
COLORREF g_swatches[] = {
    RGB(232, 168, 56), RGB(120, 90, 255), RGB(80, 220, 100), RGB(230, 70, 70),
    RGB(100, 180, 255), RGB(255, 255, 255), RGB(255, 120, 200), RGB(80, 200, 200),
};

enum CtrlType { CTRL_TOGGLE, CTRL_COLOR };
struct Ctrl { RECT r; int type; void* data; std::string label; int id; };
std::vector<Ctrl> g_ctrls;

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
    const char* lbl[] = { "Enable ESP", "Box" };
    bool* vals[] = { &g_esp.enabled, &g_esp.box };
    for (int i = 0; i < 2; i++) {
        Ctrl c; c.r = { cx, y, cx + cw, y + 26 }; c.type = CTRL_TOGGLE; c.data = vals[i]; c.label = lbl[i]; c.id = i;
        g_ctrls.push_back(c); y += 34;
    }
    y += 10;
    for (int i = 0; i < 8; i++) {
        Ctrl sw; sw.r = { cx + i*32, y, cx + i*32 + 24, y + 24 }; sw.type = CTRL_COLOR; sw.data = (void*)(uintptr_t)i; sw.id = i;
        g_ctrls.push_back(sw);
    }
}

void DrawUI(HDC dc) {
    RECT cr; GetClientRect(g_wnd, &cr);
    fillRect(dc, cr, P::BG);
    RECT hr = { 0, 0, cr.right, HH };
    fillGrad(dc, hr, P::HEAD_A, P::HEAD_B);
    fillRect(dc, { 0, 0, 3, HH }, P::ACCENT);
    txt(dc, g_fBold, P::TEXT, 16, 8, "commandvs");
    txt(dc, g_fSm, P::SUBTEXT, 16, 24, "F10 for W2S scan");

    RECT closeR = { cr.right - 36, 10, cr.right - 12, 34 };
    POINT mp; GetCursorPos(&mp); ScreenToClient(g_wnd, &mp);
    if (PtInRect(&closeR, mp)) fillRound(dc, closeR, P::CLOSE_H, 6);
    SelectObject(dc, g_fBold); SetTextColor(dc, P::TEXT); SetBkMode(dc, TRANSPARENT);
    DrawTextA(dc, "×", -1, &closeR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    fillRect(dc, { 0, HH, cr.right, HH + 1 }, P::STROKE);

    for (auto& c : g_ctrls) {
        if (c.type == CTRL_TOGGLE) {
            if (PtInRect(&c.r, mp)) fillRound(dc, c.r, P::ROW_HOV, 4);
            txt(dc, g_fB, P::TEXT, c.r.left + 6, c.r.top + 5, c.label.c_str());
            drawToggle(dc, c.r, *(bool*)c.data);
        }
        else if (c.type == CTRL_COLOR) {
            COLORREF col = g_swatches[c.id];
            bool sel = (g_esp.color == g_colors[c.id]);
            RECT r = c.r;
            if (sel) { RECT ring = { r.left - 2, r.top - 2, r.right + 2, r.bottom + 2 }; strokeRound(dc, ring, P::SW_ACTIVE, 14); }
            fillRound(dc, r, col, 12);
            if (PtInRect(&r, mp)) strokeRound(dc, r, P::ACCENT_HI, 12);
        }
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
            for (auto& c : g_ctrls) {
                if (!PtInRect(&c.r, p)) continue;
                if (c.type == CTRL_TOGGLE) { *(bool*)c.data = !*(bool*)c.data; InvalidateRect(h, NULL, FALSE); }
                else if (c.type == CTRL_COLOR) { g_esp.color = g_colors[c.id]; InvalidateRect(h, NULL, FALSE); }
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
                for (auto& c : g_ctrls) if (PtInRect(&c.r, p)) return HTCLIENT;
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
    RegisterHotKey(g_wnd, 2, MOD_NOREPEAT, VK_F10);
    
    ShowWindow(g_wnd, SW_SHOWNORMAL); SetForegroundWindow(g_wnd);

    MSG msg;
    while (!g_closing && GetMessageA(&msg, NULL, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            if (msg.wParam == 1) {
                if (IsWindowVisible(g_wnd)) { ShowWindow(g_wnd, SW_HIDE); FocusGame(); }
                else { ShowWindow(g_wnd, SW_SHOWNORMAL); SetForegroundWindow(g_wnd); }
            } else if (msg.wParam == 2) {
                ToggleDiagWindow();
            }
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
    HRESULT result = g_origPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    
    g_pDevice = pDevice;
    DrawESP();
    
    return result;
}

void HookD3D9() {
    HWND hwnd = CreateWindowA("STATIC", "dummy", WS_POPUP, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) return;

    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    d3dpp.Windowed = TRUE; 
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hwnd;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;

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
    if (Init()) {
        HookD3D9(); 
    }
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
