/*
 * Process Hacker Extra Plugins -
 *   Trusted Installer Plugin
 *
 * Copyright (C) 2016-2019 dmex
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
PH_CALLBACK_REGISTRATION PluginLoadCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginUnloadCallbackRegistration;
PH_CALLBACK_REGISTRATION MenuItemCallbackRegistration;
PH_CALLBACK_REGISTRATION MainMenuInitializingCallbackRegistration;

VOID NTAPI LoadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    NOTHING;
}

VOID NTAPI UnloadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    NOTHING;
}

VOID MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = Parameter;

    switch (menuItem->Id)
    {
    case RUNAS_MENU_ITEM:
        ShowRunAsDialog(menuItem->OwnerWindow);
        break;
    }
}

VOID NTAPI MainMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;
    PPH_EMENU_ITEM runAsMenuItem;
    ULONG indexOfMenuItem;

    if (menuInfo->u.MainMenu.SubMenuIndex != PH_MENU_ITEM_LOCATION_HACKER)
        return;

    if (!(runAsMenuItem = PhFindEMenuItem(menuInfo->Menu, PH_EMENU_FIND_STARTSWITH, L"Run as...", 0)))
        return;
 
    indexOfMenuItem = PhIndexOfEMenuItem(menuInfo->Menu, runAsMenuItem);
    runAsMenuItem = PhPluginCreateEMenuItem(PluginInstance, 0, RUNAS_MENU_ITEM, L"Run as &trusted installer...", NULL);
    PhInsertEMenuItem(menuInfo->Menu, runAsMenuItem, indexOfMenuItem + 1);

    if (!PhGetOwnTokenAttributes().Elevated)
    {
        runAsMenuItem->Flags |= PH_EMENU_DISABLED;
    }
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

        PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

        if (!PluginInstance)
            return FALSE;

        info->DisplayName = L"Trusted Installer";
        info->Author = L"dmex";
        info->Description = L"Run processes with Trusted Installer privileges via the Hacker menu > 'Run as trusted installer' menu.";
        info->Url = L"https://wj32.org/processhacker/forums/viewtopic.php?t=2407";
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
            PhGetPluginCallback(PluginInstance, PluginCallbackMenuItem),
            MenuItemCallback, 
            NULL, 
            &MenuItemCallbackRegistration
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
