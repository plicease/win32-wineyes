// wineyes - a Win32 clone of the classic X11 xeyes.
//
// Copyright (c) 2026 Graham Ollis. Licensed under the MIT License; see LICENSE.
//
// The window is a borderless per-pixel-alpha layered window: everything
// outside the eyes is fully transparent (and click-through), the eyes are
// drawn anti-aliased with GDI+ and the pupils follow the mouse cursor
// anywhere on the desktop.
//
// Controls:
//   left-drag on an eye    move the window
//   mouse wheel            grow / shrink
//   right-click            menu (always on top, reset size, exit)
//   Esc                    exit

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shellscalingapi.h>
#include <math.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shcore.lib")

using namespace Gdiplus;

// xeyes' default geometry was 150x100 pixels. X servers of the era usually
// ran at 75-100 DPI, so we treat those pixels as being at ~80 DPI and scale
// from there to the monitor's real DPI to get roughly the same physical size
// (about 1.9" x 1.25").
static const double kBaseWidth  = 150.0;
static const double kBaseHeight = 100.0;
static const double kReferenceDpi = 80.0;

// Proportions relative to an eye's horizontal radius (after xeyes).
static const double kRimThick  = 0.175; // black rim thickness
static const double kPupilRad  = 0.2;   // pupil radius
static const double kPupilPad  = 0.05;  // gap kept between pupil and rim

static const UINT kTimerId = 1;
static const UINT kTimerMs = 15;

enum { IDM_TOPMOST = 100, IDM_RESET, IDM_EXIT };

static const wchar_t kClassName[] = L"WinEyesWindow";

struct State {
    double scale = 1.0;   // user zoom (mouse wheel)
    UINT dpi = 96;
    HMONITOR monitor = nullptr;
    int width = 0, height = 0;
    bool topmost = false;
    POINT lastCursor = { LONG_MIN, LONG_MIN };
    RECT lastRect = {};
};
static State g;

// The monitor's physical DPI as reported by its EDID, so the eyes come out
// the same real-world size regardless of the Windows scaling setting. Falls
// back to the effective (scaling) DPI when the raw value is missing or silly,
// as it is for some TVs and projectors.
static UINT MonitorDpi(HMONITOR mon)
{
    UINT x = 0, y = 0;
    if (SUCCEEDED(GetDpiForMonitor(mon, MDT_RAW_DPI, &x, &y)) && x >= 50 && x <= 600)
        return x;
    if (SUCCEEDED(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &x, &y)) && x)
        return x;
    return 96;
}

static int ScaledWidth() { return (int)lround(kBaseWidth  * g.scale * g.dpi / kReferenceDpi); }
static int ScaledHeight() { return (int)lround(kBaseHeight * g.scale * g.dpi / kReferenceDpi); }

// Draw both eyes into `gr` for a w x h box at (ox, oy). `cursor` is in the
// same coordinate space as the box.
static void DrawEyes(Graphics& gr, REAL ox, REAL oy, REAL w, REAL h, PointF cursor)
{
    gr.SetSmoothingMode(SmoothingModeAntiAlias);
    gr.SetPixelOffsetMode(PixelOffsetModeHalf);

    SolidBrush black(Color(255, 0, 0, 0));
    SolidBrush white(Color(255, 255, 255, 255));

    const REAL margin = w * 0.01f;
    const REAL rx = w / 4 - margin;
    const REAL ry = h / 2 - margin;
    const REAL rim = (REAL)(kRimThick * rx);
    const REAL pr  = (REAL)(kPupilRad * rx);
    const REAL pad = (REAL)(kPupilPad * rx);

    for (int i = 0; i < 2; ++i) {
        const REAL cx = ox + w * (i == 0 ? 0.25f : 0.75f);
        const REAL cy = oy + h / 2;

        gr.FillEllipse(&black, cx - rx, cy - ry, 2 * rx, 2 * ry);
        const REAL irx = rx - rim, iry = ry - rim;
        gr.FillEllipse(&white, cx - irx, cy - iry, 2 * irx, 2 * iry);

        // Pupil travels inside an ellipse inset from the white by its radius.
        // If the cursor lies inside that ellipse the pupil sits right on it,
        // otherwise it is pinned to the edge in the cursor's direction.
        const double ax = irx - pr - pad, ay = iry - pr - pad;
        double dx = cursor.X - cx, dy = cursor.Y - cy;
        const double nx = dx / ax, ny = dy / ay;
        const double len = sqrt(nx * nx + ny * ny);
        if (len > 1.0) { dx /= len; dy /= len; }
        const REAL px = (REAL)(cx + dx), py = (REAL)(cy + dy);
        gr.FillEllipse(&black, px - pr, py - pr, 2 * pr, 2 * pr);
    }
}

// Render a w x h premultiplied-ARGB DIB with the eyes looking at `cursor`
// (client coordinates). Caller owns the returned bitmap.
static HBITMAP RenderBitmap(int w, int h, PointF cursor, void** bitsOut = nullptr)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm) return nullptr;
    ZeroMemory(bits, (size_t)w * h * 4);
    {
        Bitmap canvas(w, h, w * 4, PixelFormat32bppPARGB, (BYTE*)bits);
        Graphics gr(&canvas);
        DrawEyes(gr, 0, 0, (REAL)w, (REAL)h, cursor);
    }
    if (bitsOut) *bitsOut = bits;
    return hbm;
}

static void Redraw(HWND hwnd, bool force)
{
    POINT pt;
    GetCursorPos(&pt);
    RECT rc;
    GetWindowRect(hwnd, &rc);

    if (!force && pt.x == g.lastCursor.x && pt.y == g.lastCursor.y &&
        EqualRect(&rc, &g.lastRect))
        return;
    g.lastCursor = pt;
    g.lastRect = rc;

    const int w = g.width, h = g.height;
    HBITMAP hbm = RenderBitmap(w, h, PointF((REAL)(pt.x - rc.left), (REAL)(pt.y - rc.top)));
    if (!hbm) return;

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(mem, hbm);

    POINT src = { 0, 0 };
    POINT dst = { rc.left, rc.top };
    SIZE size = { w, h };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd, screen, &dst, &size, mem, &src, 0, &bf, ULW_ALPHA);

    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    DeleteObject(hbm);
}

// Resize around the window's current center.
static void ApplySize(HWND hwnd)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    const int cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
    g.width = ScaledWidth();
    g.height = ScaledHeight();
    SetWindowPos(hwnd, nullptr, cx - g.width / 2, cy - g.height / 2, g.width, g.height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    Redraw(hwnd, true);
}

// Re-derive the size if the window is now on a different monitor (or always,
// when `force` is set, e.g. after a display settings change).
static void UpdateMonitor(HWND hwnd, bool force)
{
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!force && mon == g.monitor) return;
    g.monitor = mon;
    const UINT dpi = MonitorDpi(mon);
    if (force || dpi != g.dpi) {
        g.dpi = dpi;
        ApplySize(hwnd);
    }
}

static void ShowMenu(HWND hwnd, POINT pt)
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (g.topmost ? MF_CHECKED : 0), IDM_TOPMOST, L"Always on &top");
    AppendMenuW(menu, MF_STRING, IDM_RESET, L"&Reset size");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"E&xit");
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, kTimerId, kTimerMs, nullptr);
        return 0;

    case WM_TIMER:
        if (wp == kTimerId) Redraw(hwnd, false);
        return 0;

    case WM_LBUTTONDOWN:
        // Let the system drag the window as if the eyes were a title bar.
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;

    case WM_RBUTTONUP: {
        POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
        ClientToScreen(hwnd, &pt);
        ShowMenu(hwnd, pt);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        double s = g.scale * pow(1.1, delta / (double)WHEEL_DELTA);
        if (s < 0.25) s = 0.25;
        if (s > 8.0) s = 8.0;
        g.scale = s;
        ApplySize(hwnd);
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_TOPMOST:
            g.topmost = !g.topmost;
            SetWindowPos(hwnd, g.topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            break;
        case IDM_RESET:
            g.scale = 1.0;
            ApplySize(hwnd);
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;

    case WM_DPICHANGED: {
        // We size from the physical DPI ourselves; just take the position.
        const RECT* r = (const RECT*)lp;
        SetWindowPos(hwnd, nullptr, r->left, r->top, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateMonitor(hwnd, true);
        return 0;
    }

    case WM_WINDOWPOSCHANGED:
        // Dragged onto a different monitor (which may have a different DPI).
        UpdateMonitor(hwnd, false);
        break;

    case WM_DISPLAYCHANGE:
        UpdateMonitor(hwnd, true);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    GdiplusStartupInput gsi;
    ULONG_PTR gdipToken = 0;
    if (GdiplusStartup(&gdipToken, &gsi, nullptr) != Ok) return 1;

    // Icon resource 1 (wineyes.ico, via wineyes.rc).
    HICON bigIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                      GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED);
    HICON smallIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = bigIcon;
    wc.hIconSm = smallIcon;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    // Start on the primary monitor, near the top-right corner.
    POINT origin = { 0, 0 };
    HMONITOR mon = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    g.monitor = mon;
    g.dpi = MonitorDpi(mon);
    g.width = ScaledWidth();
    g.height = ScaledHeight();
    const int margin = MulDiv(32, GetDpiForSystem(), 96);
    const int x = mi.rcWork.right - g.width - margin;
    const int y = mi.rcWork.top + margin;

    HWND hwnd = CreateWindowExW(WS_EX_LAYERED, kClassName, L"wineyes", WS_POPUP,
                                x, y, g.width, g.height, nullptr, nullptr, inst, nullptr);
    if (!hwnd) return 1;

    UpdateMonitor(hwnd, true);
    ShowWindow(hwnd, show);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    GdiplusShutdown(gdipToken);
    return (int)m.wParam;
}
