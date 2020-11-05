/*
 * Process Hacker Extra Plugins -
 *   Nvidia GPU Plugin
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

#ifndef _NVGPU_H_
#define _NVGPU_H_

#define PLUGIN_NAME L"dmex.NvGpuPlugin"
#define SETTING_NAME_ENABLE_FAHRENHEIT (PLUGIN_NAME L".EnableFahrenheit")

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phappresource.h>
#include <settings.h>
#include <verify.h>
#include <windowsx.h>
#include "resource.h"

#define MSG_UPDATE (WM_APP + 1)

extern BOOLEAN NvApiInitialized;
extern PPH_PLUGIN PluginInstance;

extern ULONG GpuMemoryLimit;
extern FLOAT GpuCurrentGpuUsage;
extern FLOAT GpuCurrentCoreUsage;
extern FLOAT GpuCurrentBusUsage;
extern ULONG GpuCurrentMemUsage;
extern ULONG GpuCurrentMemSharedUsage;
extern ULONG GpuCurrentCoreTemp;
extern ULONG GpuCurrentBoardTemp;
extern ULONG GpuCurrentCoreClock;
extern ULONG GpuCurrentMemoryClock;
extern ULONG GpuCurrentShaderClock;
extern ULONG GpuCurrentVoltage;
extern ULONG GpuPcieThroughputTx;
extern ULONG GpuPcieThroughputRx;
extern ULONG GpuPciePowerUsage;

BOOLEAN InitializeNvApi(VOID);
BOOLEAN DestroyNvApi(VOID);
PPH_STRING NvGpuQueryDriverVersion(VOID);
PPH_STRING NvGpuQueryVbiosVersionString(VOID);
PPH_STRING NvGpuQueryName(VOID);
PPH_STRING NvGpuQueryShortName(VOID);
PPH_STRING NvGpuQueryRevision(VOID);
PPH_STRING NvGpuQueryRamType(VOID);
PPH_STRING NvGpuQueryFoundry(VOID);
PPH_STRING NvGpuQueryDeviceId(VOID);
PPH_STRING NvGpuQueryRopsCount(VOID);
PPH_STRING NvGpuQueryShaderCount(VOID);
PPH_STRING NvGpuQueryPciInfo(VOID);
PPH_STRING NvGpuQueryBusWidth(VOID);
PPH_STRING NvGpuQueryDriverSettings(VOID);
PPH_STRING NvGpuQueryFanSpeed(VOID);
BOOLEAN NvGpuDriverIsWHQL(VOID);
VOID NvGpuUpdateValues(VOID);
VOID NvGpuUpdateVoltage(VOID);

typedef struct _PH_NVGPU_SYSINFO_CONTEXT
{
    PPH_STRING GpuName;
    HWND WindowHandle;
    PPH_SYSINFO_SECTION Section;
    PH_LAYOUT_MANAGER LayoutManager;

    RECT GpuGraphMargin;
    HWND GpuPanel;

    HWND GpuLabelHandle;
    HWND MemLabelHandle;
    HWND SharedLabelHandle;
    HWND BusLabelHandle;

    HWND GpuGraphHandle;
    HWND MemGraphHandle;
    HWND SharedGraphHandle;
    HWND BusGraphHandle;

    PH_GRAPH_STATE GpuGraphState;
    PH_GRAPH_STATE MemGraphState;
    PH_GRAPH_STATE SharedGraphState;
    PH_GRAPH_STATE BusGraphState;
} PH_NVGPU_SYSINFO_CONTEXT, *PPH_NVGPU_SYSINFO_CONTEXT;

extern PH_CIRCULAR_BUFFER_FLOAT GpuUtilizationHistory;
extern PH_CIRCULAR_BUFFER_ULONG GpuMemoryHistory;
extern PH_CIRCULAR_BUFFER_FLOAT GpuBoardHistory;
extern PH_CIRCULAR_BUFFER_FLOAT GpuBusHistory;

VOID NvGpuInitialize(
    VOID
    );

VOID NvGpuUpdate(
    VOID
    );

VOID NvGpuSysInfoInitializing(
    _In_ PPH_PLUGIN_SYSINFO_POINTERS Pointers
    );

VOID ShowDetailsDialog(
    _In_ HWND ParentHandle,
    _In_ PVOID Context
    );

#endif _NVGPU_H_
