/*
 * Process Hacker Extra Plugins -
 *   Force Shutdown Plugin
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

#include <phdk.h>
#include "resource.h"

#define REBOOT_MENU_ITEM 1
#define SHUTDOWN_MENU_ITEM 2

static PPH_PLUGIN PluginInstance;
static PH_CALLBACK_REGISTRATION MenuItemCallbackRegistration;
static PH_CALLBACK_REGISTRATION TrayMenuInitializingCallbackRegistration;
static PH_CALLBACK_REGISTRATION MainMenuInitializingCallbackRegistration;

VOID MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = Parameter;

    switch (menuItem->Id)
    {
    case REBOOT_MENU_ITEM:
        {
            NTSTATUS status;

            if (!NT_SUCCESS(status = NtShutdownSystem(ShutdownReboot)))
            {
                PhShowStatus(menuItem->OwnerWindow, L"Unable to reboot the machine", status, 0);
            }
        }
        break;
    case SHUTDOWN_MENU_ITEM:
        {
            NTSTATUS status;
           
            if (!NT_SUCCESS(status = NtShutdownSystem(ShutdownPowerOff)))
            {
                PhShowStatus(menuItem->OwnerWindow, L"Unable to shutdown the machine", status, 0);
            }
        }
        break;
    }
}

VOID TrayMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;
    PPH_EMENU_ITEM menu;

    menu = PhFindEMenuItem(menuInfo->Menu, 0, L"Computer", 0);

    if (!menu)
        return;

    PhInsertEMenuItem(menu, PhCreateEMenuItem(PH_EMENU_SEPARATOR, 0, NULL, NULL, NULL), -1);
    PhInsertEMenuItem(menu, PhPluginCreateEMenuItem(PluginInstance, 0, REBOOT_MENU_ITEM, L"F&orce reboot", NULL), -1);
    PhInsertEMenuItem(menu, PhPluginCreateEMenuItem(PluginInstance, 0, SHUTDOWN_MENU_ITEM, L"For&ce shut down", NULL), -1);
}

VOID MainMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;
    PPH_EMENU_ITEM menu;

    if (menuInfo->u.MainMenu.SubMenuIndex != 0) // 0 = Hacker menu
        return;

    menu = PhFindEMenuItem(menuInfo->Menu, 0, L"Computer", 0);

    if (!menu)
        return;
 
    PhInsertEMenuItem(menu, PhCreateEMenuItem(PH_EMENU_SEPARATOR, 0, NULL, NULL, NULL), -1);
    PhInsertEMenuItem(menu, PhPluginCreateEMenuItem(PluginInstance, 0, REBOOT_MENU_ITEM, L"F&orce reboot", NULL), -1);
    PhInsertEMenuItem(menu, PhPluginCreateEMenuItem(PluginInstance, 0, SHUTDOWN_MENU_ITEM, L"For&ce shut down", NULL), -1);
}

LOGICAL DllMain(
    _In_ HINSTANCE Instance,
    _In_ ULONG Reason,
    _Reserved_ PVOID Reserved
    )
{
    if (Reason == DLL_PROCESS_ATTACH)
    {
        PPH_PLUGIN_INFORMATION info;

        PluginInstance = PhRegisterPlugin(L"dmex.ForceShutdownPlugin", Instance, &info);

        if (!PluginInstance)
            return FALSE;

        info->DisplayName = L"Force Shutdown";
        info->Author = L"dmex";
        info->Description = L"Force a system shutdown or reboot via the Process Hacker tray menu > Computer > 'Force' shutdown or reboot menu items.";
        info->HasOptions = FALSE;

        PhRegisterCallback(
            PhGetPluginCallback(PluginInstance, PluginCallbackMenuItem),
            MenuItemCallback, 
            NULL, 
            &MenuItemCallbackRegistration
            );
        PhRegisterCallback(
            PhGetGeneralCallback(GeneralCallbackIconMenuInitializing),
            TrayMenuInitializingCallback,
            NULL, 
            &TrayMenuInitializingCallbackRegistration
            );
        PhRegisterCallback(
            PhGetGeneralCallback(GeneralCallbackMainMenuInitializing),
            MainMenuInitializingCallback,
            NULL,
            &MainMenuInitializingCallbackRegistration
            );
    }

    return TRUE;
}
