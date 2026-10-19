// commandvs.cpp — css v34 (build 4044) esp + menu
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
void* g_client = nullptr;
void* g_surface = nullptr;

template<typename T> T vf(void* inst, int idx) {
    return (T)(*(void***)inst)[idx];
}

// ═══════════════════════════════════════════
//   Vtable indexes — build 4044 (Source 2007)
//   если что-то не рисуется — крути +/-1
// ═══════════════════════════════════════════
#define VT_ENGINE_LOCALPLAYER   12
#define VT_ENGINE_SCREENSIZE     5
#define VT_ENGINE_W2SMATRIX     37
#define VT_ENGINE_ISINGAME      26
#define VT_ENTITY_GET            3
#define VT_ENTITY_HIGHEST        6
#define VT_SURF_SETCOLOR        15
#define VT_SURF_FILLED          17
#define VT_SURF_OUTLINE         19
#define VT_SURF_LINE            20
#define VT_SURF_PAINTTRAVERSE   41

// text — индексы ISurface для v34, проверь если текст не появится
#define VT_SURF_CREATEFONT      71
#define VT_SURF_SETGLYPH        72
#define VT_SURF_SETTEXTFONT     22
#define VT_SURF_SETTEXTCOLOR    24
#define VT_SURF_SETTEXTPOS      26
#define VT_SURF_PRINTTEXT       28

// ═══════════════════════════════════════════
//   Offsets — CSS v34 (build 4044)
// ═══════════════════════════════════════════
namespace Off {
    constexpr uintptr_t Health    = 0xFC;   // m_iHealth (int)
    constexpr uintptr_t Team      = 0x98;   // m_iTeamNum (int)
    constexpr uintptr_t LifeState = 0x79;   // m_lifeState (uint8) — если трупы рисуются, правь сюда
    constexpr uintptr_t BoneMat   = 0x4E4;  // m_dwBoneMatrix (pointer)
}

int  Engine_LocalPlayer()             { typedef int(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_LOCALPLAYER)(g_engine); }
void Engine_ScreenSize(int& w, int& h){ typedef void(__thiscall* F)(void*,int&,int&); vf<F>(g_engine, VT_ENGINE_SCREENSIZE)(g_engine,w,h); }
VMatrix* Engine_W2SM()                { typedef VMatrix*(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_W2SMATRIX)(g_engine); }
bool Engine_InGame()                  { typedef bool(__thiscall* F)(void*); return vf<F>(g_engine, VT_ENGINE_ISINGAME)(g_engine); }
void* Entity_Get(int i)               { typedef void*(__thiscall* F)(void*,int); return vf<F>(g_entityList, VT_ENTITY_GET)(g_entityList, i); }
int   Entity_Highest()                { typedef int(__thiscall* F)(void*); return vf<F>(g_entityList, VT_ENTITY_HIGHEST)(g_entityList); }

int  Ent_Health(void* e) { return *(int*)((uintptr_t)e + Off::Health); }
int  Ent_MaxHP(void* e)  { return 100; } // в CSS v34 у игрока фикс 100
int  Ent_Team(void* e)   { return *(int*)((uintptr_t)e + Off::Team); }
bool Ent_Alive(void* e)  { return *(uint8_t*)((uintptr_t)e + Off::LifeState) == 0; }
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
void S_Fill(int x0,int y0,int x1,int y1){ typedef void(__thiscall* F)(void*,int,int,int,int); vf<F>(g_surface, VT_SURF_FILLED)(g_surface,x0,y0,x1,y1); }
void S_Outline(int x0,int y0,int x1,int y1){ typedef void(__thiscall* F)(void*,int,int,int,int); vf<F>(g_surface, VT_SURF_OUTLINE)(g_surface,x0,y0,x1,y1); }
void S_Line(int x0,int y0,int x1,int y1){ typedef void(__thiscall* F)(void*,int,int,int,int); vf<F>(g_surface, VT_SURF_LINE)(g_surface,x0,y0,x1,y1); }

unsigned long S_CreateFont() {
    typedef unsigned long(__thiscall* F)(void*);
    return vf<F>(g_surface, VT_SURF_CREATEFONT)(g_surface);
}
void S_SetGlyph(unsigned long f, const char* name, int tall, int weight, int flags, int rangeMin, int rangeMax) {
    typedef void(__thiscall* F)(void*, unsigned long, const char*, int, int, int, int, int);
    vf<F>(g_surface, VT_SURF_SETGLYPH)(g_surface, f, name, tall, weight, 0, 0, rangeMax);
}
void S_SetTextFont(unsigned long f)     { typedef void(__thiscall* F)(void*, unsigned long); vf<F>(g_surface, VT_SURF_SETTEXTFONT)(g_surface, f); }
void S_SetTextColor(int r,int g,int b,int a){ typedef void(__thiscall* F)(void*,int,int,int,int); vf<F>(g_surface, VT_SURF_SETTEXTCOLOR)(g_surface,r,g,b,a); }
void S_SetTextPos(int x,int y)          { typedef void(__thiscall* F)(void*,int,int); vf<F>(g_surface, VT_SURF_SETTEXTPOS)(g_surface,x,y); }
void S_PrintText(const wchar_t* t, int len) {
    typedef void(__thiscall* F)(void*, const wchar_t*, int, int);
    vf<F>(g_surface, VT_SURF_PRINTTEXT)(g_surface, t, len, 0);
}

// ═══════════════════════════════════════════
//   Fonts
// ═══════════════════════════════════════════
unsigned long g_fontName = 0;
unsigned long g_fontDist = 0;
bool g_fontsReady = false;

void InitFonts() {
    if (g_fontsReady) return;
    g_fontName = S_CreateFont();
    S_SetGlyph(g_fontName, "Tahoma", 12, 700, 0, 0, 255);
    g_fontDist = S_CreateFont();
    S_SetGlyph(g_fontDist, "Tahoma", 11, 500, 0, 0, 255);
    g_fontsReady = true;
}

void S_DrawString(unsigned long font, int x, int y, int r,int g,int b, const wchar_t* txt) {
    if (!font || !txt) return;
    S_SetTextFont(font);
    S_SetTextColor(r, g, b, 255);
    S_SetTextPos(x, y);
    S_PrintText(txt, (int)wcslen(txt));
}

void S_DrawStringCenter(unsigned long font, int cx, int y, int r,int g,int b, const wchar_t* txt) {
    // приблизительная ширина: charCount * (fontTall * 0.55)
    int w = (int)(wcslen(txt) * 7);
    S_DrawString(font, cx - w / 2, y, r, g, b, txt);
}

// ═══════════════════════════════════════════
//   ESP state
// ═══════════════════════════════════════════
struct ESP {
    bool enabled = false;
    bool box = true;
    bool name = false;
    bool health = false;
    bool skeleton = false;
    bool distance = false;
    bool teamCheck = false;
    float maxDist = 3000.0f;
    COLORREF boneColor = RGB(232, 168, 56);
} g_esp;

// ═══════════════════════════════════════════
//   ESP draw
// ═══════════════════════════════════════════
void DrawESP() {
    if (!g_esp.enabled || !Engine_InGame()) return;
    if (!g_fontsReady) InitFonts();

    int lp = Engine_LocalPlayer();
    if (lp <= 0) return;
    void* lpE = Entity_Get(lp);
    if (!lpE) return;
    int lpTeam = Ent_Team(lpE);
    matrix3x4_t* lpB = Ent_Bones(lpE);
    Vector lpHead = lpB ? BonePos(lpB, 8) : Vector();

    int r = GetRValue(g_esp.boneColor);
    int g = GetGValue(g_esp.boneColor);
    int bl = GetBValue(g_esp.boneColor);

    int hi = Entity_Highest();
    for (int i = 1; i <= hi; i++) {
        if (i == lp) continue;
        void* e = Entity_Get(i);
        if (!e) continue;
        if (!Ent_Alive(e)) continue;
        int hp = Ent_Health(e);
        if (hp <= 0) continue;
        if (g_esp.teamCheck && lpTeam > 1 && Ent_Team(e) == lpTeam) continue;

        matrix3x4_t* b = Ent_Bones(e);
        if (!b) continue;

        Vector head = BonePos(b, 8);
        Vector feet = BonePos(b, 0);
        float dist = 0.0f;
        if (lpHead.Length() > 0.1f) {
            dist = (head - lpHead).Length();
            if (dist > g_esp.maxDist) continue;
        }

        Vector sH, sF;
        if (!WorldToScreen(head, sH) || !WorldToScreen(feet, sF)) continue;
        float h = sF.y - sH.y;
        if (h < 4 || h > 1800) continue;
        float w = h * 0.42f;

        int x0 = (int)(sH.x - w * 0.5f);
        int x1 = (int)(sH.x + w * 0.5f);
        int y0 = (int)sH.y;
        int y1 = (int)sF.y;

        // health bar (левее бокса)
        if (g_esp.health) {
            float frac = (float)hp / (float)Ent_MaxHP(e);
            if (frac < 0) frac = 0;
            if (frac > 1) frac = 1;
            int barX = x0 - 6;
            S_SetColor(0, 0, 0, 200);
            S_Fill(barX - 1, y0 - 1, barX + 3, y1 + 1);
            int hpH = (int)((y1 - y0) * frac);
            int hr = frac > 0.6f ? 80 : (frac > 0.3f ? 240 : 230);
            int hg = frac > 0.6f ? 220 : (frac > 0.3f ? 200 : 70);
            int hb = frac > 0.6f ? 100 : (frac > 0.3f ? 60  : 70);
            S_SetColor(hr, hg, hb, 255);
            S_Fill(barX, y1 - hpH, barX + 2, y1);
        }

        // box
        if (g_esp.box) {
            S_SetColor(r, g, bl, 255);
            S_Outline(x0, y0, x1, y1);
        }

        // skeleton
        if (g_esp.skeleton) {
            static const int pairs[][2] = {
                {8,6},{6,5},{5,0},
                {6,9},{9,10},{10,11},
                {6,12},{12,13},{13,14},
                {0,15},{15,16},{16,17},
                {0,19},{19,20},{20,21}
            };
            S_SetColor(r, g, bl, 200);
            for (auto& p : pairs) {
                Vector a = BonePos(b, p[0]);
                Vector c = BonePos(b, p[1]);
                Vector sa, sc;
                if (!WorldToScreen(a, sa) || !WorldToScreen(c, sc)) continue;
                S_Line((int)sa.x, (int)sa.y, (int)sc.x, (int)sc.y);
            }
        }

        // name
        if (g_esp.name && g_fontName) {
            const wchar_t* nm = L"player";
            S_DrawStringCenter(g_fontName, (int)(sH.x), y0 - 14, 240, 240, 245, nm);
        }

        // distance
        if (g_esp.distance && g_fontDist) {
            wchar_t buf[32];
            swprintf_s(buf, L"%dm", (int)(dist / 50.0f));
            S_DrawStringCenter(g_fontDist, (int)(sH.x), y1 + 2, r, g, bl, buf);
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
    const COLORREF TRACK     = RGB(34, 36, 44);
    const COLORREF FILL      = RGB(232, 168, 56);
    const COLORREF CLOSE_H   = RGB(210, 70, 70);
    const COLORREF ROW_HOV   = RGB(24, 26, 32);
    const COLORREF SW_ACTIVE = RGB(240, 241, 246);
}

// ═══════════════════════════════════════════
//   Menu state
// ═══════════════════════════════════════════
const int WW = 320, WH = 400, HH = 44, TBH = 34;

HWND g_wnd = nullptr;
HFONT g_fBold = nullptr, g_fB = nullptr, g_fSm = nullptr;
int g_tab = 0;
volatile bool g_closing = false;

COLORREF g_swatches[] = {
    RGB(232, 168, 56), RGB(120, 90, 255), RGB(80, 220, 100), RGB(230, 70, 70),
    RGB(100, 180, 255), RGB(255, 255, 255), RGB(255, 120, 200), RGB(80, 200, 200),
};
int g_swatchCount = 8;

enum CtrlType { CTRL_TAB, CTRL_TOGGLE, CTRL_SLIDER, CTRL_COLOR };
struct Ctrl {
    RECT r;
    int type;
    void* data;
    std::string label;
    int id;
};
std::vector<Ctrl> g_ctrls;
Ctrl* g_dragSlider = nullptr;

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

    const char* tabNames[] = { "ESP", "SETTINGS" };
    int tabW = 78;
    for (int i = 0; i < 2; i++) {
        Ctrl c;
        c.r = { cx + i*(tabW+4), HH + 6, cx + i*(tabW+4) + tabW, HH + 28 };
        c.type = CTRL_TAB;
        c.data = (void*)(uintptr_t)i;
        c.label = tabNames[i];
        c.id = i;
        g_ctrls.push_back(c);
    }

    int y = HH + TBH + 12;
    if (g_tab == 0) {
        const char* lbl[] = { "Enabled", "Box", "Name", "Health", "Skeleton", "Distance", "Team Check" };
        bool* vals[] = { &g_esp.enabled, &g_esp.box, &g_esp.name, &g_esp.health,
                         &g_esp.skeleton, &g_esp.distance, &g_esp.teamCheck };
        for (int i = 0; i < 7; i++) {
            Ctrl c;
            c.r = { cx, y, cx + cw, y + 26 };
            c.type = CTRL_TOGGLE;
            c.data = vals[i];
            c.label = lbl[i];
            c.id = i;
            g_ctrls.push_back(c);
            y += 30;
        }
        Ctrl sl;
        sl.r = { cx, y, cx + cw, y + 40 };
        sl.type = CTRL_SLIDER;
        sl.data = &g_esp.maxDist;
        sl.label = "Max Distance";
        sl.id = 0;
        g_ctrls.push_back(sl);
        y += 46;

        for (int i = 0; i < g_swatchCount; i++) {
            Ctrl sw;
            sw.r = { cx + i*32, y, cx + i*32 + 24, y + 24 };
            sw.type = CTRL_COLOR;
            sw.data = (void*)(uintptr_t)i;
            sw.id = i;
            g_ctrls.push_back(sw);
        }
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
    txt(dc, g_fSm, P::SUBTEXT, 16, 24, "css v34 // build 4044");

    RECT closeR = { cr.right - 36, 10, cr.right - 12, 34 };
    POINT mp; GetCursorPos(&mp); ScreenToClient(g_wnd, &mp);
    bool closeHover = PtInRect(&closeR, mp);
    if (closeHover) fillRound(dc, closeR, P::CLOSE_H, 6);
    SelectObject(dc, g_fBold);
    SetTextColor(dc, P::TEXT);
    SetBkMode(dc, TRANSPARENT);
    DrawTextA(dc, "×", -1, &closeR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    fillRect(dc, { 0, HH, cr.right, HH + 1 }, P::STROKE);
    fillRect(dc, { 0, HH + 1, cr.right, HH + TBH }, P::BG);

    for (auto& c : g_ctrls) {
        if (c.type == CTRL_TAB) {
            bool active = (g_tab == c.id);
            bool hover = PtInRect(&c.r, mp);
            if (active)      fillRound(dc, c.r, P::ACCENT, 6);
            else if (hover)  fillRound(dc, c.r, P::ROW_HOV, 6);

            SelectObject(dc, g_fBold);
            SetTextColor(dc, active ? RGB(20,20,24) : (hover ? P::TEXT : P::SUBTEXT));
            SetBkMode(dc, TRANSPARENT);
            DrawTextA(dc, c.label.c_str(), -1, &c.r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        else if (c.type == CTRL_TOGGLE) {
            bool hover = PtInRect(&c.r, mp);
            if (hover) fillRound(dc, c.r, P::ROW_HOV, 4);
            txt(dc, g_fB, P::TEXT, c.r.left + 6, c.r.top + 5, c.label.c_str());
            drawToggle(dc, c.r, *(bool*)c.data);
        }
        else if (c.type == CTRL_SLIDER) {
            float* val = (float*)c.data;
            txt(dc, g_fB, P::TEXT, c.r.left + 6, c.r.top, c.label.c_str());
            char buf[32]; snprintf(buf, sizeof buf, "%d", (int)*val);
            RECT valR = { c.r.right - 60, c.r.top, c.r.right, c.r.top + 16 };
            SelectObject(dc, g_fBold);
            SetTextColor(dc, P::ACCENT);
            SetBkMode(dc, TRANSPARENT);
            DrawTextA(dc, buf, -1, &valR, DT_RIGHT | DT_SINGLELINE);

            RECT tr = { c.r.left + 6, c.r.top + 26, c.r.right - 6, c.r.top + 32 };
            fillRound(dc, tr, P::TRACK, 6);
            float a = (*val - 100.0f) / (5000.0f - 100.0f);
            a = (a < 0) ? 0 : (a > 1 ? 1 : a);
            RECT fr = { tr.left, tr.top, tr.left + (int)((tr.right - tr.left) * a), tr.bottom };
            if (fr.right > fr.left) fillRound(dc, fr, P::FILL, 6);
            int kx = tr.left + (int)((tr.right - tr.left) * a);
            RECT kr = { kx - 6, tr.top - 3, kx + 6, tr.bottom + 3 };
            fillRound(dc, kr, P::KNOB, 12);
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
            if (g_dragSlider) {
                float* v = (float*)g_dragSlider->data;
                RECT tr = { g_dragSlider->r.left + 6, 0, g_dragSlider->r.right - 6, 0 };
                int mx = GET_X_LPARAM(lp);
                float a = (float)(mx - tr.left) / (float)(tr.right - tr.left);
                a = (a < 0) ? 0 : (a > 1 ? 1 : a);
                *v = 100.0f + a * (5000.0f - 100.0f);
            }
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
                if (c.type == CTRL_TAB) {
                    g_tab = c.id;
                    LayoutUI();
                    InvalidateRect(h, NULL, FALSE);
                } else if (c.type == CTRL_TOGGLE) {
                    *(bool*)c.data = !*(bool*)c.data;
                    InvalidateRect(h, NULL, FALSE);
                } else if (c.type == CTRL_SLIDER) {
                    g_dragSlider = &c;
                    SetCapture(h);
                } else if (c.type == CTRL_COLOR) {
                    g_esp.boneColor = g_swatches[c.id];
                    InvalidateRect(h, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_dragSlider) { g_dragSlider = nullptr; ReleaseCapture(); }
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
    g_client     = cifC("VClient018", nullptr);
    if (!g_client) g_client = cifC("VClient017", nullptr);

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
