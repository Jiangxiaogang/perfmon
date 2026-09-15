#include "stdafx.h"
#include "OverlayWnd.h"

#pragma comment(lib, "uxtheme.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    const UINT_PTR kRefreshTimerId = 1;
    const int      kPadding = 12;
    const int      kLineSpacing = 4;
    const int      kColumnGap = 16;
    const COLORREF kBackColor = RGB(16, 16, 16);

    CString FormatMB(double bytes)
    {
        CString s;
        s.Format(_T("%.0f"), bytes / 1024.0 / 1024.0);
        return s;
    }
}

BEGIN_MESSAGE_MAP(COverlayWnd, CWnd)
    ON_WM_TIMER()
    ON_WM_NCHITTEST()
    ON_WM_EXITSIZEMOVE()
END_MESSAGE_MAP()

COverlayWnd::COverlayWnd()
    : m_size(0, 0)
    , m_monitorsStarted(false)
    , m_draggable(false)
    , m_memDC(nullptr)
    , m_dib(nullptr)
    , m_oldBitmap(nullptr)
    , m_bits(nullptr)
    , m_surfaceSize(0, 0)
    , m_theme(nullptr)
{
}

COverlayWnd::~COverlayWnd()
{
    Shutdown();
}

BOOL COverlayWnd::Create(CWnd* pOwner)
{
    LPCTSTR className = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(nullptr, IDC_ARROW),
        nullptr,
        nullptr);

    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW
        | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT;

    HWND ownerWnd = (pOwner != nullptr) ? pOwner->GetSafeHwnd() : nullptr;

    if (!CreateEx(exStyle, className, _T("PerfMonPanel"), WS_POPUP,
                  0, 0, 120, 80, ownerWnd, nullptr))
    {
        return FALSE;
    }

    m_theme = ::OpenThemeData(m_hWnd, L"WINDOW");

    RebuildFont();
    RecomputeSize();
    EnsureSurface();
    Reposition();
    Render();
    ShowWindow(SW_SHOWNOACTIVATE);

    m_metrics.Initialize();
    m_fps.Start();
    m_monitorsStarted = true;

    SetTimer(kRefreshTimerId, 1000, nullptr);
    return TRUE;
}

void COverlayWnd::Shutdown()
{
    if (m_monitorsStarted)
    {
        KillTimer(kRefreshTimerId);
        m_fps.Stop();
        m_metrics.Shutdown();
        m_monitorsStarted = false;
    }

    DestroySurface();

    if (m_theme != nullptr)
    {
        ::CloseThemeData(m_theme);
        m_theme = nullptr;
    }
}

void COverlayWnd::ApplySettings(const PanelSettings& settings)
{
    m_settings = settings;

    if (GetSafeHwnd() == nullptr)
    {
        return;
    }

    RebuildFont();
    RecomputeSize();
    EnsureSurface();
    Reposition();
    Render();
}

void COverlayWnd::SetDraggable(bool draggable)
{
    m_draggable = draggable;

    if (GetSafeHwnd() == nullptr)
    {
        return;
    }

    LONG style = ::GetWindowLong(m_hWnd, GWL_EXSTYLE);
    if (draggable)
    {
        // Receive mouse input and allow activation so the panel can be dragged.
        style &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    }
    else
    {
        // Click-through, never activated.
        style |= (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    }
    ::SetWindowLong(m_hWnd, GWL_EXSTYLE, style);
}

LRESULT COverlayWnd::OnNcHitTest(CPoint point)
{
    if (m_draggable)
    {
        return HTCAPTION;
    }
    return CWnd::OnNcHitTest(point);
}

void COverlayWnd::OnExitSizeMove()
{
    if (m_draggable)
    {
        CRect rect;
        GetWindowRect(&rect);
        m_settings.posX = rect.left;
        m_settings.posY = rect.top;

        CWnd* owner = GetOwner();
        if (owner != nullptr)
        {
            owner->PostMessage(WM_PANELMOVED);
        }
    }

    Render();
}

void COverlayWnd::RebuildFont()
{
    if (m_font.GetSafeHandle() != nullptr)
    {
        m_font.DeleteObject();
    }

    HDC screen = ::GetDC(nullptr);
    int dpiY = GetDeviceCaps(screen, LOGPIXELSY);
    ::ReleaseDC(nullptr, screen);

    LOGFONT lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -MulDiv(m_settings.fontSize, dpiY, 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    _tcsncpy_s(lf.lfFaceName, m_settings.fontName, _TRUNCATE);

    m_font.CreateFontIndirect(&lf);
}

UINT COverlayWnd::GetLineHeight() const
{
    UINT height = m_settings.fontSize + 6;
    if (m_font.GetSafeHandle() != nullptr)
    {
        HDC screen = ::GetDC(nullptr);
        HGDIOBJ old = ::SelectObject(screen, (HGDIOBJ)m_font.GetSafeHandle());
        TEXTMETRIC tm;
        if (::GetTextMetrics(screen, &tm))
        {
            height = (UINT)(tm.tmHeight + tm.tmExternalLeading);
        }
        ::SelectObject(screen, old);
        ::ReleaseDC(nullptr, screen);
    }
    return height;
}

void COverlayWnd::BuildLines(CString labels[], CString values[], int& count) const
{
    count = 0;

    labels[count] = _T("CPU");
    values[count].Format(_T("%.1f%%"), m_snapshot.cpuPercent);
    ++count;

    labels[count] = _T("GPU");
    if (m_snapshot.gpuValid)
    {
        values[count].Format(_T("%.1f%%"), m_snapshot.gpuPercent);
    }
    else
    {
        values[count] = _T("--");
    }
    ++count;

    labels[count] = _T("RAM");
    values[count].Format(_T("%s/%s MB"),
        (LPCTSTR)FormatMB(m_snapshot.ramUsedBytes),
        (LPCTSTR)FormatMB(m_snapshot.ramTotalBytes));
    ++count;

    labels[count] = _T("VRAM");
    if (m_snapshot.vramTotalBytes > 0)
    {
        values[count].Format(_T("%s/%s MB"),
            (LPCTSTR)FormatMB((double)m_snapshot.vramUsedBytes),
            (LPCTSTR)FormatMB((double)m_snapshot.vramTotalBytes));
    }
    else if (m_snapshot.vramValid)
    {
        values[count].Format(_T("%s MB"), (LPCTSTR)FormatMB((double)m_snapshot.vramUsedBytes));
    }
    else
    {
        values[count] = _T("--");
    }
    ++count;

    labels[count] = _T("FPS");
    if (m_snapshot.fpsValid)
    {
        values[count].Format(_T("%.0f"), m_snapshot.fps);
    }
    else
    {
        values[count] = _T("--");
    }
    ++count;
}

void COverlayWnd::RecomputeSize()
{
    CString labels[8];
    CString values[8];
    int count = 0;
    BuildLines(labels, values, count);

    int labelWidth = 100;
    int valueWidth = 100;
    int lineHeight = (int)GetLineHeight();

    HDC screen = ::GetDC(nullptr);
    HGDIOBJ old = ::SelectObject(screen, (HGDIOBJ)m_font.GetSafeHandle());
    for (int i = 0; i < count; ++i)
    {
        SIZE sz;
        if (::GetTextExtentPoint32(screen, labels[i], labels[i].GetLength(), &sz) && sz.cx > labelWidth)
        {
            labelWidth = sz.cx;
        }
        if (::GetTextExtentPoint32(screen, values[i], values[i].GetLength(), &sz) && sz.cx > valueWidth)
        {
            valueWidth = sz.cx;
        }
    }
    ::SelectObject(screen, old);
    ::ReleaseDC(nullptr, screen);

    m_size.cx = labelWidth + kColumnGap + valueWidth + kPadding * 2;
    m_size.cy = lineHeight * count + kLineSpacing * (count - 1) + kPadding * 2;
}

void COverlayWnd::Reposition()
{
    if (GetSafeHwnd() == nullptr)
    {
        return;
    }

    int x = m_settings.posX;
    int y = m_settings.posY;

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // Keep the panel reachable on the virtual desktop.
    if (x < vx - m_size.cx + 40)  x = vx;
    if (x > vx + vw - 40)         x = vx + vw - m_size.cx;
    if (y < vy)                   y = vy;
    if (y > vy + vh - 40)         y = vy + vh - m_size.cy;

    SetWindowPos(nullptr, x, y, m_size.cx, m_size.cy,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

void COverlayWnd::EnsureSurface()
{
    if (m_dib != nullptr && m_surfaceSize == m_size)
    {
        return;
    }

    DestroySurface();

    if (m_size.cx <= 0 || m_size.cy <= 0)
    {
        return;
    }

    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = m_size.cx;
    info.bmiHeader.biHeight = -m_size.cy;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC screen = ::GetDC(nullptr);
    m_memDC = ::CreateCompatibleDC(screen);
    ::ReleaseDC(nullptr, screen);

    m_dib = ::CreateDIBSection(m_memDC, &info, DIB_RGB_COLORS, &m_bits, nullptr, 0);
    if (m_dib != nullptr)
    {
        m_oldBitmap = ::SelectObject(m_memDC, m_dib);
    }

    m_surfaceSize = m_size;
}

void COverlayWnd::DestroySurface()
{
    if (m_memDC != nullptr)
    {
        if (m_oldBitmap != nullptr)
        {
            ::SelectObject(m_memDC, m_oldBitmap);
        }
        ::DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
    if (m_dib != nullptr)
    {
        ::DeleteObject(m_dib);
        m_dib = nullptr;
    }

    m_bits = nullptr;
    m_oldBitmap = nullptr;
    m_surfaceSize = CSize(0, 0);
}

void COverlayWnd::DrawStringLine(HDC dc, const CString& text, const RECT& rc)
{
    RECT rect = rc;

    if (m_theme != nullptr)
    {
        DTTOPTS opts;
        ZeroMemory(&opts, sizeof(opts));
        opts.dwSize = sizeof(opts);
        opts.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR;
        opts.crText = m_settings.fontColor;
        ::DrawThemeTextEx(m_theme, dc, 0, 0, text, text.GetLength(),
            DT_LEFT | DT_TOP | DT_SINGLELINE, &rect, &opts);
    }
    else
    {
        ::SetBkMode(dc, TRANSPARENT);
        ::SetTextColor(dc, m_settings.fontColor);
        ::DrawText(dc, text, text.GetLength(), &rect, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }
}

void COverlayWnd::Render()
{
    if (GetSafeHwnd() == nullptr || m_bits == nullptr)
    {
        return;
    }

    // Background is pre-multiplied by the configured alpha; the text drawn by
    // DrawThemeTextEx(DTT_COMPOSITED) is written with full opacity.
    int alpha = m_settings.alpha;
    if (alpha < 0)   alpha = 0;
    if (alpha > 255) alpha = 255;

    BYTE r = (BYTE)(GetRValue(kBackColor) * alpha / 255);
    BYTE g = (BYTE)(GetGValue(kBackColor) * alpha / 255);
    BYTE b = (BYTE)(GetBValue(kBackColor) * alpha / 255);
    DWORD background = ((DWORD)alpha << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;

    int total = m_size.cx * m_size.cy;
    DWORD* pixels = (DWORD*)m_bits;
    for (int i = 0; i < total; ++i)
    {
        pixels[i] = background;
    }

    HGDIOBJ oldFont = ::SelectObject(m_memDC, (HGDIOBJ)m_font.GetSafeHandle());

    CString labels[8];
    CString values[8];
    int count = 0;
    BuildLines(labels, values, count);

    int lineHeight = (int)GetLineHeight();
    int y = kPadding;
    for (int i = 0; i < count; ++i)
    {
        RECT labelRect;
        labelRect.left = kPadding;
        labelRect.top = y;
        labelRect.right = m_size.cx - kPadding;
        labelRect.bottom = y + lineHeight;
        DrawStringLine(m_memDC, labels[i], labelRect);

        SIZE valueSize;
        ::GetTextExtentPoint32(m_memDC, values[i], values[i].GetLength(), &valueSize);

        RECT valueRect;
        valueRect.left = m_size.cx - kPadding - valueSize.cx;
        valueRect.top = y;
        valueRect.right = m_size.cx - kPadding;
        valueRect.bottom = y + lineHeight;
        DrawStringLine(m_memDC, values[i], valueRect);

        y += lineHeight + kLineSpacing;
    }

    ::SelectObject(m_memDC, oldFont);

    CRect windowRect;
    GetWindowRect(&windowRect);

    POINT dest = { windowRect.left, windowRect.top };
    SIZE size = { m_size.cx, m_size.cy };
    POINT src = { 0, 0 };
    BLENDFUNCTION blend;
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    HDC screen = ::GetDC(nullptr);
    ::UpdateLayeredWindow(m_hWnd, screen, &dest, &size, m_memDC, &src, 0, &blend, ULW_ALPHA);
    ::ReleaseDC(nullptr, screen);
}

void COverlayWnd::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == kRefreshTimerId)
    {
        m_fps.Update();
        m_snapshot = m_metrics.Collect();
        m_snapshot.fps = m_fps.Fps();
        m_snapshot.fpsValid = m_fps.Valid();
        Render();
    }

    CWnd::OnTimer(nIDEvent);
}
