/*
 * Process Hacker Extra Plugins -
 *   CPU graph plugins
 *
 * Copyright (C) 2020 dmex
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

#ifndef _CPUPLUGIN_H_
#define _CPUPLUGIN_H_

#include <phdk.h>
#include <phappresource.h>
#include <settings.h>
#include <windowsx.h>
#include "resource.h"

#define PLUGIN_NAME L"dmex.ExtraCpuGraphsPlugin"
#define MSG_UPDATE (WM_APP + 1)

typedef struct _PH_CPUPLUGIN_SYSINFO_CONTEXT
{
    HWND WindowHandle;
    PPH_SYSINFO_SECTION Section;
    PH_LAYOUT_MANAGER LayoutManager;

    RECT ContextSwitchesGraphMargin;
    HWND ContextSwitchesLabelHandle;
    HWND InterruptsLabelHandle;
    HWND DpcsLabelHandle;
    HWND SystemCallsLabelHandle;

    HWND ContextSwitchesGraphHandle;
    HWND InterruptsGraphHandle;
    HWND DpcsGraphHandle;
    HWND SystemCallsGraphHandle;

    PH_GRAPH_STATE ContextSwitchesGraphState;
    PH_GRAPH_STATE InterruptsGraphState;
    PH_GRAPH_STATE DpcsGraphState;
    PH_GRAPH_STATE SystemCallsGraphState;
} PH_CPUPLUGIN_SYSINFO_CONTEXT, *PPH_CPUPLUGIN_SYSINFO_CONTEXT;

extern PPH_PLUGIN PluginInstance;
extern PH_UINT64_DELTA ContextSwitchesDelta;
extern PH_UINT64_DELTA InterruptsDelta;
extern PH_UINT64_DELTA DpcsDelta;
extern PH_UINT64_DELTA SystemCallsDelta;
extern PH_CIRCULAR_BUFFER_ULONG64 ContextSwitchesHistory;
extern PH_CIRCULAR_BUFFER_ULONG64 InterruptsHistory;
extern PH_CIRCULAR_BUFFER_ULONG64 DpcsHistory;
extern PH_CIRCULAR_BUFFER_ULONG64 SystemCallsHistory;

VOID CpuPluginSysInfoInitializing(
    _In_ PPH_PLUGIN_SYSINFO_POINTERS Pointers
    );

#endif _CPUPLUGIN_H_
