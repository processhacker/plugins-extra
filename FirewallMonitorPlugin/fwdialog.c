/*
 * Process Hacker Extra Plugins -
 *   Firewall Monitor
 *
 * Copyright (C) 2015 dmex
 *
 * This file is part of Process Hacker.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

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