/*
 * Process Hacker Extra Plugins -
 *   Live Dump Plugin
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
PH_CALLBACK_REGISTRATION MenuItemCallbackRegistration;
PH_CALLBACK_REGISTRATION MainMenuInitializingCallbackRegistration;

VOID MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = Parameter;

    switch (menuItem->Id)
    {
    case PLUGIN_MENU_ITEM:
        ShowLiveDumpConfigDialog(menuItem->OwnerWindow);
        break;
    }
}

VOID NTAPI MainMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;
    PPH_EMENU_ITEM liveDumpMenu;

    if (menuInfo->u.MainMenu.SubMenuIndex != PH_MENU_ITEM_LOCATION_TOOLS)
        return;

    PhInsertEMenuItem(menuInfo->Menu, liveDumpMenu = PhPluginCreateEMenuItem(PluginInstance, 0, PLUGIN_MENU_ITEM, L"Live kernel dump...", NULL), -1);

    if (WindowsVersion < WINDOWS_8_1 || !PhGetOwnTokenAttributes().Elevated)
    {
        liveDumpMenu->Flags |= PH_EMENU_DISABLED;
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

        info->DisplayName = L"Live Kernel Dump Plugin";
        info->Author = L"dmex";
        info->Description = L"Create live kernel dumps via the Tools menu > 'Live Kernel Dump' menu.";
        info->Url = L"";
        info->HasOptions = FALSE;

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
