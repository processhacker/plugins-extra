/*
 * Process Hacker Extra Plugins -
 *   Taskbar Extensions
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
static PH_CALLBACK_REGISTRATION MainWindowShowingCallbackRegistration;
static PH_CALLBACK_REGISTRATION ProcessesUpdatedCallbackRegistration;

static TASKBAR_ICON TaskbarIconType = TASKBAR_ICON_NONE;
static ULONG ProcessesUpdatedCount = 0;
static UINT TaskbarButtonCreatedMsgId = 0;
static ITaskbarList3* TaskbarListClass = NULL;
static WNDPROC OldMainWindowProc = NULL;
static HICON BlackIcon = NULL;
static HIMAGELIST ButtonsImageList = NULL;
static THUMBBUTTON ButtonsArray[4] = { 0 }; // maximum 8

VOID NTAPI ProcessesUpdatedCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    HICON overlayIcon = NULL;
    ULONG taskbarIconType = TASKBAR_ICON_NONE;
    PH_PLUGIN_SYSTEM_STATISTICS statistics;

    if (ProcessesUpdatedCount != 3)
    {
        ProcessesUpdatedCount++;
        return;
    }
    
    PhPluginGetSystemStatistics(&statistics);

    // Check if we need to clear the icon
    taskbarIconType = PhGetIntegerSetting(SETTING_NAME_TASKBAR_ICON_TYPE);
    if (taskbarIconType != TaskbarIconType && taskbarIconType == TASKBAR_ICON_NONE)
    {
        // Clear the icon
        if (TaskbarListClass)
        {
            ITaskbarList3_SetOverlayIcon(TaskbarListClass, PhMainWndHandle, NULL, NULL);
        }
    }

    TaskbarIconType = taskbarIconType;

    switch (TaskbarIconType)
    {
    case TASKBAR_ICON_CPU_HISTORY:
        overlayIcon = PhUpdateIconCpuHistory(statistics);
        break;
    case TASKBAR_ICON_IO_HISTORY:
        overlayIcon = PhUpdateIconIoHistory(statistics);
        break;
    case TASKBAR_ICON_COMMIT_HISTORY:
        overlayIcon = PhUpdateIconCommitHistory(statistics);
        break;
    case TASKBAR_ICON_PHYSICAL_HISTORY:
        overlayIcon = PhUpdateIconPhysicalHistory(statistics);
        break;
    case TASKBAR_ICON_CPU_USAGE:
        overlayIcon = PhUpdateIconCpuUsage(statistics);
        break;
    }

    if (overlayIcon)
    {
        if (TaskbarListClass)
        {
            ITaskbarList3_SetOverlayIcon(TaskbarListClass, PhMainWndHandle, overlayIcon, NULL);
        }

        DestroyIcon(overlayIcon);
    }
}

LRESULT CALLBACK MainWndSubclassProc(
    _In_ HWND hWnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    if (uMsg == WM_DESTROY)
    {
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)OldMainWindowProc);
    }
    else if (uMsg == TaskbarButtonCreatedMsgId)
    {
        static PH_INITONCE initOnce = PH_INITONCE_INIT;

        if (PhBeginInitOnce(&initOnce))
        {
            BlackIcon = PhGetBlackIcon();
            ButtonsImageList = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_COLOR32 | ILC_MASK, 0, 0);
            ImageList_SetImageCount(ButtonsImageList, 4);
            PhSetImageListBitmap(ButtonsImageList, 0, PluginInstance->DllBase, MAKEINTRESOURCE(IDB_CHART_LINE_BMP));
            PhSetImageListBitmap(ButtonsImageList, 1, PluginInstance->DllBase, MAKEINTRESOURCE(IDB_FIND_BMP));
            PhSetImageListBitmap(ButtonsImageList, 2, PluginInstance->DllBase, MAKEINTRESOURCE(IDB_APPLICATION_BMP));
            PhSetImageListBitmap(ButtonsImageList, 3, PluginInstance->DllBase, MAKEINTRESOURCE(IDB_APPLICATION_GO_BMP));

            ButtonsArray[0].dwMask = THB_FLAGS | THB_BITMAP | THB_TOOLTIP;
            ButtonsArray[0].dwFlags = THBF_ENABLED | THBF_DISMISSONCLICK;
            ButtonsArray[0].iId = PHAPP_ID_VIEW_SYSTEMINFORMATION;
            ButtonsArray[0].iBitmap = 0;
            wcsncpy_s(ButtonsArray[0].szTip, ARRAYSIZE(ButtonsArray[0].szTip), L"System Information", _TRUNCATE);

            ButtonsArray[1].dwMask = THB_FLAGS | THB_BITMAP | THB_TOOLTIP;
            ButtonsArray[1].dwFlags = THBF_ENABLED | THBF_DISMISSONCLICK;
            ButtonsArray[1].iId = PHAPP_ID_HACKER_FINDHANDLESORDLLS;
            ButtonsArray[1].iBitmap = 1;
            wcsncpy_s(ButtonsArray[1].szTip, ARRAYSIZE(ButtonsArray[1].szTip), L"Find Handles or DLLs", _TRUNCATE);

            ButtonsArray[2].dwMask = THB_FLAGS | THB_BITMAP | THB_TOOLTIP;
            ButtonsArray[2].dwFlags = THBF_ENABLED | THBF_DISMISSONCLICK;
            ButtonsArray[2].iId = PHAPP_ID_HELP_LOG;
            ButtonsArray[2].iBitmap = 2;
            wcsncpy_s(ButtonsArray[2].szTip, ARRAYSIZE(ButtonsArray[2].szTip), L"Application Log", _TRUNCATE);

            ButtonsArray[3].dwMask = THB_FLAGS | THB_BITMAP | THB_TOOLTIP;
            ButtonsArray[3].dwFlags = THBF_ENABLED | THBF_DISMISSONCLICK;
            ButtonsArray[3].iId = PHAPP_ID_TOOLS_INSPECTEXECUTABLEFILE;
            ButtonsArray[3].iBitmap = 3;
            wcsncpy_s(ButtonsArray[3].szTip, ARRAYSIZE(ButtonsArray[3].szTip), L"Inspect Executable File", _TRUNCATE);

            PhEndInitOnce(&initOnce);
        }

        if (TaskbarListClass)
        {
            // Set the ThumbBar image list
            ITaskbarList3_ThumbBarSetImageList(TaskbarListClass, PhMainWndHandle, ButtonsImageList);
            // Set the ThumbBar buttons array
            ITaskbarList3_ThumbBarAddButtons(TaskbarListClass, PhMainWndHandle, ARRAYSIZE(ButtonsArray), ButtonsArray);

            if (TaskbarIconType != TASKBAR_ICON_NONE)
            {
                // Set the initial ThumbBar icon
                ITaskbarList3_SetOverlayIcon(TaskbarListClass, PhMainWndHandle, BlackIcon, NULL);
            }
        }
    }

    return CallWindowProc(OldMainWindowProc, hWnd, uMsg, wParam, lParam);
}

VOID NTAPI LoadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    // Update settings
    TaskbarIconType = PhGetIntegerSetting(SETTING_NAME_TASKBAR_ICON_TYPE);

    // Get the TaskbarButtonCreated Id
    TaskbarButtonCreatedMsgId = RegisterWindowMessage(L"TaskbarButtonCreated");

    // Allow the TaskbarButtonCreated message to pass through UIPI.
    ChangeWindowMessageFilterEx(PhMainWndHandle, TaskbarButtonCreatedMsgId, MSGFLT_ALLOW, NULL);
    // Allow WM_COMMAND messages to pass through UIPI (Required for ThumbBar buttons if elevated...TODO: Review security.)
    ChangeWindowMessageFilterEx(PhMainWndHandle, WM_COMMAND, MSGFLT_ALLOW, NULL);

    // Set the process-wide AppUserModelID
    //SetCurrentProcessExplicitAppUserModelID(L"ProcessHacker3");

    if (SUCCEEDED(PhGetClassObject(L"explorerframe.dll", &CLSID_TaskbarList, &IID_ITaskbarList3, &TaskbarListClass)))
    {
        if (!SUCCEEDED(ITaskbarList3_HrInit(TaskbarListClass)))
        {
            ITaskbarList3_Release(TaskbarListClass);
            TaskbarListClass = NULL;
        }
    }

    PhRegisterCallback(PhGetGeneralCallback(GeneralCallbackProcessProviderUpdatedEvent), ProcessesUpdatedCallback, NULL, &ProcessesUpdatedCallbackRegistration);
}

VOID NTAPI UnloadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PhUnregisterCallback(PhGetGeneralCallback(GeneralCallbackProcessProviderUpdatedEvent), &ProcessesUpdatedCallbackRegistration);

    if (TaskbarListClass)
    {
        ITaskbarList3_Release(TaskbarListClass);
        TaskbarListClass = NULL;
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
        L"Taskbar Extensions",
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_OPTIONS),
        OptionsDlgProc,
        NULL
        );
}

VOID NTAPI MainWindowShowingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    OldMainWindowProc = (WNDPROC)GetWindowLongPtr(PhMainWndHandle, GWLP_WNDPROC);
    SetWindowLongPtr(PhMainWndHandle, GWLP_WNDPROC, (LONG_PTR)MainWndSubclassProc);
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
                { IntegerSettingType, SETTING_NAME_TASKBAR_ICON_TYPE, L"4" }
            };

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->DisplayName = L"Taskbar Extensions";
            info->Author = L"dmex";
            info->Description = L"Plugin for Taskbar ThumbBar buttons and Overlay icon support.";
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
                PhGetGeneralCallback(GeneralCallbackOptionsWindowInitializing),
                ShowOptionsCallback,
                NULL,
                &PluginShowOptionsCallbackRegistration
                );
            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackMainWindowShowing),
                MainWindowShowingCallback,
                NULL,
                &MainWindowShowingCallbackRegistration
                );

            PhAddSettings(settings, RTL_NUMBER_OF(settings));
        }
        break;
    }

    return TRUE;
}
