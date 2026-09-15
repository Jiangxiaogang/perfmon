#pragma once

#include "resource.h"
#include "Settings.h"
#include "OverlayWnd.h"

#define WM_TRAYICON (WM_APP + 1)

#define IDM_TRAY_SHOW 40001
#define IDM_TRAY_EXIT 40002

class CSettingsDlg : public CDialog
{
public:
    CSettingsDlg(CWnd* pParent = nullptr);
    ~CSettingsDlg();

    enum { IDD = IDD_SETTINGS_DIALOG };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);

    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnCancel();
    afx_msg void OnCbnSelchangeComboFont();
    afx_msg void OnCbnSelchangeComboSize();
    afx_msg void OnBnClickedBtnColor();
    afx_msg void OnBnClickedBtnApply();
    afx_msg void OnBnClickedBtnHide();
    afx_msg void OnBnClickedBtnExit();
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg LRESULT OnTrayMessage(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnPanelMoved(WPARAM wParam, LPARAM lParam);
    afx_msg void OnTrayShow();
    afx_msg void OnTrayExit();
    DECLARE_MESSAGE_MAP()

private:
    void FillFontList();
    void FillSizeList();
    void LoadToControls();
    void ReadFromControls();
    void UpdateSwatch();
    void ApplyLive();
    void HideToTray();
    void ShowFromTray();
    void AddTrayIcon();
    void RemoveTrayIcon();
    void ExitApplication();

    PanelSettings m_settings;
    COverlayWnd   m_overlay;

    COLORREF m_swatchColor;
    CBrush   m_swatchBrush;
    COLORREF m_customColors[16];

    NOTIFYICONDATA m_nid;
    bool m_trayAdded;
    bool m_controlsReady;
};
