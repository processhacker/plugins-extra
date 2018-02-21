/*
* Process Hacker Extra Plugins -
*   Nvidia GPU Plugin
*
* Copyright (C) 2015-2016 dmex
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

#include "main.h"

VOID NvUpdateDetails(
    _In_ HWND DetailsHandle
    )
{
    SetDlgItemText(DetailsHandle, IDC_EDIT1, ((PPH_STRING)PhAutoDereferenceObject(NvGpuQueryName()))->Buffer);
    SetDlgItemText(DetailsHandle, IDC_EDIT2, ((PPH_STRING)PhAutoDereferenceObject(NvGpuQueryShortName()))->Buffer);
    SetDlgItemText(DetailsHandle, IDC_EDIT3, ((PPH_STRING)PhAutoDereferenceObject(NvGpuQueryVbiosVersionString()))->Buffer);
    SetDlgItemText(DetailsHandle, IDC_EDIT4, ((PPH_STRING)PhAutoDereferenceObject(NvGpuQueryRevision()))->Buffer);
    SetDlgItemText(DetailsHandle, IDC_EDIT5, ((PPH_STRING)PhAutoDereferenceObject(NvGpuQueryDeviceId()))->Buffer);
    SetDlgItemText(DetailsHandle, IDC_EDIT6, ((PPH_STRING)PhAutoDereferenceObject(NvGpuQueryRopsCount()))->Buffer);
    SetDlgItemText(DetailsHandle, IDC_EDIT10, ((PPH_STRING)PhAutoDereferenceObject(NvGpuQueryDriverVersion()))->Buffer);
    SetDlgItemText(DetailsHandle, IDC_EDIT14, ((PPH_STRING)PhAutoDereferenceObject(NvGpuQueryRamType()))->Buffer);
    //SetDlgItemText(DetailsHandle, IDC_EDIT13, NvGpuDriverIsWHQL() ? L"WHQL" : L"");
}

INT_PTR CALLBACK DetailsDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PPH_NVGPU_SYSINFO_CONTEXT context = NULL;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PPH_NVGPU_SYSINFO_CONTEXT)lParam;

        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);

        if (uMsg == WM_NCDESTROY)
        {
            PhRemoveWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
        }
    }

    if (context == NULL)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            SetWindowText(hwndDlg, ((PPH_STRING)PhAutoDereferenceObject(NvGpuQueryName()))->Buffer);
            PhCenterWindow(hwndDlg, GetParent(hwndDlg));

            NvUpdateDetails(hwndDlg);
        }
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

VOID ShowDetailsDialog(
    _In_ HWND ParentHandle,
    _In_ PVOID Context
    )
{
    DialogBoxParam(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_GPU_DETAILS),
        ParentHandle,
        DetailsDlgProc,
        (LPARAM)Context
        );
}