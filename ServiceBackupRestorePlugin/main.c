/*
 * Process Hacker Extra Plugins -
 *   Service Backup and Restore Plugin
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
static PH_CALLBACK_REGISTRATION PluginShowOptionsCallbackRegistration;
static PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
static PH_CALLBACK_REGISTRATION ServiceMenuInitializingCallbackRegistration;

VOID NTAPI LoadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    HANDLE tokenHandle;

    if (NT_SUCCESS(NtOpenProcessToken(
        NtCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES,
        &tokenHandle
        )))
    {
        // Enable the backup and restore privileges
        PhSetTokenPrivilege(tokenHandle, SE_BACKUP_NAME, NULL, SE_PRIVILEGE_ENABLED);
        PhSetTokenPrivilege(tokenHandle, SE_RESTORE_NAME, NULL, SE_PRIVILEGE_ENABLED);
        NtClose(tokenHandle);
    }
}

VOID NTAPI UnloadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    NOTHING;
}

VOID NTAPI ShowOptionsCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    NOTHING;
}

VOID NTAPI MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = Parameter;

    switch (menuItem->Id)
    {
    case 1001:
        PhBackupService(menuItem->OwnerWindow, menuItem->Context);
        break;
    case 1002:
        {
            if (PhGetIntegerSetting(L"EnableWarnings"))
            {
                if (MessageBox(
                    menuItem->OwnerWindow,
                    L"Restoring the incorrect file will permanently destroy this service.\r\n\r\nAre you sure you want to continue?",
                    L"WARNING",
                    MB_ICONEXCLAMATION | MB_YESNO
                    ) != IDYES)
                {
                    break;
                }
            }

            PhRestoreService(menuItem->OwnerWindow, menuItem->Context);
        }
        break;
    }
}

VOID NTAPI ServiceMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;
    PPH_EMENU_ITEM menuItemBackup;
    PPH_EMENU_ITEM menuItemRestore;
    ULONG indexOfMenuItem;

    if (menuInfo->u.Service.NumberOfServices != 1)
        return;

    menuItemBackup = PhFindEMenuItem(menuInfo->Menu, PH_EMENU_FIND_STARTSWITH, L"Open Key", 0);

    if (menuItemBackup)
        indexOfMenuItem = PhIndexOfEMenuItem(menuInfo->Menu, menuItemBackup);
    else
        indexOfMenuItem = -1;

    menuItemBackup = PhPluginCreateEMenuItem(PluginInstance, 0, 1001, L"Back&up...", menuInfo->u.Service.Services[0]);
    menuItemRestore = PhPluginCreateEMenuItem(PluginInstance, 0, 1002, L"R&estore...", menuInfo->u.Service.Services[0]);
    PhInsertEMenuItem(menuInfo->Menu, menuItemBackup, indexOfMenuItem);
    PhInsertEMenuItem(menuInfo->Menu, menuItemRestore, indexOfMenuItem + 1);
    PhInsertEMenuItem(menuInfo->Menu, PhCreateEMenuSeparator(), indexOfMenuItem + 2);
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
                { IntegerSettingType, SETTING_NAME_REG_FORMAT, L"2" }
            };

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->DisplayName = L"Service Backup and Restore";
            info->Author = L"dmex";
            info->Description = L"Plugin for backing up and restoring individual services.";
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
                PhGetPluginCallback(PluginInstance, PluginCallbackShowOptions),
                ShowOptionsCallback,
                NULL,
                &PluginShowOptionsCallbackRegistration
                );

            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackMenuItem),
                MenuItemCallback,
                NULL,
                &PluginMenuItemCallbackRegistration
                );

            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackServiceMenuInitializing),
                ServiceMenuInitializingCallback,
                NULL,
                &ServiceMenuInitializingCallbackRegistration
                );

            PhAddSettings(settings, ARRAYSIZE(settings));
        }
        break;
    }

    return TRUE;
}