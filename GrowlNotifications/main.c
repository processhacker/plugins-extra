/*
 * Process Hacker Extended Notifications -
 *   main program
 *
 * Copyright (C) 2010-2011 wj32
 * Copyright (C) 2017-2020 dmex
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

// NOTE: Growl support was retired on 25/11/2020 and was previously located here: (dmex)
// https://github.com/processhacker/processhacker/tree/master/plugins/ExtendedNotifications

#include <phdk.h>
#include <workqueue.h>
#include <settings.h>

#include "extnoti.h"
#include "resource.h"
#include "gntp-send/growl.h"

VOID NTAPI LoadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI ShowOptionsCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI NotifyEventCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID RegisterGrowl(
    _In_ BOOLEAN Force
    );

VOID NotifyGrowl(
    _In_ PPH_PLUGIN_NOTIFY_EVENT NotifyEvent
    );

NTSTATUS NTAPI RegisterGrowlCallback(
    _In_ PVOID Parameter
    );

INT_PTR CALLBACK GrowlDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

PPH_PLUGIN PluginInstance;
PH_CALLBACK_REGISTRATION PluginLoadCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginShowOptionsCallbackRegistration;
PH_CALLBACK_REGISTRATION NotifyEventCallbackRegistration;

PPH_LIST ProcessFilterList;
PPH_LIST ServiceFilterList;

PSTR GrowlNotifications[] =
{
    "Process Created",
    "Process Terminated",
    "Service Created",
    "Service Deleted",
    "Service Started",
    "Service Stopped"
};

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

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->DisplayName = L"Growl Notifications";
            info->Author = L"wj32";
            info->Description = L"Filters notifications.";
            info->Url = L"https://wj32.org/processhacker/forums/viewtopic.php?t=1112";

            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackLoad),
                LoadCallback,
                NULL,
                &PluginLoadCallbackRegistration
                );
            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackOptionsWindowInitializing),
                ShowOptionsCallback,
                NULL,
                &PluginShowOptionsCallbackRegistration
                );

            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackNotifyEvent),
                NotifyEventCallback,
                NULL,
                &NotifyEventCallbackRegistration
                );

            {
                static PH_SETTING_CREATE settings[] =
                {
                    { IntegerSettingType, SETTING_NAME_ENABLE_GROWL, L"0" },
                };

                PhAddSettings(settings, sizeof(settings) / sizeof(PH_SETTING_CREATE));
            }

            ProcessFilterList = PhCreateList(10);
            ServiceFilterList = PhCreateList(10);
        }
        break;
    }

    return TRUE;
}

VOID NTAPI LoadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    if (PhGetIntegerSetting(SETTING_NAME_ENABLE_GROWL))
    {
        PhQueueItemWorkQueue(PhGetGlobalWorkQueue(), RegisterGrowlCallback, NULL);
    }
}

VOID NTAPI ShowOptionsCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_OPTIONS_POINTERS optionsEntry = (PPH_PLUGIN_OPTIONS_POINTERS)Parameter;

    if (!optionsEntry)
        return;

    optionsEntry->CreateSection(
        L"Notifications - Growl",
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_GROWL),
        GrowlDlgProc,
        NULL
        );
}

VOID NTAPI NotifyEventCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_NOTIFY_EVENT notifyEvent = Parameter;

    if (!notifyEvent)
        return;

    if (PhGetIntegerSetting(SETTING_NAME_ENABLE_GROWL))
        NotifyGrowl(notifyEvent);
}

VOID RegisterGrowl(
    _In_ BOOLEAN Force
    )
{
    static BOOLEAN registered = FALSE;

    if (!Force && registered)
        return;

    growl_tcp_register("127.0.0.1", "Process Hacker", GrowlNotifications, sizeof(GrowlNotifications) / sizeof(PSTR), NULL, NULL);

    registered = TRUE;
}

VOID NotifyGrowl(
    _In_ PPH_PLUGIN_NOTIFY_EVENT NotifyEvent
    )
{
    PSTR notification;
    PPH_STRING title;
    PPH_BYTES titleUtf8;
    PPH_STRING message;
    PPH_BYTES messageUtf8;
    PPH_PROCESS_ITEM processItem;
    PPH_SERVICE_ITEM serviceItem;
    PPH_PROCESS_ITEM parentProcessItem;

    if (NotifyEvent->Handled)
        return;

    switch (NotifyEvent->Type)
    {
    case PH_NOTIFY_PROCESS_CREATE:
        processItem = NotifyEvent->Parameter;
        notification = GrowlNotifications[0];
        title = processItem->ProcessName;

        parentProcessItem = PhReferenceProcessItemForParent(processItem);

        message = PhaFormatString(
            L"The process %s (%lu) was started by %s.",
            processItem->ProcessName->Buffer,
            HandleToUlong(processItem->ProcessId),
            parentProcessItem ? parentProcessItem->ProcessName->Buffer : L"an unknown process"
            );

        if (parentProcessItem)
            PhDereferenceObject(parentProcessItem);

        break;
    case PH_NOTIFY_PROCESS_DELETE:
        processItem = NotifyEvent->Parameter;
        notification = GrowlNotifications[1];
        title = processItem->ProcessName;

        message = PhaFormatString(L"The process %s (%lu) was terminated.",
            processItem->ProcessName->Buffer,
            HandleToUlong(processItem->ProcessId)
            );

        break;
    case PH_NOTIFY_SERVICE_CREATE:
        serviceItem = NotifyEvent->Parameter;
        notification = GrowlNotifications[2];
        title = serviceItem->DisplayName;

        message = PhaFormatString(L"The service %s (%s) has been created.",
            serviceItem->Name->Buffer,
            serviceItem->DisplayName->Buffer
            );

        break;
    case PH_NOTIFY_SERVICE_DELETE:
        serviceItem = NotifyEvent->Parameter;
        notification = GrowlNotifications[3];
        title = serviceItem->DisplayName;

        message = PhaFormatString(L"The service %s (%s) has been deleted.",
            serviceItem->Name->Buffer,
            serviceItem->DisplayName->Buffer
            );

        break;
    case PH_NOTIFY_SERVICE_START:
        serviceItem = NotifyEvent->Parameter;
        notification = GrowlNotifications[4];
        title = serviceItem->DisplayName;

        message = PhaFormatString(L"The service %s (%s) has been started.",
            serviceItem->Name->Buffer,
            serviceItem->DisplayName->Buffer
            );

        break;
    case PH_NOTIFY_SERVICE_STOP:
        serviceItem = NotifyEvent->Parameter;
        notification = GrowlNotifications[5];
        title = serviceItem->DisplayName;

        message = PhaFormatString(L"The service %s (%s) has been stopped.",
            serviceItem->Name->Buffer,
            serviceItem->DisplayName->Buffer
            );

        break;
    default:
        return;
    }

    titleUtf8 = PH_AUTO(PhConvertUtf16ToUtf8Ex(title->Buffer, title->Length));
    messageUtf8 = PH_AUTO(PhConvertUtf16ToUtf8Ex(message->Buffer, message->Length));

    RegisterGrowl(TRUE);

    if (growl_tcp_notify("127.0.0.1", "Process Hacker", notification, titleUtf8->Buffer, messageUtf8->Buffer, NULL, NULL, NULL) == 0)
        NotifyEvent->Handled = TRUE;
}

NTSTATUS NTAPI RegisterGrowlCallback(
    _In_ PVOID Parameter
    )
{
    RegisterGrowl(FALSE);

    return STATUS_SUCCESS;
}

INT_PTR CALLBACK GrowlDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    static PH_LAYOUT_MANAGER LayoutManager;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PhSetDialogItemText(hwndDlg, IDC_LICENSE, PH_AUTO_T(PH_STRING, PhConvertUtf8ToUtf16(gntp_send_license_text))->Buffer);

            Button_SetCheck(GetDlgItem(hwndDlg, IDC_ENABLEGROWL), PhGetIntegerSetting(SETTING_NAME_ENABLE_GROWL) ? BST_CHECKED : BST_UNCHECKED);

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_LICENSE), NULL, PH_ANCHOR_ALL);
        }
        break;
    case WM_DESTROY:
        {
            PhSetIntegerSetting(SETTING_NAME_ENABLE_GROWL, Button_GetCheck(GetDlgItem(hwndDlg, IDC_ENABLEGROWL)) == BST_CHECKED);

            if (PhGetIntegerSetting(SETTING_NAME_ENABLE_GROWL))
                RegisterGrowl(FALSE);

            PhDeleteLayoutManager(&LayoutManager);
        }
        break;
    case WM_SIZE:
        {
            PhLayoutManagerLayout(&LayoutManager);
        }
        break;
    }

    return FALSE;
}
