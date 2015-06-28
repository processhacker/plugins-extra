#include "fwmon.h"

static INT_PTR CALLBACK OptionsDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PFW_EVENT_NODE context;

    if (uMsg == WM_INITDIALOG)
    {
        SetProp(hwndDlg, L"Context", (HANDLE)lParam);
        context = (PFW_EVENT_NODE)GetProp(hwndDlg, L"Context");
    }
    else
    {
        context = (PFW_EVENT_NODE)GetProp(hwndDlg, L"Context");

        if (uMsg == WM_DESTROY)
            RemoveProp(hwndDlg, L"Context");
    }

    if (!context)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        PhCenterWindow(hwndDlg, PhMainWndHandle);
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDCANCEL:
            case IDOK:
                EndDialog(hwndDlg, IDOK);
                break;
            }
        }
        break;
    }

    return FALSE;
}

NTSTATUS NTAPI ShowFwRuleProperties(
    _In_ PVOID ThreadParameter
    )
{
    //DialogBoxParam(
    //    PluginInstance->DllBase, 
    //    MAKEINTRESOURCE(IDD_PROPDIALOG), 
    //    PhMainWndHandle, 
    //    OptionsDlgProc,
    //    (LPARAM)ThreadParameter
    //    );

    return STATUS_SUCCESS;
}