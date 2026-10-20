// commandvs.cpp — css v34 (build 4044) dx9 esp DEBUG
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
#define VT_ENGINE_W2SMATRIX     37
#define VT_ENGINE_ISINGAME      26
#define VT_ENTITY_GET            3
#define VT_ENTITY_HIGHEST        6

namespace Off {
    constexpr uintptr_t Health    = 0xFC;   
    constexpr uintptr_t Origin    = 0x128;  
    constexpr uintptr_t BoneMat   = 0x4E4;  
}

int  Engine_LocalPlayer()             { typedef int(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_LOCALPLAYER)(g_engine); }
void Engine_ScreenSize(int& w, int& h){ typedef void(__thiscall* F)(void*,int&,int&); vf<F>(g_engine, VT_ENGINE_SCREENSIZE)(g_engine,w,h); }
VMatrix* Engine_W2SM()                { typedef VMatrix*(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_W2SMATRIX)(g_engine); }
bool Engine_InGame()                  { typedef bool(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_ISINGAME)(g_engine); }
void* Entity_Get(int i)               { typedef void*(__thiscall* F)(void*,int); return vf<F>(g_entityList, VT_ENTITY_GET)(g_entityList, i); }
int   Entity_Highest()                { typedef int(__thiscall* F)(void*); return vf<F>(g_entityList, VT_ENTITY_HIGHEST)(g_entityList); }

bool IsPlayerEntity(void* ent) {
    if (!ent || (uintptr_t)ent < 0x10000) return false;
    typedef void* (__thiscall* GetClientClassFn)(void*);
    void* cc = vf<GetClientClassFn>(ent, 2)(ent);
    if (!cc) return false;
    const char* name = *(const char**)((uintptr_t)cc + 0x8);
    if (!name) return false;
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
//   ESP Логика (DEBUG)
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

    // 1. КРАСНЫЙ: Хук работает
    DrawRect(10, 10, 20, 20, D3DCOLOR_ARGB(255, 255, 0, 0));

    if (!Engine_InGame()) return;
    // 2. ЗЕЛЕНЫЙ: Мы на карте
    DrawRect(40, 10, 20, 20, D3DCOLOR_ARGB(255, 0, 255, 0));

    int lp = Engine_LocalPlayer();
    if (lp <= 0) return;
    // 3. СИНИЙ: Локальный игрок найден
    DrawRect(70, 10, 20, 20, D3DCOLOR_ARGB(255, 0, 0, 255));

    int validHealthCount = 0;
    int validBoneCount = 0;
    int validW2SCount = 0;

    int hi = Entity_Highest();
    for (int i = 1; i <= hi; i++) {
        if (i == lp) continue;
        void* e = Entity_Get(i);
        if (!e || !IsPlayerEntity(e)) continue;

        int hp = Ent_Health(e);
        if (hp > 0 && hp <= 100) {
            validHealthCount++;
            
            matrix3x4_t* b = Ent_Bones(e);
            if (b) {
                Vector head = BonePos(b, 8);
                Vector feet = Ent_Origin(e);

                // Проверка на нулевые координаты
                if (head.x != 0.0f && head.z != 0.0f && fabsf(head.x) < 10000.0f) {
                    validBoneCount++;

                    Vector sH, sF;
                    if (WorldToScreen(head, sH) && WorldToScreen(feet, sF)) {
                        validW2SCount++;
                        
                        float h = sF.y - sH.y;
                        if (h > 4 && h < 1800) {
                            float w = h * 0.42f;
                            float x0 = sH.x - w * 0.5f;
                            float y0 = sH.y;

                            DrawRect(x0, y0, w, h, g_esp.fillColor);
                            if (g_esp.box) DrawBox(x0, y0, w, h, 1.5f, g_esp.color);
                        }
                    }
                }
            }
        }
    }

    // 4. БЕЛЫЙ: Найдены игроки по HP
    if (validHealthCount > 0) DrawRect(100, 10, 20, 20, D3DCOLOR_ARGB(255, 255, 255, 255));
    // 5. ЖЕЛТЫЙ: Кости прочитаны
    if (validBoneCount > 0) DrawRect(130, 10, 20, 20, D3DCOLOR_ARGB(255, 255, 255, 0));
    // 6. ФИОЛЕТОВЫЙ: W2S сработал
    if (validW2SCount > 0) DrawRect(160, 10, 20, 20, D3DCOLOR_ARGB(255, 255, 0, 255));

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
    txt(dc, g_fSm, P::SUBTEXT, 16, 24, "css v34 // debug");

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
            if (PtInRect(&closeR, p)) { ShowWindow(h, SW_HIDE); return 0; }
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
        case WM_CLOSE: ShowWindow(h, SW_HIDE); return 0;
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
            if (IsWindowVisible(g_wnd)) ShowWindow(g_wnd, SW_HIDE);
            else { ShowWindow(g_wnd, SW_SHOWNORMAL); SetForegroundWindow(g_wnd); }
            continue;
        }
        TranslateMessage(&msg); DispatchMessageA(&msg);
    }
    return 0;
}

// ═══════════════════════════════════════════
//   D3D9 Хук
// ═══════════════════════════════════════════
typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9*);
EndScene_t g_origEndScene = nullptr;

HRESULT __stdcall hkEndScene(IDirect3DDevice9* pDevice) {
    g_pDevice = pDevice;
    DrawESP();
    return g_origEndScene(pDevice);
}

void HookD3D9() {
    HWND hwnd = CreateWindowA("STATIC", "dummy", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) return;

    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    d3dpp.Windowed = TRUE; d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    IDirect3DDevice9* pDevice = nullptr;
    pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice);
    
    if (pDevice) {
        void** vTable = *(void***)pDevice;
        g_origEndScene = (EndScene_t)vTable[42];

        DWORD old;
        VirtualProtect(&vTable[42], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
        vTable[42] = (void*)hkEndScene;
        VirtualProtect(&vTable[42], sizeof(void*), old, &old);
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

    g_engine     = cifE("VEngineClient014", nullptr);
    if (!g_engine) g_engine = cifE("VEngineClient013", nullptr);
    g_entityList = cifC("VClientEntityList003", nullptr);

    return g_engine && g_entityList;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        Sleep(1000); 
        if (Init()) HookD3D9(); 
        CreateThread(NULL, 0, MenuThread, NULL, 0, NULL);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        g_closing = true; if (g_wnd) PostMessageA(g_wnd, WM_QUIT, 0, 0);
    }
    return TRUE;
}
