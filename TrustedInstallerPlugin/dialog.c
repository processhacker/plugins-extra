/*
 * Process Hacker Extra Plugins -
 *   Trusted Installer Plugin
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

NTSTATUS RunAsTrustedInstallerThread(
    _In_ PVOID Parameter
    )
{
    HANDLE threadHandle;
    THREAD_BASIC_INFORMATION basicInfo;

    if (threadHandle = PhCreateThread(0, RunAsCreateProcessThread, Parameter))
    {
        LARGE_INTEGER timeout;

        NtWaitForSingleObject(threadHandle, FALSE, PhTimeoutFromMilliseconds(&timeout, 20 * 1000));
        
        if (NT_SUCCESS(PhGetThreadBasicInformation(threadHandle, &basicInfo)))
        {
            if (basicInfo.ExitStatus != STATUS_SUCCESS)
            {
                // Show Error
                PhShowStatus(
                    PhMainWndHandle, 
                    L"Error creating process with TrustedInstaller privileges", 
                    basicInfo.ExitStatus, 
                    0
                    );
            }
        }

        NtClose(threadHandle);
    }

    return STATUS_SUCCESS;
}

INT_PTR CALLBACK RunAsTrustedInstallerDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PhRegisterDialog(hwndDlg);

            SendMessage(hwndDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hwndDlg, IDC_PROGRAM), TRUE);
        }
        break;
    case WM_DESTROY:
        {
            PhUnregisterDialog(hwndDlg);
        }
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDCANCEL:
                EndDialog(hwndDlg, IDCANCEL);
                break;
            case IDC_BROWSE:
                {
                    static PH_FILETYPE_FILTER filters[] =
                    {
                        { L"Programs (*.exe;)", L"*.exe;" },
                        { L"All files (*.*)", L"*.*" }
                    };
                    PVOID fileDialog;
                    PPH_STRING fileName;

                    fileDialog = PhCreateOpenFileDialog();
                    PhSetFileDialogFilter(fileDialog, filters, sizeof(filters) / sizeof(PH_FILETYPE_FILTER));

                    if (PhShowFileDialog(hwndDlg, fileDialog))
                    {
                        fileName = PH_AUTO(PhGetFileDialogFileName(fileDialog));
                        SetDlgItemText(hwndDlg, IDC_PROGRAM, fileName->Buffer);
                    }

                    PhFreeFileDialog(fileDialog);
                }
                break;
            case IDOK:
                {
                    PPH_STRING program;
                    HANDLE threadHandle;

                    program = PhGetWindowText(GetDlgItem(hwndDlg, IDC_PROGRAM));

                    if (PhIsNullOrEmptyString(program))
                    {
                        PhDereferenceObject(program);
                        break;
                    }

                    if (threadHandle = PhCreateThread(0, RunAsTrustedInstallerThread, program))
                    {
                        NtClose(threadHandle);
                    }

                    EndDialog(hwndDlg, IDOK);
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}

VOID ShowRunAsDialog(
    _In_opt_ HWND Parent
    )
{
    DialogBox(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_RUNASDIALOG),
        Parent,
        RunAsTrustedInstallerDlgProc
        );
}