// commandvs.cpp — css v34 (VEngineClient011) FINAL ESP + SMALL TEST BOXES
#include <windows.h>
#include <windowsx.h>
#include <d3d9.h>
#include <string>
#include <cstring>
#include <cstdint>
#include <cmath>

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

// ТВОИ КАСТОМНЫЕ ОФФСЕТЫ
namespace Off {
    constexpr uintptr_t Health    = 0xDA4;
    constexpr uintptr_t Origin    = 0x1b18; 
    constexpr uintptr_t BoneMat   = 0x4E4;
    constexpr uintptr_t W2SMatrix = 0x3094;
}

int  Engine_LocalPlayer()             { typedef int(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_LOCALPLAYER)(g_engine); }
void Engine_ScreenSize(int& w, int& h){ typedef void(__thiscall* F)(void*,int&,int&); vf<F>(g_engine, VT_ENGINE_SCREENSIZE)(g_engine,w,h); }
bool Engine_InGame()                  { typedef bool(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_ISINGAME)(g_engine); }
void* Entity_Get(int i)               { typedef void*(__thiscall* F)(void*,int); return vf<F>(g_entityList, VT_ENTITY_GET)(g_entityList, i); }
int   Entity_Highest()                { typedef int(__thiscall* F)(void*); return vf<F>(g_entityList, VT_ENTITY_HIGHEST)(g_entityList); }

bool SafeRead(uintptr_t addr, void* out, size_t size) {
    if (addr < 0x10000) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
            memcpy(out, (void*)addr, size);
            return true;
        }
    }
    return false;
}

Vector BonePos(matrix3x4_t* b, int i) {
    if (!b) return {};
    return { b[i].m[0][3], b[i].m[1][3], b[i].m[2][3] };
}

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
//   Глобалы DX9
// ═══════════════════════════════════════════
IDirect3DDevice9* g_pDevice = nullptr;
bool g_showSquare = false;
bool g_espEnabled = true;

typedef HRESULT(__stdcall* Present_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
Present_t g_origPresent = nullptr;
typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9*);
EndScene_t g_origEndScene = nullptr;

// ═══════════════════════════════════════════
//   DirectX 9 Render
// ═══════════════════════════════════════════
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

void SafeRender() {
    if (!g_pDevice) return;

    // 1. СОХРАНЯЕМ ВСЕ НАСТРОЙКИ ИГРЫ
    DWORD oldZ, oldAlpha, oldLighting, oldCull, oldFVF, oldScissor;
    DWORD oldColorOp, oldColorArg1, oldAlphaOp, oldAlphaArg1;
    IDirect3DBaseTexture9* oldTex = nullptr;
    IDirect3DVertexShader9* oldVS = nullptr;
    IDirect3DPixelShader9* oldPS = nullptr;
    IDirect3DVertexDeclaration9* oldDecl = nullptr;

    g_pDevice->GetRenderState(D3DRS_ZENABLE, &oldZ);
    g_pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlpha);
    g_pDevice->GetRenderState(D3DRS_LIGHTING, &oldLighting);
    g_pDevice->GetRenderState(D3DRS_CULLMODE, &oldCull);
    g_pDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &oldScissor);
    g_pDevice->GetFVF(&oldFVF);
    g_pDevice->GetTexture(0, &oldTex);
    g_pDevice->GetVertexShader(&oldVS);
    g_pDevice->GetPixelShader(&oldPS);
    g_pDevice->GetVertexDeclaration(&oldDecl);

    g_pDevice->GetTextureStageState(0, D3DTSS_COLOROP, &oldColorOp);
    g_pDevice->GetTextureStageState(0, D3DTSS_COLORARG1, &oldColorArg1);
    g_pDevice->GetTextureStageState(0, D3DTSS_ALPHAOP, &oldAlphaOp);
    g_pDevice->GetTextureStageState(0, D3DTSS_ALPHAARG1, &oldAlphaArg1);

    // 2. СТАВИМ СВОИ НАСТРОЙКИ ДЛЯ 2D
    g_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    g_pDevice->SetVertexShader(NULL);
    g_pDevice->SetPixelShader(NULL);
    g_pDevice->SetVertexDeclaration(NULL);

    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

    // 3. МАЛЕНЬКИЕ ТЕСТОВЫЕ КВАДРАТЫ ПО ЦЕНТРУ
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (g_showSquare) {
        DrawRect(sw/2 - 70, sh/2 - 25, 50, 50, D3DCOLOR_ARGB(255, 255, 0, 0)); // Красный
        DrawRect(sw/2 + 20, sh/2 - 25, 50, 50, D3DCOLOR_ARGB(255, 0, 0, 255)); // Синий
    }

    // 4. ESP С ДЕБАГОМ
    if (g_espEnabled && Engine_InGame()) {
        DrawRect(10, 10, 20, 20, D3DCOLOR_ARGB(255, 0, 255, 0)); // ЗЕЛЕНЫЙ: В игре

        int lp = Engine_LocalPlayer();
        if (lp > 0) {
            DrawRect(40, 10, 20, 20, D3DCOLOR_ARGB(255, 0, 0, 255)); // СИНИЙ: Нашли себя

            int hi = Entity_Highest();
            int foundHp = 0, foundBones = 0, foundW2S = 0;

            for (int i = 1; i <= hi; i++) {
                if (i == lp) continue;
                void* e = Entity_Get(i);
                if (!e) continue;

                int hp = 0;
                if (SafeRead((uintptr_t)e + Off::Health, &hp, 4) && hp > 0 && hp <= 100) {
                    foundHp++;
                    if (foundHp == 1) DrawRect(70, 10, 20, 20, D3DCOLOR_ARGB(255, 255, 255, 0)); // ЖЕЛТЫЙ: Нашли бота

                    uintptr_t bonePtr = 0;
                    if (SafeRead((uintptr_t)e + Off::BoneMat, &bonePtr, 4) && bonePtr > 0x10000000) {
                        foundBones++;
                        if (foundBones == 1) DrawRect(100, 10, 20, 20, D3DCOLOR_ARGB(255, 255, 255, 255)); // БЕЛЫЙ: Есть кости

                        matrix3x4_t* bones = (matrix3x4_t*)bonePtr;
                        Vector head = BonePos(bones, 8);
                        Vector feet = BonePos(bones, 0);

                        Vector sH, sF;
                        if (WorldToScreen(head, sH) && WorldToScreen(feet, sF)) {
                            foundW2S++;
                            if (foundW2S == 1) DrawRect(130, 10, 20, 20, D3DCOLOR_ARGB(255, 255, 0, 255)); // ФИОЛЕТОВЫЙ: W2S сработал

                            float h = sF.y - sH.y;
                            if (h > 4 && h < 1800) {
                                float w = h * 0.42f;
                                float x0 = sH.x - w * 0.5f;
                                float y0 = sH.y;
                                
                                D3DCOLOR col = D3DCOLOR_ARGB(255, 232, 168, 56);
                                DrawRect(x0, y0, w, 1.5f, col);
                                DrawRect(x0, y0 + h - 1.5f, w, 1.5f, col);
                                DrawRect(x0, y0, 1.5f, h, col);
                                DrawRect(x0 + w - 1.5f, y0, 1.5f, h, col);
                            }
                        }
                    }
                }
            }
        }
    }

    // 5. ВОЗВРАЩАЕМ ВСЕ КАК БЫЛО
    g_pDevice->SetRenderState(D3DRS_ZENABLE, oldZ);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlpha);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, oldLighting);
    g_pDevice->SetRenderState(D3DRS_CULLMODE, oldCull);
    g_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, oldScissor);
    g_pDevice->SetFVF(oldFVF);
    g_pDevice->SetTexture(0, oldTex);
    g_pDevice->SetVertexShader(oldVS);
    g_pDevice->SetPixelShader(oldPS);
    g_pDevice->SetVertexDeclaration(oldDecl);

    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, oldColorOp);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, oldColorArg1);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, oldAlphaOp);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, oldAlphaArg1);

    if (oldTex) oldTex->Release();
    if (oldVS) oldVS->Release();
    if (oldPS) oldPS->Release();
    if (oldDecl) oldDecl->Release();
}

// ═══════════════════════════════════════════
//   Inline Hook
// ═══════════════════════════════════════════
void* HookFunc(void* target, void* detour, int len) {
    if (len < 5) return nullptr;
    DWORD oldProtect;
    VirtualProtect(target, len, PAGE_EXECUTE_READWRITE, &oldProtect);
    void* trampoline = VirtualAlloc(nullptr, len + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(trampoline, target, len);
    uintptr_t src = (uintptr_t)trampoline + len;
    uintptr_t dst = (uintptr_t)target + len;
    *(uint8_t*)src = 0xE9;
    *(uint32_t*)(src + 1) = (uint32_t)(dst - (src + 5));
    *(uint8_t*)target = 0xE9;
    *(uint32_t*)((uintptr_t)target + 1) = (uint32_t)((uintptr_t)detour - ((uintptr_t)target + 5));
    for (int i = 5; i < len; i++) *(uint8_t*)((uintptr_t)target + i) = 0x90;
    VirtualProtect(target, len, oldProtect, &oldProtect);
    return trampoline;
}

HRESULT __stdcall hkPresent(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    g_pDevice = pDevice;
    SafeRender();
    return g_origPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT __stdcall hkEndScene(IDirect3DDevice9* pDevice) {
    g_pDevice = pDevice;
    return g_origEndScene(pDevice);
}

void SetupHooks() {
    HWND hwnd = CreateWindowA("STATIC", "dummy", WS_POPUP, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) return;
    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    d3dpp.Windowed = TRUE; d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hwnd; d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    IDirect3DDevice9* pDevice = nullptr;
    pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice);
    if (pDevice) {
        void** vTable = *(void***)pDevice;
        g_origPresent = (Present_t)HookFunc(vTable[17], (void*)hkPresent, 5);
        g_origEndScene = (EndScene_t)HookFunc(vTable[42], (void*)hkEndScene, 5);
        pDevice->Release();
    }
    pD3D->Release();
    DestroyWindow(hwnd);
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

const int WW = 320, WH = 190, HH = 44, TBH = 34;
HWND g_wnd = nullptr;
HFONT g_fBold = nullptr, g_fB = nullptr, g_fSm = nullptr;
volatile bool g_closing = false;
RECT g_toggleRectEsp;
RECT g_toggleRectTest;

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

void DrawUI(HDC dc) {
    RECT cr; GetClientRect(g_wnd, &cr);
    fillRect(dc, cr, P::BG);
    RECT hr = { 0, 0, cr.right, HH };
    fillGrad(dc, hr, P::HEAD_A, P::HEAD_B);
    fillRect(dc, { 0, 0, 3, HH }, P::ACCENT);
    txt(dc, g_fBold, P::TEXT, 16, 8, "commandvs");
    txt(dc, g_fSm, P::SUBTEXT, 16, 24, "css v34 // small boxes");

    RECT closeR = { cr.right - 36, 10, cr.right - 12, 34 };
    POINT mp; GetCursorPos(&mp); ScreenToClient(g_wnd, &mp);
    if (PtInRect(&closeR, mp)) fillRound(dc, closeR, P::CLOSE_H, 6);
    SelectObject(dc, g_fBold); SetTextColor(dc, P::TEXT); SetBkMode(dc, TRANSPARENT);
    DrawTextA(dc, "×", -1, &closeR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    fillRect(dc, { 0, HH, cr.right, HH + 1 }, P::STROKE);

    if (PtInRect(&g_toggleRectEsp, mp)) fillRound(dc, g_toggleRectEsp, P::ROW_HOV, 4);
    txt(dc, g_fB, P::TEXT, g_toggleRectEsp.left + 6, g_toggleRectEsp.top + 5, "Enable ESP");
    drawToggle(dc, g_toggleRectEsp, g_espEnabled);

    if (PtInRect(&g_toggleRectTest, mp)) fillRound(dc, g_toggleRectTest, P::ROW_HOV, 4);
    txt(dc, g_fB, P::TEXT, g_toggleRectTest.left + 6, g_toggleRectTest.top + 5, "Test Squares");
    drawToggle(dc, g_toggleRectTest, g_showSquare);
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            g_fBold = CreateFontA(13, 0,0,0, FW_BOLD, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_fB    = CreateFontA(13, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_fSm   = CreateFontA(11, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_toggleRectEsp = { 16, HH + TBH + 10, WW - 32, HH + TBH + 36 };
            g_toggleRectTest = { 16, HH + TBH + 44, WW - 32, HH + TBH + 70 };
            return 0;
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
            if (PtInRect(&g_toggleRectEsp, p)) { g_espEnabled = !g_espEnabled; InvalidateRect(h, NULL, FALSE); }
            if (PtInRect(&g_toggleRectTest, p)) { g_showSquare = !g_showSquare; InvalidateRect(h, NULL, FALSE); }
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
                if (PtInRect(&g_toggleRectEsp, p)) return HTCLIENT;
                if (PtInRect(&g_toggleRectTest, p)) return HTCLIENT;
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
    if (Init()) SetupHooks();
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
