/*
 * Process Hacker Plugins -
 *   Performance Monitor Plugin
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

#include "perfmon.h"

VOID PerfMonEntryDeleteProcedure(
    _In_ PVOID Object,
    _In_ ULONG Flags
    )
{
    PPERF_COUNTER_ENTRY entry = Object;
        
    PhAcquireQueuedLockExclusive(&DiskDrivesListLock);
    PhRemoveItemList(DiskDrivesList, PhFindItemList(DiskDrivesList, entry));
    PhReleaseQueuedLockExclusive(&DiskDrivesListLock);

    DeletePerfCounterId(&entry->Id);

    PhDeleteCircularBuffer_ULONG64(&entry->HistoryBuffer);
}

VOID PerfMonInitialize(
    VOID
    )
{
    DiskDrivesList = PhCreateList(1);
    DiskDriveEntryType = PhCreateObjectType(L"PerfMonEntry", 0, PerfMonEntryDeleteProcedure);
}

VOID PerfMonUpdate(
    VOID
    )
{
    static ULONG runCount = 0; // MUST keep in sync with runCount in process provider

    PhAcquireQueuedLockShared(&DiskDrivesListLock);

    for (ULONG i = 0; i < DiskDrivesList->Count; i++)
    {
        PPERF_COUNTER_ENTRY entry;
        ULONG counterType = 0;
        PDH_FMT_COUNTERVALUE displayValue = { 0 };

        entry = PhReferenceObjectSafe(DiskDrivesList->Items[i]);

        if (!entry)
            continue;

        if (entry->HaveFirstSample)
        {
            if (!entry->PerfQueryHandle)
            {
                ULONG counterLength = 0;
                PDH_STATUS counterStatus = 0;

                // Create the counter query handle
                if ((counterStatus = PdhOpenQuery(NULL, 0, &entry->PerfQueryHandle)) != ERROR_SUCCESS)
                {
                    //PhShowError(NULL, L"PdhOpenQuery failed with status 0x%x.", counterStatus);
                }

                // Add the selected counter to the query handle
                if ((counterStatus = PdhAddCounter(entry->PerfQueryHandle, PhGetString(entry->Id.PerfCounterPath), 0, &entry->PerfCounterHandle)))
                {
                    //PhShowError(NULL, L"PdhAddCounter failed with status 0x%x.", counterStatus);
                }

                if ((counterStatus = PdhGetCounterInfo(entry->PerfCounterHandle, TRUE, &counterLength, NULL)) == PDH_MORE_DATA)
                {
                    entry->PerfCounterInfo = PhAllocate(counterLength);
                    memset(entry->PerfCounterInfo, 0, counterLength);
                }

                if ((counterStatus = PdhGetCounterInfo(entry->PerfCounterHandle, TRUE, &counterLength, entry->PerfCounterInfo)))
                {
                    //PhShowError(NULL, L"PdhGetCounterInfo failed with status 0x%x.", counterStatus);
                }
            }

            PdhCollectQueryData(entry->PerfQueryHandle);

            //PdhSetCounterScaleFactor(entry->PerfCounterHandle, PDH_MAX_SCALE);
            PdhGetFormattedCounterValue(
                entry->PerfCounterHandle,
                PDH_FMT_LARGE | PDH_FMT_NOSCALE | PDH_FMT_NOCAP100,
                &counterType,
                &displayValue
                );

            //if (counterType == PERF_COUNTER_COUNTER)
            PhUpdateDelta(&entry->HistoryDelta, displayValue.largeValue);
        }
        else
        {
            // The first sample must be zero
            PhInitializeDelta(&entry->HistoryDelta);
            entry->HaveFirstSample = TRUE;
        }

        if (runCount != 0)
        {
            PhAddItemCircularBuffer_ULONG64(&entry->HistoryBuffer, entry->HistoryDelta.Value);
        }

        PhDereferenceObjectDeferDelete(entry);
    }

    PhReleaseQueuedLockShared(&DiskDrivesListLock);

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

    entry = PhCreateObject(sizeof(PERF_COUNTER_ENTRY), DiskDriveEntryType);
    memset(entry, 0, sizeof(PERF_COUNTER_ENTRY));

    CopyPerfCounterId(&entry->Id, Id);
    PhInitializeCircularBuffer_ULONG64(&entry->HistoryBuffer, PhGetIntegerSetting(L"SampleCount"));

    PhAcquireQueuedLockExclusive(&DiskDrivesListLock);
    PhAddItemList(DiskDrivesList, entry);
    PhReleaseQueuedLockExclusive(&DiskDrivesListLock);

    return entry;
}