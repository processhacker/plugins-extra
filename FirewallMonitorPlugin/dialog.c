/*
 * Process Hacker Extra Plugins -
 *   Firewall Plugin
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

#include "fwmon.h"
#include "wf.h"

VOID ChangeNotificationThread()
{
    HANDLE eventWaitHandle = 0;
    HANDLE zero = 0;

    //eventWaitHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (NT_SUCCESS(NtCreateEvent(&eventWaitHandle, EVENT_ALL_ACCESS, NULL, SynchronizationEvent, FALSE)))
    {
        if (FWChangeNotificationCreate(eventWaitHandle, &zero) == ERROR_SUCCESS)
        {

        }

        if (zero)
        {
            FWChangeNotificationDestroy(zero);
        }

        NtClose(eventWaitHandle);
    }
}


BOOLEAN CALLBACK WfAddRules(
    _In_ PFW_RULE FwRule, 
    _In_ PVOID Context
    )
{
    PBOOT_WINDOW_CONTEXT context = (PBOOT_WINDOW_CONTEXT)Context;
    INT lvItemIndex;

    if (FwRule->wszLocalApplication)
    {
        HICON icon = PhGetFileShellIcon(FwRule->wszLocalApplication, L".exe", TRUE);

        if (icon)
        {
            INT imageIndex = ImageList_AddIcon(ListView_GetImageList(context->ListViewHandle, LVSIL_SMALL), icon);
            lvItemIndex = PhAddListViewItem(context->ListViewHandle, MAXINT, FwRule->wszName, (PVOID)FwRule);
            PhSetListViewItemImageIndex(context->ListViewHandle, lvItemIndex, imageIndex);
            DestroyIcon(icon);
        }
        else
        {
            lvItemIndex = PhAddListViewItem(context->ListViewHandle, MAXINT, FwRule->wszName, (PVOID)FwRule);
            PhSetListViewItemImageIndex(context->ListViewHandle, lvItemIndex, 0);
        }
    }

    if (FwRule->wszEmbeddedContext)
    {
        WCHAR localized[MAX_PATH] = L"";

        if (SUCCEEDED(SHLoadIndirectString(
            FwRule->wszEmbeddedContext,
            localized,
            MAX_PATH,
            NULL
            )))
        {
            ListView_SetItemText(context->ListViewHandle, lvItemIndex, 1, localized);
        }
        else
        {
            //if (SUCCEEDED(SHGetLocalizedName(
            //    FwRule->wszEmbeddedContext, 
            //    FwRule->wszEmbeddedContext, 
            //    MAX_PATH, 
            //    &id
            //    )))
            //{
            //ExpandEnvironmentStrings(FwRule->wszEmbeddedContext, module_expanded, MAX_PATH);
            //LoadString(GetModuleHandle(module_expanded), *id, localized, MAX_PATH);
            //}

            swprintf_s(
                localized,
                ARRAYSIZE(localized),
                L"%s",
                FwRule->wszEmbeddedContext
                );

            ListView_SetItemText(context->ListViewHandle, lvItemIndex, 1, localized);
        }
    }

    return TRUE;
}

NTSTATUS FWRulesEnumThreadStart(
    _In_ PVOID Parameter
    )
{
    PBOOT_WINDOW_CONTEXT context = (PBOOT_WINDOW_CONTEXT)Parameter;
        
    if (InitializeFirewallApi())
    {
        EnumerateFirewallRules(FW_POLICY_STORE_DYNAMIC, FW_PROFILE_TYPE_CURRENT, FW_DIR_IN, WfAddRules, context);
        FreeFirewallApi();
    } 

    return STATUS_SUCCESS;
}

NTSTATUS EnumerateEnvironmentValues(
    _In_ PVOID Parameter
    )
{
    //ExtendedListView_SetRedraw(ListViewHandle, FALSE);
    //ListView_DeleteAllItems(ListViewHandle);

    PhCreateThread(0, FWRulesEnumThreadStart, Parameter);

    //if (NT_SUCCESS(status = EnumerateFirmwareValues(&variables)))
    //  {
    //PVARIABLE_NAME_AND_VALUE i;

    // for (i = PH_FIRST_EFI_VARIABLE(variables); i; i = PH_NEXT_EFI_VARIABLE(i))
    //  {       
    /*   INT index;
    GUID vendorGuid;
    PPH_STRING guidString;

    vendorGuid = i->VendorGuid;
    guidString = PhFormatGuid(&vendorGuid);

    index = PhAddListViewItem(
    ListViewHandle,
    MAXINT,
    i->Name,
    NULL
    );
    PhSetListViewSubItem(
    ListViewHandle,
    index,
    1,
    FirmwareAttributeToString(i->Attributes)->Buffer
    );

    PhSetListViewSubItem(
    ListViewHandle,
    index,
    2,
    FirmwareGuidToNameString(&vendorGuid)
    );

    PhSetListViewSubItem(
    ListViewHandle,
    index,
    3,
    guidString->Buffer
    );

    PhSetListViewSubItem(ListViewHandle, index, 4, PhaFormatSize(i->ValueLength, -1)->Buffer);

    PhDereferenceObject(guidString);
    }*/
    //}

  //  ExtendedListView_SortItems(ListViewHandle);
   // ExtendedListView_SetRedraw(ListViewHandle, TRUE);

    return 0;
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
        WCHAR textBuffer[MAX_PATH] = L"";

        LVITEM item;
        item.mask = LVIF_TEXT;
        item.iItem = index;
        item.iSubItem = 0;
        item.pszText = textBuffer;
        item.cchTextMax = ARRAYSIZE(textBuffer);

        if (ListView_GetItem(hWnd, &item))
            return PhCreateString(textBuffer);
    }

    return NULL;
}

VOID ShowBootEntryMenu(
    _In_ PBOOT_WINDOW_CONTEXT Context,
    _In_ HWND hwndDlg
    )
{
    PPH_STRING bootEntryName;

    if (bootEntryName = PhGetSelectedListViewItemText(Context->ListViewHandle))
    {
        PhDereferenceObject(bootEntryName);
    }
}

INT NTAPI FirmwareNameCompareFunction(
    _In_ PVOID Item1,
    _In_ PVOID Item2,
    _In_opt_ PVOID Context
    )
{
    PPH_STRING item1 = Item1;
    PPH_STRING item2 = Item2;

    return PhCompareStringZ(PhGetStringOrEmpty(item1), PhGetStringOrEmpty(item2), TRUE);
}

INT_PTR CALLBACK FwEntriesDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PBOOT_WINDOW_CONTEXT context;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PBOOT_WINDOW_CONTEXT)PhAllocate(sizeof(BOOT_WINDOW_CONTEXT));
        memset(context, 0, sizeof(BOOT_WINDOW_CONTEXT));

        SetProp(hwndDlg, L"Context", (HANDLE)context);
    }
    else
    {
        context = (PBOOT_WINDOW_CONTEXT)GetProp(hwndDlg, L"Context");

        if (uMsg == WM_DESTROY)
        {
            PhSaveListViewColumnsToSetting(SETTING_NAME_LISTVIEW_COLUMNS, context->ListViewHandle);
            PhSaveWindowPlacementToSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            PhDeleteLayoutManager(&context->LayoutManager);
            PhUnregisterDialog(hwndDlg);
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
            context->ListViewHandle = GetDlgItem(hwndDlg, IDC_LIST1);

            PhCenterWindow(hwndDlg, PhMainWndHandle);
            PhRegisterDialog(hwndDlg);

            PhSetListViewStyle(context->ListViewHandle, FALSE, TRUE);
            PhSetControlTheme(context->ListViewHandle, L"explorer");
            PhAddListViewColumn(context->ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 200, L"Name");
            PhAddListViewColumn(context->ListViewHandle, 1, 1, 1, LVCFMT_LEFT, 140, L"Attributes");
            PhAddListViewColumn(context->ListViewHandle, 2, 2, 2, LVCFMT_LEFT, 140, L"Guid Name");
            PhSetExtendedListView(context->ListViewHandle);

            //ExtendedListView_SetSortFast(context->ListViewHandle, TRUE);
            ListView_SetImageList(context->ListViewHandle, ImageList_Create(20, 20, ILC_COLOR32 | ILC_MASK, 0, 1), LVSIL_SMALL);
            ExtendedListView_SetCompareFunction(context->ListViewHandle, 0, FirmwareNameCompareFunction);
            ExtendedListView_SetCompareFunction(context->ListViewHandle, 1, FirmwareNameCompareFunction);
            PhLoadListViewColumnsFromSetting(SETTING_NAME_LISTVIEW_COLUMNS, context->ListViewHandle);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, context->ListViewHandle, NULL, PH_ANCHOR_ALL);
            //PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_BOOT_REFRESH), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDOK), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
            PhLoadWindowPlacementFromSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            
            EnumerateEnvironmentValues(context);
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&context->LayoutManager);
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
    case WM_NOTIFY:
        {
            LPNMHDR hdr = (LPNMHDR)lParam;

            switch (hdr->code)
            {
            case NM_RCLICK:
                {
                    if (hdr->hwndFrom == context->ListViewHandle)
                        ShowBootEntryMenu(context, hwndDlg);
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}


VOID ShowFwDialog(
    VOID
    )
{
    DialogBox(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_FWTABLE),
        NULL,
        FwEntriesDlgProc
        );
    //if (!DbgDialogThreadHandle)
    //{
    //    if (!(DbgDialogThreadHandle = PhCreateThread(0, DbgViewDialogThread, NULL)))
    //    {
    //        PhShowStatus(PhMainWndHandle, L"Unable to create the window.", 0, GetLastError());
    //        return;
    //    }

    //    PhWaitForEvent(&InitializedEvent, NULL);
    //}

    //PostMessage(DbgDialogHandle, WM_SHOWDIALOG, 0, 0);
}