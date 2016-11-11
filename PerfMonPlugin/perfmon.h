/*
 * Process Hacker Extra Plugins -
 *   Performance Monitor Plugin
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

#ifndef _PERFMON_H_
#define _PERFMON_H_

#pragma comment(lib, "pdh.lib")

#define PLUGIN_NAME L"dmex.PerfMonPlugin"
#define SETTING_NAME_PERFMON_LIST (PLUGIN_NAME L".CountersList")

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phappresource.h>
#include <pdh.h>
#include <pdhmsg.h>

#include "resource.h"

extern PPH_PLUGIN PluginInstance;

extern PPH_LIST CountersList;
extern PPH_OBJECT_TYPE DiskDriveEntryType;
extern PPH_LIST DiskDrivesList;
extern PH_QUEUED_LOCK DiskDrivesListLock;

typedef struct _PERF_COUNTER_ID
{
    PPH_STRING PerfCounterPath;
} PERF_COUNTER_ID, *PPERF_COUNTER_ID;

typedef struct _DV_DISK_ENTRY
{
    PERF_COUNTER_ID Id;

    HQUERY PerfQueryHandle;
    HCOUNTER PerfCounterHandle;

    ULONG DiskIndex;

    union
    {
        BOOLEAN BitField;
        struct
        {
            BOOLEAN UserReference : 1;
            BOOLEAN HaveFirstSample : 1;
            BOOLEAN Spare : 6;
        };
    };

    PH_UINT64_DELTA HistoryDelta;
    PH_CIRCULAR_BUFFER_ULONG64 HistoryBuffer;
} DV_DISK_ENTRY, *PDV_DISK_ENTRY;



VOID PerfMonInitialize(
    VOID
    );
VOID PerfMonUpdate(
    VOID
    );
VOID InitializeDiskId(
    _Out_ PPERF_COUNTER_ID Id,
    _In_ PPH_STRING DevicePath
    );
VOID CopyDiskId(
    _Out_ PPERF_COUNTER_ID Destination,
    _In_ PPERF_COUNTER_ID Source
    );
VOID DeleteDiskId(
    _Inout_ PPERF_COUNTER_ID Id
    );
BOOLEAN EquivalentDiskId(
    _In_ PPERF_COUNTER_ID Id1,
    _In_ PPERF_COUNTER_ID Id2
    );
PDV_DISK_ENTRY CreateDiskEntry(
    _In_ PPERF_COUNTER_ID Id
    );

VOID PerfMonLoadList(
    VOID
    );
VOID PerfMonSaveList(
    VOID
    );


typedef struct _PH_PERFMON_ENTRY
{
    PPH_STRING Name;
} PH_PERFMON_ENTRY, *PPH_PERFMON_ENTRY;

typedef struct _PH_PERFMON_CONTEXT
{
    BOOLEAN OptionsChanged;
    BOOLEAN AddedChanged;
    HWND ListViewHandle;
} PH_PERFMON_CONTEXT, *PPH_PERFMON_CONTEXT;

typedef struct _PH_PERFMON_SYSINFO_CONTEXT
{
    PDV_DISK_ENTRY Entry;

    HWND WindowHandle;
    HWND GraphHandle;

    PPH_SYSINFO_SECTION SysinfoSection;
    PH_GRAPH_STATE GraphState;
    PH_LAYOUT_MANAGER LayoutManager;
    PH_CALLBACK_REGISTRATION ProcessesUpdatedRegistration;

} PH_PERFMON_SYSINFO_CONTEXT, *PPH_PERFMON_SYSINFO_CONTEXT;

#define ITEM_CHECKED (INDEXTOSTATEIMAGEMASK(2))
#define ITEM_UNCHECKED (INDEXTOSTATEIMAGEMASK(1))

VOID ShowOptionsDialog(
    _In_ HWND ParentHandle
    );

VOID PerfCounterSysInfoInitializing(
    _In_ PPH_PLUGIN_SYSINFO_POINTERS Pointers,
    _In_ PDV_DISK_ENTRY Entry
    );

#endif _ROTHEADER_