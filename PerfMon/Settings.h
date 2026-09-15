#pragma once

struct PanelSettings
{
    CString    fontName;
    int        fontSize;
    COLORREF   fontColor;
    int        alpha;
    int        posX;
    int        posY;

    PanelSettings();
};

void LoadSettings(PanelSettings& s);
void SaveSettings(const PanelSettings& s);
