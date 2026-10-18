#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include "imgui.h"
#include "imgui_impl_dx8.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d8.lib")

typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice8*);
EndScene_t oEndScene = nullptr;
WNDPROC    oWndProc  = nullptr;

HWND  g_gameWnd  = nullptr;
HWND  g_dummyWnd = nullptr;
bool  g_init      = false;
bool  g_menuOpen  = false;

bool  g_bhop      = false;
bool  g_wallhack  = false;
bool  g_norecoil  = false;
float g_speed     = 1.0f;
float g_fov       = 90.0f;

HWND findGameWindow() {
    return FindWindowA("Valve001", nullptr);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_menuOpen && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_menuOpen = !g_menuOpen;
    }

    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

HRESULT __stdcall hkEndScene(IDirect3DDevice8* device) {
    if (!g_init) {
        D3DDEVICE_CREATION_PARAMETERS cp{};
        device->GetCreationParameters(&cp);
        g_gameWnd = cp.hFocusWindow;

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        ImGui_ImplWin32_Init(g_gameWnd);
        ImGui_ImplDX8_Init(device); // Инициализация DX8

        oWndProc = (WNDPROC)SetWindowLongPtrA(g_gameWnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
        g_init = true;
    }

    ImGui_ImplDX8_NewFrame(); // Новый кадр DX8
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_menuOpen) {
        ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
        ImGui::Begin("commandvs // css v34", &g_menuOpen, ImGuiWindowFlags_NoSavedSettings);

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
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX8_RenderDrawData(ImGui::GetDrawData()); // Отрисовка DX8

    return oEndScene(device);
}

void createDummyWindow() {
    WNDCLASSEXA wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DefWindowProcA;
    wc.hInstance     = GetModuleHandleA(nullptr);
    wc.lpszClassName = "commandvs_dummy";
    RegisterClassExA(&wc);
    g_dummyWnd = CreateWindowA("commandvs_dummy", "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
}

void hookEndScene() {
    createDummyWindow();
    IDirect3D8* d3d = Direct3DCreate8(D3D_SDK_VERSION); // Создаем DX8
    if (!d3d) return;

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed         = TRUE;
    pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow    = g_dummyWnd;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;

    IDirect3DDevice8* dummy = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_dummyWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dummy);
    if (FAILED(hr) || !dummy) { d3d->Release(); return; }

    void** vtbl = *(void***)dummy;
    oEndScene = (EndScene_t)vtbl[35]; // ИНДЕКС 35 ДЛЯ DX8

    DWORD oldProt = 0;
    VirtualProtect(&vtbl[35], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
    vtbl[35] = (void*)hkEndScene;
    VirtualProtect(&vtbl[35], sizeof(void*), oldProt, &oldProt);

    dummy->Release();
    d3d->Release();
    if (g_dummyWnd) { DestroyWindow(g_dummyWnd); g_dummyWnd = nullptr; }
}

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
