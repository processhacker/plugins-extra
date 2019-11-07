/*
 * Process Hacker Extra Plugins -
 *   Live Dump Plugin
 *
 * Copyright (C) 2017 dmex
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

NTSTATUS CreateLiveKernelDump(
    _In_ PLIVE_DUMP_CONFIG LiveDumpConfig
    )
{
    NTSTATUS status;
    HANDLE fileHandle = NULL;
    SYSDBG_LIVEDUMP_CONTROL liveDumpControl;
    SYSDBG_LIVEDUMP_CONTROL_FLAGS flags;
    SYSDBG_LIVEDUMP_CONTROL_ADDPAGES pages;

    status = PhCreateFileWin32(
        &fileHandle,
        PhGetString(LiveDumpConfig->FileName),
        FILE_READ_ATTRIBUTES | GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
        FILE_ATTRIBUTE_NORMAL,
        0,
        FILE_OVERWRITE_IF,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_NO_INTERMEDIATE_BUFFERING | FILE_WRITE_THROUGH
        );

    if (!NT_SUCCESS(status))
    {
        PhShowStatus(LiveDumpConfig->WindowHandle, L"Unable to create the dump file", status, 0);
        goto CleanupExit;
    }

    memset(&liveDumpControl, 0, sizeof(SYSDBG_LIVEDUMP_CONTROL));
    memset(&flags, 0, sizeof(SYSDBG_LIVEDUMP_CONTROL_FLAGS));
    memset(&pages, 0, sizeof(SYSDBG_LIVEDUMP_CONTROL_ADDPAGES));

    if (LiveDumpConfig->UseDumpStorageStack)
        flags.UseDumpStorageStack = TRUE;
    if (LiveDumpConfig->CompressMemoryPages)
        flags.CompressMemoryPagesData = TRUE;
    if (LiveDumpConfig->IncludeUserSpaceMemory)
        flags.IncludeUserSpaceMemoryPages = TRUE;
    if (LiveDumpConfig->IncludeHypervisorPages)
        pages.HypervisorPages = TRUE;

    liveDumpControl.Version = SYSDBG_LIVEDUMP_CONTROL_VERSION;
    liveDumpControl.DumpFileHandle = fileHandle;
    liveDumpControl.AddPagesControl = pages;
    liveDumpControl.Flags = flags;

    status = NtSystemDebugControl(
        SysDbgGetLiveKernelDump,
        &liveDumpControl,
        sizeof(SYSDBG_LIVEDUMP_CONTROL),
        NULL,
        0,
        NULL
        );

    if (!NT_SUCCESS(status))
    {
        PhShowStatus(LiveDumpConfig->WindowHandle, L"Unable to generate a live kernel dump", status, 0);
        goto CleanupExit;
    }

CleanupExit:

    if (fileHandle)
    {
        NtClose(fileHandle);
    }

    LiveDumpConfig->KernelDumpActive = FALSE;
    SendMessage(LiveDumpConfig->WindowHandle, TDM_CLICK_BUTTON, IDCANCEL, 0);

    return status;
}

HRESULT CALLBACK DumpProgressDialogCallbackProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam,
    _In_ LONG_PTR dwRefData
    )
{
    PLIVE_DUMP_CONFIG context = (PLIVE_DUMP_CONFIG)dwRefData;

    switch (uMsg)
    {
    case TDN_CREATED:
        {
            SendMessage(hwndDlg, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
            SendMessage(hwndDlg, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 1);

            context->WindowHandle = hwndDlg;
            context->KernelDumpActive = TRUE;

            PhCreateThread2(CreateLiveKernelDump, context);
        }
        break;
    case TDN_BUTTON_CLICKED:
        {
            if (context->KernelDumpActive)
                return S_FALSE;
        }
        break;
    }

    return S_OK;
}

NTSTATUS ShowDumpProgressDialogThread(
    _In_ PVOID ThreadParameter
    )
{
    TASKDIALOGCONFIG config;
    PLIVE_DUMP_CONFIG context = (PLIVE_DUMP_CONFIG)ThreadParameter;

    memset(&config, 0, sizeof(TASKDIALOGCONFIG));
    config.cbSize = sizeof(TASKDIALOGCONFIG);
    config.dwFlags = TDF_USE_HICON_MAIN | TDF_SHOW_MARQUEE_PROGRESS_BAR;
    config.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    config.hMainIcon = PH_LOAD_SHARED_ICON_LARGE(PhInstanceHandle, MAKEINTRESOURCE(PHAPP_IDI_PROCESSHACKER));
    config.pfCallback = DumpProgressDialogCallbackProc;
    config.lpCallbackData = (LONG_PTR)ThreadParameter;
    config.cxWidth = 200;

    config.pszWindowTitle = L"Process Hacker";
    config.pszMainInstruction = L"Creating live kernel dump...";
    config.pszContent = L"This might take some time to complete...";

    TaskDialogIndirect(&config, NULL, NULL, NULL);

    PhDereferenceObject(context->FileName);
    PhFree(context);

    return STATUS_SUCCESS;
}

INT_PTR CALLBACK LiveDumpConfigDlgProc(
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
            PhCenterWindow(hwndDlg, GetParent(hwndDlg));

            EnableThemeDialogTexture(hwndDlg, ETDT_ENABLETAB);

            SendMessage(hwndDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hwndDlg, IDCANCEL), TRUE);
        }
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDCANCEL:
                EndDialog(hwndDlg, IDCANCEL);
                break;
            case IDOK:
                {
                    static PH_FILETYPE_FILTER filters[] =
                    {
                        { L"Windows Memory Dump (*.dmp)", L"*.dmp" },
                        { L"All files (*.*)", L"*.*" }
                    };
                    PVOID fileDialog;
                    PLIVE_DUMP_CONFIG dumpConfig;

                    dumpConfig = PhAllocate(sizeof(LIVE_DUMP_CONFIG));
                    memset(dumpConfig, 0, sizeof(LIVE_DUMP_CONFIG));

                    dumpConfig->CompressMemoryPages = Button_GetCheck(GetDlgItem(hwndDlg, IDC_COMPRESS)) == BST_CHECKED;
                    dumpConfig->IncludeUserSpaceMemory = Button_GetCheck(GetDlgItem(hwndDlg, IDC_USERMODE)) == BST_CHECKED;
                    dumpConfig->IncludeHypervisorPages = Button_GetCheck(GetDlgItem(hwndDlg, IDC_HYPERVISOR)) == BST_CHECKED;
                    dumpConfig->UseDumpStorageStack = Button_GetCheck(GetDlgItem(hwndDlg, IDC_DUMPSTACK)) == BST_CHECKED;

                    if (dumpConfig->IncludeUserSpaceMemory && !USER_SHARED_DATA->KdDebuggerEnabled)
                    {
                        PhShowError2(hwndDlg, L"Live kernel dump configuration error", 
                            L"The UserSpace memory option requires Kernel Debugging enabled.\r\n\r\nUncheck the 'Include UserSpace memory' option or run 'bcdedit /debug on' and reboot.");
                        PhFree(dumpConfig);
                        break;
                    }

                    if (fileDialog = PhCreateSaveFileDialog())
                    {
                        PhSetFileDialogFilter(fileDialog, filters, ARRAYSIZE(filters));
                        PhSetFileDialogFileName(fileDialog, L"kerneldump.dmp");

                        if (PhShowFileDialog(hwndDlg, fileDialog))
                        {
                            dumpConfig->FileName = PhGetFileDialogFileName(fileDialog);
                        }

                        PhFreeFileDialog(fileDialog);
                    }

                    if (!PhIsNullOrEmptyString(dumpConfig->FileName))
                    {
                        PhCreateThread2(ShowDumpProgressDialogThread, dumpConfig);

                        EndDialog(hwndDlg, IDOK);
                    }
                    else
                    {
                        PhFree(dumpConfig);
                    }
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}

VOID ShowLiveDumpConfigDialog(
    _In_opt_ HWND Parent
    )
{
    DialogBox(PluginInstance->DllBase, MAKEINTRESOURCE(IDD_LIVEDUMP), Parent, LiveDumpConfigDlgProc);
}
