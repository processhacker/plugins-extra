/*
 * Process Hacker Extra Plugins -
 *   Terminator Plugin
 *
 * Copyright (C) 2009-2016 wj32
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
#include "kph2user.h"

BOOLEAN ShowKphWarning(
    _In_ HWND ParentWindowHandle
    )
{
    return PhShowMessage(
        ParentWindowHandle,
        MB_ICONWARNING | MB_YESNO,
        L"WARNING WARNING WARNING\r\nYou can be banned by anti-cheat software for using this plugin. "
        L"Are you absolutely sure you want to continue?"
        ) == IDYES;
}

VOID ShowDebugWarning(
    _In_ HWND ParentWindowHandle,
    _In_ PPH_PROCESS_ITEM ProcessItem
    )
{
    NTSTATUS status;
    HANDLE processHandle;
    HANDLE debugObjectHandle;

    if (NT_SUCCESS(PhOpenProcess(
        &processHandle,
        PROCESS_QUERY_INFORMATION | PROCESS_SUSPEND_RESUME,
        ProcessItem->ProcessId
        )))
    {
        if (NT_SUCCESS(PhGetProcessDebugObject(
            processHandle,
            &debugObjectHandle
            )))
        {
            if (PhShowMessage(
                ParentWindowHandle,
                MB_ICONWARNING | MB_YESNO,
                L"The selected process is currently being debugged, which can prevent it from being terminated. "
                L"Do you want to detach the process from its debugger?"
                ) == IDYES)
            {
                ULONG killProcessOnExit;

                // Disable kill-on-close.
                killProcessOnExit = 0;

                NtSetInformationDebugObject(
                    debugObjectHandle,
                    DebugObjectKillProcessOnExitInformation,
                    &killProcessOnExit,
                    sizeof(ULONG),
                    NULL
                    );

                if (!NT_SUCCESS(status = NtRemoveProcessDebug(processHandle, debugObjectHandle)))
                    PhShowStatus(ParentWindowHandle, L"Unable to detach the process", status, 0);
            }

            NtClose(debugObjectHandle);
        }

        NtClose(processHandle);
    }
}

VOID InitializeKph2(
    VOID
    )
{
    NTSTATUS status;
    KPH_PARAMETERS parameters;

    if (Kph2IsConnected())
        return;

    parameters.SecurityLevel = KphSecurityPrivilegeCheck;
    parameters.CreateDynamicConfiguration = TRUE;

    if (!NT_SUCCESS(status = Kph2Install(&parameters)))
    {
        PhShowStatus(NULL, L"Unable to install KProcessHacker2", status, 0);
    }

    if (!NT_SUCCESS(status = Kph2Connect()))
    {
        PhShowStatus(NULL, L"Unable to connect KProcessHacker2", status, 0);
    }
}

VOID ShutdownAndDeleteKph2(
    VOID
    )
{
    NTSTATUS status;

    if (Kph2IsConnected() && !NT_SUCCESS(status = Kph2Disconnect()))
    {
        PhShowStatus(NULL, L"Unable to disconnect KProcessHacker2", status, 0);
    }

    if (!NT_SUCCESS(status = Kph2Uninstall()))
    {
        if (status != STATUS_OBJECT_NAME_NOT_FOUND)
        {
            PhShowStatus(NULL, L"Unable to uninstall KProcessHacker2", status, 0);
        }
    }
}

INT_PTR CALLBACK PhpProcessTerminatorDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PTERMINATOR_CONTEXT context = NULL;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PTERMINATOR_CONTEXT)PhAllocate(sizeof(TERMINATOR_CONTEXT));
        memset(context, 0, sizeof(TERMINATOR_CONTEXT));

        context->ProcessItem = (PPH_PROCESS_ITEM)lParam;

        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);

        if (uMsg == WM_DESTROY)
        {
            PhDeleteLayoutManager(&context->LayoutManager);
            PhRemoveWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);

            ShutdownAndDeleteKph2();
        }
    }

    if (context == NULL)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            HWND lvHandle;
            HIMAGELIST imageList;
                       
            PhSetWindowText(hwndDlg, PhaFormatString(
                L"Terminator - %s (%u)",
                context->ProcessItem->ProcessName->Buffer,
                HandleToUlong(context->ProcessItem->ProcessId)
                )->Buffer);

            PhCenterWindow(hwndDlg, GetParent(hwndDlg));
            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_TERMINATOR_LIST), NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_RUNSELECTED), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDOK), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);

            lvHandle = GetDlgItem(hwndDlg, IDC_TERMINATOR_LIST);
            PhAddListViewColumn(lvHandle, 0, 0, 0, LVCFMT_LEFT, 70, L"ID");
            PhAddListViewColumn(lvHandle, 1, 1, 1, LVCFMT_LEFT, 400, L"Description");
            ListView_SetExtendedListViewStyleEx(lvHandle, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES, -1);
            PhSetControlTheme(lvHandle, L"explorer");

            imageList = ImageList_Create(16, 16, ILC_COLOR32, 0, 0);
            ImageList_SetImageCount(imageList, 2);
            PhSetImageListBitmap(imageList, 0, (HINSTANCE)NtCurrentPeb()->ImageBaseAddress, MAKEINTRESOURCE(PHAPP_IDB_CROSS));
            PhSetImageListBitmap(imageList, 1, (HINSTANCE)NtCurrentPeb()->ImageBaseAddress, MAKEINTRESOURCE(PHAPP_IDB_TICK));

            for (ULONG i = 0; i < sizeof(PhTerminatorTests) / sizeof(TEST_ITEM); i++)
            {
                INT itemIndex;
                BOOLEAN check;

                itemIndex = PhAddListViewItem(
                    lvHandle,
                    MAXINT,
                    PhTerminatorTests[i].Id,
                    &PhTerminatorTests[i]
                    );
                PhSetListViewSubItem(lvHandle, itemIndex, 1, PhTerminatorTests[i].Description);
                PhSetListViewItemImageIndex(lvHandle, itemIndex, -1);

                check = TRUE;

                if (PhEqualStringZ(PhTerminatorTests[i].Id, L"M1", FALSE))
                    check = FALSE;

                ListView_SetCheckState(lvHandle, itemIndex, check);
            }

            ListView_SetImageList(lvHandle, imageList, LVSIL_SMALL);

            PhSetDialogItemText(
                hwndDlg,
                IDC_TERMINATOR_TEXT,
                L"Double-click a termination method or click Run Selected."
                );
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&context->LayoutManager);
        break;
    case WM_COMMAND:
        {
            INT id = LOWORD(wParam);

            switch (id)
            {
            case IDCANCEL: // Esc and X button to close
            case IDOK:
                EndDialog(hwndDlg, IDOK);
                break;
            case IDC_RUNSELECTED:
                {    
                    //if (!ShowKphWarning(ParentWindowHandle))
                    //    return;

                    InitializeKph2();

                    if (PhShowConfirmMessage(hwndDlg, L"run", L"the selected terminator tests", NULL, FALSE))
                    {
                        HWND lvHandle;
                        ULONG i;

                        lvHandle = GetDlgItem(hwndDlg, IDC_TERMINATOR_LIST);

                        for (i = 0; i < sizeof(PhTerminatorTests) / sizeof(TEST_ITEM); i++)
                        {
                            if (ListView_GetCheckState(lvHandle, i))
                            {
                                if (PhpRunTerminatorTest(hwndDlg, context->ProcessItem, i))
                                {
                                    break;
                                }
                            }
                        }
                    }
                }
                break;
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR header = (LPNMHDR)lParam;

            if (header->hwndFrom == GetDlgItem(hwndDlg, IDC_TERMINATOR_LIST))
            {
                if (header->code == NM_DBLCLK)
                {
                    LPNMITEMACTIVATE itemActivate = (LPNMITEMACTIVATE)header;

                    if (itemActivate->iItem != -1)
                    {
                        if (PhShowConfirmMessage(hwndDlg, L"run", L"the selected test", NULL, FALSE))
                        {
                            InitializeKph2();

                            PhpRunTerminatorTest(
                                hwndDlg,
                                context->ProcessItem,
                                itemActivate->iItem
                                );
                        }
                    }
                }
                else if (header->code == LVN_ITEMCHANGED)
                {
                    ULONG i;
                    BOOLEAN oneSelected;

                    oneSelected = FALSE;

                    for (i = 0; i < sizeof(PhTerminatorTests) / sizeof(TEST_ITEM); i++)
                    {
                        if (ListView_GetCheckState(header->hwndFrom, i))
                        {
                            oneSelected = TRUE;
                            break;
                        }
                    }

                    EnableWindow(GetDlgItem(hwndDlg, IDC_RUNSELECTED), oneSelected);
                }
            }
        }
        break;
    }

    return FALSE;
}

VOID PhShowProcessTerminatorDialog(
    _In_ HWND ParentWindowHandle,
    _In_ PPH_PROCESS_ITEM ProcessItem
    )
{
    ShowDebugWarning(ParentWindowHandle, ProcessItem);

    DialogBoxParam(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_TERMINATOR),
        NULL,
        PhpProcessTerminatorDlgProc,
        (LPARAM)ProcessItem
        );
}
