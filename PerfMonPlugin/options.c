/*
 * Process Hacker Extra Plugins -
 *   Performance Monitor Plugin
 *
 * Copyright (C) 2015-2019 dmex
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

    PhAcquireQueuedLockShared(&PerfCounterListLock);

    lvItemIndex = PhAddListViewItem(
        Context->ListViewHandle,
        MAXINT,
        PhGetStringOrEmpty(entry->Id.PerfCounterPath),
        entry
        );

    ListView_SetCheckState(Context->ListViewHandle, lvItemIndex, TRUE);

    PhReleaseQueuedLockShared(&PerfCounterListLock);
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
            lvItemIndex = PhAddListViewItem(
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

    if (!(settingsString = PhGetStringSetting(SETTING_NAME_PERFMON_LIST)))
        return;

    remaining = PhGetStringRef(settingsString);

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

    PhDereferenceObject(settingsString);
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
            PhAppendFormatStringBuilder(
                &stringBuilder,
                L"%s,",
                PhGetStringOrEmpty(entry->Id.PerfCounterPath)
                );
        }

        PhDereferenceObjectDeferDelete(entry);
    }

    PhReleaseQueuedLockShared(&PerfCounterListLock);

    if (stringBuilder.String->Length != 0)
        PhRemoveEndStringBuilder(&stringBuilder, 1);

    settingsString = PhFinalStringBuilderString(&stringBuilder);
    PhSetStringSetting2(SETTING_NAME_PERFMON_LIST, &settingsString->sr);
    PhDereferenceObject(settingsString);
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
    PDH_BROWSE_DLG_CONFIG browseConfig;
    WCHAR counterPathBuffer[PDH_MAX_COUNTER_PATH + 1] = L"";

    memset(&browseConfig, 0, sizeof(PDH_BROWSE_DLG_CONFIG));
    browseConfig.szDialogBoxCaption = L"Select a counter to monitor.";
    browseConfig.bSingleCounterPerDialog = TRUE;
    browseConfig.bWildCardInstances = TRUE;
    browseConfig.bHideDetailBox = TRUE;   
    browseConfig.dwDefaultDetailLevel = PERF_DETAIL_EXPERT;
    browseConfig.hWndOwner = Parent;
    browseConfig.szReturnPathBuffer = counterPathBuffer;
    browseConfig.cchReturnPathLength = PDH_MAX_COUNTER_PATH;

    if (PdhBrowseCounters(&browseConfig) != ERROR_SUCCESS)
        return;
    if (PhCountStringZ(counterPathBuffer) == 0)
        return;

    for (PWSTR counterPath = counterPathBuffer; *counterPath; counterPath += PhCountStringZ(counterPath) + 1)
    {
        PPH_STRING counterPathString = PhCreateString(counterPath);

        if (PhFindCharInString(counterPathString, 0, L'*') != -1) // check for wildcards
        {
            PPH_STRING counterWildCardString;
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
                PH_STRINGREF remaining;

                remaining = PhGetStringRef(counterWildCardString);

                while (remaining.Length != 0)
                {
                    PPH_STRING counterPart;

                    if (!PhSplitStringRefAtChar(&remaining, L'\0', &part, &remaining))
                        break;
                    if (remaining.Length == 0)
                        break;

                    counterPart = PhCreateString2(&part);

                    if (PdhValidatePath(PhGetString(counterPart)) != ERROR_SUCCESS)
                    {
                        PhDereferenceObject(counterPart);
                        PhDereferenceObject(counterWildCardString);
                        PhDereferenceObject(counterPathString);
                        continue;
                    }

                    PerfMonAddCounter(Context, counterPart);
                    PhDereferenceObject(counterPart);
                }
            }

            PhDereferenceObject(counterWildCardString);
        }
        else
        {
            if (PdhValidatePath(PhGetString(counterPathString)) != ERROR_SUCCESS)
            {
                PhDereferenceObject(counterPathString);
                continue;
            }

            PerfMonAddCounter(Context, counterPathString);
        }

        PhDereferenceObject(counterPathString);
    }
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
        context = PhAllocateZero(sizeof(PH_PERFMON_CONTEXT));
        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
    }

    if (context == NULL)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            context->ListViewHandle = GetDlgItem(hwndDlg, IDC_PERFCOUNTER_LISTVIEW);

            PhSetListViewStyle(context->ListViewHandle, FALSE, TRUE);
            ListView_SetExtendedListViewStyleEx(context->ListViewHandle, 
                LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER, 
                LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            PhSetControlTheme(context->ListViewHandle, L"explorer");
            PhAddListViewColumn(context->ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 500, L"Counter");
            PhSetExtendedListView(context->ListViewHandle);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, context->ListViewHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_ADD_BUTTON), NULL, PH_ANCHOR_RIGHT | PH_ANCHOR_BOTTOM);

            PerfMonLoadCounters(context);
        }
        break;
    case WM_DESTROY:
        {
            //if (context->OptionsChanged)
            PerfMonSaveList();

            PhDeleteLayoutManager(&context->LayoutManager);

            PhRemoveWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
            PhFree(context);
        }
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDC_ADD_BUTTON:
                PerfMonShowCounters(context, hwndDlg);
                break;
            }
        }
        break;
    case WM_SIZE:
        {
            PhLayoutManagerLayout(&context->LayoutManager);

            ExtendedListView_SetColumnWidth(context->ListViewHandle, 0, ELVSCW_AUTOSIZE_REMAININGSPACE);
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
                    case INDEXTOSTATEIMAGEMASK(2): // checked
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
                    case INDEXTOSTATEIMAGEMASK(1): // unchecked
                        {
                            PPERF_COUNTER_ID param = (PPERF_COUNTER_ID)listView->lParam;

                            PerfMonFindEntry(param, TRUE);

                            // HACK: Remove item from listview since this code currently doesn't re-reference the existing object.
                            ListView_DeleteItem(listView->hdr.hwndFrom, listView->iItem);

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
