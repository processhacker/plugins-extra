/*
 * Process Hacker Extra Plugins -
 *   Performance Monitor Plugin
 *
 * Copyright (C) 2015-2016 dmex
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

#include "perfmon.h"

INT AddListViewItemGroupId(
    _In_ HWND ListViewHandle,
    _In_ INT Index,
    _In_ PWSTR Text,
    _In_opt_ PVOID Param
)
{
    LVITEM item;

    memset(&item, 0, sizeof(LVITEM));

    item.mask = LVIF_TEXT;
    item.iItem = Index;
    item.iSubItem = 0;
    item.pszText = Text;

    if (Param)
    {
        item.mask |= LVIF_PARAM;
        item.lParam = (LPARAM)Param;
    }

    return ListView_InsertItem(ListViewHandle, &item);
}

VOID PerfMonAddCounter(
    _In_ PPH_PERFMON_CONTEXT Context,
    _In_ PPH_STRING CounterPath
    )
{
    INT lvItemIndex;
    PERF_COUNTER_ID id;
    PPERF_COUNTER_ENTRY entry;

    InitializePerfCounterId(&id, CounterPath);
    entry = CreatePerfCounterEntry(&id);
    DeletePerfCounterId(&id);

    entry->UserReference = TRUE;

    lvItemIndex = AddListViewItemGroupId(
        Context->ListViewHandle,
        MAXINT,
        PhGetStringOrEmpty(entry->Id.PerfCounterPath),
        entry
        );

    ListView_SetItemState(Context->ListViewHandle, lvItemIndex, ITEM_CHECKED, LVIS_STATEIMAGEMASK);
}

VOID PerfMonLoadCounters(
    _In_ PPH_PERFMON_CONTEXT Context
    )
{
    PhAcquireQueuedLockShared(&PerfCounterListLock);
    for (ULONG i = 0; i < PerfCounterList->Count; i++)
    {
        INT lvItemIndex;
        PPERF_COUNTER_ENTRY entry = PhReferenceObjectSafe(PerfCounterList->Items[i]);

        if (!entry)
            continue;

        if (entry->UserReference)
        {
            lvItemIndex = AddListViewItemGroupId(
                Context->ListViewHandle,
                MAXINT,
                PhGetStringOrEmpty(entry->Id.PerfCounterPath),
                entry
                );

            ListView_SetItemState(Context->ListViewHandle, lvItemIndex, ITEM_CHECKED, LVIS_STATEIMAGEMASK);
        }
    }
    PhReleaseQueuedLockShared(&PerfCounterListLock);
}

VOID PerfMonLoadList(
    VOID
    )
{
    PPH_STRING settingsString;
    PH_STRINGREF remaining;

    settingsString = PhaGetStringSetting(SETTING_NAME_PERFMON_LIST);
    remaining = settingsString->sr;

    while (remaining.Length != 0)
    {
        PH_STRINGREF part;
        PERF_COUNTER_ID id;
        PPERF_COUNTER_ENTRY entry;

        if (remaining.Length == 0)
            break;

        PhSplitStringRefAtChar(&remaining, ',', &part, &remaining);

        InitializePerfCounterId(&id, PhCreateString2(&part));
        entry = CreatePerfCounterEntry(&id);
        DeletePerfCounterId(&id);

        entry->UserReference = TRUE;
    }
}

VOID PerfMonSaveList(
    VOID
    )
{
    PH_STRING_BUILDER stringBuilder;
    PPH_STRING settingsString;

    PhInitializeStringBuilder(&stringBuilder, 260);

    PhAcquireQueuedLockShared(&PerfCounterListLock);
    for (ULONG i = 0; i < PerfCounterList->Count; i++)
    {
        PPERF_COUNTER_ENTRY entry = PhReferenceObjectSafe(PerfCounterList->Items[i]);

        if (!entry)
            continue;

        if (entry->UserReference)
        {
            PhAppendFormatStringBuilder(&stringBuilder, L"%s,", PhGetStringOrEmpty(entry->Id.PerfCounterPath));
        }

        PhDereferenceObjectDeferDelete(entry);
    }
    PhReleaseQueuedLockShared(&PerfCounterListLock);

    if (stringBuilder.String->Length != 0)
        PhRemoveEndStringBuilder(&stringBuilder, 1);

    settingsString = PH_AUTO(PhFinalStringBuilderString(&stringBuilder));
    PhSetStringSetting2(SETTING_NAME_PERFMON_LIST, &settingsString->sr);
}

BOOLEAN PerfMonFindEntry(
    _In_ PPERF_COUNTER_ID Id,
    _In_ BOOLEAN RemoveUserReference
    )
{
    BOOLEAN found = FALSE;

    PhAcquireQueuedLockShared(&PerfCounterListLock);
    for (ULONG i = 0; i < PerfCounterList->Count; i++)
    {
        PPERF_COUNTER_ENTRY entry = PhReferenceObjectSafe(PerfCounterList->Items[i]);

        if (!entry)
            continue;

        found = EquivalentPerfCounterId(&entry->Id, Id);

        if (found)
        {
            if (RemoveUserReference)
            {
                if (entry->UserReference)
                {
                    PhDereferenceObjectDeferDelete(entry);
                    entry->UserReference = FALSE;
                }
            }

            PhDereferenceObjectDeferDelete(entry);
            break;
        }
        else
        {
            PhDereferenceObjectDeferDelete(entry);
        }
    }
    PhReleaseQueuedLockShared(&PerfCounterListLock);

    return found;
}

VOID PerfMonShowCounters(
    _In_ PPH_PERFMON_CONTEXT Context,
    _In_ HWND Parent
    )
{
    PDH_STATUS counterStatus = 0;
    PPH_STRING counterPathString = NULL;
    PPH_STRING counterWildCardString = NULL;
    PDH_BROWSE_DLG_CONFIG browseConfig = { 0 };
    WCHAR counterPathBuffer[PDH_MAX_COUNTER_PATH] = L"";

    browseConfig.bIncludeInstanceIndex = FALSE;
    browseConfig.bSingleCounterPerAdd = FALSE; // Fix empty CounterPathBuffer
    browseConfig.bSingleCounterPerDialog = TRUE;
    browseConfig.bLocalCountersOnly = FALSE;
    browseConfig.bWildCardInstances = TRUE; // Seems to cause a lot of crashes
    browseConfig.bHideDetailBox = TRUE;
    browseConfig.bInitializePath = FALSE;
    browseConfig.bDisableMachineSelection = FALSE;
    browseConfig.bIncludeCostlyObjects = FALSE;
    browseConfig.bShowObjectBrowser = FALSE;
    browseConfig.hWndOwner = Parent;
    browseConfig.szReturnPathBuffer = counterPathBuffer;
    browseConfig.cchReturnPathLength = PDH_MAX_COUNTER_PATH;
    browseConfig.CallBackStatus = ERROR_SUCCESS;
    browseConfig.dwDefaultDetailLevel = PERF_DETAIL_WIZARD;
    browseConfig.szDialogBoxCaption = L"Select a counter to monitor.";

    if ((counterStatus = PdhBrowseCounters(&browseConfig)) != ERROR_SUCCESS)
    {
        goto CleanupExit;
    }
    
    if (PhCountStringZ(counterPathBuffer) == 0)
    {
        goto CleanupExit;
    }

    counterPathString = PhCreateString(counterPathBuffer);

    if (PhFindCharInString(counterPathString, 0, '*') != -1) // Check for wildcards
    {
        ULONG wildCardLength = 0;

        PdhExpandWildCardPath(
            NULL,
            PhGetString(counterPathString),
            NULL,
            &wildCardLength,
            0
            );

        counterWildCardString = PhCreateStringEx(NULL, wildCardLength * sizeof(WCHAR));

        if (PdhExpandWildCardPath(
            NULL, 
            PhGetString(counterPathString), 
            PhGetString(counterWildCardString), 
            &wildCardLength, 
            0
            ) == ERROR_SUCCESS)
        {
            PH_STRINGREF part;
            PH_STRINGREF remaining = counterWildCardString->sr;

            while (remaining.Length != 0)
            {
                if (!PhSplitStringRefAtChar(&remaining, '\0', &part, &remaining))
                    break;
                if (remaining.Length == 0)
                    break;

                if (PdhValidatePath(part.Buffer) != ERROR_SUCCESS)
                {
                    goto CleanupExit;
                }

                PerfMonAddCounter(Context, PhCreateString2(&part));
            }
        }
    }
    else
    {
        if (PdhValidatePath(PhGetString(counterPathString)) != ERROR_SUCCESS)
        {
            goto CleanupExit;
        }

        PerfMonAddCounter(Context, PhCreateString2(&counterPathString->sr));
    }

CleanupExit:
    if (counterWildCardString)
        PhDereferenceObject(counterWildCardString);

    if (counterPathString)
        PhDereferenceObject(counterPathString);
}

INT_PTR CALLBACK OptionsDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PPH_PERFMON_CONTEXT context = NULL;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PPH_PERFMON_CONTEXT)PhAllocate(sizeof(PH_PERFMON_CONTEXT));
        memset(context, 0, sizeof(PH_PERFMON_CONTEXT));

        SetProp(hwndDlg, L"Context", (HANDLE)context);
    }
    else
    {
        context = (PPH_PERFMON_CONTEXT)GetProp(hwndDlg, L"Context");

        if (uMsg == WM_DESTROY)
        {
            PerfMonSaveList();

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
            context->ListViewHandle = GetDlgItem(hwndDlg, IDC_PERFCOUNTER_LISTVIEW);
 
            PhCenterWindow(hwndDlg, GetParent(hwndDlg));

            PhSetListViewStyle(context->ListViewHandle, FALSE, TRUE);
            ListView_SetExtendedListViewStyleEx(context->ListViewHandle, 
                LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER, 
                LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            PhSetControlTheme(context->ListViewHandle, L"explorer");
            PhAddListViewColumn(context->ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 500, L"Counter");
            PhSetExtendedListView(context->ListViewHandle);

            PerfMonLoadCounters(context);
        }
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDC_ADD_BUTTON:
                PerfMonShowCounters(context, hwndDlg);
                break;
            case IDCANCEL:
            case IDOK:
                EndDialog(hwndDlg, IDOK);
                break;
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR header = (LPNMHDR)lParam;

            if (header->code == LVN_ITEMCHANGED)
            {
                LPNM_LISTVIEW listView = (LPNM_LISTVIEW)lParam;

                if (!PhTryAcquireReleaseQueuedLockExclusive(&PerfCounterListLock))
                    break;

                if (listView->uChanged & LVIF_STATE)
                {
                    switch (listView->uNewState & LVIS_STATEIMAGEMASK)
                    {
                    case 0x2000: // checked
                        {
                            PPERF_COUNTER_ID param = (PPERF_COUNTER_ID)listView->lParam;

                            if (!PerfMonFindEntry(param, FALSE))
                            {
                                PPERF_COUNTER_ENTRY entry;

                                entry = CreatePerfCounterEntry(param);
                                entry->UserReference = TRUE;
                            }

                            context->OptionsChanged = TRUE;
                        }
                        break;
                    case 0x1000: // unchecked
                        {
                            PPERF_COUNTER_ID param = (PPERF_COUNTER_ID)listView->lParam;

                            PerfMonFindEntry(param, TRUE);

                            context->OptionsChanged = TRUE;
                        }
                        break;
                    }
                }
            }
        }
        break;
    }

    return FALSE;
}

VOID ShowOptionsDialog(
    _In_ HWND ParentHandle
    )
{
    DialogBox(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_PERFMON_OPTIONS),
        ParentHandle,
        OptionsDlgProc
        );
}