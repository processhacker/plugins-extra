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

//VOID ChangeNotificationThread(
//    VOID
//    )
//{
//    HANDLE eventWaitHandle;
//    HANDLE zero = NULL;
//
//    if (NT_SUCCESS(NtCreateEvent(&eventWaitHandle, EVENT_ALL_ACCESS, NULL, SynchronizationEvent, FALSE)))
//    {
//        if (FWChangeNotificationCreate(eventWaitHandle, &zero) == ERROR_SUCCESS)
//        {
//
//        }
//
//        if (zero)
//        {
//            FWChangeNotificationDestroy(zero);
//        }
//
//        NtClose(eventWaitHandle);
//    }
//}

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
        WCHAR localized[DOS_MAX_PATH_LENGTH] = L"";

        if (SUCCEEDED(SHLoadIndirectString(
            FwRule->wszEmbeddedContext,
            localized,
            DOS_MAX_PATH_LENGTH,
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
            //    DOS_MAX_PATH_LENGTH, 
            //    &id
            //    )))
            //{
            //ExpandEnvironmentStrings(FwRule->wszEmbeddedContext, module_expanded, DOS_MAX_PATH_LENGTH);
            //LoadString(GetModuleHandle(module_expanded), *id, localized, DOS_MAX_PATH_LENGTH);
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

    HANDLE FwEngineHandle = NULL;
    FWPM_SESSION session = { 0 };
    session.flags = 0;// FWPM_SESSION_FLAG_DYNAMIC;
    session.displayData.name = L"PhFirewallFilterSession";
    session.displayData.description = L"Non-Dynamic session for Process Hacker";

    // Create a non-dynamic BFE session
    if (FwpmEngineOpen(
        NULL,
        RPC_C_AUTHN_WINNT,
        NULL,
        &session,
        &FwEngineHandle
        ) != ERROR_SUCCESS)
    {
        return FALSE;
    }

    HANDLE FwEnumHandle = NULL;
    FWPM_FILTER_ENUM_TEMPLATE enumTemplate = { 0 };
    //enumTemplate.actionMask = 0;// 0xFFFFFFFF; // We want to see all filters regardless of action.

    enumTemplate.enumType = FWP_FILTER_ENUM_FULLY_CONTAINED;
    enumTemplate.flags = FWP_FILTER_ENUM_FLAG_INCLUDE_BOOTTIME;// | FWP_FILTER_ENUM_FLAG_INCLUDE_DISABLED;
    enumTemplate.actionMask = 0xFFFFFFFF;

    FwpmFilterCreateEnumHandle0(FwEngineHandle, NULL, &FwEnumHandle);


    FWPM_FILTER** filters = NULL;
    UINT32 numFilters = 0;

    if (FwpmFilterEnum(FwEngineHandle, FwEnumHandle, UINT_MAX, &filters, &numFilters) == ERROR_SUCCESS)
    {
        for (UINT32 i = 0; i < numFilters; i++)
        {
            FWPM_FILTER* entry = filters[i];
            INT lvItemIndex;
            WCHAR string[MAX_PATH];

            if (SUCCEEDED(SHLoadIndirectString(entry->displayData.name, string, RTL_NUMBER_OF(string), NULL)))
            {
                lvItemIndex = PhAddListViewItem(context->ListViewHandle, MAXINT, string, NULL);
            }
            else
            {
                lvItemIndex = PhAddListViewItem(context->ListViewHandle, MAXINT, entry->displayData.name, NULL);
            }

            if (entry->displayData.description && PhCountStringZ(entry->displayData.description) > 0)
            {
                if (SUCCEEDED(SHLoadIndirectString(entry->displayData.description, string, RTL_NUMBER_OF(string), NULL)))
                {
                    ListView_SetItemText(context->ListViewHandle, lvItemIndex, 1, string);
                }
                else
                {
                    ListView_SetItemText(context->ListViewHandle, lvItemIndex, 1, entry->displayData.description);
                }       
            }

            switch (entry->action.type)
            {
            case FWP_ACTION_BLOCK:
                ListView_SetItemText(context->ListViewHandle, lvItemIndex, 2, L"Block");
                break;
            case FWP_ACTION_PERMIT:
                ListView_SetItemText(context->ListViewHandle, lvItemIndex, 2, L"Allow");
                break;
            default:
                ListView_SetItemText(context->ListViewHandle, lvItemIndex, 2, L"ERROR");
                break;
            }

            //if (entry->numFilterConditions)
            //{
            //    for (UINT32 ii = 0; ii < entry->numFilterConditions; ii++)
            //    {
            //        FWPM_FILTER_CONDITION0 filterCondition = entry->filterCondition[ii];
            //
            //        PPH_STRING str = PhFormatGuid(&filterCondition.fieldKey);
            //
            //        dprintf("%S\n", str->Buffer);
            //
            //        if (IsEqualGUID(&filterCondition.fieldKey, &FWPM_CONDITION_DIRECTION))
            //        {
            //
            //        }
            //    }
            //
            //    dprintf("\n");
            //}
        }

        //FwpmFreeMemory(filters);
    }

    //if (InitializeFirewallApi())
    //{
    //    EnumerateFirewallRules(FW_PROFILE_TYPE_CURRENT, FW_DIR_IN, WfAddRules, context);
    //    FreeFirewallApi();
    //} 

    return STATUS_SUCCESS;
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
        WCHAR textBuffer[DOS_MAX_PATH_LENGTH] = L"";

        LVITEM item;

        memset(&item, 0, sizeof(LVITEM));
        item.mask = LVIF_TEXT;
        item.iItem = index;
        item.iSubItem = 0;
        item.pszText = textBuffer;
        item.cchTextMax = RTL_NUMBER_OF(textBuffer);

        if (ListView_GetItem(hWnd, &item))
        {
            return PhCreateString(textBuffer);
        }
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


LRESULT SysButtonCustomDraw(
    _In_ PBOOT_WINDOW_CONTEXT Context,
    _In_ LPARAM lParam
    )
{
    LPNMTVCUSTOMDRAW drawInfo = (LPNMTVCUSTOMDRAW)lParam;
    BOOLEAN isGrayed = (drawInfo->nmcd.uItemState & CDIS_GRAYED) == CDIS_GRAYED;
    BOOLEAN isChecked = (drawInfo->nmcd.uItemState & CDIS_CHECKED) == CDIS_CHECKED;
    BOOLEAN isDisabled = (drawInfo->nmcd.uItemState & CDIS_DISABLED) == CDIS_DISABLED;
    BOOLEAN isSelected = (drawInfo->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED;
    BOOLEAN isHighlighted = (drawInfo->nmcd.uItemState & CDIS_HOT) == CDIS_HOT;
    BOOLEAN isFocused = (drawInfo->nmcd.uItemState & CDIS_FOCUS) == CDIS_FOCUS;

    if (drawInfo->nmcd.dwDrawStage == CDDS_PREPAINT)
    {
        HPEN PenActive = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
        HPEN PenNormal = CreatePen(PS_SOLID, 1, RGB(0, 0xff, 0));
        HBRUSH BrushNormal = GetSysColorBrush(COLOR_3DFACE);
        HBRUSH BrushSelected = CreateSolidBrush(RGB(0xff, 0xff, 0xff));
        HBRUSH BrushPushed = CreateSolidBrush(RGB(153, 209, 255));

        PPH_STRING windowText = PH_AUTO(PhGetWindowText(drawInfo->nmcd.hdr.hwndFrom));

        SetBkMode(drawInfo->nmcd.hdc, TRANSPARENT);

        if (isHighlighted || Context->PluginMenuActiveId == drawInfo->nmcd.hdr.idFrom)
        {
            FillRect(drawInfo->nmcd.hdc, &drawInfo->nmcd.rc, BrushNormal);

            SelectObject(drawInfo->nmcd.hdc, PenActive);
            SelectObject(drawInfo->nmcd.hdc, Context->BoldFontHandle);
        }
        else if (isSelected)
        {
            FillRect(drawInfo->nmcd.hdc, &drawInfo->nmcd.rc, BrushPushed);
           
            SelectObject(drawInfo->nmcd.hdc, PenActive);
            SelectObject(drawInfo->nmcd.hdc, Context->BoldFontHandle);
        }
        else
        {
            FillRect(drawInfo->nmcd.hdc, &drawInfo->nmcd.rc, BrushNormal);
        }

        DrawText(
            drawInfo->nmcd.hdc,
            windowText->Buffer,
            (ULONG)windowText->Length / 2,
            &drawInfo->nmcd.rc,
            DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_SINGLELINE
            );

        MoveToEx(drawInfo->nmcd.hdc, drawInfo->nmcd.rc.left, drawInfo->nmcd.rc.bottom, 0);
        LineTo(drawInfo->nmcd.hdc, drawInfo->nmcd.rc.right, drawInfo->nmcd.rc.bottom);

        DeletePen(PenNormal);
        DeletePen(PenActive);
        DeleteBrush(BrushNormal);
        DeleteBrush(BrushSelected);
        DeleteBrush(BrushPushed);

        return CDRF_SKIPDEFAULT;
    }

    return CDRF_DODEFAULT;
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

        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);

        if (uMsg == WM_DESTROY)
        {
            PhSaveListViewColumnsToSetting(SETTING_NAME_LISTVIEW_COLUMNS, context->ListViewHandle);
            PhSaveWindowPlacementToSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            PhDeleteLayoutManager(&context->LayoutManager);
            PhUnregisterDialog(hwndDlg);
            PhRemoveWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
            PhFree(context);
        }
    }

    if (!context)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            LOGFONT logFont;

            //context->DialogHandle = hwndDlg;
            context->PluginMenuActiveId = IDC_INBOUND;
            context->PluginMenuActive = GetDlgItem(hwndDlg, IDC_INBOUND);
            context->ListViewHandle = GetDlgItem(hwndDlg, IDC_LIST1);
            context->SearchHandle = GetDlgItem(hwndDlg, IDC_SEARCHBOX);

            //CreateSearchControl(hwndDlg, context->SearchHandle, ID_SEARCH_CLEAR);

            if (SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), &logFont, 0))
            {
                context->NormalFontHandle = CreateFont(
                    -PhMultiplyDivideSigned(-14, PhGlobalDpi, 72),
                    0,
                    0,
                    0,
                    FW_NORMAL,
                    FALSE,
                    FALSE,
                    FALSE,
                    ANSI_CHARSET,
                    OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY | ANTIALIASED_QUALITY,
                    DEFAULT_PITCH,
                    logFont.lfFaceName
                    );

                context->BoldFontHandle = CreateFont(
                    -PhMultiplyDivideSigned(-16, PhGlobalDpi, 72),
                    0,
                    0,
                    0,
                    FW_BOLD,
                    FALSE,
                    FALSE,
                    FALSE,
                    ANSI_CHARSET,
                    OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY | ANTIALIASED_QUALITY,
                    DEFAULT_PITCH,
                    logFont.lfFaceName
                    );
            }

            PhSetListViewStyle(context->ListViewHandle, FALSE, TRUE);
            PhSetControlTheme(context->ListViewHandle, L"explorer");
            PhAddListViewColumn(context->ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 450, L"Name");
            PhAddListViewColumn(context->ListViewHandle, 1, 1, 1, LVCFMT_LEFT, 440, L"Attributes");
            PhAddListViewColumn(context->ListViewHandle, 2, 2, 2, LVCFMT_LEFT, 80, L"Type");
            PhSetExtendedListView(context->ListViewHandle);

            //ExtendedListView_SetSortFast(context->ListViewHandle, TRUE);
            ListView_SetImageList(context->ListViewHandle, ImageList_Create(20, 20, ILC_COLOR32 | ILC_MASK, 0, 1), LVSIL_SMALL);
            ExtendedListView_SetCompareFunction(context->ListViewHandle, 0, FirmwareNameCompareFunction);
            ExtendedListView_SetCompareFunction(context->ListViewHandle, 1, FirmwareNameCompareFunction);
            //PhLoadListViewColumnsFromSetting(SETTING_NAME_LISTVIEW_COLUMNS, context->ListViewHandle);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_SEARCHBOX), NULL, PH_ANCHOR_TOP | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&context->LayoutManager, context->ListViewHandle, NULL, PH_ANCHOR_ALL);

            if (PhGetIntegerPairSetting(SETTING_NAME_WINDOW_POSITION).X != 0)
                PhLoadWindowPlacementFromSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            else
                PhCenterWindow(hwndDlg, PhMainWndHandle);
            
            PhCreateThread2(FWRulesEnumThreadStart, context);
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
            case NM_CUSTOMDRAW:
                {
                    if (hdr->idFrom == IDC_INBOUND || hdr->idFrom == IDC_OUTBOUND)
                    {
                        SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, SysButtonCustomDraw(context, lParam));
                        return TRUE;
                    }
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
    //    if (!NT_SUCCESS(PhCreateThreadEx(&DbgDialogThreadHandle, DbgViewDialogThread, NULL)))
    //    {
    //        PhShowError(PhMainWndHandle, L"Unable to create the window.");
    //        return;
    //    }

    //    PhWaitForEvent(&InitializedEvent, NULL);
    //}

    //PostMessage(DbgDialogHandle, WM_SHOWDIALOG, 0, 0);
}
