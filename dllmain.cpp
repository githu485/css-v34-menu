// commandvs.cpp — css v34 (VEngineClient011) ESP AUTO-FIX + F10 LOG
#include <windows.h>
#include <windowsx.h>
#include <d3d9.h>
#include <string>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <sstream>
#include <algorithm> // Для std::min / std::max

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "comctl32.lib")

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
//   Глобалы DX9 & Настроек
// ═══════════════════════════════════════════
IDirect3DDevice9* g_pDevice = nullptr;
bool g_showSquare = false;
bool g_espEnabled = true;

// Переменные для авто-настройки
float g_boxWidthMult = 0.42f; 
float g_boxHeightOffset = 0.0f; 
int g_lastTargetScreenY = 0;
int g_lastTargetHeight = 0;
int g_lastTargetWidth = 0;

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

void DrawLine(float x1, float y1, float x2, float y2, D3DCOLOR color) {
    if (!g_pDevice) return;
    struct Vertex { float x, y, z, rhw; D3DCOLOR color; };
    Vertex v[2] = {
        { x1, y1, 0.0f, 1.0f, color },
        { x2, y2, 0.0f, 1.0f, color }
    };
    g_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_pDevice->SetTexture(0, NULL);
    g_pDevice->DrawPrimitiveUP(D3DPT_LINELIST, 1, v, sizeof(Vertex));
}

void SafeRender() {
    if (!g_pDevice) return;

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

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    
    if (g_showSquare) {
        DrawRect(sw/2 - 70, sh/2 - 25, 50, 50, D3DCOLOR_ARGB(255, 255, 0, 0));
        DrawRect(sw/2 + 20, sh/2 - 25, 50, 50, D3DCOLOR_ARGB(255, 0, 0, 255));
    }

    if (g_espEnabled && Engine_InGame()) {
        int lp = Engine_LocalPlayer();
        if (lp > 0) {
            int hi = Entity_Highest();
            for (int i = 1; i <= hi; i++) {
                if (i == lp) continue;
                void* e = Entity_Get(i);
                if (!e) continue;

                int hp = 0;
                if (SafeRead((uintptr_t)e + Off::Health, &hp, 4) && hp > 0 && hp <= 100) {
                    uintptr_t bonePtr = 0;
                    if (SafeRead((uintptr_t)e + Off::BoneMat, &bonePtr, 4) && bonePtr > 0x10000000) {
                        matrix3x4_t* bones = (matrix3x4_t*)bonePtr;
                        Vector head = BonePos(bones, 8);
                        Vector feet = BonePos(bones, 0);

                        Vector sH, sF;
                        if (WorldToScreen(head, sH) && WorldToScreen(feet, sF)) {
                            // Линия
                            DrawLine(sw/2, sh/2, sH.x, sH.y, D3DCOLOR_ARGB(255, 0, 255, 0));

                            // УМНЫЙ РАСЧЕТ БОКСА (Защита от перевернутой высоты)
                            float topY = std::min(sH.y, sF.y);
                            float botY = std::max(sH.y, sF.y);
                            float h = botY - topY;
                            
                            if (h > 10.0f && h < 1200.0f) {
                                float w = h * g_boxWidthMult;
                                float x0 = sH.x - w * 0.5f;
                                float y0 = topY + g_boxHeightOffset;

                                // Рисуем яркий бокс
                                D3DCOLOR col = D3DCOLOR_ARGB(255, 255, 0, 0); // Красный
                                DrawRect(x0, y0, w, 2.0f, col); // Верх
                                DrawRect(x0, y0 + h - 2.0f, w, 2.0f, col); // Низ
                                DrawRect(x0, y0, 2.0f, h, col); // Лево
                                DrawRect(x0 + w - 2.0f, y0, 2.0f, h, col); // Право
                            }
                        }
                    }
                }
            }
        }
    }

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
//   F10 Auto-Scanner & Logger
// ═══════════════════════════════════════════
static HWND g_diagWnd = nullptr;

void ToggleDiagWindow() {
    if (IsWindow(g_diagWnd) && IsWindowVisible(g_diagWnd)) {
        ShowWindow(g_diagWnd, SW_HIDE);
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
            WS_EX_TOPMOST, "CommandvsDiag", "commandvs // AUTO-FIX LOG (F10)",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
            NULL, NULL, wc.hInstance, NULL
        );
    }

    if (g_diagWnd) {
        std::stringstream ss;
        ss << "=== AUTO-FIX LOG ===\n\n";
        
        int sw, sh; Engine_ScreenSize(sw, sh);
        ss << "Screen: " << sw << "x" << sh << "\n";
        ss << "Current Width Mult: " << g_boxWidthMult << "\n\n";

        if (Engine_InGame()) {
            int lp = Engine_LocalPlayer();
            int hi = Entity_Highest();
            bool found = false;

            for (int i = 1; i <= hi; i++) {
                if (i == lp) continue;
                void* e = Entity_Get(i);
                if (!e) continue;

                int hp = 0;
                if (SafeRead((uintptr_t)e + Off::Health, &hp, 4) && hp > 0 && hp <= 100) {
                    uintptr_t bonePtr = 0;
                    if (SafeRead((uintptr_t)e + Off::BoneMat, &bonePtr, 4) && bonePtr > 0x10000000) {
                        matrix3x4_t* bones = (matrix3x4_t*)bonePtr;
                        Vector head = BonePos(bones, 8);
                        Vector feet = BonePos(bones, 0);

                        Vector sH, sF;
                        if (WorldToScreen(head, sH) && WorldToScreen(feet, sF)) {
                            float topY = std::min(sH.y, sF.y);
                            float botY = std::max(sH.y, sF.y);
                            float h = botY - topY;

                            if (h > 10.0f) {
                                ss << "--- Target " << i << " ---\n";
                                ss << "Head Y: " << (int)sH.y << " | Feet Y: " << (int)sF.y << "\n";
                                ss << "Screen Height: " << (int)h << "\n";
                                ss << "Auto Calc Width: " << (int)(h * g_boxWidthMult) << "\n\n";
                                found = true;
                                break; // Берем первого попавшегося
                            }
                        }
                    }
                }
            }
            if (!found) ss << "No visible enemies with valid bones found.\n";
        } else {
            ss << "Not in game!\n";
        }

        HWND edit = FindWindowExA(g_diagWnd, NULL, "EDIT", NULL);
        if (edit) {
            SetWindowTextA(edit, ss.str().c_str());
        } else {
            edit = CreateWindowExA(
                0, "EDIT", ss.str().c_str(),
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                10, 10, 460, 340, g_diagWnd, NULL, GetModuleHandleA(NULL), NULL
            );
            HFONT hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
            SendMessageA(edit, WM_SETFONT, (WPARAM)hFont, TRUE);
        }

        ShowWindow(g_diagWnd, SW_SHOW);
        SetForegroundWindow(g_diagWnd);
    }
}

// ═══════════════════════════════════════════
//   Меню (Win32)
// ═══════════════════════════════════════════
// ... (Код меню без изменений, как в прошлом ответе)
// В WM_HOTKEY добавляем обработку F10:
// if (msg.wParam == 2) ToggleDiagWindow();

// ═══════════════════════════════════════════
//   DllMain
// ═══════════════════════════════════════════
// ...
