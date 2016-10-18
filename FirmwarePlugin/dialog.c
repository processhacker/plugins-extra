/*
 * Process Hacker Extra Plugins -
 *   Firmware Plugin
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
#include "efi_guid_list.h"

PPH_STRING FirmwareAttributeToString(
    _In_ ULONG Attribute
    )
{
    PH_STRING_BUILDER sb;

    PhInitializeStringBuilder(&sb, 0x100);

    if (Attribute & EFI_VARIABLE_NON_VOLATILE)
        PhAppendStringBuilder2(&sb, L"Non Volatile, ");

    if (Attribute & EFI_VARIABLE_BOOTSERVICE_ACCESS)
        PhAppendStringBuilder2(&sb, L"Boot Service, ");

    if (Attribute & EFI_VARIABLE_RUNTIME_ACCESS)
        PhAppendStringBuilder2(&sb, L"Runtime Access, ");

    if (Attribute & EFI_VARIABLE_HARDWARE_ERROR_RECORD)
        PhAppendStringBuilder2(&sb, L"Hardware Error Record, ");

    if (Attribute & EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS)
        PhAppendStringBuilder2(&sb, L"Authenticated Write Access, ");

    if (Attribute & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS)
        PhAppendStringBuilder2(&sb, L"Authenticated Write Access (Time Based), ");

    if (Attribute & EFI_VARIABLE_APPEND_WRITE)
        PhAppendStringBuilder2(&sb, L"Append Write, ");

    if (PhEndsWithStringRef2(&sb.String->sr, L", ", FALSE))
        PhRemoveEndStringBuilder(&sb, 2);

    return PhFinalStringBuilderString(&sb);
}

PWSTR FirmwareGuidToNameString(
    _In_ PGUID VendorGuid
    )
{
    for (ULONG i = 0; i < ARRAYSIZE(table); i++)
    {
        if (IsEqualGUID(VendorGuid, &table[i].Guid))
            return table[i].Name;
    }

    return L"";
}

NTSTATUS EnumerateEnvironmentValues(
    _In_ HWND ListViewHandle
    )
{
    NTSTATUS status;
    PVOID variables;

    ExtendedListView_SetRedraw(ListViewHandle, FALSE);
    ListView_DeleteAllItems(ListViewHandle);

    if (NT_SUCCESS(status = EnumerateFirmwareValues(&variables)))
    {
        PVARIABLE_NAME_AND_VALUE i;

        for (i = PH_FIRST_EFI_VARIABLE(variables); i; i = PH_NEXT_EFI_VARIABLE(i))
        {       
            INT index;
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
        }

        PhFree(variables);
    }

    ExtendedListView_SortItems(ListViewHandle);
    ExtendedListView_SetRedraw(ListViewHandle, TRUE);

    return status;
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

INT_PTR CALLBACK BootEntriesDlgProc(
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
            context->ListViewHandle = GetDlgItem(hwndDlg, IDC_BOOT_LIST);

            PhRegisterDialog(hwndDlg);
            PhSetListViewStyle(context->ListViewHandle, FALSE, TRUE);
            PhSetControlTheme(context->ListViewHandle, L"explorer");
            PhAddListViewColumn(context->ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 100, L"Name");
            PhAddListViewColumn(context->ListViewHandle, 1, 1, 1, LVCFMT_LEFT, 140, L"Attributes");
            PhAddListViewColumn(context->ListViewHandle, 2, 2, 2, LVCFMT_LEFT, 140, L"Guid Name");
            PhAddListViewColumn(context->ListViewHandle, 3, 3, 3, LVCFMT_LEFT, 140, L"Guid");
            PhAddListViewColumn(context->ListViewHandle, 4, 4, 4, LVCFMT_LEFT, 50, L"Data Length");
            PhSetExtendedListView(context->ListViewHandle);

            //ExtendedListView_SetSortFast(context->ListViewHandle, TRUE);
            ExtendedListView_SetCompareFunction(context->ListViewHandle, 0, FirmwareNameCompareFunction);
            ExtendedListView_SetCompareFunction(context->ListViewHandle, 1, FirmwareNameCompareFunction);
            PhLoadListViewColumnsFromSetting(SETTING_NAME_LISTVIEW_COLUMNS, context->ListViewHandle);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, context->ListViewHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_BOOT_REFRESH), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDOK), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
            PhLoadWindowPlacementFromSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            
            EnumerateEnvironmentValues(context->ListViewHandle);
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&context->LayoutManager);
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDC_BOOT_REFRESH:
                {
                    EnumerateEnvironmentValues(context->ListViewHandle);
                }
                break;
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