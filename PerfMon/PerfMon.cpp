#include "stdafx.h"
#include "PerfMon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

CPerfMonApp theApp;

CPerfMonApp::CPerfMonApp()
{
}

BOOL CPerfMonApp::InitInstance()
{
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES;
    ::InitCommonControlsEx(&icc);

    if (!CWinApp::InitInstance())
    {
        return FALSE;
    }

    SetRegistryKey(_T("PerfMon"));

    if (!m_dlg.Create(IDD_SETTINGS_DIALOG, nullptr))
    {
        return FALSE;
    }

    m_pMainWnd = &m_dlg;
    m_dlg.ShowWindow(SW_SHOW);
    m_dlg.UpdateWindow();

    return TRUE;
}
