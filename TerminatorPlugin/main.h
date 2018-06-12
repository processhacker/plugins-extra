/*
 * Process Hacker Extra Plugins -
 *   Terminator Plugin
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

#ifndef _T_H_
#define _T_H_

#define PLUGIN_NAME L"dmex.TerminatorPlugin"
#define KPH_PATH32 L"plugins\\plugindata\\kprocesshacker2_x32.sys"
#define KPH_PATH64 L"plugins\\plugindata\\kprocesshacker2_x64.sys"
#define TERMINATOR_MENU_ITEM 1

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phappresource.h>
#include <windowsx.h>

#include "resource.h"

typedef struct _TERMINATOR_CONTEXT
{
    HWND WindowHandle;
    PH_LAYOUT_MANAGER LayoutManager;
    PPH_PROCESS_ITEM ProcessItem;
} TERMINATOR_CONTEXT, *PTERMINATOR_CONTEXT;

typedef NTSTATUS (NTAPI *PTEST_PROC)(
    _In_ HANDLE ProcessId
    );

typedef struct _TEST_ITEM
{
    PWSTR Id;
    PWSTR Description;
    PTEST_PROC TestProc;
} TEST_ITEM, *PTEST_ITEM;

extern PPH_PLUGIN PluginInstance;
extern TEST_ITEM PhTerminatorTests[17];

VOID PhShowProcessTerminatorDialog(
    _In_ HWND ParentWindowHandle,
    _In_ PPH_PROCESS_ITEM ProcessItem
    );

BOOLEAN PhpRunTerminatorTest(
    _In_ HWND WindowHandle,
    _In_ PPH_PROCESS_ITEM ProcessItem,
    _In_ INT Index
    );

#endif _T_H_