/*
 * Process Hacker Extra Plugins -
 *   Boot Entries Plugin
 *
 * Copyright (C) 2015 dmex
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
static PH_CALLBACK_REGISTRATION PluginLoadCallbackRegistration;
static PH_CALLBACK_REGISTRATION PluginUnloadCallbackRegistration;
static PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
static PH_CALLBACK_REGISTRATION MainMenuInitializingCallbackRegistration;
static PH_CALLBACK_REGISTRATION PluginShowOptionsCallbackRegistration;

static VOID NTAPI LoadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PVOID ntdll;

    if (ntdll = PhGetDllHandle(L"ntdll.dll"))
    {
        NtAddBootEntry_I = PhGetProcedureAddress(ntdll, "NtAddBootEntry", 0);
        NtDeleteBootEntry_I = PhGetProcedureAddress(ntdll, "NtDeleteBootEntry", 0);
        NtModifyBootEntry_I = PhGetProcedureAddress(ntdll, "NtModifyBootEntry", 0);
        NtEnumerateBootEntries_I = PhGetProcedureAddress(ntdll, "NtEnumerateBootEntries", 0);
        NtQueryBootEntryOrder_I = PhGetProcedureAddress(ntdll, "NtQueryBootEntryOrder", 0);
        NtSetBootEntryOrder_I = PhGetProcedureAddress(ntdll, "NtSetBootEntryOrder", 0);
        NtQueryBootOptions_I = PhGetProcedureAddress(ntdll, "NtQueryBootOptions", 0);
        NtSetBootOptions_I = PhGetProcedureAddress(ntdll, "NtSetBootOptions", 0);
        NtTranslateFilePath_I = PhGetProcedureAddress(ntdll, "NtTranslateFilePath", 0);
    }
}

static VOID NTAPI UnloadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    NOTHING;
}

static VOID NTAPI MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = (PPH_PLUGIN_MENU_ITEM)Parameter;

    switch (menuItem->Id)
    {
    case BOOT_ENTRIES_MENUITEM:
        {
            if (!PhElevated)
            {
                PhShowInformation(menuItem->OwnerWindow, L"This option requires elevation.");
                break;
            }

            DialogBox(
                PluginInstance->DllBase,
                MAKEINTRESOURCE(IDD_BOOT),
                NULL,
                BootEntriesDlgProc
                );
        }
        break;
    }
}

static VOID NTAPI MainMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;

    if (menuInfo->u.MainMenu.SubMenuIndex != PH_MENU_ITEM_LOCATION_TOOLS)
        return;

    PhInsertEMenuItem(menuInfo->Menu, PhPluginCreateEMenuItem(PluginInstance, 0, BOOT_ENTRIES_MENUITEM, L"Boot Entries", NULL), -1);
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
                { IntegerPairSettingType, SETTING_NAME_WINDOW_POSITION, L"100,100" },
                { ScalableIntegerPairSettingType, SETTING_NAME_WINDOW_SIZE, L"@96|490,340" },
                { StringSettingType, SETTING_NAME_LISTVIEW_COLUMNS, L"" }
            };

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->DisplayName = L"Boot Entries Plugin";
            info->Author = L"dmex";
            info->Description = L"Plugin for viewing native Boot Entries via the Tools menu.";
            info->HasOptions = FALSE;

            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackLoad),
                LoadCallback,
                NULL,
                &PluginLoadCallbackRegistration
                );
            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackUnload),
                UnloadCallback,
                NULL,
                &PluginUnloadCallbackRegistration
                );
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