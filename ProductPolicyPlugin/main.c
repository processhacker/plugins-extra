/*
 * Process Hacker Extra Plugins -
 *   NT Product Policy Plugin
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

PPH_PLUGIN PluginInstance;
HWND ListViewWndHandle;
PH_LAYOUT_MANAGER LayoutManager;
PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
PH_CALLBACK_REGISTRATION MainMenuInitializingCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginShowOptionsCallbackRegistration;

VOID LoadPolicyTable(VOID)
{
    PPH_LIST productPolicies;

    ExtendedListView_SetRedraw(ListViewWndHandle, FALSE);
    ListView_DeleteAllItems(ListViewWndHandle);

    if (productPolicies = QueryProductPolicies())
    {
        for (ULONG i = 0; i < productPolicies->Count; i++)
        {
            PNT_POLICY_ENTRY entry = productPolicies->Items[i];

            INT index = PhAddListViewItem(ListViewWndHandle, MAXINT, PhGetString(entry->Name), NULL);
            PhSetListViewSubItem(ListViewWndHandle, index, 1, PhGetString(entry->Value));

            if (entry->Name)
                PhDereferenceObject(entry->Name);
            if (entry->Value)
                PhDereferenceObject(entry->Value);
        }

        PhDereferenceObject(productPolicies);
    }

    ExtendedListView_SetRedraw(ListViewWndHandle, TRUE);
}

PPH_STRING PhGetSelectedListViewSubItemText(
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
        item.iSubItem = 1;
        item.pszText = buffer;
        item.cchTextMax = ARRAYSIZE(buffer);

        if (ListView_GetItem(hWnd, &item))
            return PhCreateString(buffer);
    }

    return NULL;
}

INT_PTR CALLBACK ViewPolicyDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    static PH_LAYOUT_MANAGER ViewLayoutManager;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PPH_STRING value = (PPH_STRING)lParam;

            PhCenterWindow(hwndDlg, GetParent(hwndDlg));

            PhInitializeLayoutManager(&ViewLayoutManager, hwndDlg);
            PhAddLayoutItem(&ViewLayoutManager, GetDlgItem(hwndDlg, IDC_POLICYEDIT), NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&ViewLayoutManager, GetDlgItem(hwndDlg, IDOK), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);

            PhSetWindowText(GetDlgItem(hwndDlg, IDC_POLICYEDIT), value->Buffer);
        }
        break;    
    case WM_SIZE:
        PhLayoutManagerLayout(&ViewLayoutManager);
        break;
    case WM_DESTROY:
        PhDeleteLayoutManager(&ViewLayoutManager);
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

INT_PTR CALLBACK MainWindowDlgProc(
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
            PhCenterWindow(hwndDlg, PhMainWndHandle);
            ListViewWndHandle = GetDlgItem(hwndDlg, IDC_ATOMLIST);

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, ListViewWndHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDRETRY), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDOK), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);

            PhRegisterDialog(hwndDlg);
            PhLoadWindowPlacementFromSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);

            PhSetListViewStyle(ListViewWndHandle, FALSE, TRUE);
            PhSetControlTheme(ListViewWndHandle, L"explorer");
            PhAddListViewColumn(ListViewWndHandle, 0, 0, 0, LVCFMT_LEFT, 350, L"Name");
            PhAddListViewColumn(ListViewWndHandle, 1, 1, 1, LVCFMT_LEFT, 100, L"Value");
            PhSetExtendedListView(ListViewWndHandle);
            PhLoadListViewColumnsFromSetting(SETTING_NAME_LISTVIEW_COLUMNS, ListViewWndHandle);

            LoadPolicyTable();
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&LayoutManager);
        break;
    case WM_DESTROY:
        PhSaveWindowPlacementToSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
        PhSaveListViewColumnsToSetting(SETTING_NAME_LISTVIEW_COLUMNS, ListViewWndHandle);
        PhDeleteLayoutManager(&LayoutManager);
        PhUnregisterDialog(hwndDlg);
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDCANCEL:
            case IDOK:
                EndDialog(hwndDlg, IDOK);
                break;
            case IDRETRY:
                LoadPolicyTable();
                break;
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR hdr = (LPNMHDR)lParam;

            switch (hdr->code)
            {
            case NM_DBLCLK:
                {
                    if (hdr->hwndFrom == ListViewWndHandle)
                    {
                        PPH_STRING subItemText = PhGetSelectedListViewSubItemText(ListViewWndHandle);

                        DialogBoxParam(
                            PluginInstance->DllBase, 
                            MAKEINTRESOURCE(IDD_POLICYVIEW), 
                            hwndDlg, 
                            ViewPolicyDlgProc, 
                            (LPARAM)subItemText
                            );

                        PhDereferenceObject(subItemText);
                    }
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}

VOID NTAPI MainMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;
    PPH_EMENU_ITEM systemMenu;

    if (menuInfo->u.MainMenu.SubMenuIndex != PH_MENU_ITEM_LOCATION_TOOLS)
        return;

    if (!(systemMenu = PhFindEMenuItem(menuInfo->Menu, 0, L"System", 0)))
    {
        PhInsertEMenuItem(menuInfo->Menu, systemMenu = PhPluginCreateEMenuItem(PluginInstance, 0, 0, L"&System", NULL), -1);
    }

    PhInsertEMenuItem(systemMenu, PhPluginCreateEMenuItem(PluginInstance, 0, POLICY_TABLE_MENUITEM, L"&Product Policy", NULL), -1);
}

VOID NTAPI MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = (PPH_PLUGIN_MENU_ITEM)Parameter;

    switch (menuItem->Id)
    {
    case POLICY_TABLE_MENUITEM:
        {
            DialogBox(
                PluginInstance->DllBase,
                MAKEINTRESOURCE(IDD_POLICYDIALOG),
                NULL,
                MainWindowDlgProc
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
            info->DisplayName = L"Software License Policy";
            info->Description = L"Plugin for viewing the current Software License Policy via the Tools menu.";
            info->HasOptions = FALSE;

            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackMainMenuInitializing),
                MainMenuInitializingCallback,
                NULL,
                &MainMenuInitializingCallbackRegistration
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
