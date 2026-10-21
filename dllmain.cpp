// commandvs.cpp — css v34 (VEngineClient011) IAT HOOK (FINAL RENDER TEST)
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <cmath>
#include <cstdint>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "d3d9.lib")

// ═══════════════════════════════════════════
//   Globals
// ═══════════════════════════════════════════
typedef void* (*CreateInterfaceFn)(const char*, int*);
void* g_engine = nullptr;
IDirect3DDevice9* g_pDevice = nullptr;
bool g_showSquare = false;

template<typename T> T vf(void* inst, int idx) {
    return (T)(*(void***)inst)[idx];
}

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

void RenderTest(int offsetX, D3DCOLOR color) {
    if (!g_showSquare || !g_pDevice) return;

    DWORD oldZ, oldAlpha, oldLighting, oldCull;
    g_pDevice->GetRenderState(D3DRS_ZENABLE, &oldZ);
    g_pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlpha);
    g_pDevice->GetRenderState(D3DRS_LIGHTING, &oldLighting);
    g_pDevice->GetRenderState(D3DRS_CULLMODE, &oldCull);

    g_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    
    // Рисуем квадрат 200х200
    DrawRect(sw/2 - 100 + offsetX, sh/2 - 100, 200, 200, color);

    g_pDevice->SetRenderState(D3DRS_ZENABLE, oldZ);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlpha);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, oldLighting);
    g_pDevice->SetRenderState(D3DRS_CULLMODE, oldCull);
}

// ═══════════════════════════════════════════
//   Hooks (Present + EndScene)
// ═══════════════════════════════════════════
typedef HRESULT(__stdcall* Present_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
Present_t g_origPresent = nullptr;

HRESULT __stdcall hkPresent(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        MessageBoxA(NULL, "Present Hook Fired!", "DEBUG SUCCESS", MB_OK);
    }

    g_pDevice = pDevice;
    RenderTest(-120, D3DCOLOR_ARGB(255, 255, 0, 0)); // Красный слева
    return g_origPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9*);
EndScene_t g_origEndScene = nullptr;

HRESULT __stdcall hkEndScene(IDirect3DDevice9* pDevice) {
    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        MessageBoxA(NULL, "EndScene Hook Fired!", "DEBUG SUCCESS", MB_OK);
    }

    g_pDevice = pDevice;
    RenderTest(120, D3DCOLOR_ARGB(255, 0, 0, 255)); // Синий справа
    return g_origEndScene(pDevice);
}

// ═══════════════════════════════════════════
//   IAT Hook for Direct3DCreate9
// ═══════════════════════════════════════════
typedef IDirect3D9* (WINAPI* Direct3DCreate9_t)(UINT);
Direct3DCreate9_t g_origDirect3DCreate9 = nullptr;

IDirect3D9* WINAPI hkDirect3DCreate9(UINT SDKVersion) {
    IDirect3D9* pD3D = g_origDirect3DCreate9(SDKVersion);
    if (pD3D) {
        // Хукаем CreateDevice, чтобы получить настоящий девайс игры
        typedef HRESULT(__thiscall* CreateDevice_t)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
        CreateDevice_t origCreateDevice = (CreateDevice_t)(*(void***)pD3D)[16]; // CreateDevice index 16

        // Создаем наш хук для CreateDevice
        auto hkCreateDevice = [](IDirect3D9* _this, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) -> HRESULT {
            HRESULT hr = ((CreateDevice_t)(*(void***)_this)[16])(_this, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
            
            if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface) {
                IDirect3DDevice9* dev = *ppReturnedDeviceInterface;
                void** vTable = *(void***)dev;
                DWORD old;

                // Хук Present (17)
                g_origPresent = (Present_t)vTable[17];
                VirtualProtect(&vTable[17], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
                vTable[17] = (void*)hkPresent;
                VirtualProtect(&vTable[17], sizeof(void*), old, &old);

                // Хук EndScene (42)
                g_origEndScene = (EndScene_t)vTable[42];
                VirtualProtect(&vTable[42], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
                vTable[42] = (void*)hkEndScene;
                VirtualProtect(&vTable[42], sizeof(void*), old, &old);
            }
            return hr;
        };

        // Заменяем CreateDevice на наш хук
        DWORD old;
        VirtualProtect(&(*(void***)pD3D)[16], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
        (*(void***)pD3D)[16] = (void*)hkCreateDevice;
        VirtualProtect(&(*(void***)pD3D)[16], sizeof(void*), old, &old);
    }
    return pD3D;
}

void HookIAT() {
    // Получаем базовый адрес игры (hl2.exe)
    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    if (!base) return;

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR* imports = (IMAGE_IMPORT_DESCRIPTOR*)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    // Ищем d3d9.dll в таблице импортов
    while (imports->Name) {
        const char* dllName = (const char*)(base + imports->Name);
        if (_stricmp(dllName, "d3d9.dll") == 0) {
            IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)(base + imports->FirstThunk);
            IMAGE_THUNK_DATA* origThunk = (IMAGE_THUNK_DATA*)(base + imports->OriginalFirstThunk);

            while (origThunk->u1.Function) {
                if (origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                    // Импорт по ординалу
                } else {
                    IMAGE_IMPORT_BY_NAME* importName = (IMAGE_IMPORT_BY_NAME*)(base + origThunk->u1.AddressOfData);
                    if (strcmp((const char*)importName->Name, "Direct3DCreate9") == 0) {
                        // Нашли функцию! Подменяем.
                        DWORD oldProtect;
                        VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
                        
                        g_origDirect3DCreate9 = (Direct3DCreate9_t)thunk->u1.Function;
                        thunk->u1.Function = (uintptr_t)hkDirect3DCreate9;
                        
                        VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
                        MessageBoxA(NULL, "IAT Hooked Direct3DCreate9!", "DEBUG SUCCESS", MB_OK);
                        return;
                    }
                }
                origThunk++;
                thunk++;
            }
        }
        imports++;
    }
    MessageBoxA(NULL, "Failed to find Direct3DCreate9 in IAT!", "DEBUG ERROR", MB_OK);
}

// ═══════════════════════════════════════════
//   Меню
// ═══════════════════════════════════════════
namespace P {
    const COLORREF BG = RGB(18, 19, 23), HEAD_A = RGB(30, 32, 40), HEAD_B = RGB(22, 24, 30);
    const COLORREF STROKE = RGB(46, 48, 58), TEXT = RGB(232, 233, 240), SUBTEXT = RGB(135, 137, 148);
    const COLORREF ACCENT = RGB(232, 168, 56), ON = RGB(232, 168, 56), OFF = RGB(48, 50, 58);
    const COLORREF KNOB = RGB(240, 241, 246), CLOSE_H = RGB(210, 70, 70), ROW_HOV = RGB(24, 26, 32);
}

const int WW = 320, WH = 160, HH = 44, TBH = 34;
HWND g_wnd = nullptr;
HFONT g_fBold = nullptr, g_fB = nullptr, g_fSm = nullptr;
volatile bool g_closing = false;
RECT g_toggleRect;

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
    txt(dc, g_fSm, P::SUBTEXT, 16, 24, "css v34 // IAT render test");

    RECT closeR = { cr.right - 36, 10, cr.right - 12, 34 };
    POINT mp; GetCursorPos(&mp); ScreenToClient(g_wnd, &mp);
    if (PtInRect(&closeR, mp)) fillRound(dc, closeR, P::CLOSE_H, 6);
    SelectObject(dc, g_fBold); SetTextColor(dc, P::TEXT); SetBkMode(dc, TRANSPARENT);
    DrawTextA(dc, "×", -1, &closeR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    fillRect(dc, { 0, HH, cr.right, HH + 1 }, P::STROKE);

    if (PtInRect(&g_toggleRect, mp)) fillRound(dc, g_toggleRect, P::ROW_HOV, 4);
    txt(dc, g_fB, P::TEXT, g_toggleRect.left + 6, g_toggleRect.top + 5, "Show Square");
    drawToggle(dc, g_toggleRect, g_showSquare);
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            g_fBold = CreateFontA(13, 0,0,0, FW_BOLD, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_fB    = CreateFontA(13, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_fSm   = CreateFontA(11, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_toggleRect = { 16, HH + TBH + 16, WW - 32, HH + TBH + 42 };
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
            if (PtInRect(&g_toggleRect, p)) { g_showSquare = !g_showSquare; InvalidateRect(h, NULL, FALSE); }
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
                if (PtInRect(&g_toggleRect, p)) return HTCLIENT;
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
    if (!eng) return false;
    auto cifE = (CreateInterfaceFn)GetProcAddress(eng, "CreateInterface");
    if (!cifE) return false;
    g_engine = cifE("VEngineClient011", nullptr);
    return g_engine != nullptr;
}

DWORD WINAPI InitThread(LPVOID) {
    Sleep(2000); 
    if (Init()) {
        HookIAT(); 
    } else {
        MessageBoxA(NULL, "Engine init failed!", "DEBUG ERROR", MB_OK);
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
