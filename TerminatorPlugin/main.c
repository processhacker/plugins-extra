/*
 * Process Hacker Extra Plugins -
 *   Terminator Plugin
 *
 * Copyright (C) 2016-2017 dmex
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
static PH_CALLBACK_REGISTRATION MenuItemCallbackRegistration;
static PH_CALLBACK_REGISTRATION ProcessMenuInitializingCallbackRegistration;

VOID MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = Parameter;

    switch (menuItem->Id)
    {
    case TERMINATOR_MENU_ITEM:
        {
            PhShowProcessTerminatorDialog(menuItem->OwnerWindow, menuItem->Context);
        }
        break;
    }
}

VOID NTAPI ProcessMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;
    PPH_EMENU_ITEM miscMenuItem;
    PPH_EMENU_ITEM terminatorMenuItem;
    PPH_PROCESS_ITEM processItem;

    miscMenuItem = PhFindEMenuItem(menuInfo->Menu, 0, L"Miscellaneous", 0);

    if (!miscMenuItem)
        return;

    processItem = menuInfo->u.Process.NumberOfProcesses == 1 ? menuInfo->u.Process.Processes[0] : NULL;
    terminatorMenuItem = PhPluginCreateEMenuItem(PluginInstance, 0, TERMINATOR_MENU_ITEM, L"T&erminator", processItem);
    PhInsertEMenuItem(miscMenuItem, terminatorMenuItem, -1);

    if (!PhGetOwnTokenAttributes().Elevated || !processItem)
    {
        terminatorMenuItem->Flags |= PH_EMENU_DISABLED;
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

        info->DisplayName = L"Terminator";
        info->Description = L"Terminator";
        info->Author = L"wj32, dmex";

        PhRegisterCallback(
            PhGetPluginCallback(PluginInstance, PluginCallbackMenuItem),
            MenuItemCallback, 
            NULL, 
            &MenuItemCallbackRegistration
            );
        PhRegisterCallback(
            PhGetGeneralCallback(GeneralCallbackProcessMenuInitializing),
            ProcessMenuInitializingCallback,
            NULL,
            &ProcessMenuInitializingCallbackRegistration
            );
    }

    return TRUE;
}
