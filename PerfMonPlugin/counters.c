/*
 * Process Hacker Plugins -
 *   Performance Monitor Plugin
 *
 * Copyright (C) 2016-2019 dmex
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

VOID PerfMonEntryDeleteProcedure(
    _In_ PVOID Object,
    _In_ ULONG Flags
    )
{
    PPERF_COUNTER_ENTRY entry = Object;
        
    PhAcquireQueuedLockExclusive(&PerfCounterListLock);
    PhRemoveItemList(PerfCounterList, PhFindItemList(PerfCounterList, entry));
    PhReleaseQueuedLockExclusive(&PerfCounterListLock);

    DeletePerfCounterId(&entry->Id);
    PhDeleteCircularBuffer_ULONG64(&entry->HistoryBuffer);

    if (entry->PerfQueryHandle)
    {
        PdhCloseQuery(entry->PerfQueryHandle);
        entry->PerfQueryHandle = NULL;
    }
}

VOID PerfMonInitialize(
    VOID
    )
{
    PerfCounterList = PhCreateList(1);
    PerfCounterEntryType = PhCreateObjectType(L"PerfMonEntry", 0, PerfMonEntryDeleteProcedure);
}

VOID PerfMonUpdate(
    VOID
    )
{
    static ULONG runCount = 0; // MUST keep in sync with runCount in process provider

    PhAcquireQueuedLockShared(&PerfCounterListLock);

    for (ULONG i = 0; i < PerfCounterList->Count; i++)
    {
        PPERF_COUNTER_ENTRY entry;
        ULONG counterType = 0;
        PDH_FMT_COUNTERVALUE displayValue = { 0 };

        entry = PhReferenceObjectSafe(PerfCounterList->Items[i]);

        if (!entry)
            continue;

        if (entry->HaveFirstSample)
        {
            if (!entry->PerfQueryHandle)
            {               
                if (PdhOpenQuery(NULL, 0, &entry->PerfQueryHandle) == ERROR_SUCCESS)
                {
                    if (PdhAddCounter(entry->PerfQueryHandle, PhGetString(entry->Id.PerfCounterPath), 0, &entry->PerfCounterHandle) == ERROR_SUCCESS)
                    {
                        //ULONG counterLength = 0;
                        //PPDH_COUNTER_INFO perfCounterInfo;
                        //
                        //if (PdhGetCounterInfo(entry->PerfCounterHandle, TRUE, &counterLength, NULL) == PDH_MORE_DATA)
                        //{
                        //    perfCounterInfo = PhAllocate(counterLength);
                        //    memset(perfCounterInfo, 0, counterLength);
                        //}
                        //
                        //if (PdhGetCounterInfo(entry->PerfCounterHandle, TRUE, &counterLength, perfCounterInfo) == ERROR_SUCCESS)
                        //{
                        //    
                        //}
                        //PhFree(perfCounterInfo);
                    }
                }
            }

            // Update the counter sample data
            PdhCollectQueryData(entry->PerfQueryHandle);

            // Get the counter value
            if (PdhGetFormattedCounterValue(
                entry->PerfCounterHandle,
                PDH_FMT_LARGE | PDH_FMT_NOSCALE | PDH_FMT_NOCAP100,
                &counterType,
                &displayValue
                ) == ERROR_SUCCESS && counterType == PERF_COUNTER_COUNTER)
            {
                PhUpdateDelta(&entry->HistoryDelta, displayValue.largeValue);
            }
        }
        else
        {
            PhInitializeDelta(&entry->HistoryDelta); // The first sample must be zero

            entry->HaveFirstSample = TRUE;
        }

        if (runCount != 0)
        {
            PhAddItemCircularBuffer_ULONG64(&entry->HistoryBuffer, entry->HistoryDelta.Value);
        }

        PhDereferenceObjectDeferDelete(entry);
    }

    PhReleaseQueuedLockShared(&PerfCounterListLock);

    runCount++;
}

VOID InitializePerfCounterId(
    _Out_ PPERF_COUNTER_ID Id,
    _In_ PPH_STRING PerfCounterPath
    )
{
    PhSetReference(&Id->PerfCounterPath, PerfCounterPath);
}

VOID CopyPerfCounterId(
    _Out_ PPERF_COUNTER_ID Destination,
    _In_ PPERF_COUNTER_ID Source
    )
{
    InitializePerfCounterId(Destination, Source->PerfCounterPath);
}

VOID DeletePerfCounterId(
    _Inout_ PPERF_COUNTER_ID Id
    )
{
    PhClearReference(&Id->PerfCounterPath);
}

BOOLEAN EquivalentPerfCounterId(
    _In_ PPERF_COUNTER_ID Id1,
    _In_ PPERF_COUNTER_ID Id2
    )
{
    if (!Id1 || !Id2)
        return FALSE;

    return PhEqualString(Id1->PerfCounterPath, Id2->PerfCounterPath, TRUE);
}

PPERF_COUNTER_ENTRY CreatePerfCounterEntry(
    _In_ PPERF_COUNTER_ID Id
    )
{
    PPERF_COUNTER_ENTRY entry;

    entry = PhCreateObject(sizeof(PERF_COUNTER_ENTRY), PerfCounterEntryType);
    memset(entry, 0, sizeof(PERF_COUNTER_ENTRY));

    CopyPerfCounterId(&entry->Id, Id);

    PhInitializeCircularBuffer_ULONG64(&entry->HistoryBuffer, PhGetIntegerSetting(L"SampleCount"));

    PhAcquireQueuedLockExclusive(&PerfCounterListLock);
    PhAddItemList(PerfCounterList, entry);
    PhReleaseQueuedLockExclusive(&PerfCounterListLock);

    return entry;
}
