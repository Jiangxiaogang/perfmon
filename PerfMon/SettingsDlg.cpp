#include "stdafx.h"
#include "SettingsDlg.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    int CALLBACK EnumFontProc(const ENUMLOGFONTEX* lpelfe, const TEXTMETRIC*, DWORD, LPARAM lParam)
    {
        CComboBox* combo = reinterpret_cast<CComboBox*>(lParam);
        const LOGFONT& lf = lpelfe->elfLogFont;

        if (lf.lfFaceName[0] != _T('@'))
        {
            if (combo->FindStringExact(-1, lf.lfFaceName) == CB_ERR)
            {
                combo->AddString(lf.lfFaceName);
            }
        }
        return 1;
    }
}

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialog)
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_HSCROLL()
    ON_WM_CTLCOLOR()
    ON_CBN_SELCHANGE(IDC_COMBO_FONT, &CSettingsDlg::OnCbnSelchangeComboFont)
    ON_CBN_SELCHANGE(IDC_COMBO_SIZE, &CSettingsDlg::OnCbnSelchangeComboSize)
    ON_BN_CLICKED(IDC_BTN_COLOR, &CSettingsDlg::OnBnClickedBtnColor)
    ON_BN_CLICKED(IDC_BTN_APPLY, &CSettingsDlg::OnBnClickedBtnApply)
    ON_BN_CLICKED(IDC_BTN_HIDE, &CSettingsDlg::OnBnClickedBtnHide)
    ON_BN_CLICKED(IDC_BTN_EXIT, &CSettingsDlg::OnBnClickedBtnExit)
    ON_MESSAGE(WM_TRAYICON, &CSettingsDlg::OnTrayMessage)
    ON_MESSAGE(WM_PANELMOVED, &CSettingsDlg::OnPanelMoved)
    ON_COMMAND(IDM_TRAY_SHOW, &CSettingsDlg::OnTrayShow)
    ON_COMMAND(IDM_TRAY_EXIT, &CSettingsDlg::OnTrayExit)
END_MESSAGE_MAP()

CSettingsDlg::CSettingsDlg(CWnd* pParent)
    : CDialog(IDD_SETTINGS_DIALOG, pParent)
    , m_swatchColor(RGB(255, 255, 255))
    , m_trayAdded(false)
    , m_controlsReady(false)
{
    ZeroMemory(&m_nid, sizeof(m_nid));
    ZeroMemory(m_customColors, sizeof(m_customColors));
}

CSettingsDlg::~CSettingsDlg()
{
}

void CSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BOOL CSettingsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetIcon(AfxGetApp()->LoadIcon(IDI_PERFMON), TRUE);

    m_controlsReady = false;
    LoadSettings(m_settings);
    m_swatchColor = m_settings.fontColor;

    FillFontList();
    FillSizeList();

    CSliderCtrl* slider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_ALPHA);
    if (slider != nullptr)
    {
        slider->SetRange(20, 255, TRUE);
        slider->SetTicFreq(25);
        slider->SetPos(m_settings.alpha);
    }

    UpdateSwatch();
    LoadToControls();

    if (m_overlay.Create(this))
    {
        m_overlay.ApplySettings(m_settings);
        m_overlay.SetDraggable(true);
    }

    AddTrayIcon();
    m_controlsReady = true;

    CenterWindow();
    return TRUE;
}

void CSettingsDlg::FillFontList()
{
    CComboBox* combo = (CComboBox*)GetDlgItem(IDC_COMBO_FONT);
    if (combo == nullptr)
    {
        return;
    }

    CClientDC dc(this);
    LOGFONT lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;

    ::EnumFontFamiliesEx(dc.GetSafeHdc(), &lf, (FONTENUMPROC)EnumFontProc,
        (LPARAM)combo, 0);

    int index = combo->FindStringExact(-1, m_settings.fontName);
    if (index == CB_ERR)
    {
        index = combo->FindStringExact(-1, _T("Segoe UI"));
        if (index == CB_ERR)
        {
            index = 0;
        }
        if (index >= 0)
        {
            combo->GetLBText(index, m_settings.fontName);
        }
    }
    combo->SetCurSel(index);
}

void CSettingsDlg::FillSizeList()
{
    CComboBox* combo = (CComboBox*)GetDlgItem(IDC_COMBO_SIZE);
    if (combo == nullptr)
    {
        return;
    }

    static const int sizes[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 28, 32, 36, 40, 48, 56, 64, 72 };
    for (int i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); ++i)
    {
        CString text;
        text.Format(_T("%d"), sizes[i]);
        combo->AddString(text);
    }

    CString current;
    current.Format(_T("%d"), m_settings.fontSize);
    if (combo->FindStringExact(-1, current) == CB_ERR)
    {
        combo->AddString(current);
    }
    combo->SelectString(-1, current);
}

void CSettingsDlg::LoadToControls()
{
    CString alphaText;
    alphaText.Format(_T("%d"), m_settings.alpha);
    SetDlgItemText(IDC_STATIC_ALPHAVAL, alphaText);
}

void CSettingsDlg::ReadFromControls()
{
    CComboBox* font = (CComboBox*)GetDlgItem(IDC_COMBO_FONT);
    if (font != nullptr)
    {
        int index = font->GetCurSel();
        if (index != CB_ERR)
        {
            font->GetLBText(index, m_settings.fontName);
        }
    }

    CComboBox* size = (CComboBox*)GetDlgItem(IDC_COMBO_SIZE);
    if (size != nullptr)
    {
        int index = size->GetCurSel();
        if (index != CB_ERR)
        {
            CString text;
            size->GetLBText(index, text);
            m_settings.fontSize = _ttoi(text);
        }
    }

    CSliderCtrl* slider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_ALPHA);
    if (slider != nullptr)
    {
        m_settings.alpha = slider->GetPos();
    }

    m_settings.fontColor = m_swatchColor;
}

void CSettingsDlg::UpdateSwatch()
{
    if (m_swatchBrush.GetSafeHandle() != nullptr)
    {
        m_swatchBrush.DeleteObject();
    }
    m_swatchBrush.CreateSolidBrush(m_swatchColor);

    CWnd* swatch = GetDlgItem(IDC_STATIC_SWATCH);
    if (swatch != nullptr)
    {
        swatch->Invalidate();
    }
}

void CSettingsDlg::ApplyLive()
{
    if (m_controlsReady)
    {
        m_overlay.ApplySettings(m_settings);
    }
}

HBRUSH CSettingsDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH brush = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

    if (nCtlColor == CTLCOLOR_STATIC && pWnd != nullptr &&
        pWnd->GetDlgCtrlID() == IDC_STATIC_SWATCH)
    {
        pDC->SetBkColor(m_swatchColor);
        return (HBRUSH)m_swatchBrush.GetSafeHandle();
    }

    return brush;
}

void CSettingsDlg::OnCbnSelchangeComboFont()
{
    ReadFromControls();
    ApplyLive();
}

void CSettingsDlg::OnCbnSelchangeComboSize()
{
    ReadFromControls();
    ApplyLive();
}

LRESULT CSettingsDlg::OnPanelMoved(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    m_settings.posX = m_overlay.PanelX();
    m_settings.posY = m_overlay.PanelY();
    SaveSettings(m_settings);
    return 0;
}

void CSettingsDlg::OnBnClickedBtnColor()
{
    CHOOSECOLOR cc;
    ZeroMemory(&cc, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = GetSafeHwnd();
    cc.lpCustColors = m_customColors;
    cc.rgbResult = m_swatchColor;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (::ChooseColor(&cc))
    {
        m_swatchColor = cc.rgbResult;
        UpdateSwatch();
        ReadFromControls();
        ApplyLive();
    }
}

void CSettingsDlg::OnBnClickedBtnApply()
{
    ReadFromControls();
    m_settings.posX = m_overlay.PanelX();
    m_settings.posY = m_overlay.PanelY();
    SaveSettings(m_settings);
    m_overlay.ApplySettings(m_settings);
}

void CSettingsDlg::OnBnClickedBtnHide()
{
    HideToTray();
}

void CSettingsDlg::OnBnClickedBtnExit()
{
    ExitApplication();
}

void CSettingsDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (pScrollBar != nullptr && pScrollBar->GetDlgCtrlID() == IDC_SLIDER_ALPHA)
    {
        CSliderCtrl* slider = (CSliderCtrl*)pScrollBar;
        CString text;
        text.Format(_T("%d"), slider->GetPos());
        SetDlgItemText(IDC_STATIC_ALPHAVAL, text);

        ReadFromControls();
        ApplyLive();
    }

    CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CSettingsDlg::OnClose()
{
    int result = MessageBox(
        _T("关闭设置窗口：\n\n“是(Y)” 隐藏到系统托盘\n“否(N)” 退出程序\n“取消” 返回"),
        _T("性能监控面板"),
        MB_YESNOCANCEL | MB_ICONQUESTION);

    if (result == IDYES)
    {
        HideToTray();
    }
    else if (result == IDNO)
    {
        ExitApplication();
    }
}

void CSettingsDlg::OnCancel()
{
    // A modeless dialog must not be closed by Esc; the close prompt handles it.
}

void CSettingsDlg::OnDestroy()
{
    RemoveTrayIcon();
    if (m_overlay.GetSafeHwnd() != nullptr)
    {
        m_overlay.Shutdown();
        m_overlay.DestroyWindow();
    }
    CDialog::OnDestroy();
}

void CSettingsDlg::HideToTray()
{
    // The panel becomes click-through again once the settings window is hidden.
    m_overlay.SetDraggable(false);
    ShowWindow(SW_HIDE);
}

void CSettingsDlg::ShowFromTray()
{
    ShowWindow(SW_SHOW);
    m_overlay.SetDraggable(true);
    SetForegroundWindow();
}

void CSettingsDlg::AddTrayIcon()
{
    if (m_trayAdded)
    {
        return;
    }

    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = GetSafeHwnd();
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = AfxGetApp()->LoadIcon(IDI_PERFMON);
    _tcsncpy_s(m_nid.szTip, _T("性能监控面板"), _TRUNCATE);

    m_trayAdded = (Shell_NotifyIcon(NIM_ADD, &m_nid) == TRUE);
}

void CSettingsDlg::RemoveTrayIcon()
{
    if (m_trayAdded)
    {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        m_trayAdded = false;
    }
}

LRESULT CSettingsDlg::OnTrayMessage(WPARAM /*wParam*/, LPARAM lParam)
{
    switch (lParam)
    {
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
        ShowFromTray();
        break;

    case WM_RBUTTONUP:
    {
        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, IDM_TRAY_SHOW, _T("显示窗口"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, IDM_TRAY_EXIT, _T("退出"));

        CPoint point;
        GetCursorPos(&point);
        SetForegroundWindow();
        menu.TrackPopupMenu(TPM_RIGHTBUTTON, point.x, point.y, this);
        PostMessage(WM_NULL);
        break;
    }

    default:
        break;
    }

    return 0;
}

void CSettingsDlg::OnTrayShow()
{
    ShowFromTray();
}

void CSettingsDlg::OnTrayExit()
{
    ExitApplication();
}

void CSettingsDlg::ExitApplication()
{
    RemoveTrayIcon();

    if (m_overlay.GetSafeHwnd() != nullptr)
    {
        m_overlay.Shutdown();
        m_overlay.DestroyWindow();
    }

    PostQuitMessage(0);
}
