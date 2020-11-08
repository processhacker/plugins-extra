/*
 * Process Hacker Extra Plugins -
 *   Graph Explorer Plugin
 *
 * Copyright (C) 2020 dmex
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
PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
PH_CALLBACK_REGISTRATION MainMenuInitializingCallbackRegistration;
PH_CALLBACK_REGISTRATION ProcessAddedCallbackRegistration;
PH_CALLBACK_REGISTRATION ProcessRemovedCallbackRegistration;
PH_CALLBACK_REGISTRATION ProcessesUpdatedCallbackRegistration;

VOID NTAPI ProcessesAddedCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PROCESS_ITEM processItem = Parameter;

    if (processItem && WindowContext && WindowContext->NodeList)
    {
        PhReferenceObject(processItem);

        AddProcessNode(WindowContext, processItem);
    }
}

VOID NTAPI ProcessesRemovedCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PROCESS_ITEM processItem = Parameter;

    if (WindowContext && WindowContext->NodeList)
    {
        for (ULONG i = 0; i < WindowContext->NodeList->Count; i++)
        {
            PPH_GRAPH_TREE_ROOT_NODE node = WindowContext->NodeList->Items[i];

            if (node->ProcessItem && (node->ProcessItem == processItem))
            {
                RemoveGraphNode(WindowContext, node);

                PhReferenceObject(node->ProcessItem);
            }
        }
    }
}

//LIST_ENTRY ProcessListHead = { &ProcessListHead, &ProcessListHead };
//PH_QUEUED_LOCK ProcessListLock = PH_QUEUED_LOCK_INIT;
//
//VOID ProcessItemCreateCallback(
//    _In_ PVOID Object,
//    _In_ PH_EM_OBJECT_TYPE ObjectType,
//    _In_ PVOID Extension
//    )
//{
//    PPH_PROCESS_ITEM processItem = Object;
//    PPROCESS_EXTENSION extension = Extension;
//
//    memset(extension, 0, sizeof(PROCESS_EXTENSION));
//    extension->ProcessItem = processItem;
//
//    PhAcquireQueuedLockExclusive(&ProcessListLock);
//    InsertTailList(&ProcessListHead, &extension->ListEntry);
//    PhReleaseQueuedLockExclusive(&ProcessListLock);
//
//    //PPROCESS_EXTENSION extension = PhPluginGetObjectExtension(PluginInstance, processItem, EmProcessItemType);
//    //InsertTailList(&ProcessListHead, &extension->ListEntry);
//
//    if (WindowContext && WindowContext->NodeList)
//    {
//        AddNode(WindowContext, processItem);
//    }
//}
//
//VOID ProcessItemDeleteCallback(
//    _In_ PVOID Object,
//    _In_ PH_EM_OBJECT_TYPE ObjectType,
//    _In_ PVOID Extension
//    )
//{
//    PPH_PROCESS_ITEM processItem = Object;
//    PPROCESS_EXTENSION extension = Extension;
//
//    if (WindowContext && WindowContext->NodeList)
//    {
//        for (ULONG i = 0; i < WindowContext->NodeList->Count; i++)
//        {
//            PPH_TREE_ROOT_NODE node = WindowContext->NodeList->Items[i];
//
//            if (node->ProcessItem == processItem)
//            {
//                RemoveNode(WindowContext, node);
//            }
//        }
//    }
//
//    PhClearReference(&extension->Comment);
//    PhAcquireQueuedLockExclusive(&ProcessListLock);
//    RemoveEntryList(&extension->ListEntry);
//    PhReleaseQueuedLockExclusive(&ProcessListLock);
//
//    //PPROCESS_EXTENSION extension = PhPluginGetObjectExtension(PluginInstance, processItem, EmProcessItemType);
//    //RemoveEntryList(&extension->ListEntry);
//}

VOID NTAPI ProcessesUpdatedCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    NOTHING;
}

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

VOID NTAPI MainMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;

    if (!menuInfo || menuInfo->u.MainMenu.SubMenuIndex != PH_MENU_ITEM_LOCATION_TOOLS)
        return;

    PhInsertEMenuItem(menuInfo->Menu, PhPluginCreateEMenuItem(PluginInstance, 0, 1, L"Graph Explorer", NULL), -1);
}

VOID NTAPI MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = (PPH_PLUGIN_MENU_ITEM)Parameter;

    switch (menuItem->Id)
    {
    case 1:
        ShowGraphDialog();
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
                { IntegerPairSettingType, SETTING_NAME_WINDOW_POSITION, L"0,0" },
                { ScalableIntegerPairSettingType, SETTING_NAME_WINDOW_SIZE, L"@96|800,600" }
            };

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->DisplayName = L"GraphTestExplorer";
            info->Author = L"dmex";
            info->Description = L"Plugin for testing many graphs.";
            info->HasOptions = TRUE;

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

            PhRegisterCallback(PhGetGeneralCallback(GeneralCallbackProcessProviderAddedEvent), ProcessesAddedCallback, NULL, &ProcessAddedCallbackRegistration);
            PhRegisterCallback(PhGetGeneralCallback(GeneralCallbackProcessProviderRemovedEvent), ProcessesRemovedCallback, NULL, &ProcessRemovedCallbackRegistration);
            PhRegisterCallback(PhGetGeneralCallback(GeneralCallbackProcessProviderUpdatedEvent), ProcessesUpdatedCallback, NULL, &ProcessesUpdatedCallbackRegistration);

            PhAddSettings(settings, ARRAYSIZE(settings));
        }
        break;
    }

    return TRUE;
}
