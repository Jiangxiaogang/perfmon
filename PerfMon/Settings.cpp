#include "stdafx.h"
#include "Settings.h"

PanelSettings::PanelSettings()
    : fontName(_T("Microsoft YaHei UI"))
    , fontSize(18)
    , fontColor(RGB(255, 255, 255))
    , alpha(190)
    , posX(16)
    , posY(16)
{
}

void LoadSettings(PanelSettings& s)
{
    CWinApp* app = AfxGetApp();
    if (app == nullptr)
    {
        return;
    }

    s.fontName  = app->GetProfileString(_T("Panel"), _T("FontName"), s.fontName);
    s.fontSize  = app->GetProfileInt(_T("Panel"), _T("FontSize"), s.fontSize);
    s.fontColor = (COLORREF)app->GetProfileInt(_T("Panel"), _T("FontColor"), (int)s.fontColor);
    s.alpha     = app->GetProfileInt(_T("Panel"), _T("Alpha"), s.alpha);
    s.posX      = app->GetProfileInt(_T("Panel"), _T("PosX"), s.posX);
    s.posY      = app->GetProfileInt(_T("Panel"), _T("PosY"), s.posY);

    if (s.fontSize < 8)   s.fontSize = 8;
    if (s.fontSize > 96)  s.fontSize = 96;
    if (s.alpha < 20)     s.alpha = 20;
    if (s.alpha > 255)    s.alpha = 255;
    if (s.posX < -30000)  s.posX = 16;
    if (s.posX > 30000)   s.posX = 16;
    if (s.posY < -30000)  s.posY = 16;
    if (s.posY > 30000)   s.posY = 16;
}

void SaveSettings(const PanelSettings& s)
{
    CWinApp* app = AfxGetApp();
    if (app == nullptr)
    {
        return;
    }

    app->WriteProfileString(_T("Panel"), _T("FontName"), s.fontName);
    app->WriteProfileInt(_T("Panel"), _T("FontSize"), s.fontSize);
    app->WriteProfileInt(_T("Panel"), _T("FontColor"), (int)s.fontColor);
    app->WriteProfileInt(_T("Panel"), _T("Alpha"), s.alpha);
    app->WriteProfileInt(_T("Panel"), _T("PosX"), s.posX);
    app->WriteProfileInt(_T("Panel"), _T("PosY"), s.posY);
}
