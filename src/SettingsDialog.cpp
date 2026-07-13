// =============================================================================
// SettingsDialog.cpp  -  Configuration dialog (/c mode)
// -----------------------------------------------------------------------------
// A resource-template dialog that loads the current Settings, lets the user
// tune the ecosystem / presentation / AI-tips / audio options, then persists
// them to the registry on OK. Reachable from Windows Settings ("Screen saver
// settings -> Settings...") and from double-clicking the .scr.
// =============================================================================
#include "Common.h"
#include "Settings.h"
#include "ScreenSaverHost.h"
#include "../resources/resource.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace nyx {

static Settings g_cfg;

static void ComboAdd(HWND dlg, int id, const wchar_t* text) {
    SendDlgItemMessageW(dlg, id, CB_ADDSTRING, 0, (LPARAM)text);
}

static void InitControls(HWND dlg) {
    g_cfg.Load();

    SetDlgItemInt(dlg, IDC_FISHCOUNT, g_cfg.fishCount, FALSE);

    // Trackbars 0..200 -> 0.0..2.0
    SendDlgItemMessageW(dlg, IDC_BUBBLE, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
    SendDlgItemMessageW(dlg, IDC_BUBBLE, TBM_SETPOS, TRUE, (LPARAM)(g_cfg.bubbleDensity * 100));
    SendDlgItemMessageW(dlg, IDC_LIGHT, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
    SendDlgItemMessageW(dlg, IDC_LIGHT, TBM_SETPOS, TRUE, (LPARAM)(g_cfg.lightingIntensity * 100));

    ComboAdd(dlg, IDC_QUALITY, L"Performance");
    ComboAdd(dlg, IDC_QUALITY, L"Balanced");
    ComboAdd(dlg, IDC_QUALITY, L"Ultra");
    SendDlgItemMessageW(dlg, IDC_QUALITY, CB_SETCURSEL, (WPARAM)g_cfg.quality, 0);

    ComboAdd(dlg, IDC_THEME, L"Reef");
    ComboAdd(dlg, IDC_THEME, L"Abyss");
    ComboAdd(dlg, IDC_THEME, L"Cyberpunk");
    SendDlgItemMessageW(dlg, IDC_THEME, CB_SETCURSEL, (WPARAM)g_cfg.theme, 0);

    CheckDlgButton(dlg, IDC_THREAT, g_cfg.threatCreatures ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_HUD,    g_cfg.cyberHud       ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_TIPS,   g_cfg.educationalTips? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_OLLAMA, g_cfg.useOllama      ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_MUSIC,  g_cfg.music          ? BST_CHECKED : BST_UNCHECKED);

    SetDlgItemTextW(dlg, IDC_MODEL, g_cfg.ollamaModel.c_str());
}

static void ReadControls(HWND dlg) {
    BOOL ok = FALSE;
    int fc = (int)GetDlgItemInt(dlg, IDC_FISHCOUNT, &ok, FALSE);
    if (ok) g_cfg.fishCount = (fc < 8) ? 8 : (fc > 1000 ? 1000 : fc);

    g_cfg.bubbleDensity    = (float)SendDlgItemMessageW(dlg, IDC_BUBBLE, TBM_GETPOS, 0, 0) / 100.0f;
    g_cfg.lightingIntensity= (float)SendDlgItemMessageW(dlg, IDC_LIGHT, TBM_GETPOS, 0, 0) / 100.0f;

    g_cfg.quality = (Quality)SendDlgItemMessageW(dlg, IDC_QUALITY, CB_GETCURSEL, 0, 0);
    g_cfg.theme   = (Theme)SendDlgItemMessageW(dlg, IDC_THEME, CB_GETCURSEL, 0, 0);

    g_cfg.threatCreatures  = IsDlgButtonChecked(dlg, IDC_THREAT) == BST_CHECKED;
    g_cfg.cyberHud         = IsDlgButtonChecked(dlg, IDC_HUD)    == BST_CHECKED;
    g_cfg.educationalTips  = IsDlgButtonChecked(dlg, IDC_TIPS)   == BST_CHECKED;
    g_cfg.useOllama        = IsDlgButtonChecked(dlg, IDC_OLLAMA) == BST_CHECKED;
    g_cfg.music            = IsDlgButtonChecked(dlg, IDC_MUSIC)  == BST_CHECKED;

    wchar_t model[256] = {};
    GetDlgItemTextW(dlg, IDC_MODEL, model, _countof(model));
    if (model[0]) g_cfg.ollamaModel = model;

    g_cfg.Save();
}

static INT_PTR CALLBACK DlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        InitControls(dlg);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK)      { ReadControls(dlg); EndDialog(dlg, IDOK); return TRUE; }
        if (LOWORD(wp) == IDCANCEL)  { EndDialog(dlg, IDCANCEL); return TRUE; }
        break;
    }
    return FALSE;
}

int RunConfig(HINSTANCE hInst, HWND parent) {
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_SETTINGS), parent, DlgProc, 0);
    return 0;
}

} // namespace nyx
