/*
 * Process Hacker Extra Plugins -
 *   Environment Edit Plugin
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

PPH_PLUGIN PluginInstance;
static PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
static PH_CALLBACK_REGISTRATION ProcessMenuInitializingCallbackRegistration;

static VOID NTAPI ProcessMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;
    PPH_EMENU_ITEM miscMenuItem;
    PPH_EMENU_ITEM menuItem;
    PPH_PROCESS_ITEM processItem;

    miscMenuItem = PhFindEMenuItem(menuInfo->Menu, 0, L"Miscellaneous", 0);

    if (!miscMenuItem)
        return;

    processItem = menuInfo->u.Process.NumberOfProcesses == 1 ? menuInfo->u.Process.Processes[0] : NULL;
    PhInsertEMenuItem(miscMenuItem, menuItem = PhPluginCreateEMenuItem(PluginInstance, 0, ENV_MENUITEM, L"Environment Variables", processItem), 0);

    if (!processItem)
    {
        menuItem->Flags |= PH_EMENU_DISABLED;
    }
}

static VOID NTAPI MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = (PPH_PLUGIN_MENU_ITEM)Parameter;
    PPH_PROCESS_ITEM processItem = (PPH_PROCESS_ITEM)menuItem->Context;

    switch (menuItem->Id)
    {
    case ENV_MENUITEM:
        {
            NTSTATUS status;
            HANDLE processHandle;

            if (NT_SUCCESS(status = PhOpenProcess(
                &processHandle,
                PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, // PROCESS_ALL_ACCESS
                processItem->ProcessId
                )))
            {
                PENVIRONMENT_DIALOG_CONTEXT context;

                context = (PENVIRONMENT_DIALOG_CONTEXT)PhAllocate(sizeof(ENVIRONMENT_DIALOG_CONTEXT));
                memset(context, 0, sizeof(ENVIRONMENT_DIALOG_CONTEXT));
                
                context->ProcessHandle = processHandle;
                context->ProcessItem = processItem;

                PhReferenceObject(processItem);

                ShowEnvironmentDialog(context);
            }
            else
            {
                PhShowStatus(menuItem->OwnerWindow, NULL, status, 0);
            }
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
                { IntegerSettingType, SETTING_NAME_FIRST_RUN, L"1" },
                { IntegerPairSettingType, SETTING_NAME_WINDOW_POSITION, L"350,350" },
                { ScalableIntegerPairSettingType, SETTING_NAME_WINDOW_SIZE, L"@96|686,412" },
                { StringSettingType, SETTING_NAME_COLUMNS, L"" }
            };

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->DisplayName = L"Environment Editor Plugin";
            info->Author = L"dmex";
            info->Description = L"Plugin for editing the Environment of a process via the Process -> Right-click -> Miscellaneous -> Environment variables menu.";
            info->HasOptions = FALSE;

            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackMenuItem),
                MenuItemCallback,
                NULL,
                &PluginMenuItemCallbackRegistration
                );
            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackProcessMenuInitializing),
                ProcessMenuInitializingCallback,
                NULL,
                &ProcessMenuInitializingCallbackRegistration
                );

            PhAddSettings(settings, ARRAYSIZE(settings));
        }
        break;
    }

    return TRUE;
}