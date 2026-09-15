#pragma once

#include "Settings.h"
#include "MetricsCollector.h"
#include "FpsMonitor.h"

// Posted to the overlay's owner when the user finishes dragging the panel.
#define WM_PANELMOVED (WM_APP + 2)

// Transparent, always-on-top panel that renders the live performance values in
// the style of the Xbox Game Bar overlay. The background is alpha blended while
// the text stays fully opaque, so adjusting the opacity only affects the panel
// background. When the settings window is open the panel can be dragged.
class COverlayWnd : public CWnd
{
public:
    COverlayWnd();
    ~COverlayWnd();

    BOOL Create(CWnd* pOwner);
    void Shutdown();

    void ApplySettings(const PanelSettings& settings);
    void SetDraggable(bool draggable);

    const MetricsSnapshot& Snapshot() const { return m_snapshot; }
    int PanelX() const { return m_settings.posX; }
    int PanelY() const { return m_settings.posY; }

protected:
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnExitSizeMove();
    DECLARE_MESSAGE_MAP()

private:
    void RebuildFont();
    void RecomputeSize();
    void Reposition();
    void BuildLines(CString labels[], CString values[], int& count) const;
    UINT GetLineHeight() const;
    void DrawStringLine(HDC dc, const CString& text, const RECT& rc);

    void EnsureSurface();
    void DestroySurface();
    void Render();

    PanelSettings     m_settings;
    CFont             m_font;
    CMetricsCollector m_metrics;
    CFpsMonitor       m_fps;
    MetricsSnapshot   m_snapshot;
    CSize             m_size;
    bool              m_monitorsStarted;
    bool              m_draggable;

    HDC               m_memDC;
    HBITMAP           m_dib;
    HGDIOBJ           m_oldBitmap;
    void*             m_bits;
    CSize             m_surfaceSize;
    HTHEME            m_theme;
};
