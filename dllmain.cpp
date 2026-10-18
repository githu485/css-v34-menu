#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")

// ---------- globals ----------
typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9*);
EndScene_t oEndScene = nullptr;
WNDPROC    oWndProc  = nullptr;

HWND  g_gameWnd  = nullptr;
HWND  g_dummyWnd = nullptr;
bool  g_init      = false;
bool  g_menuOpen  = true;

// тумблеры (пока ни к чему не привязаны)
bool  g_bhop      = false;
bool  g_wallhack  = false;
bool  g_norecoil  = false;
float g_speed     = 1.0f;
float g_fov       = 90.0f;

// ---------- helpers ----------
HWND findGameWindow() {
    // Valve: cs:s main window class всегда "Valve001"
    return FindWindowA("Valve001", nullptr);
}

// ---------- wndproc ----------
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_menuOpen && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_menuOpen = !g_menuOpen;
    }

    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

// ---------- endscene hook ----------
HRESULT __stdcall hkEndScene(IDirect3DDevice9* device) {
    if (!g_init) {
        // окно игры находим через сам девайс — надёжнее чем FindWindow
        D3DDEVICE_CREATION_PARAMETERS cp{};
        device->GetCreationParameters(&cp);
        g_gameWnd = cp.hFocusWindow;

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        ImGui_ImplWin32_Init(g_gameWnd);
        ImGui_ImplDX9_Init(device);

        oWndProc = (WNDPROC)SetWindowLongPtrA(g_gameWnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
        g_init = true;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_menuOpen) {
        ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
        ImGui::Begin("commandvs // css v34", &g_menuOpen,
                     ImGuiWindowFlags_NoSavedSettings);

        ImGui::TextDisabled("CS:S v34  |  INSERT = menu");
        ImGui::Separator();

        if (ImGui::BeginTabBar("tabs")) {
            if (ImGui::BeginTabItem("COMBAT")) {
                ImGui::Checkbox("Bhop",      &g_bhop);
                ImGui::Checkbox("No Recoil", &g_norecoil);
                ImGui::SliderFloat("Speed",  &g_speed, 0.5f, 5.0f, "%.1fx");
                ImGui::SliderFloat("FOV",    &g_fov,   60.0f, 140.0f, "%.0f");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("VISUAL")) {
                ImGui::Checkbox("Wallhack (glow)", &g_wallhack);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("INFO")) {
                ImGui::Text("menu state  : %s", g_menuOpen ? "open" : "hidden");
                ImGui::Text("hwnd        : 0x%08X", (unsigned)(uintptr_t)g_gameWnd);
                ImGui::Text("imgui       : %s", IMGUI_VERSION);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::Separator();
        ImGui::TextDisabled("features not implemented yet — GUI shell only");

        ImGui::End();
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return oEndScene(device);
}

// ---------- vtable hook ----------
void createDummyWindow() {
    WNDCLASSEXA wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DefWindowProcA;
    wc.hInstance     = GetModuleHandleA(nullptr);
    wc.lpszClassName = "commandvs_dummy";
    RegisterClassExA(&wc);
    g_dummyWnd = CreateWindowA("commandvs_dummy", "", WS_OVERLAPPEDWINDOW,
                               0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
}

void hookEndScene() {
    createDummyWindow();

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) return;

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed         = TRUE;
    pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow    = g_dummyWnd;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;

    IDirect3DDevice9* dummy = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                   g_dummyWnd,
                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                   &pp, &dummy);
    if (FAILED(hr) || !dummy) { d3d->Release(); return; }

    // vtable[42] = EndScene для DX9
    void** vtbl = *(void***)dummy;
    oEndScene = (EndScene_t)vtbl[42];

    DWORD oldProt = 0;
    VirtualProtect(&vtbl[42], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
    vtbl[42] = hkEndScene;
    VirtualProtect(&vtbl[42], sizeof(void*), oldProt, &oldProt);

    dummy->Release();
    d3d->Release();

    if (g_dummyWnd) {
        DestroyWindow(g_dummyWnd);
        g_dummyWnd = nullptr;
    }
}

// ---------- entry ----------
DWORD WINAPI mainThread(LPVOID) {
    while (!findGameWindow()) Sleep(150);
    hookEndScene();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, mainThread, hMod, 0, nullptr);
    }
    return TRUE;
}