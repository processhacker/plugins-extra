/*
 * Process Hacker Sandboxie Support -
 *   main program
 *
 * Copyright (C) 2010-2011 wj32
 * Copyright (C) 2018 dmex
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
#include <settings.h>
#include <windowsx.h>
#include "resource.h"
#include "sbiedll.h"

#define PhCsColorSandboxed RGB(0x33, 0x33, 0x00)
#define PhCsColorSandboxedSuspended RGB(0x45, 0x45, 0x37)

typedef enum _TOOLS_MENU_ITEMS
{
    ID_SANDBOXED_TERMINATE = 1,
    ID_SANDBOXED_SUSPEND,
    ID_SANDBOXED_RESUME
} TOOLS_MENU_ITEMS;

typedef struct _BOX_INFO
{
    WCHAR BoxName[34];
    PH_STRINGREF IpcRoot;
    WCHAR IpcRootBuffer[256];
} BOX_INFO, *PBOX_INFO;

typedef struct _BOXED_PROCESS
{
    HANDLE ProcessId;
    WCHAR BoxName[34];
} BOXED_PROCESS, *PBOXED_PROCESS;

VOID NTAPI LoadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI ShowOptionsCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI MainMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI ProcessesUpdatedCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI GetProcessHighlightingColorCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI GetProcessTooltipTextCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI RefreshSandboxieInfo(
    _In_opt_ PVOID Context,
    _In_ BOOLEAN TimerOrWaitFired
    );

INT_PTR CALLBACK OptionsDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

PPH_PLUGIN PluginInstance;
PH_CALLBACK_REGISTRATION PluginLoadCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginShowOptionsCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
PH_CALLBACK_REGISTRATION MainMenuInitializingCallbackRegistration;
PH_CALLBACK_REGISTRATION ProcessesUpdatedCallbackRegistration;
PH_CALLBACK_REGISTRATION GetProcessHighlightingColorCallbackRegistration;
PH_CALLBACK_REGISTRATION GetProcessTooltipTextCallbackRegistration;

P_SbieApi_QueryBoxPath SbieApi_QueryBoxPath = NULL;
P_SbieApi_EnumBoxes SbieApi_EnumBoxes = NULL;
P_SbieApi_EnumProcessEx SbieApi_EnumProcessEx = NULL;
P_SbieDll_KillAll SbieDll_KillAll = NULL;

PPH_HASHTABLE BoxedProcessesHashtable = NULL;
PH_QUEUED_LOCK BoxedProcessesLock = PH_QUEUED_LOCK_INIT;
BOOLEAN BoxedProcessesUpdated = FALSE;

BOX_INFO BoxInfo[16] = { 0 };
ULONG BoxInfoCount = 0;

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
            PH_SETTING_CREATE settings[] =
            {
                { StringSettingType, SETTING_NAME_SBIE_DLL_PATH, L"%ProgramFiles%\\Sandboxie\\SbieDll.dll" }
            };
            PPH_PLUGIN_INFORMATION info;

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->DisplayName = L"Sandboxie Support";
            info->Author = L"wj32";
            info->Description = L"Provides functionality for sandboxed processes.";
            info->Url = L"https://wj32.org/processhacker/forums/viewtopic.php?t=1115";
            info->HasOptions = TRUE;

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
                PhGetPluginCallback(PluginInstance, PluginCallbackMenuItem),
                MenuItemCallback,
                NULL,
                &PluginMenuItemCallbackRegistration
                );

            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackMainMenuInitializing),
                MainMenuInitializingCallback,
                NULL,
                &MainMenuInitializingCallbackRegistration
                );
            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackProcessesUpdated),
                ProcessesUpdatedCallback,
                NULL,
                &ProcessesUpdatedCallbackRegistration
                );
            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackGetProcessHighlightingColor),
                GetProcessHighlightingColorCallback,
                NULL,
                &GetProcessHighlightingColorCallbackRegistration
                );
            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackGetProcessTooltipText),
                GetProcessTooltipTextCallback,
                NULL,
                &GetProcessTooltipTextCallbackRegistration
                );
      
            PhAddSettings(settings, RTL_NUMBER_OF(settings));
        }
        break;
    }

    return TRUE;
}

BOOLEAN NTAPI BoxedProcessesEqualFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
    )
{
    return ((PBOXED_PROCESS)Entry1)->ProcessId == ((PBOXED_PROCESS)Entry2)->ProcessId;
}

ULONG NTAPI BoxedProcessesHashFunction(
    _In_ PVOID Entry
    )
{
    return HandleToUlong(((PBOXED_PROCESS)Entry)->ProcessId) / 4;
}

VOID NTAPI LoadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_STRING sbieDllPath;
    PVOID module;
    HANDLE timerQueueHandle;
    HANDLE timerHandle;

    BoxedProcessesHashtable = PhCreateHashtable(
        sizeof(BOXED_PROCESS),
        BoxedProcessesEqualFunction,
        BoxedProcessesHashFunction,
        32
        );

    sbieDllPath = PhaGetStringSetting(SETTING_NAME_SBIE_DLL_PATH);
    sbieDllPath = PH_AUTO(PhExpandEnvironmentStrings(&sbieDllPath->sr));
    module = LoadLibrary(sbieDllPath->Buffer);

    if (module)
    {
        SbieApi_QueryBoxPath = PhGetProcedureAddress(module, SbieApi_QueryBoxPath_Name, 0);
        SbieApi_EnumBoxes = PhGetProcedureAddress(module, SbieApi_EnumBoxes_Name, 0);
        SbieApi_EnumProcessEx = PhGetProcedureAddress(module, SbieApi_EnumProcessEx_Name, 0);
        SbieDll_KillAll = PhGetProcedureAddress(module, SbieDll_KillAll_Name, 0);
    }

    if (NT_SUCCESS(RtlCreateTimerQueue(&timerQueueHandle)))
    {
        RtlCreateTimer(timerQueueHandle, &timerHandle, RefreshSandboxieInfo, NULL, 0, 4000, 0);
    }
}

VOID NTAPI ShowOptionsCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_OPTIONS_POINTERS optionsEntry = (PPH_PLUGIN_OPTIONS_POINTERS)Parameter;

    optionsEntry->CreateSection(
        L"SbieSupport",
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_OPTIONS),
        OptionsDlgProc,
        (HWND)Parameter
        );
}

VOID DoSandboxedMenuAction(
    _In_ ULONG ActionId
    )
{
    if (ActionId == ID_SANDBOXED_TERMINATE)
    {
        if (!PhShowConfirmMessage(
            PhMainWndHandle,
            L"terminate",
            L"all sandboxed processes",
            NULL,
            FALSE
            ))
            return;
    }

    PBOXED_PROCESS boxedProcess;
    ULONG enumerationKey = 0;

    // Make sure we have an up-to-date list.
    RefreshSandboxieInfo(NULL, FALSE);

    PhAcquireQueuedLockShared(&BoxedProcessesLock);

    while (PhEnumHashtable(BoxedProcessesHashtable, &boxedProcess, &enumerationKey))
    {
        HANDLE processHandle;

        if (ActionId == ID_SANDBOXED_TERMINATE)
        {
            if (NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_TERMINATE, boxedProcess->ProcessId)))
            {
                PhTerminateProcess(processHandle, STATUS_SUCCESS);
                NtClose(processHandle);
            }
        }
        else
        {
            if (NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_SUSPEND_RESUME, boxedProcess->ProcessId)))
            {
                if (ActionId == ID_SANDBOXED_SUSPEND)
                {
                    NtSuspendProcess(processHandle);
                }
                else if (ActionId == ID_SANDBOXED_RESUME)
                {
                    NtResumeProcess(processHandle);
                }
                NtClose(processHandle);
            }
        }
    }

    PhReleaseQueuedLockShared(&BoxedProcessesLock);

}

VOID NTAPI MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = Parameter;

    switch (menuItem->Id)
    {
    case ID_SANDBOXED_TERMINATE:
    case ID_SANDBOXED_SUSPEND:
    case ID_SANDBOXED_RESUME:
        {
            DoSandboxedMenuAction(menuItem->Id);
        }
        break;
    }
}

VOID NTAPI MainMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;

    if (!SbieDll_KillAll)
        return;
    if (menuInfo->u.MainMenu.SubMenuIndex != PH_MENU_ITEM_LOCATION_TOOLS)
        return;

    PhInsertEMenuItem(menuInfo->Menu, PhCreateEMenuSeparator(), -1);
    PhInsertEMenuItem(menuInfo->Menu, PhPluginCreateEMenuItem(PluginInstance, 0, ID_SANDBOXED_TERMINATE, L"T&erminate sandboxed processes", NULL), -1);
    PhInsertEMenuItem(menuInfo->Menu, PhPluginCreateEMenuItem(PluginInstance, 0, ID_SANDBOXED_SUSPEND, L"Sus&pend sandboxed processes", NULL), -1);
    PhInsertEMenuItem(menuInfo->Menu, PhPluginCreateEMenuItem(PluginInstance, 0, ID_SANDBOXED_RESUME, L"Res&ume sandboxed processes", NULL), -1);
}

VOID NTAPI ProcessesUpdatedCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PBOXED_PROCESS boxedProcess;
    ULONG enumerationKey = 0;

    if (BoxedProcessesUpdated)
    {
        // Invalidate the nodes of boxed processes (so they use the correct highlighting color).

        PhAcquireQueuedLockShared(&BoxedProcessesLock);

        if (BoxedProcessesUpdated)
        {
            while (PhEnumHashtable(BoxedProcessesHashtable, &boxedProcess, &enumerationKey))
            {
                PPH_PROCESS_NODE processNode;

                if (processNode = PhFindProcessNode(boxedProcess->ProcessId))
                    PhUpdateProcessNode(processNode);
            }

            BoxedProcessesUpdated = FALSE;
        }

        PhReleaseQueuedLockShared(&BoxedProcessesLock);
    }
}

VOID NTAPI GetProcessHighlightingColorCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_GET_HIGHLIGHTING_COLOR getHighlightingColor = Parameter;
    BOXED_PROCESS lookupBoxedProcess;
    PBOXED_PROCESS boxedProcess;

    PhAcquireQueuedLockShared(&BoxedProcessesLock);

    lookupBoxedProcess.ProcessId = ((PPH_PROCESS_ITEM)getHighlightingColor->Parameter)->ProcessId;

    if (boxedProcess = PhFindEntryHashtable(BoxedProcessesHashtable, &lookupBoxedProcess))
    {
        if (((PPH_PROCESS_ITEM)getHighlightingColor->Parameter)->IsSuspended)
        {
            getHighlightingColor->BackColor = PhCsColorSandboxedSuspended;
        }
        else
        {
            getHighlightingColor->BackColor = PhCsColorSandboxed;
        }
        getHighlightingColor->Cache = TRUE;
        getHighlightingColor->Handled = TRUE;
    }

    PhReleaseQueuedLockShared(&BoxedProcessesLock);
}

VOID NTAPI GetProcessTooltipTextCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_GET_TOOLTIP_TEXT getTooltipText = Parameter;
    BOXED_PROCESS lookupBoxedProcess;
    PBOXED_PROCESS boxedProcess;

    PhAcquireQueuedLockShared(&BoxedProcessesLock);

    lookupBoxedProcess.ProcessId = ((PPH_PROCESS_ITEM)getTooltipText->Parameter)->ProcessId;

    if (boxedProcess = PhFindEntryHashtable(BoxedProcessesHashtable, &lookupBoxedProcess))
    {
        PhAppendFormatStringBuilder(getTooltipText->StringBuilder, L"Sandboxie:\n    Box name: %s\n", boxedProcess->BoxName);
    }

    PhReleaseQueuedLockShared(&BoxedProcessesLock);
}

VOID NTAPI RefreshSandboxieInfo(
    _In_opt_ PVOID Context,
    _In_ BOOLEAN TimerOrWaitFired
    )
{
    LONG index;
    WCHAR boxName[34];
    ULONG pids[512];
    PBOX_INFO boxInfo;

    if (!SbieApi_QueryBoxPath || !SbieApi_EnumBoxes || !SbieApi_EnumProcessEx)
        return;

    PhAcquireQueuedLockExclusive(&BoxedProcessesLock);

    PhClearHashtable(BoxedProcessesHashtable);

    BoxInfoCount = 0;

    index = -1;

    while ((index = SbieApi_EnumBoxes(index, boxName)) != -1)
    {
        if (SbieApi_EnumProcessEx(boxName, TRUE, 0, pids) == 0)
        {
            ULONG count;
            PULONG pid;

            count = pids[0];
            pid = &pids[1];

            while (count != 0)
            {
                BOXED_PROCESS boxedProcess;

                boxedProcess.ProcessId = UlongToHandle(*pid);
                memcpy(boxedProcess.BoxName, boxName, sizeof(boxName));

                PhAddEntryHashtable(BoxedProcessesHashtable, &boxedProcess);

                count--;
                pid++;
            }
        }

        if (BoxInfoCount < 16)
        {
            ULONG filePathLength = 0;
            ULONG keyPathLength = 0;
            ULONG ipcPathLength = 0;

            boxInfo = &BoxInfo[BoxInfoCount++];
            memcpy(boxInfo->BoxName, boxName, sizeof(boxName));

            SbieApi_QueryBoxPath(
                boxName,
                NULL,
                NULL,
                NULL,
                &filePathLength,
                &keyPathLength,
                &ipcPathLength
                );

            if (ipcPathLength < sizeof(boxInfo->IpcRootBuffer))
            {
                boxInfo->IpcRootBuffer[0] = 0;
                SbieApi_QueryBoxPath(
                    boxName,
                    NULL,
                    NULL,
                    boxInfo->IpcRootBuffer,
                    NULL,
                    NULL,
                    &ipcPathLength
                    );

                if (boxInfo->IpcRootBuffer[0] != 0)
                {
                    PhInitializeStringRef(&boxInfo->IpcRoot, boxInfo->IpcRootBuffer);
                }
                else
                {
                    BoxInfoCount--;
                }
            }
            else
            {
                BoxInfoCount--;
            }
        }
    }

    BoxedProcessesUpdated = TRUE;

    PhReleaseQueuedLockExclusive(&BoxedProcessesLock);
}

VOID NTAPI ReloadSandboxieLibrary(
    VOID
    )
{
    PPH_STRING sbieDllPath;
    PVOID module;

    sbieDllPath = PhaGetStringSetting(SETTING_NAME_SBIE_DLL_PATH);
    sbieDllPath = PH_AUTO(PhExpandEnvironmentStrings(&sbieDllPath->sr));

    if (PhGetDllHandle(sbieDllPath->Buffer))
        return;
    if (!(module = LoadLibrary(sbieDllPath->Buffer)))
        return;

    PhAcquireQueuedLockExclusive(&BoxedProcessesLock);

    SbieApi_QueryBoxPath = PhGetProcedureAddress(module, SbieApi_QueryBoxPath_Name, 0);
    SbieApi_EnumBoxes = PhGetProcedureAddress(module, SbieApi_EnumBoxes_Name, 0);
    SbieApi_EnumProcessEx = PhGetProcedureAddress(module, SbieApi_EnumProcessEx_Name, 0);
    SbieDll_KillAll = PhGetProcedureAddress(module, SbieDll_KillAll_Name, 0);

    PhReleaseQueuedLockExclusive(&BoxedProcessesLock);
}

INT_PTR CALLBACK OptionsDlgProc(
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
            PhSetDialogItemText(hwndDlg, IDC_SBIEDLLPATH, PhaGetStringSetting(SETTING_NAME_SBIE_DLL_PATH)->Buffer);

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_SBIEDLLPATH), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_BROWSE), NULL, PH_ANCHOR_TOP | PH_ANCHOR_RIGHT);
        }
        break;
    case WM_DESTROY:
        {
            PhSetStringSetting2(SETTING_NAME_SBIE_DLL_PATH, &PhaGetDlgItemText(hwndDlg, IDC_SBIEDLLPATH)->sr);

            ReloadSandboxieLibrary();

            PhDeleteLayoutManager(&LayoutManager);
        }
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDC_BROWSE:
                {
                    static PH_FILETYPE_FILTER filters[] =
                    {
                        { L"SbieDll.dll", L"SbieDll.dll" },
                        { L"All files (*.*)", L"*.*" }
                    };
                    PVOID fileDialog;
                    PPH_STRING fileName;

                    fileDialog = PhCreateOpenFileDialog();
                    PhSetFileDialogFilter(fileDialog, filters, RTL_NUMBER_OF(filters));

                    fileName = PH_AUTO(PhGetFileName(PhaGetDlgItemText(hwndDlg, IDC_SBIEDLLPATH)));
                    PhSetFileDialogFileName(fileDialog, fileName->Buffer);

                    if (PhShowFileDialog(hwndDlg, fileDialog))
                    {
                        fileName = PH_AUTO(PhGetFileDialogFileName(fileDialog));
                        PhSetDialogItemText(hwndDlg, IDC_SBIEDLLPATH, fileName->Buffer);
                    }

                    PhFreeFileDialog(fileDialog);
                }
                break;
            }
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
