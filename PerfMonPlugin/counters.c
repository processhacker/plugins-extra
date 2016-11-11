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
    PDV_DISK_ENTRY entry = Object;
        
    PhAcquireQueuedLockExclusive(&DiskDrivesListLock);
    PhRemoveItemList(DiskDrivesList, PhFindItemList(DiskDrivesList, entry));
    PhReleaseQueuedLockExclusive(&DiskDrivesListLock);

    DeleteDiskId(&entry->Id);

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
        PDV_DISK_ENTRY entry;
        ULONG counterType = 0;
        PDH_FMT_COUNTERVALUE displayValue = { 0 };

        entry = PhReferenceObjectSafe(DiskDrivesList->Items[i]);

        if (!entry)
            continue;

        // TODO: Handle this on a different thread.
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

        if (!entry->HaveFirstSample)
        {
            // The first sample must be zero.
            entry->HistoryDelta.Delta = 0;
            entry->HaveFirstSample = TRUE;
        }

        if (runCount != 0)
        {
            PhAddItemCircularBuffer_ULONG64(&entry->HistoryBuffer, displayValue.largeValue);
        }

        PhDereferenceObjectDeferDelete(entry);
    }

    PhReleaseQueuedLockShared(&DiskDrivesListLock);

    runCount++;
}

VOID InitializeDiskId(
    _Out_ PPERF_COUNTER_ID Id,
    _In_ PPH_STRING PerfCounterPath
    )
{
    PhSetReference(&Id->PerfCounterPath, PerfCounterPath);
}

VOID CopyDiskId(
    _Out_ PPERF_COUNTER_ID Destination,
    _In_ PPERF_COUNTER_ID Source
    )
{
    InitializeDiskId(Destination, Source->PerfCounterPath);
}

VOID DeleteDiskId(
    _Inout_ PPERF_COUNTER_ID Id
    )
{
    PhClearReference(&Id->PerfCounterPath);
}

BOOLEAN EquivalentDiskId(
    _In_ PPERF_COUNTER_ID Id1,
    _In_ PPERF_COUNTER_ID Id2
    )
{
    return PhEqualString(Id1->PerfCounterPath, Id2->PerfCounterPath, TRUE);
}

PDV_DISK_ENTRY CreateDiskEntry(
    _In_ PPERF_COUNTER_ID Id
    )
{
    PDV_DISK_ENTRY entry;
    ULONG counterLength = 0;
    PDH_STATUS counterStatus = 0;

    entry = PhCreateObject(sizeof(DV_DISK_ENTRY), DiskDriveEntryType);
    memset(entry, 0, sizeof(DV_DISK_ENTRY));

    entry->DiskIndex = ULONG_MAX;
    CopyDiskId(&entry->Id, Id);

    // Create the counter query handle.
    if ((counterStatus = PdhOpenQuery(NULL, 0, &entry->PerfQueryHandle)) != ERROR_SUCCESS)
    {
        PhShowError(NULL, L"PdhOpenQuery failed with status 0x%x.", counterStatus);
    }

    // Add the selected counter to the query handle.
    if ((counterStatus = PdhAddCounter(entry->PerfQueryHandle, PhGetString(entry->Id.PerfCounterPath), 0, &entry->PerfCounterHandle)))
    {
        PhShowError(NULL, L"PdhAddCounter failed with status 0x%x.", counterStatus);
    }

    //PPDH_COUNTER_INFO counterInfo;
    //if ((counterStatus = PdhGetCounterInfo(context->PerfCounterHandle, TRUE, &counterLength, NULL)) == PDH_MORE_DATA)
    //{
    //    counterInfo = PhAllocate(counterLength);
    //    memset(counterInfo, 0, counterLength);
    //}
    //if ((counterStatus = PdhGetCounterInfo(context->PerfCounterHandle, TRUE, &counterLength, counterInfo)))
    //{
    //    PhShowError(NULL, L"PdhGetCounterInfo failed with status 0x%x.", counterStatus);
    //}

    PhInitializeCircularBuffer_ULONG64(&entry->HistoryBuffer, PhGetIntegerSetting(L"SampleCount"));

    PhAcquireQueuedLockExclusive(&DiskDrivesListLock);
    PhAddItemList(DiskDrivesList, entry);
    PhReleaseQueuedLockExclusive(&DiskDrivesListLock);

    return entry;
}