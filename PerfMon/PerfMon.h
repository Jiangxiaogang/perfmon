#pragma once

#include "resource.h"
#include "SettingsDlg.h"

class CPerfMonApp : public CWinApp
{
public:
    CPerfMonApp();
    virtual BOOL InitInstance();

private:
    CSettingsDlg m_dlg;
};

extern CPerfMonApp theApp;
