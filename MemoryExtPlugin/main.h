/*
 * Process Hacker Extra Plugins -
 *   Memory Extras Plugin
 *
 * Copyright (C) 2017 dmex
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

#pragma once

#define MEM_MENUITEM 1000
#define PLUGIN_NAME L"dmex.MemoryExtPlugin"
#define SETTING_NAME_WINDOW_POSITION (PLUGIN_NAME L".WindowPosition")
#define SETTING_NAME_WINDOW_SIZE (PLUGIN_NAME L".WindowSize")
#define WM_SHOWDIALOG (WM_APP + 1)
#define MEM_LOG_UPDATED (WM_APP + 2)

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phappresource.h>
#include <settings.h>
#include <windowsx.h>
#include "resource.h"

extern PPH_PLUGIN PluginInstance;

typedef struct _ROT_WINDOW_CONTEXT
{
    ULONG ListViewCount;
    HWND WindowHandle;
    HWND ListViewHandle;
    HWND SearchboxHandle;
    PH_LAYOUT_MANAGER LayoutManager;
    PPH_LIST LogMessageList;
    PH_QUEUED_LOCK LogMessageListLock;// = PH_QUEUED_LOCK_INIT;
} ROT_WINDOW_CONTEXT, *PROT_WINDOW_CONTEXT;

typedef struct _PAGE_ENTRY
{
    ULONG Type;
    ULONG List;
    ULONG Priority;
    ULONG_PTR UniqueProcessKey;
    PVOID Address;
    PVOID VirtualAddress;
    PPH_STRING ProcessName;
    PPH_STRING FileName;
} PAGE_ENTRY, *PPAGE_ENTRY;

VOID ShowPageTableWindow(VOID);