/*
 * Process Hacker Extra Plugins -
 *   Wait Chain Traversal (WCT) Plugin
 *
 * Copyright (C) 2013-2015 dmex
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

// Wait Chain Traversal Documentation:
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms681622.aspx

static PPH_PLUGIN PluginInstance;
static PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
static PH_CALLBACK_REGISTRATION ProcessMenuInitializingCallbackRegistration;
static PH_CALLBACK_REGISTRATION ThreadMenuInitializingCallbackRegistration;

BOOLEAN WaitChainRegisterCallbacks(
    _Inout_ PWCT_CONTEXT Context
    )
{
    PCOGETCALLSTATE coGetCallStateCallback = NULL;
    PCOGETACTIVATIONSTATE coGetActivationStateCallback = NULL;

    if (!(Context->Ole32ModuleHandle = LoadLibrary(L"ole32.dll")))
        return FALSE;

    if (!(coGetCallStateCallback = PhGetProcedureAddress(Context->Ole32ModuleHandle, "CoGetCallState", 0)))
        return FALSE;

    if (!(coGetActivationStateCallback = PhGetProcedureAddress(Context->Ole32ModuleHandle, "CoGetActivationState", 0)))
        return FALSE;

    RegisterWaitChainCOMCallback(coGetCallStateCallback, coGetActivationStateCallback);
    return TRUE;
}

VOID WaitChainCheckThread(
    _Inout_ PWCT_CONTEXT Context,
    _In_ HANDLE ThreadId
    )
{
    BOOL isDeadLocked = FALSE;
    ULONG nodeInfoLength = WCT_MAX_NODE_COUNT;
    WAITCHAIN_NODE_INFO nodeInfoArray[WCT_MAX_NODE_COUNT];
    PWCT_ROOT_NODE rootNode = NULL;

    memset(nodeInfoArray, 0, sizeof(nodeInfoArray));

    // Retrieve the thread wait chain.
    if (!GetThreadWaitChain(
        Context->WctSessionHandle,
        0,
        WCT_GETINFO_ALL_FLAGS,
        HandleToUlong(ThreadId),
        &nodeInfoLength,
        nodeInfoArray,
        &isDeadLocked
        ))
    {
        return;
    }

    // Check if the wait chain is too big for the array we passed in.
    if (nodeInfoLength > WCT_MAX_NODE_COUNT)
        nodeInfoLength = WCT_MAX_NODE_COUNT;


    for (ULONG i = 0; i < nodeInfoLength; i++)
    {
        PWAITCHAIN_NODE_INFO wctNode = &nodeInfoArray[i];

        if (wctNode->ObjectType == WctThreadType)
        {
            rootNode = WeAddWindowNode(&Context->TreeContext);

            rootNode->ObjectType = wctNode->ObjectType;
            rootNode->ObjectStatus = wctNode->ObjectStatus;
            rootNode->Alertable = wctNode->LockObject.Alertable;
            rootNode->ThreadId = UlongToHandle(wctNode->ThreadObject.ThreadId);
            rootNode->ProcessId = UlongToHandle(wctNode->ThreadObject.ProcessId);
            rootNode->ThreadIdString = PhFormatString(L"%lu", wctNode->ThreadObject.ThreadId);
            rootNode->ProcessIdString = PhFormatString(L"%lu", wctNode->ThreadObject.ProcessId);
            rootNode->WaitTimeString = PhFormatString(L"%lu", wctNode->ThreadObject.WaitTime);
            rootNode->ContextSwitchesString = PhFormatString(L"%lu", wctNode->ThreadObject.ContextSwitches);
            rootNode->TimeoutString = PhFormatString(L"%I64d", wctNode->LockObject.Timeout.QuadPart);

            if (wctNode->LockObject.ObjectName[0] != '\0')
            {
                // -- ProcessID --
                //wctNode->LockObject.ObjectName[0]
                // -- ThreadID --
                //wctNode->LockObject.ObjectName[2]
                // -- Unknown --
                //wctNode->LockObject.ObjectName[4]
                // -- ContextSwitches --
                //wctNode->LockObject.ObjectName[6]

                if (PhIsDigitCharacter(wctNode->LockObject.ObjectName[0]))
                {
                    rootNode->ObjectNameString = PhFormatString(L"%s", wctNode->LockObject.ObjectName);
                }
                //else
                //{
                //    rootNode->ObjectNameString = PhFormatString(L"[%lu, %lu]",
                //        wctNode.LockObject.ObjectName[0],
                //        wctNode.LockObject.ObjectName[2]
                //        );
                //}
            }

            rootNode->Node.Expanded = TRUE;
            rootNode->HasChildren = TRUE;

            // This is a root node.
            PhAddItemList(Context->TreeContext.NodeRootList, rootNode);
        }
        else
        {
            WctAddChildWindowNode(&Context->TreeContext, rootNode, wctNode, isDeadLocked);
        }
    }
}

NTSTATUS WaitChainCallbackThread(
    _In_ PVOID Parameter
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PWCT_CONTEXT context = (PWCT_CONTEXT)Parameter;

    if (!WaitChainRegisterCallbacks(context))
        return NTSTATUS_FROM_WIN32(GetLastError());

    // Synchronous WCT session
    if (!(context->WctSessionHandle = OpenThreadWaitChainSession(0, NULL)))
        return NTSTATUS_FROM_WIN32(GetLastError());

    //TreeNew_SetRedraw(context->TreeNewHandle, FALSE);
    
    if (context->IsProcessItem)
    {
        NTSTATUS status;
        HANDLE threadHandle;
        HANDLE newThreadHandle;
        THREAD_BASIC_INFORMATION basicInfo;

        status = NtGetNextThread(
            context->ProcessItem->QueryHandle, 
            NULL, 
            ThreadQueryAccess,
            0, 
            0, 
            &threadHandle
            );

        while (NT_SUCCESS(status))
        {
            if (NT_SUCCESS(PhGetThreadBasicInformation(threadHandle, &basicInfo)))
            {
                WaitChainCheckThread(context, basicInfo.ClientId.UniqueThread);
            }

            status = NtGetNextThread(
                context->ProcessItem->QueryHandle, 
                threadHandle, 
                ThreadQueryAccess, 
                0, 
                0, 
                &newThreadHandle
                );

            NtClose(threadHandle);

            threadHandle = newThreadHandle;
        }
    }
    else
    {
        WaitChainCheckThread(context, context->ThreadItem->ThreadId);
    }

    if (context->WctSessionHandle)
    {
        CloseThreadWaitChainSession(context->WctSessionHandle);
    }

    if (context->Ole32ModuleHandle)
    {
        FreeLibrary(context->Ole32ModuleHandle);
    }

    //TreeNew_SetRedraw(context->TreeNewHandle, TRUE);
    TreeNew_NodesStructured(context->TreeNewHandle);

    return status;
}

INT_PTR CALLBACK WaitChainDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PWCT_CONTEXT context = NULL;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PWCT_CONTEXT)lParam;
        SetProp(hwndDlg, L"Context", (HANDLE)context);
    }
    else
    {
        context = (PWCT_CONTEXT)GetProp(hwndDlg, L"Context");

        if (uMsg == WM_DESTROY)
        {
            PhUnregisterDialog(hwndDlg);
            PhSaveWindowPlacementToSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            PhDeleteLayoutManager(&context->LayoutManager);
            WtcDeleteWindowTree(&context->TreeContext);

            RemoveProp(hwndDlg, L"Context");
            PhFree(context);
        }
    }

    if (context == NULL)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            HANDLE threadHandle = NULL;

            context->TreeNewHandle = GetDlgItem(hwndDlg, IDC_CUSTOM1);

            PhRegisterDialog(hwndDlg);
            WtcInitializeWindowTree(hwndDlg, context->TreeNewHandle, &context->TreeContext);
            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, context->TreeNewHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDOK), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
            PhLoadWindowPlacementFromSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);

            if (threadHandle = PhCreateThread(0, WaitChainCallbackThread, (PVOID)context))
                NtClose(threadHandle);
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&context->LayoutManager);
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDCANCEL:
            case IDOK:
                EndDialog(hwndDlg, IDOK);
                break;
            case ID_WCTSHOWCONTEXTMENU:
                {
                    POINT point;
                    PPH_EMENU menu;
                    PWCT_ROOT_NODE selectedNode;

                    point.x = (SHORT)LOWORD(lParam);
                    point.y = (SHORT)HIWORD(lParam);

                    if (selectedNode = WeGetSelectedWindowNode(&context->TreeContext))
                    {
                        menu = PhCreateEMenu();
                        PhLoadResourceEMenuItem(menu, PluginInstance->DllBase, MAKEINTRESOURCE(IDR_MAIN_MENU), 0);
                        PhSetFlagsEMenuItem(menu, ID_MENU_PROPERTIES, PH_EMENU_DEFAULT, PH_EMENU_DEFAULT);

                        if (selectedNode->ThreadId > 0)
                        {
                            PhSetFlagsEMenuItem(menu, ID_MENU_GOTOTHREAD, PH_EMENU_DISABLED, 0);
                            PhSetFlagsEMenuItem(menu, ID_MENU_GOTOPROCESS, PH_EMENU_DISABLED, 0);
                        }
                        else
                        {
                            PhSetFlagsEMenuItem(menu, ID_MENU_GOTOTHREAD, PH_EMENU_DISABLED, PH_EMENU_DISABLED);
                            PhSetFlagsEMenuItem(menu, ID_MENU_GOTOPROCESS, PH_EMENU_DISABLED, PH_EMENU_DISABLED);
                        }
                       
                        PhShowEMenu(menu, hwndDlg, PH_EMENU_SHOW_SEND_COMMAND | PH_EMENU_SHOW_LEFTRIGHT, PH_ALIGN_LEFT | PH_ALIGN_TOP, point.x, point.y);                       
                        PhDestroyEMenu(menu);
                    }
                }
                break;
            case ID_MENU_GOTOPROCESS:
                {
                    PWCT_ROOT_NODE selectedNode = NULL;
                    PPH_PROCESS_NODE processNode = NULL;

                    if (selectedNode = WeGetSelectedWindowNode(&context->TreeContext))
                    {
                        if (processNode = PhFindProcessNode(selectedNode->ProcessId))
                        {
                            ProcessHacker_SelectTabPage(PhMainWndHandle, 0);
                            PhSelectAndEnsureVisibleProcessNode(processNode);
                        }
                    }
                }
                break;
            case ID_MENU_GOTOTHREAD:
                {
                    PWCT_ROOT_NODE selectedNode = NULL;
                    PPH_PROCESS_ITEM processItem = NULL;
                    PPH_PROCESS_PROPCONTEXT propContext = NULL;

                    if (selectedNode = WeGetSelectedWindowNode(&context->TreeContext))
                    {
                        if (processItem = PhReferenceProcessItem(selectedNode->ProcessId))
                        {
                            if (propContext = PhCreateProcessPropContext(NULL, processItem))
                            {
                                if (selectedNode->ThreadId)
                                {
                                    PhSetSelectThreadIdProcessPropContext(propContext, selectedNode->ThreadId);
                                }

                                PhShowProcessProperties(propContext);
                                PhDereferenceObject(propContext);
                            }

                            PhDereferenceObject(processItem);
                        }
                        else
                        {
                            PhShowError(hwndDlg, L"The process does not exist.");
                        }
                    }
                }
                break;
            case ID_MENU_COPY:
                {
                    PPH_STRING text;

                    text = PhGetTreeNewText(context->TreeNewHandle, 0);
                    PhSetClipboardString(hwndDlg, &text->sr);
                    PhDereferenceObject(text);
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}

VOID NTAPI MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = (PPH_PLUGIN_MENU_ITEM)Parameter;

    switch (menuItem->Id)
    {
    case IDD_WCT_MENUITEM:
        {
            DialogBoxParam(
                PluginInstance->DllBase,
                MAKEINTRESOURCE(IDD_WCT_DIALOG),
                NULL,
                WaitChainDlgProc,
                (LPARAM)menuItem->Context
                );
        }
        break;
    }
}

VOID NTAPI ProcessMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    ULONG insertIndex = 0;
    PWCT_CONTEXT context = NULL;
    PPH_PLUGIN_MENU_INFORMATION menuInfo = NULL;
    PPH_PROCESS_ITEM processItem = NULL;
    PPH_EMENU_ITEM menuItem = NULL;
    PPH_EMENU_ITEM miscMenuItem = NULL;
    PPH_EMENU_ITEM wsMenuItem = NULL;

    menuInfo = (PPH_PLUGIN_MENU_INFORMATION)Parameter;

    if (menuInfo->u.Process.NumberOfProcesses == 1)
        processItem = menuInfo->u.Process.Processes[0];
    else
    {
        processItem = NULL;
    }

    if (processItem == NULL)
        return;

    context = (PWCT_CONTEXT)PhAllocate(sizeof(WCT_CONTEXT));
    memset(context, 0, sizeof(WCT_CONTEXT));

    context->IsProcessItem = TRUE;
    context->ProcessItem = processItem;

    if (miscMenuItem = PhFindEMenuItem(menuInfo->Menu, 0, L"Miscellaneous", 0))
    {
        PhInsertEMenuItem(miscMenuItem, menuItem = PhPluginCreateEMenuItem(PluginInstance, 0, IDD_WCT_MENUITEM, L"Wait Chain Tra&versal", context), -1);

        if (!processItem || !processItem->QueryHandle || processItem->ProcessId == NtCurrentProcessId())
            menuItem->Flags |= PH_EMENU_DISABLED;

        if (!PhGetOwnTokenAttributes().Elevated)
            menuItem->Flags |= PH_EMENU_DISABLED;
    }
    else
    {
        PhFree(context);
    }
}

VOID NTAPI ThreadMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PWCT_CONTEXT context = NULL;
    PPH_PLUGIN_MENU_INFORMATION menuInfo = NULL;
    PPH_THREAD_ITEM threadItem = NULL;
    PPH_EMENU_ITEM menuItem = NULL;
    PPH_EMENU_ITEM miscMenuItem = NULL;

    menuInfo = (PPH_PLUGIN_MENU_INFORMATION)Parameter;

    if (menuInfo->u.Thread.NumberOfThreads == 1)
        threadItem = menuInfo->u.Thread.Threads[0];
    else
        threadItem = NULL;

    context = (PWCT_CONTEXT)PhAllocate(sizeof(WCT_CONTEXT));
    memset(context, 0, sizeof(WCT_CONTEXT));

    context->IsProcessItem = FALSE;
    context->ThreadItem = threadItem;

    miscMenuItem = PhFindEMenuItem(menuInfo->Menu, 0, L"Analyze", 0);
    if (miscMenuItem)
    {
        menuItem = PhPluginCreateEMenuItem(PluginInstance, 0, IDD_WCT_MENUITEM, L"Wait Chain Tra&versal", context);
        PhInsertEMenuItem(miscMenuItem, menuItem, -1);

        // Disable menu if current process selected.
        if (threadItem == NULL || menuInfo->u.Thread.ProcessId == NtCurrentProcessId())
            menuItem->Flags |= PH_EMENU_DISABLED;
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

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->DisplayName = L"Wait Chain Traversal";
            info->Author = L"dmex";
            info->Description = L"Plugin for Wait Chain analysis via right-click Miscellaneous > Wait Chain Traversal or individual threads via Process properties > Threads tab > Analyze > Wait Chain Traversal.";
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
            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackThreadMenuInitializing),
                ThreadMenuInitializingCallback,
                NULL,
                &ThreadMenuInitializingCallbackRegistration
                );

            {
                PH_SETTING_CREATE settings[] =
                {
                    { StringSettingType, SETTING_NAME_TREE_LIST_COLUMNS, L"" },
                    { IntegerPairSettingType, SETTING_NAME_WINDOW_POSITION, L"100,100" },
                    { ScalableIntegerPairSettingType, SETTING_NAME_WINDOW_SIZE, L"@96|690,540" }
                };

                PhAddSettings(settings, ARRAYSIZE(settings));
            }
        }
        break;
    }

    return TRUE;
}