/*
 * Process Hacker Extra Plugins -
 *   VectorHandlerListPlugin
 *
 * Copyright (C) 2021 dmex
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

#include <phdk.h>
#include <phappresource.h>
#include <settings.h>
#include <symprv.h>
#include "resource.h"

#define VECTORHANDLER_TABLE_MENUITEM 1000
#define PLUGIN_NAME L"dmex.VectorHandlerListPlugin"
#define SETTING_NAME_WINDOW_POSITION (PLUGIN_NAME L".WindowPosition")
#define SETTING_NAME_WINDOW_SIZE (PLUGIN_NAME L".WindowSize")
#define SETTING_NAME_LISTVIEW_COLUMNS (PLUGIN_NAME L".ListViewColumns")
#define ENUM_VECTOR_ENTRY_LIMIT 0x40

PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
PH_CALLBACK_REGISTRATION ProcessMenuInitializingCallbackRegistration;
PPH_PLUGIN PluginInstance;

typedef struct _VECTORED_WINDOW_CONTEXT
{
    HWND ListViewHandle;
    PPH_PROCESS_ITEM ProcessItem;
    PH_LAYOUT_MANAGER LayoutManager;
    ULONG Count;
} VECTORED_WINDOW_CONTEXT, *PVECTORED_WINDOW_CONTEXT;

typedef VOID (NTAPI *VECTOR_CALLBACK_FUNCTION)(
    _In_ PVOID AddressOfEntry,
    _In_ PVOID AddressOfCallback,
    _In_ BOOLEAN ContinueHandler,
    _In_opt_ PVOID Context
    );

typedef struct _RTL_VECTORED_HANDLER_LIST
{
    SRWLOCK ExceptionLock;
    LIST_ENTRY ExceptionList;
    SRWLOCK ContinueLock;
    LIST_ENTRY ContinueList;
} RTL_VECTORED_HANDLER_LIST, *PRTL_VECTORED_HANDLER_LIST;

typedef struct _RTL_VECTORED_EXCEPTION_ENTRY
{
    LIST_ENTRY List;
    PULONG_PTR Flag;
    ULONG RefCount;
    PVECTORED_EXCEPTION_HANDLER VectoredHandler;
} RTL_VECTORED_EXCEPTION_ENTRY, *PRTL_VECTORED_EXCEPTION_ENTRY;

static NTSTATUS (NTAPI* RtlDecodeRemotePointer_I)(
    _In_ HANDLE ProcessHandle,
    _In_ PVOID Pointer,
    _Out_ PVOID* DecodedPointer
    ) = NULL;

NTSTATUS EnumerateVectorHandlerList(
    _In_ VECTOR_CALLBACK_FUNCTION Callback,
    _In_ PVECTORED_WINDOW_CONTEXT Context
    )
{
    NTSTATUS status;
    ULONG i = 0;
    HANDLE processHandle;
    PLIST_ENTRY startLink;
    PLIST_ENTRY currentLink;
    RTL_VECTORED_HANDLER_LIST ldrVectorHandlerList;
    RTL_VECTORED_EXCEPTION_ENTRY currentEntry;
    PPH_SYMBOL_PROVIDER symbolProvider;
    PH_SYMBOL_INFORMATION symbolInfo;
    PVOID ldrpVectorHandlerList = NULL;

    if (symbolProvider = PhCreateSymbolProvider(NULL))
    {
        static PH_STRINGREF dllname = PH_STRINGREF_INIT(L"ntdll.dll");
        PLDR_DATA_TABLE_ENTRY entry;

        PhLoadSymbolProviderOptions(symbolProvider);

        if (entry = PhFindLoaderEntry(NULL, NULL, &dllname))
        {
            PPH_STRING fileName;

            if (NT_SUCCESS(PhGetProcessMappedFileName(NtCurrentProcess(), entry->DllBase, &fileName)))
            {
                if (PhLoadModuleSymbolProvider(
                    symbolProvider,
                    fileName,
                    (ULONG64)entry->DllBase,
                    entry->SizeOfImage
                    ))
                {
                    if (PhGetSymbolFromName(symbolProvider, L"LdrpVectorHandlerList", &symbolInfo))
                    {
                        //if (symbolInfo.Size == sizeof(RTL_VECTORED_HANDLER_LIST))
                        ldrpVectorHandlerList = (PVOID)symbolInfo.Address;
                    }
                }

                PhDereferenceObject(fileName);
            }
        }

        PhDereferenceObject(symbolProvider);
    }

    if (!ldrpVectorHandlerList)
    {
        return STATUS_UNSUCCESSFUL;
    }

    status = PhOpenProcess(
        &processHandle,
        MAXIMUM_ALLOWED,
        Context->ProcessItem->ProcessId
        );

    if (!NT_SUCCESS(status))
        return status;

    status = NtReadVirtualMemory(
        processHandle,
        ldrpVectorHandlerList,
        &ldrVectorHandlerList,
        sizeof(RTL_VECTORED_HANDLER_LIST),
        NULL
        );

    if (NT_SUCCESS(status))
    {
        startLink = PTR_ADD_OFFSET(ldrpVectorHandlerList, UFIELD_OFFSET(RTL_VECTORED_HANDLER_LIST, ExceptionList));
        currentLink = ldrVectorHandlerList.ExceptionList.Flink;

        while (currentLink != startLink && i <= ENUM_VECTOR_ENTRY_LIMIT)
        {
            PVOID addressOfEntry;
            PVOID addressOfCallback;

            addressOfEntry = CONTAINING_RECORD(currentLink, RTL_VECTORED_EXCEPTION_ENTRY, List);
            status = NtReadVirtualMemory(
                processHandle,
                addressOfEntry,
                &currentEntry,
                sizeof(RTL_VECTORED_EXCEPTION_ENTRY),
                NULL
                );

            if (!NT_SUCCESS(status))
                break;

            if (NT_SUCCESS(RtlDecodeRemotePointer_I(
                processHandle,
                (PVOID)currentEntry.VectoredHandler,
                &addressOfCallback
                )))
            {
                Callback(addressOfEntry, addressOfCallback, FALSE, Context);
            }

            currentLink = currentEntry.List.Flink;
            i++;
        }

        startLink = PTR_ADD_OFFSET(ldrpVectorHandlerList, UFIELD_OFFSET(RTL_VECTORED_HANDLER_LIST, ContinueList));
        currentLink = ldrVectorHandlerList.ContinueList.Flink;

        while (currentLink != startLink && i <= ENUM_VECTOR_ENTRY_LIMIT)
        {
            PVOID addressOfEntry;
            PVOID addressOfCallback;

            addressOfEntry = CONTAINING_RECORD(currentLink, RTL_VECTORED_EXCEPTION_ENTRY, List);
            status = NtReadVirtualMemory(
                processHandle,
                addressOfEntry,
                &currentEntry,
                sizeof(RTL_VECTORED_EXCEPTION_ENTRY),
                NULL
                );

            if (!NT_SUCCESS(status))
                break;

            if (NT_SUCCESS(RtlDecodeRemotePointer_I(
                processHandle,
                (PVOID)currentEntry.VectoredHandler,
                &addressOfCallback
                )))
            {
                Callback(addressOfEntry, addressOfCallback, TRUE, Context);
            }

            currentLink = currentEntry.List.Flink;
            i++;
        }
    }

    return status;
}

VOID NTAPI VectorEnumerateCallback(
    _In_ PVOID AddressOfEntry,
    _In_ PVOID AddressOfCallback,
    _In_ BOOLEAN ContinueHandler,
    _In_ PVECTORED_WINDOW_CONTEXT Context
    )
{
    PPH_STRING fileName;
    INT index;

    Context->Count++;

    index = PhAddListViewItem(
        Context->ListViewHandle,
        MAXINT,
        PhaFormatString(L"%lu", Context->Count)->Buffer,
        NULL
        );

    PhSetListViewSubItem(
        Context->ListViewHandle,
        index,
        1,
        ContinueHandler ? L"ContinueHandler" : L"ExceptionHandler"
        );

    PhSetListViewSubItem(
        Context->ListViewHandle,
        index,
        2,
        PhaFormatString(L"0x%p", AddressOfEntry)->Buffer
        );

    PhSetListViewSubItem(
        Context->ListViewHandle,
        index,
        3,
        PhaFormatString(L"0x%p", AddressOfCallback)->Buffer
        );

    if (NT_SUCCESS(PhGetProcessMappedFileName(
        Context->ProcessItem->QueryHandle,
        AddressOfCallback,
        &fileName
        )))
    {
        PhMoveReference(&fileName, PhGetBaseName(fileName));

        PhSetListViewSubItem(
            Context->ListViewHandle,
            index,
            4,
            PhGetString(fileName)
            );

        PhDereferenceObject(fileName);
    }
}

VOID EnumerateVectorHandlers(
    _In_ PVECTORED_WINDOW_CONTEXT Context
    )
{
    ExtendedListView_SetRedraw(Context->ListViewHandle, FALSE);
    ListView_DeleteAllItems(Context->ListViewHandle);

    EnumerateVectorHandlerList(VectorEnumerateCallback, Context);

    ExtendedListView_SetRedraw(Context->ListViewHandle, TRUE);
}

PPH_STRING PhGetSelectedListViewItemText(
    _In_ HWND hWnd
    )
{
    INT index = PhFindListViewItemByFlags(
        hWnd,
        -1,
        LVNI_SELECTED
        );

    if (index != -1)
    {
        WCHAR buffer[DOS_MAX_PATH_LENGTH] = L"";

        LVITEM item;
        item.mask = LVIF_TEXT;
        item.iItem = index;
        item.iSubItem = 0;
        item.pszText = buffer;
        item.cchTextMax = ARRAYSIZE(buffer);

        if (ListView_GetItem(hWnd, &item))
            return PhCreateString(buffer);
    }

    return NULL;
}

INT_PTR CALLBACK MainWindowDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PVECTORED_WINDOW_CONTEXT context;

    if (uMsg == WM_INITDIALOG)
    {
        context = PhAllocateZero(sizeof(VECTORED_WINDOW_CONTEXT));
        context->ProcessItem = (PPH_PROCESS_ITEM)lParam;

        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
    }

    if (!context)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PhSetApplicationWindowIcon(hwndDlg);

            PhCenterWindow(hwndDlg, PhMainWndHandle);

            context->ListViewHandle = GetDlgItem(hwndDlg, IDC_LIST);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, context->ListViewHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDRETRY), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDOK), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
            PhLoadWindowPlacementFromSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);

            PhSetListViewStyle(context->ListViewHandle, TRUE, TRUE);
            PhSetControlTheme(context->ListViewHandle, L"explorer");
            PhAddListViewColumn(context->ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 50, L"#");
            PhAddListViewColumn(context->ListViewHandle, 1, 1, 1, LVCFMT_LEFT, 100, L"Type");
            PhAddListViewColumn(context->ListViewHandle, 2, 2, 2, LVCFMT_LEFT, 140, L"Address");
            PhAddListViewColumn(context->ListViewHandle, 3, 3, 3, LVCFMT_LEFT, 140, L"Handler");
            PhAddListViewColumn(context->ListViewHandle, 4, 4, 4, LVCFMT_LEFT, 140, L"Module");
            PhSetExtendedListView(context->ListViewHandle);
            //PhLoadListViewColumnsFromSetting(SETTING_NAME_LISTVIEW_COLUMNS, context->ListViewHandle);
   
            if (!RtlDecodeRemotePointer_I)
            {
                RtlDecodeRemotePointer_I = PhGetModuleProcAddress(L"ntdll.dll", "RtlDecodeRemotePointer");
            }

            if (RtlDecodeRemotePointer_I)
            {
                EnumerateVectorHandlers(context);
            }

            PhInitializeWindowTheme(hwndDlg, !!PhGetIntegerSetting(L"EnableThemeSupport"));
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&context->LayoutManager);
        break;
    case WM_DESTROY:
        {
            PhSaveWindowPlacementToSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            PhSaveListViewColumnsToSetting(SETTING_NAME_LISTVIEW_COLUMNS, context->ListViewHandle);
            PhDeleteLayoutManager(&context->LayoutManager);

            PhDereferenceObject(context->ProcessItem);
        }
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDCANCEL:
            case IDOK:
                EndDialog(hwndDlg, IDOK);
                break;
            case IDRETRY:
                //EnumerateVectorHandlers();
                break;
            }
        }
        break;
    case WM_CTLCOLORBTN:
        return HANDLE_WM_CTLCOLORBTN(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORDLG:
        return HANDLE_WM_CTLCOLORDLG(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORSTATIC:
        return HANDLE_WM_CTLCOLORSTATIC(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    }

    return FALSE;
}

VOID NTAPI ProcessMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;
    PPH_EMENU_ITEM miscMenuItem;
    PPH_EMENU_ITEM menuItem;
    PPH_PROCESS_ITEM processItem;

    if (!menuInfo)
        return;

    miscMenuItem = PhFindEMenuItem(menuInfo->Menu, 0, L"Miscellaneous", 0);

    if (!miscMenuItem)
        return;

    processItem = menuInfo->u.Process.NumberOfProcesses == 1 ? menuInfo->u.Process.Processes[0] : NULL;
    menuItem = PhPluginCreateEMenuItem(PluginInstance, 0, VECTORHANDLER_TABLE_MENUITEM, L"VectorHandlerList", processItem);
    PhInsertEMenuItem(miscMenuItem, menuItem, ULONG_MAX);
}

VOID NTAPI MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = (PPH_PLUGIN_MENU_ITEM)Parameter;

    if (!menuItem)
        return;

    switch (menuItem->Id)
    {
    case VECTORHANDLER_TABLE_MENUITEM:
        {
            PhReferenceObject(menuItem->Context);

            DialogBoxParam(
                PluginInstance->DllBase,
                MAKEINTRESOURCE(IDD_VECTORDIALOG),
                NULL,
                MainWindowDlgProc,
                (LPARAM)menuItem->Context
                );
        }
        break;
    }
}

LOGICAL DllMain(
    _In_ HINSTANCE Instance,
    _In_ ULONG Reason,
    _Reserved_ PVOID Reserved
    )
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        {
            PPH_PLUGIN_INFORMATION info;
            PH_SETTING_CREATE settings[] =
            {
                { IntegerPairSettingType, SETTING_NAME_WINDOW_POSITION, L"350,350" },
                { ScalableIntegerPairSettingType, SETTING_NAME_WINDOW_SIZE, L"@96|510,380" },
                { StringSettingType, SETTING_NAME_LISTVIEW_COLUMNS, L"" }
            };

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->Author = L"dmex";
            info->DisplayName = L"VectorHandlerListPlugin Plugin";
            info->Description = L"Plugin for viewing the VectorHandlerList via the process menu.";
            info->HasOptions = FALSE;

            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackProcessMenuInitializing),
                ProcessMenuInitializingCallback,
                NULL,
                &ProcessMenuInitializingCallbackRegistration
                );
            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackMenuItem),
                MenuItemCallback,
                NULL,
                &PluginMenuItemCallbackRegistration
                );

            PhAddSettings(settings, ARRAYSIZE(settings));
        }
        break;
    }

    return TRUE;
}
