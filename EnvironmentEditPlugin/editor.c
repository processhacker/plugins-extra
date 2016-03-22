/*
 * Process Hacker Extra Plugins -
 *   Environment Edit Plugin
 *
 * Copyright (C) 2016 dmex
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

static INT_PTR CALLBACK EnvironmentEditorDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PENVIRONMENT_EDIT_CONTEXT context;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PENVIRONMENT_EDIT_CONTEXT)lParam;
        SetProp(hwndDlg, L"Context", (HANDLE)context);
    }
    else
    {
        context = (PENVIRONMENT_EDIT_CONTEXT)GetProp(hwndDlg, L"Context");

        if (uMsg == WM_NCDESTROY)
        {
            RemoveProp(hwndDlg, L"Context");
            PhFree(context);
        }
    }

    if (!context)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            context->DialogHandle = hwndDlg;
            PhCenterWindow(hwndDlg, GetParent(hwndDlg));

            if (context->VariableName)
                SetDlgItemText(hwndDlg, IDC_NAME_TEXT, context->VariableName->Buffer);

            if (context->VariableValue)
                SetDlgItemText(hwndDlg, IDC_VALUE_TEXT, context->VariableValue->Buffer);
            
            PhRegisterDialog(hwndDlg);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_NAME_TEXT), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_VALUE_TEXT), NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDOK), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDCANCEL), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
        }
        return TRUE;
    case WM_DESTROY:
        {
            //PhDereferenceObject(context->ProcessItem);

            //PhSaveListViewColumnsToSetting(SETTING_NAME_COLUMNS, context->ListViewHandle);
            //PhSaveWindowPlacementToSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);

            PhDeleteLayoutManager(&context->LayoutManager);

            PhUnregisterDialog(hwndDlg);
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&context->LayoutManager);
        break;
    case WM_SHOWDIALOG:
        {
            if (IsMinimized(hwndDlg))
                ShowWindow(hwndDlg, SW_RESTORE);
            else
                ShowWindow(hwndDlg, SW_SHOW);

            SetForegroundWindow(hwndDlg);
        }
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDCLOSE:
            case IDCANCEL:
                EndDialog(hwndDlg, IDCLOSE);
                break;
            case IDOK:
                {
                    NTSTATUS status;
                    PPH_STRING name;
                    PPH_STRING value;

                    name = PhGetWindowText(GetDlgItem(hwndDlg, IDC_NAME_TEXT));
                    value = PhGetWindowText(GetDlgItem(hwndDlg, IDC_VALUE_TEXT));
                    
                    if (!NT_SUCCESS(status = SetRemoteEnvironmentVariable(
                        context->ProcessHandle,
                        name->Buffer,
                        value->Buffer
                        )))
                    {
                        PhShowStatus(context->DialogHandle, NULL, status, 0);
                    }

                    PhClearReference(&name);
                    PhClearReference(&value);

                    EndDialog(hwndDlg, IDOK);
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}

VOID ShowAddRemoveDialog(
    _In_ HWND Parent,
    _In_ HANDLE ProcessHandle,
    _In_opt_ PPH_STRING VariableName,
    _In_opt_ PPH_STRING VariableValue
    )
{
    PENVIRONMENT_EDIT_CONTEXT context;

    context = (PENVIRONMENT_EDIT_CONTEXT)PhAllocate(sizeof(ENVIRONMENT_EDIT_CONTEXT));
    memset(context, 0, sizeof(ENVIRONMENT_EDIT_CONTEXT));

    context->ProcessHandle = ProcessHandle;
    context->VariableName = VariableName;
    context->VariableValue = VariableValue;

    DialogBoxParam(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_ADD_EDIT_DIALOG),
        Parent,
        EnvironmentEditorDlgProc,
        (LPARAM)context
        );
}