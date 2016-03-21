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

static HANDLE EnvDialogThreadHandle = NULL;
static HWND EnvDialogHandle = NULL;
static PH_EVENT InitializedEvent = PH_EVENT_INIT;

VOID DestroyItems(
    _Inout_ PENVIRONMENT_DIALOG_CONTEXT context
    )
{
    ULONG index = -1;
    PVOID param;

    while ((index = PhFindListViewItemByFlags(
        context->ListViewHandle,
        index,
        LVNI_ALL
        )) != -1)
    {
        if (PhGetListViewItemParam(context->ListViewHandle, index, &param))
        {
            PENVIRONMENT_ENTRY entry = param;

            PhClearReference(&entry->Name);
            PhClearReference(&entry->Value);

            PhFree(entry);
        }
    }

    ListView_DeleteAllItems(context->ListViewHandle);
}

static VOID EnumerateProcessEnvironment(
    _Inout_ PENVIRONMENT_DIALOG_CONTEXT context
    )
{
    PVOID environment;
    ULONG environmentLength;
    ULONG enumerationKey;
    PH_ENVIRONMENT_VARIABLE variable;
    ULONG flags = 0;

    ExtendedListView_SetRedraw(context->ListViewHandle, FALSE);

    DestroyItems(context);

#ifdef _WIN64
    BOOLEAN isWow64;

    PhGetProcessIsWow64(context->ProcessHandle, &isWow64);

    if (isWow64)
        flags |= PH_GET_PROCESS_ENVIRONMENT_WOW64;
#endif

    if (NT_SUCCESS(PhGetProcessEnvironment(
        context->ProcessHandle,
        flags,
        &environment,
        &environmentLength
        )))
    {
        enumerationKey = 0;

        while (PhEnumProcessEnvironmentVariables(environment, environmentLength, &enumerationKey, &variable))
        {
            INT lvItemIndex;
            PENVIRONMENT_ENTRY entry;

            // Don't display pairs with no name.
            if (variable.Name.Length == 0)
                continue;
           
            entry = PhAllocate(sizeof(ENVIRONMENT_ENTRY));
            memset(entry, 0, sizeof(ENVIRONMENT_ENTRY));

            // The strings are not guaranteed to be null-terminated, so we need to create the strings.
            entry->Name = PhCreateString2(&variable.Name);
            entry->Value = PhCreateString2(&variable.Value);

            lvItemIndex = PhAddListViewItem(context->ListViewHandle, MAXINT, entry->Name->Buffer, entry);
            PhSetListViewSubItem(context->ListViewHandle, lvItemIndex, 1, entry->Value->Buffer);
        }

        PhFreePage(environment);
    }

    ExtendedListView_SetRedraw(context->ListViewHandle, TRUE);
}

static VOID ShowListViewMenu(
    _Inout_ PENVIRONMENT_DIALOG_CONTEXT Context
    )
{
    PENVIRONMENT_ENTRY entry;
    
    entry = PhGetSelectedListViewItemParam(Context->ListViewHandle);

    if (entry)
    {
        PPH_EMENU menu;
        PPH_EMENU_ITEM menuItem;
        POINT cursorPos;

        GetCursorPos(&cursorPos);

        menu = PhCreateEMenu();
        PhLoadResourceEMenuItem(menu, PluginInstance->DllBase, MAKEINTRESOURCE(IDR_MAIN_MENU), 0);

        menuItem = PhShowEMenu(
            menu,
            Context->DialogHandle,
            PH_EMENU_SHOW_LEFTRIGHT, // PH_EMENU_SHOW_SEND_COMMAND
            PH_ALIGN_LEFT | PH_ALIGN_TOP,
            cursorPos.x,
            cursorPos.y
            );

        if (menuItem)
        {
            switch (menuItem->Id)
            {
            case ID_EDIT:
                {
                    ShowAddRemoveDialog(
                        Context->DialogHandle,
                        Context->ProcessHandle,
                        entry->Name,
                        entry->Value
                        );

                    EnumerateProcessEnvironment(Context);
                }
                break;
            case ID_DELETE:
                {
                    NTSTATUS status;

                    if (PhShowConfirmMessage(
                        Context->DialogHandle,
                        L"delete",
                        PhaFormatString(L"the '%s' variable", entry->Name->Buffer)->Buffer,
                        NULL,
                        FALSE
                        ))
                    {
                        if (!NT_SUCCESS(status = SetRemoteEnvironmentVariable(
                            Context->ProcessHandle,
                            entry->Name->Buffer,
                            NULL
                            )))
                        {
                            PhShowStatus(Context->DialogHandle, NULL, status, 0);
                        }

                        EnumerateProcessEnvironment(Context);
                    }
                }
                break;
            case ID_COPY:
                {
                    PPH_STRING text;

                    text = PhFormatString(L"%s\r\n%s", entry->Name->Buffer, entry->Value->Buffer);
                    PhSetClipboardString(Context->DialogHandle, &text->sr);
                    PhDereferenceObject(text);
                }
                break;
            }
        }

        PhDestroyEMenu(menu);
    }
}

static INT_PTR CALLBACK DbgViewDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PENVIRONMENT_DIALOG_CONTEXT context;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PENVIRONMENT_DIALOG_CONTEXT)lParam;
        SetProp(hwndDlg, L"Context", (HANDLE)context);
    }
    else
    {
        context = (PENVIRONMENT_DIALOG_CONTEXT)GetProp(hwndDlg, L"Context");

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
            if (context->ProcessItem->ProcessName)
            {
                SetWindowText(hwndDlg, PhaFormatString(
                    L"%s [%lu]- Environment Variables",
                    context->ProcessItem->ProcessName->Buffer,
                    HandleToUlong(context->ProcessItem->ProcessId)
                    )->Buffer);
            }

            PhCenterWindow(hwndDlg, PhMainWndHandle);

            context->DialogHandle = hwndDlg;
            context->ListViewHandle = GetDlgItem(hwndDlg, IDC_DEBUGLISTVIEW);
           
            PhRegisterDialog(hwndDlg);
            PhSetListViewStyle(context->ListViewHandle, FALSE, TRUE);
            PhSetControlTheme(context->ListViewHandle, L"explorer");
            PhAddListViewColumn(context->ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 150, L"Name");
            PhAddListViewColumn(context->ListViewHandle, 1, 1, 1, LVCFMT_LEFT, 320, L"Value");
            PhSetExtendedListView(context->ListViewHandle);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, context->ListViewHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_ADD), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_EDIT), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_REMOVE), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDCLOSE), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);

            if (PhGetIntegerSetting(SETTING_NAME_FIRST_RUN) == 0)
            {
                PhLoadWindowPlacementFromSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
                PhLoadListViewColumnsFromSetting(SETTING_NAME_COLUMNS, context->ListViewHandle); 
            }
            PhSetIntegerSetting(SETTING_NAME_FIRST_RUN, 0);

            EnumerateProcessEnvironment(context);
        }
        return TRUE;
    case WM_DESTROY:
        {
            DestroyItems(context);

            if (context->ProcessHandle)
                NtClose(context->ProcessHandle);

            PhDereferenceObject(context->ProcessItem);

            PhSaveListViewColumnsToSetting(SETTING_NAME_COLUMNS, context->ListViewHandle);
            PhSaveWindowPlacementToSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);

            PhDeleteLayoutManager(&context->LayoutManager);

            PhUnregisterDialog(hwndDlg);

            PostQuitMessage(0);
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&context->LayoutManager);
        break;
    case WM_SHOWDIALOG:
        {
            if (IsIconic(hwndDlg))
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
                DestroyWindow(hwndDlg);
                break;
            case IDC_ADD:
                {
                    ShowAddRemoveDialog(
                        hwndDlg, 
                        context->ProcessHandle, 
                        NULL, 
                        NULL
                        );

                    EnumerateProcessEnvironment(context);
                }
                break;
            case IDC_EDIT:
                {
                    PENVIRONMENT_ENTRY entry;

                    if (entry = PhGetSelectedListViewItemParam(context->ListViewHandle))
                    {
                        ShowAddRemoveDialog(
                            hwndDlg,
                            context->ProcessHandle, 
                            entry->Name, 
                            entry->Value
                            );

                        EnumerateProcessEnvironment(context);
                    }
                }
                break;
            case IDC_REMOVE:
                {
                    PENVIRONMENT_ENTRY entry;

                    if (entry = PhGetSelectedListViewItemParam(context->ListViewHandle))
                    {
                        NTSTATUS status;

                        if (PhShowConfirmMessage(
                            context->DialogHandle,
                            L"delete",
                            PhaFormatString(L"the '%s' variable", entry->Name->Buffer)->Buffer,
                            NULL,
                            FALSE
                            ))
                        {
                            if (!NT_SUCCESS(status = SetRemoteEnvironmentVariable(
                                context->ProcessHandle,
                                entry->Name->Buffer,
                                NULL
                                )))
                            {
                                PhShowStatus(context->DialogHandle, NULL, status, 0);
                            }

                            EnumerateProcessEnvironment(context);
                        }
                    }
                }
                break;
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR hdr = (LPNMHDR)lParam;

            switch (hdr->code)
            {
            case NM_RCLICK:
                {
                    if (hdr->hwndFrom == context->ListViewHandle)
                    {
                        ShowListViewMenu(context);
                    }
                }
                break;
            case NM_DBLCLK:
                {
                    PENVIRONMENT_ENTRY entry;

                    if (entry = PhGetSelectedListViewItemParam(context->ListViewHandle))
                    {
                        ShowAddRemoveDialog(
                            hwndDlg,
                            context->ProcessHandle,
                            entry->Name,
                            entry->Value
                            );

                        EnumerateProcessEnvironment(context);
                    }
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}

static NTSTATUS EnvViewDialogThread(
    _In_ PVOID Parameter
    )
{
    BOOL result;
    MSG message;
    PH_AUTO_POOL autoPool;

    PhInitializeAutoPool(&autoPool);

    EnvDialogHandle = CreateDialogParam(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_ENVIRONMENT_DIALOG),
        NULL,
        DbgViewDlgProc,
        (LPARAM)Parameter
        );

    PhSetEvent(&InitializedEvent);

    while (result = GetMessage(&message, NULL, 0, 0))
    {
        if (result == -1)
            break;

        if (!IsDialogMessage(EnvDialogHandle, &message))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        PhDrainAutoPool(&autoPool);
    }

    PhDeleteAutoPool(&autoPool);
    PhResetEvent(&InitializedEvent);

    if (EnvDialogThreadHandle)
    {
        NtClose(EnvDialogThreadHandle);
        EnvDialogThreadHandle = NULL;
    }

    return STATUS_SUCCESS;
}

VOID ShowEnvironmentDialog(
    _In_ PENVIRONMENT_DIALOG_CONTEXT Context
    )
{
    if (!EnvDialogThreadHandle)
    {
        if (!(EnvDialogThreadHandle = PhCreateThread(0, EnvViewDialogThread, Context)))
        {
            PhShowStatus(PhMainWndHandle, L"Unable to create the window.", 0, GetLastError());
            return;
        }

        PhWaitForEvent(&InitializedEvent, NULL);
    }

    PostMessage(EnvDialogHandle, WM_SHOWDIALOG, 0, 0);
}