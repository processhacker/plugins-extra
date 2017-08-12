/*
 * Process Hacker Extra Plugins -
 *   DNS Cache Plugin
 *
 * Copyright (C) 2014 dmex
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

static HINSTANCE DnsApiHandle = NULL;
static _DnsGetCacheDataTable DnsGetCacheDataTable_I = NULL;
static _DnsFlushResolverCache DnsFlushResolverCache_I = NULL;
static _DnsFlushResolverCacheEntry DnsFlushResolverCacheEntry_I = NULL;

static HWND ListViewWndHandle;
static PH_LAYOUT_MANAGER LayoutManager;
static PPH_PLUGIN PluginInstance;
static PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
static PH_CALLBACK_REGISTRATION MainMenuInitializingCallbackRegistration;
static PH_CALLBACK_REGISTRATION PluginShowOptionsCallbackRegistration;

VOID EnumDnsCacheTable(
    _In_ HWND hwndDlg
    )
{
    PDNS_CACHE_ENTRY dnsCacheDataTable = NULL;

    if (!DnsGetCacheDataTable_I(&dnsCacheDataTable))
        goto CleanupExit;

    while (dnsCacheDataTable)
    {
        PDNS_RECORD dnsQueryResultPtr = NULL;

		DNS_STATUS dnsStatus = DNS_ERROR_RECORD_DOES_NOT_EXIST;
		for (int type = DNS_TYPE_A; type <= DNS_TYPE_SRV; type++)
		{
			unsigned atype = type & 0xff;
			dnsStatus = DnsQuery(
				dnsCacheDataTable->Name,
				atype,
				DNS_QUERY_NO_WIRE_QUERY | 32768, // Undocumented flags
				NULL,
				&dnsQueryResultPtr,
				NULL
			);
			if (dnsStatus == ERROR_SUCCESS)
			{
				break;
			}
		}


        if (dnsStatus == ERROR_SUCCESS)
        {
            PDNS_RECORD dnsRecordPtr = dnsQueryResultPtr;

            while (dnsRecordPtr)
            {
                INT itemIndex = MAXINT;
                ULONG ipAddrStringLength = INET6_ADDRSTRLEN;
                WCHAR ipAddrString[INET6_ADDRSTRLEN] = L"";

                itemIndex = PhAddListViewItem(hwndDlg, MAXINT,
                    PhaFormatString(L"%s", dnsRecordPtr->pName)->Buffer,
                    NULL
                    );

                if (dnsRecordPtr->wType == DNS_TYPE_A)
                {
                    IN_ADDR ipv4Address = { 0 };

                    ipv4Address.s_addr = dnsRecordPtr->Data.A.IpAddress;

                    RtlIpv4AddressToString(&ipv4Address, ipAddrString);

                    PhSetListViewSubItem(hwndDlg, itemIndex, 1, PhaFormatString(L"A")->Buffer);
                    PhSetListViewSubItem(hwndDlg, itemIndex, 2, PhaFormatString(L"%s", ipAddrString)->Buffer);
                }
                else if (dnsRecordPtr->wType == DNS_TYPE_AAAA)
                {
                    IN6_ADDR ipv6Address = { 0 };

                    memcpy_s(
                        ipv6Address.s6_addr,
                        sizeof(ipv6Address.s6_addr),
                        dnsRecordPtr->Data.AAAA.Ip6Address.IP6Byte,
                        sizeof(dnsRecordPtr->Data.AAAA.Ip6Address.IP6Byte)
                        );

                    RtlIpv6AddressToString(&ipv6Address, ipAddrString);

                    PhSetListViewSubItem(hwndDlg, itemIndex, 1, PhaFormatString(L"AAAA")->Buffer);
                    PhSetListViewSubItem(hwndDlg, itemIndex, 2, PhaFormatString(L"%s", ipAddrString)->Buffer);
                }
                else if (dnsRecordPtr->wType == DNS_TYPE_PTR)
                {
                    PhSetListViewSubItem(hwndDlg, itemIndex, 1, PhaFormatString(L"PTR")->Buffer);
                    PhSetListViewSubItem(hwndDlg, itemIndex, 2, PhaFormatString(L"%s", dnsRecordPtr->Data.PTR.pNameHost)->Buffer);
                }
                else if (dnsRecordPtr->wType == DNS_TYPE_CNAME)
                {
                    PhSetListViewSubItem(hwndDlg, itemIndex, 1, PhaFormatString(L"CNAME")->Buffer);
                    PhSetListViewSubItem(hwndDlg, itemIndex, 2, PhaFormatString(L"%s", dnsRecordPtr->Data.CNAME.pNameHost)->Buffer);
                }
                else
                {
                    PhSetListViewSubItem(hwndDlg, itemIndex, 1, PhaFormatString(L"UNKNOWN")->Buffer);
                    PhSetListViewSubItem(hwndDlg, itemIndex, 2, PhaFormatString(L"")->Buffer);
                }

                PhSetListViewSubItem(hwndDlg, itemIndex, 3, PhaFormatString(L"%lu", dnsRecordPtr->dwTtl)->Buffer);

                dnsRecordPtr = dnsRecordPtr->pNext;
            }

            DnsRecordListFree(dnsQueryResultPtr, DnsFreeRecordList);
        }

        dnsCacheDataTable = dnsCacheDataTable->Next;
    }

CleanupExit:

    if (dnsCacheDataTable)
    {
        DnsRecordListFree(dnsCacheDataTable, DnsFreeRecordList);
    }
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

VOID ShowStatusMenu(
    _In_ HWND hwndDlg
    )
{
    PPH_STRING cacheEntryName;
    
    cacheEntryName = PhGetSelectedListViewItemText(ListViewWndHandle);

    if (cacheEntryName)
    {
        POINT cursorPos;
        PPH_EMENU menu;
        PPH_EMENU_ITEM selectedItem;

        GetCursorPos(&cursorPos);

        menu = PhCreateEMenu();
        PhInsertEMenuItem(menu, PhCreateEMenuItem(0, 1, L"Remove", NULL, NULL), -1);

        selectedItem = PhShowEMenu(
            menu,
            PhMainWndHandle,
            PH_EMENU_SHOW_LEFTRIGHT,
            PH_ALIGN_LEFT | PH_ALIGN_TOP,
            cursorPos.x,
            cursorPos.y
            );

        if (selectedItem && selectedItem->Id != -1)
        {
            switch (selectedItem->Id)
            {
            case 1:
                {
                    INT lvItemIndex = PhFindListViewItemByFlags(
                        ListViewWndHandle,
                        -1,
                        LVNI_SELECTED
                        );

                    if (lvItemIndex != -1)
                    {
                        if (!PhGetIntegerSetting(L"EnableWarnings") || PhShowConfirmMessage(
                            hwndDlg,
                            L"remove",
                            cacheEntryName->Buffer,
                            NULL,
                            FALSE
                            ))
                        {
                            if (DnsFlushResolverCacheEntry_I(cacheEntryName->Buffer))
                            {
                                ListView_DeleteAllItems(ListViewWndHandle);
                                EnumDnsCacheTable(ListViewWndHandle);
                            }
                        }
                    }
                }
                break;
            }
        }

        PhDestroyEMenu(menu);
        PhDereferenceObject(cacheEntryName);
    }
}

INT_PTR CALLBACK DnsCacheDlgProc(
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
            ListViewWndHandle = GetDlgItem(hwndDlg, IDC_DNSLIST);

            PhRegisterDialog(hwndDlg);
            PhSetListViewStyle(ListViewWndHandle, FALSE, TRUE);
            PhSetControlTheme(ListViewWndHandle, L"explorer");
            PhAddListViewColumn(ListViewWndHandle, 0, 0, 0, LVCFMT_LEFT, 280, L"Host Name");
            PhAddListViewColumn(ListViewWndHandle, 1, 1, 1, LVCFMT_LEFT, 70, L"Type");
            PhAddListViewColumn(ListViewWndHandle, 2, 2, 2, LVCFMT_LEFT, 100, L"IP Address");
            PhAddListViewColumn(ListViewWndHandle, 3, 3, 3, LVCFMT_LEFT, 50, L"TTL");
            PhSetExtendedListView(ListViewWndHandle);

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, ListViewWndHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_DNS_REFRESH), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_DNS_CLEAR), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDOK), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
            PhLoadWindowPlacementFromSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            PhLoadListViewColumnsFromSetting(SETTING_NAME_COLUMNS, ListViewWndHandle);

            if (DnsApiHandle = LoadLibrary(L"dnsapi.dll"))
            {
                DnsGetCacheDataTable_I = PhGetProcedureAddress(DnsApiHandle, "DnsGetCacheDataTable", 0);
                DnsFlushResolverCache_I = PhGetProcedureAddress(DnsApiHandle, "DnsFlushResolverCache", 0);
                DnsFlushResolverCacheEntry_I = PhGetProcedureAddress(DnsApiHandle, "DnsFlushResolverCacheEntry_W", 0);
            }

            EnumDnsCacheTable(ListViewWndHandle);
        }
        break;
    case WM_DESTROY:
        {
            if (DnsApiHandle)
            {
                FreeLibrary(DnsApiHandle);
                DnsApiHandle = NULL;
            }

            PhSaveWindowPlacementToSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            PhSaveListViewColumnsToSetting(SETTING_NAME_COLUMNS, ListViewWndHandle);
            PhDeleteLayoutManager(&LayoutManager);
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&LayoutManager);
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDC_DNS_CLEAR:
                {
                    if (!PhGetIntegerSetting(L"EnableWarnings") || PhShowConfirmMessage(
                        hwndDlg,
                        L"Flush",
                        L"the dns cache",
                        NULL,
                        FALSE
                        ))
                    {
                        if (DnsFlushResolverCache_I)
                            DnsFlushResolverCache_I();

                        ListView_DeleteAllItems(ListViewWndHandle);
                        EnumDnsCacheTable(ListViewWndHandle);
                    }
                }
                break;
            case IDC_DNS_REFRESH:
                ListView_DeleteAllItems(ListViewWndHandle);
                EnumDnsCacheTable(ListViewWndHandle);
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
                    if (hdr->hwndFrom == ListViewWndHandle)
                        ShowStatusMenu(hwndDlg);
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

    if (!(systemMenu = PhFindEMenuItem(menuInfo->Menu, 0, L"&System", 0)))
    {
        PhInsertEMenuItem(menuInfo->Menu, PhPluginCreateEMenuItem(PluginInstance, PH_EMENU_SEPARATOR, 0, L"", NULL), -1);
        PhInsertEMenuItem(menuInfo->Menu, systemMenu = PhPluginCreateEMenuItem(PluginInstance, 0, 0, L"&System", NULL), -1);
    }

    PhInsertEMenuItem(systemMenu, PhPluginCreateEMenuItem(PluginInstance, 0, DNSCACHE_MENUITEM, L"&DNS Cache Table", NULL), -1);
}

VOID NTAPI MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = (PPH_PLUGIN_MENU_ITEM)Parameter;

    switch (menuItem->Id)
    {
    case DNSCACHE_MENUITEM:
        {
            DialogBox(
                PluginInstance->DllBase,
                MAKEINTRESOURCE(IDD_DNSVIEW),
                NULL,
                DnsCacheDlgProc
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
                { StringSettingType, SETTING_NAME_COLUMNS, L"" }
            };

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->DisplayName = L"DNS Cache Viewer";
            info->Author = L"dmex";
            info->Description = L"Plugin for viewing the DNS Resolver Cache via the Tools menu.";
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