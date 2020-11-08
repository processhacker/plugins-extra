/*
 * Process Hacker Extra Plugins -
 *   Graph Explorer Plugin
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

#ifndef _PPH_GRAPHTREE_H_
#define _PPH_GRAPHTREE_H_

#define PLUGIN_NAME L"dmex.GraphExplorerPlugin"
#define SETTING_NAME_WINDOW_POSITION (PLUGIN_NAME L".WindowPosition")
#define SETTING_NAME_WINDOW_SIZE (PLUGIN_NAME L".WindowSize")
#define MSG_UPDATE (WM_APP + 1)

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phappresource.h>
#include <settings.h>
#include <pdh.h>
#include <pdhmsg.h>

#include "resource.h"

extern PPH_PLUGIN PluginInstance;

typedef struct _PH_GRAPHTREE_CONTEXT
{
    HWND WindowHandle;
    HWND SearchHandle;
    HWND TreeNewHandle;
    HFONT NormalFontHandle;
    HFONT TitleFontHandle;
    PH_LAYOUT_MANAGER LayoutManager;

    //PPERF_COUNTER_ENTRY Entry;

    ULONG ProcessesUpdatedCount;
    PH_CALLBACK_REGISTRATION ProcessesUpdatedCallbackRegistration;

    ULONG TreeNewSortColumn;
    PH_SORT_ORDER TreeNewSortOrder;
    PPH_HASHTABLE NodeHashtable;
    PPH_LIST NodeList;

    PPH_STRING SearchboxText;
    PH_TN_FILTER_SUPPORT FilterSupport;
    PPH_TN_FILTER_ENTRY TreeFilterEntry;

    WNDPROC WindowProc;
    POINT LastCursorLocation;
} PH_GRAPHTREE_CONTEXT, *PPH_GRAPHTREE_CONTEXT;

typedef enum _PH_GRAPH_TREE_COLUMN_ITEM
{
    PH_GRAPH_TREE_COLUMN_ITEM_NAME,
    PH_GRAPH_TREE_COLUMN_ITEM_VERSION,
    PH_GRAPH_TREE_COLUMN_ITEM_MAXIMUM
} PH_GRAPH_TREE_COLUMN_ITEM;

typedef struct _PH_GRAPH_TREE_ROOT_NODE
{
    PH_TREENEW_NODE Node;
    ULONG64 Index;
    ULONG Sequence;
    PH_GRAPH_BUFFERS GraphBuffers;

    PPH_PROCESS_ITEM ProcessItem;
    //PPERF_COUNTER_ENTRY CounterEntry;

    PPH_STRING Name;
    PPH_STRING TooltipText;
    PH_STRINGREF TextCache[PH_GRAPH_TREE_COLUMN_ITEM_MAXIMUM];
} PH_GRAPH_TREE_ROOT_NODE, *PPH_GRAPH_TREE_ROOT_NODE;

extern PPH_GRAPHTREE_CONTEXT WindowContext;

VOID ShowGraphDialog(
    VOID
    );

PPH_GRAPH_TREE_ROOT_NODE AddProcessNode(
    _Inout_ PPH_GRAPHTREE_CONTEXT Context,
    _In_ PPH_PROCESS_ITEM ProcessItem
    );

VOID RemoveGraphNode(
    _In_ PPH_GRAPHTREE_CONTEXT Context,
    _In_ PPH_GRAPH_TREE_ROOT_NODE Node
    );

#endif _PPH_GRAPHTREE_H_
