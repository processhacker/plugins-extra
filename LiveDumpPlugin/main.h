/*
 * Process Hacker Extra Plugins -
 *   Live Dump Plugin
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

#ifndef _LIVEDUMP_H_
#define _LIVEDUMP_H_

#define PLUGIN_NAME L"dmex.LiveDumpPlugin"
#define PLUGIN_MENU_ITEM 1

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phappresource.h>
#include <windowsx.h>
#include <uxtheme.h>
#include "resource.h"

extern PPH_PLUGIN PluginInstance;

VOID ShowLiveDumpConfigDialog(
    _In_opt_ HWND Parent
    );

typedef struct _LIVE_DUMP_CONFIG
{
    HWND WindowHandle;
    PPH_STRING FileName;

    union
    {
        BOOLEAN Flags;
        struct
        {
            BOOLEAN KernelDumpActive : 1;

            BOOLEAN CompressMemoryPages : 1;
            BOOLEAN IncludeUserSpaceMemory : 1;
            BOOLEAN IncludeHypervisorPages : 1;
            BOOLEAN UseDumpStorageStack : 1;

            BOOLEAN Spare : 3;
        };
    };
} LIVE_DUMP_CONFIG, *PLIVE_DUMP_CONFIG;

#endif _LIVEDUMP_H_