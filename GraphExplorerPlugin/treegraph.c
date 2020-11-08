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

#include "main.h"

#define WM_MSG_WINDOW_PROPERTIES (WM_APP + 1)

static HANDLE PhGraphThreadHandle = NULL;
static HWND PhGraphWindowHandle = NULL;
static PH_EVENT PhGraphInitializedEvent = PH_EVENT_INIT;
PPH_GRAPHTREE_CONTEXT WindowContext = NULL;

#pragma region Plugin TreeList

#define SORT_FUNCTION(Column) GraphTreeNewCompare##Column
#define BEGIN_SORT_FUNCTION(Column) static int __cdecl GraphTreeNewCompare##Column( \
    _In_ void *_context, \
    _In_ const void *_elem1, \
    _In_ const void *_elem2 \
    ) \
{ \
    PPH_GRAPH_TREE_ROOT_NODE node1 = *(PPH_GRAPH_TREE_ROOT_NODE*)_elem1; \
    PPH_GRAPH_TREE_ROOT_NODE node2 = *(PPH_GRAPH_TREE_ROOT_NODE*)_elem2; \
    int sortResult = 0;

#define END_SORT_FUNCTION \
    if (sortResult == 0) \
        sortResult = uintptrcmp((ULONG_PTR)node1->Index, (ULONG_PTR)node2->Index); \
    \
    return PhModifySort(sortResult, ((PPH_GRAPHTREE_CONTEXT)_context)->TreeNewSortOrder); \
}

BEGIN_SORT_FUNCTION(Name)
{
    //if (node1->ProcessItem->VmCounters.PeakPagefileUsage != 0 && node2->ProcessItem->VmCounters.PeakPagefileUsage)
    //{
    //    sortResult = uintptrcmp(node1->ProcessItem->VmCounters.PeakPagefileUsage, node2->ProcessItem->VmCounters.PeakPagefileUsage);
    //}
    //else
    //{
    //    sortResult = uintptrcmp(
    //        node1->ProcessItem->IoReadDelta.Value + node1->ProcessItem->IoWriteDelta.Value + node1->ProcessItem->IoOtherDelta.Value,
    //        node2->ProcessItem->IoReadDelta.Value + node2->ProcessItem->IoWriteDelta.Value + node2->ProcessItem->IoOtherDelta.Value);
    //}

    sortResult = uintptrcmp((ULONG_PTR)node1->ProcessItem->ProcessId, (ULONG_PTR)node2->ProcessItem->ProcessId);
    //sortResult = uintptrcmp(node1->CounterEntry->HistoryDelta.Value, node2->CounterEntry->HistoryDelta.Value);
}
END_SORT_FUNCTION

//VOID GraphLoadSettingsTreeList(
//    _Inout_ PPH_GRAPHTREE_CONTEXT Context
//    )
//{
//    PPH_STRING settings;
//    
//    settings = PhGetStringSetting(L"TreeListColumns");
//    PhCmLoadSettings(Context->TreeNewHandle, &settings->sr);
//    PhDereferenceObject(settings);
//}
//
//VOID GraphSaveSettingsTreeList(
//    _Inout_ PPH_GRAPHTREE_CONTEXT Context
//    )
//{
//    PPH_STRING settings;
//
//    settings = PhCmSaveSettings(Context->TreeNewHandle);
//    PhSetStringSetting2(L"TreeListColumns", &settings->sr);
//    PhDereferenceObject(settings);
//}

BOOLEAN GraphNodeHashtableEqualFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
    )
{
    PPH_GRAPH_TREE_ROOT_NODE node1 = *(PPH_GRAPH_TREE_ROOT_NODE *)Entry1;
    PPH_GRAPH_TREE_ROOT_NODE node2 = *(PPH_GRAPH_TREE_ROOT_NODE *)Entry2;

    return node1->Index == node2->Index;
}

ULONG GraphNodeHashtableHashFunction(
    _In_ PVOID Entry
    )
{
    return PhHashInt64((*(PPH_GRAPH_TREE_ROOT_NODE*)Entry)->Index);
}

VOID DestroyGraphNode(
    _In_ PPH_GRAPH_TREE_ROOT_NODE Node
    )
{
    PhClearReference(&Node->Name);
    PhClearReference(&Node->TooltipText);

    PhFree(Node);
}

PPH_GRAPH_TREE_ROOT_NODE AddProcessNode(
    _Inout_ PPH_GRAPHTREE_CONTEXT Context,
    _In_ PPH_PROCESS_ITEM ProcessItem
    )
{
    static ULONG64 Index = 0;
    PPH_GRAPH_TREE_ROOT_NODE node;

    node = PhAllocateZero(sizeof(PH_GRAPH_TREE_ROOT_NODE));
    node->Index = ++Index;
    node->ProcessItem = ProcessItem;
    //node->Sequence = PhGraphSequence;

    PhInitializeTreeNewNode(&node->Node);

    memset(node->TextCache, 0, sizeof(PH_STRINGREF) * PH_GRAPH_TREE_COLUMN_ITEM_MAXIMUM);
    node->Node.TextCache = node->TextCache;
    node->Node.TextCacheSize = PH_GRAPH_TREE_COLUMN_ITEM_MAXIMUM;

    PhAddEntryHashtable(Context->NodeHashtable, &node);
    PhAddItemList(Context->NodeList, node);

    if (Context->FilterSupport.FilterList)
        node->Node.Visible = PhApplyTreeNewFiltersToNode(&Context->FilterSupport, &node->Node);

    TreeNew_NodesStructured(Context->TreeNewHandle);

    return node;
}

//PPH_PLUGIN_TREE_ROOT_NODE AddPluginsNode(
//    _Inout_ PPH_GRAPHTREE_CONTEXT Context,
//    _In_ PPERF_COUNTER_ENTRY CounterEntry
//    )
//{
//    static ULONG64 Index = 0;
//    PPH_PLUGIN_TREE_ROOT_NODE node;
//
//    node = PhAllocate(sizeof(PH_PLUGIN_TREE_ROOT_NODE));
//    memset(node, 0, sizeof(PH_PLUGIN_TREE_ROOT_NODE));
//    node->Index = ++Index;
//    node->CounterEntry = CounterEntry;
//
//    PhInitializeTreeNewNode(&node->Node);
//
//    memset(node->TextCache, 0, sizeof(PH_STRINGREF) * PH_PLUGIN_TREE_COLUMN_ITEM_MAXIMUM);
//    node->Node.TextCache = node->TextCache;
//    node->Node.TextCacheSize = PH_PLUGIN_TREE_COLUMN_ITEM_MAXIMUM;
//
//    PhAddEntryHashtable(Context->NodeHashtable, &node);
//    PhAddItemList(Context->NodeList, node);
//
//    //TreeNew_NodesStructured(Context->TreeNewHandle);
//
//    return node;
//}

PPH_GRAPH_TREE_ROOT_NODE FindGraphNode(
    _In_ PPH_GRAPHTREE_CONTEXT Context,
    _In_ ULONG64 Index
    )
{
    PH_GRAPH_TREE_ROOT_NODE lookupGraphNode;
    PPH_GRAPH_TREE_ROOT_NODE lookupGraphNodePtr = &lookupGraphNode;
    PPH_GRAPH_TREE_ROOT_NODE*pluginsNode;

    lookupGraphNode.Index = Index;

    pluginsNode = (PPH_GRAPH_TREE_ROOT_NODE*)PhFindEntryHashtable(
        Context->NodeHashtable,
        &lookupGraphNodePtr
        );

    if (pluginsNode)
        return *pluginsNode;
    else
        return NULL;
}

VOID RemoveGraphNode(
    _In_ PPH_GRAPHTREE_CONTEXT Context,
    _In_ PPH_GRAPH_TREE_ROOT_NODE Node
    )
{
    ULONG index = 0;

    PhRemoveEntryHashtable(Context->NodeHashtable, &Node);

    if ((index = PhFindItemList(Context->NodeList, Node)) != ULONG_MAX)
    {
        PhRemoveItemList(Context->NodeList, index);
    }

    DestroyGraphNode(Node);
    TreeNew_NodesStructured(Context->TreeNewHandle);
}

VOID UpdateGraphNode(
    _In_ PPH_GRAPHTREE_CONTEXT Context,
    _In_ PPH_GRAPH_TREE_ROOT_NODE Node
    )
{
    memset(Node->TextCache, 0, sizeof(PH_STRINGREF) * PH_GRAPH_TREE_COLUMN_ITEM_MAXIMUM);
    PhInvalidateTreeNewNode(&Node->Node, TN_CACHE_COLOR | TN_CACHE_ICON);
    TreeNew_InvalidateNode(Context->TreeNewHandle, &Node->Node);

    TreeNew_NodesStructured(Context->TreeNewHandle);
}

static HDC GraphContext = NULL;
static ULONG GraphContextWidth = 0;
static ULONG GraphContextHeight = 0;
static HBITMAP GraphOldBitmap;
static HBITMAP GraphBitmap = NULL;
static PVOID GraphBits = NULL;

static VOID PhpNeedGraphContext(
    _In_ HDC hdc,
    _In_ ULONG Width,
    _In_ ULONG Height
)
{
    BITMAPINFOHEADER header;

    // If we already have a graph context and it's the right size, then return immediately.
    if (GraphContextWidth == Width && GraphContextHeight == Height)
        return;

    if (GraphContext)
    {
        // The original bitmap must be selected back into the context, otherwise
        // the bitmap can't be deleted.
        SelectBitmap(GraphContext, GraphBitmap);
        DeleteBitmap(GraphBitmap);
        DeleteDC(GraphContext);

        GraphContext = NULL;
        GraphBitmap = NULL;
        GraphBits = NULL;
    }

    GraphContext = CreateCompatibleDC(hdc);

    memset(&header, 0, sizeof(BITMAPINFOHEADER));
    header.biSize = sizeof(BITMAPINFOHEADER);
    header.biWidth = Width;
    header.biHeight = Height;
    header.biPlanes = 1;
    header.biBitCount = 32;

    if (GraphBitmap = CreateDIBSection(hdc, (BITMAPINFO*)&header, DIB_RGB_COLORS, &GraphBits, NULL, 0))
    {
        GraphOldBitmap = SelectBitmap(GraphContext, GraphBitmap);
    }
}

RECT NormalGraphTextMargin = { 5, 5, 5, 5 };
RECT NormalGraphTextPadding = { 3, 3, 3, 3 };

BOOLEAN NTAPI GraphTreeNewCallback(
    _In_ HWND hwnd,
    _In_ PH_TREENEW_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2,
    _In_opt_ PVOID Context
    )
{
    PPH_GRAPHTREE_CONTEXT context = Context;
    PPH_GRAPH_TREE_ROOT_NODE node;

    if (!context)
        return FALSE;

    switch (Message)
    {
    case TreeNewGetChildren:
        {
            PPH_TREENEW_GET_CHILDREN getChildren = Parameter1;

            if (!getChildren)
                break;

            node = (PPH_GRAPH_TREE_ROOT_NODE)getChildren->Node;

            if (!getChildren->Node)
            {
                static PVOID sortFunctions[] =
                {
                    SORT_FUNCTION(Name)
                };
                int (__cdecl *sortFunction)(void*, const void*, const void*);

                if (context->TreeNewSortColumn < PH_GRAPH_TREE_COLUMN_ITEM_MAXIMUM)
                    sortFunction = sortFunctions[context->TreeNewSortColumn];
                else
                    sortFunction = NULL;

                if (sortFunction)
                {
                    qsort_s(context->NodeList->Items, context->NodeList->Count, sizeof(PVOID), sortFunction, context);
                }

                getChildren->Children = (PPH_TREENEW_NODE *)context->NodeList->Items;
                getChildren->NumberOfChildren = context->NodeList->Count;
            }
        }
        return TRUE;
    case TreeNewIsLeaf:
        {
            PPH_TREENEW_IS_LEAF isLeaf = (PPH_TREENEW_IS_LEAF)Parameter1;

            if (!isLeaf)
                break;

            node = (PPH_GRAPH_TREE_ROOT_NODE)isLeaf->Node;
            isLeaf->IsLeaf = TRUE;
        }
        return TRUE;
    case TreeNewGetCellText:
        {
            PPH_TREENEW_GET_CELL_TEXT getCellText = (PPH_TREENEW_GET_CELL_TEXT)Parameter1;

            if (!getCellText)
                break;

            node = (PPH_GRAPH_TREE_ROOT_NODE)getCellText->Node;

            switch (getCellText->Id)
            {
            case PH_GRAPH_TREE_COLUMN_ITEM_NAME:
                getCellText->Text = PhGetStringRef(node->Name);
                break;
            default:
                return FALSE;
            }

            getCellText->Flags = TN_CACHE;
        }
        return TRUE;
    case TreeNewGetNodeColor:
        {
            PPH_TREENEW_GET_NODE_COLOR getNodeColor = Parameter1;

            if (!getNodeColor)
                break;

            node = (PPH_GRAPH_TREE_ROOT_NODE)getNodeColor->Node;

            getNodeColor->Flags = TN_CACHE | TN_AUTO_FORECOLOR;
        }
        return TRUE;
    case TreeNewSortChanged:
        {
            TreeNew_GetSort(hwnd, &context->TreeNewSortColumn, &context->TreeNewSortOrder);
            // Force a rebuild to sort the items.
            TreeNew_NodesStructured(hwnd);
        }
        return TRUE;
    case TreeNewKeyDown:
        {
            PPH_TREENEW_KEY_EVENT keyEvent = Parameter1;

            if (!keyEvent)
                break;

            switch (keyEvent->VirtualKey)
            {
            case 'C':
                //if (GetKeyState(VK_CONTROL) < 0)
                // SendMessage(context->WindowHandle, WM_COMMAND, ID_OBJECT_COPY, 0);
                break;
            case 'A':
                if (GetKeyState(VK_CONTROL) < 0)
                    TreeNew_SelectRange(context->TreeNewHandle, 0, -1);
                break;
            case VK_DELETE:
                //SendMessage(context->WindowHandle, WM_COMMAND, ID_OBJECT_CLOSE, 0);
                break;
            }
        }
        return TRUE;
    case TreeNewLeftDoubleClick:
        {
            PPH_TREENEW_MOUSE_EVENT mouseEvent = Parameter1;

            SendMessage(context->WindowHandle, WM_COMMAND, WM_MSG_WINDOW_PROPERTIES, (LPARAM)mouseEvent);
        }
        return TRUE;
    case TreeNewContextMenu:
        {
            PPH_TREENEW_CONTEXT_MENU contextMenuEvent = Parameter1;

            //SendMessage(context->WindowHandle, WM_COMMAND, ID_SHOWCONTEXTMENU, (LPARAM)contextMenuEvent);
        }
        return TRUE;
    //case TreeNewHeaderRightClick:
    //    {
    //        PH_TN_COLUMN_MENU_DATA data;
    //
    //        data.TreeNewHandle = hwnd;
    //        data.MouseEvent = Parameter1;
    //        data.DefaultSortColumn = 0;
    //        data.DefaultSortOrder = AscendingSortOrder;
    //        PhInitializeTreeNewColumnMenu(&data);
    //
    //        data.Selection = PhShowEMenu(data.Menu, hwnd, PH_EMENU_SHOW_LEFTRIGHT,
    //            PH_ALIGN_LEFT | PH_ALIGN_TOP, data.MouseEvent->ScreenLocation.x, data.MouseEvent->ScreenLocation.y);
    //        PhHandleTreeNewColumnMenu(&data);
    //        PhDeleteTreeNewColumnMenu(&data);
    //    }
    //    return TRUE;
    case TreeNewCustomDraw:
        {
            PPH_TREENEW_CUSTOM_DRAW customDraw = Parameter1;
            RECT rect;

            if (!customDraw)
                break;

            rect = customDraw->CellRect;
            node = (PPH_GRAPH_TREE_ROOT_NODE)customDraw->Node;

            switch (customDraw->Column->Id)
            {
            case PH_GRAPH_TREE_COLUMN_ITEM_NAME:
                {
                    PH_GRAPH_DRAW_INFO drawInfo;

                    if (rect.right - rect.left <= 1) // nothing to draw
                        break;

                    memset(&drawInfo, 0, sizeof(PH_GRAPH_DRAW_INFO));
                    drawInfo.Width = rect.right - rect.left - 10;
                    drawInfo.Height = rect.bottom - rect.top - 10;
                    drawInfo.GridXOffset = node->Sequence ; // increment
                    drawInfo.TextFont = context->NormalFontHandle;
                    //drawInfo.GridWidth = 20;
                    drawInfo.Flags = PH_GRAPH_USE_GRID_X | PH_GRAPH_USE_GRID_Y | PH_GRAPH_USE_LINE_2;
                    drawInfo.Step = 2;
                    //drawInfo.BackColor = RGB(0xef, 0xef, 0xef);
                    drawInfo.BackColor = RGB(0x00, 0x00, 0x00);
                    drawInfo.LineDataCount = 0;
                    drawInfo.LineData1 = NULL;
                    drawInfo.LineData2 = NULL;
                    //drawInfo.LineColor1 = RGB(0x00, 0xff, 0x00);
                    //drawInfo.LineColor2 = RGB(0xff, 0x00, 0x00);
                    //drawInfo.LineBackColor1 = RGB(0x00, 0x77, 0x00);
                    //drawInfo.LineBackColor2 = RGB(0x77, 0x00, 0x00);
                    drawInfo.GridColor = RGB(0xc7, 0xc7, 0xc7);
                    drawInfo.GridWidth = 20;
                    drawInfo.GridHeight = 0.25f;
                    drawInfo.GridYThreshold = 10;
                    drawInfo.GridBase = 2.0f;
                    drawInfo.LabelYColor = RGB(0x77, 0x77, 0x77);
                    drawInfo.LabelMaxYIndexLimit = -1;
                    drawInfo.TextColor = RGB(0x00, 0xff, 0x00);
                    drawInfo.TextBoxColor = RGB(0x00, 0x22, 0x00);

                    switch (PhGetIntegerSetting(L"GraphColorMode"))
                    {
                    case 0: // New colors
                        {
                            drawInfo.BackColor = RGB(0xef, 0xef, 0xef);
                            drawInfo.GridColor = RGB(0xc7, 0xc7, 0xc7);
                            drawInfo.LabelYColor = RGB(0xa0, 0x60, 0x20);
                            drawInfo.TextColor = RGB(0x00, 0x00, 0x00);
                            drawInfo.TextBoxColor = RGB(0xe7, 0xe7, 0xe7);
                            ULONG value = PhGetIntegerSetting(L"ColorIoReadOther");
                            drawInfo.LineBackColor1 = PhMakeColorBrighter(value, 125);
                            drawInfo.LineColor1 = PhHalveColorBrightness(value);
                            value = PhGetIntegerSetting(L"ColorIoWrite");
                            drawInfo.LineBackColor2 = PhMakeColorBrighter(value, 125);
                            drawInfo.LineColor2 = PhHalveColorBrightness(value);
                        }
                        break;
                    case 1: // Old colors
                        {
                            drawInfo.BackColor = RGB(0x00, 0x00, 0x00);
                            drawInfo.GridColor = RGB(0x00, 0x57, 0x00);
                            drawInfo.LabelYColor = RGB(0xd0, 0xa0, 0x70);
                            drawInfo.TextColor = RGB(0x00, 0xff, 0x00);
                            drawInfo.TextBoxColor = RGB(0x00, 0x22, 0x00);
                            drawInfo.LineColor1 = PhGetIntegerSetting(L"ColorIoReadOther");
                            drawInfo.LineBackColor1 = PhHalveColorBrightness(drawInfo.LineColor1);
                            drawInfo.LineColor2 = PhGetIntegerSetting(L"ColorIoWrite");
                            drawInfo.LineBackColor2 = PhHalveColorBrightness(drawInfo.LineColor2);
                        }
                        break;
                    }
                    
                    PhGetDrawInfoGraphBuffers(
                        &node->GraphBuffers,
                        &drawInfo,
                        node->ProcessItem->IoReadHistory.Count // node->CounterEntry->HistoryBuffer.Count
                        );

                    if (!node->GraphBuffers.Valid)
                    {
                        ULONG i;
                        FLOAT max = 0;

                        for (i = 0; i < drawInfo.LineDataCount; i++)
                        {
                            FLOAT data1;
                            FLOAT data2;

                            node->GraphBuffers.Data1[i] = data1 =
                                (FLOAT)PhGetItemCircularBuffer_ULONG64(&node->ProcessItem->IoReadHistory, i) +
                                (FLOAT)PhGetItemCircularBuffer_ULONG64(&node->ProcessItem->IoOtherHistory, i);
                            node->GraphBuffers.Data2[i] = data2 =
                                (FLOAT)PhGetItemCircularBuffer_ULONG64(&node->ProcessItem->IoWriteHistory, i);

                            if (max < data1 + data2)
                                max = data1 + data2;
                        }

                        if (max != 0)
                        {
                            // Scale the data.

                            PhDivideSinglesBySingle(
                                node->GraphBuffers.Data1,
                                max,
                                drawInfo.LineDataCount
                            );
                            PhDivideSinglesBySingle(
                                node->GraphBuffers.Data2,
                                max,
                                drawInfo.LineDataCount
                            );
                        }

                        drawInfo.LabelYFunction = PhSiSizeLabelYFunction;
                        drawInfo.LabelYFunctionParameter = max;

                        node->GraphBuffers.Valid = TRUE;
                    }

                    //if (!node->GraphBuffers.Valid)
                    //{
                    //    for (ULONG i = 0; i < drawInfo.LineDataCount; i++)
                    //    {
                    //        node->GraphBuffers.Data1[i] = (FLOAT)PhGetItemCircularBuffer_SIZE_T(&node->ProcessItem->PrivateBytesHistory, i);
                    //    }
                    //    if (node->ProcessItem->VmCounters.PeakPagefileUsage != 0)
                    //    {
                    //        PhDivideSinglesBySingle(node->GraphBuffers.Data1, (FLOAT)node->ProcessItem->VmCounters.PeakPagefileUsage, drawInfo.LineDataCount);
                    //    }
                    //    node->GraphBuffers.Valid = TRUE;
                    //}
                    //if (!node->GraphBuffers.Valid)
                    //{
                    //    PhCopyCircularBuffer_FLOAT(&node->ProcessItem->PrivateBytesHistory, node->GraphBuffers.Data1, drawInfo.LineDataCount);
                    //    node->GraphBuffers.Valid = TRUE;
                    //}
                    //if (!node->GraphBuffers.Valid)
                    //{
                    //    ULONG i;
                    //    FLOAT total = 0;
                    //    FLOAT max = 1000;
                    //    for (i = 0; i < drawInfo.LineDataCount; i++)
                    //    {
                    //        FLOAT data1;
                    //        node->GraphBuffers.Data1[i] = data1 = (FLOAT)PhGetItemCircularBuffer_ULONG64(&node->CounterEntry->HistoryBuffer, i);
                    //        if (max < data1)
                    //            max = data1;
                    //    }
                    //    if (max < total)
                    //        max = total;
                    //    if (max != 0)
                    //    {
                    //        PhDivideSinglesBySingle(node->GraphBuffers.Data1, max, drawInfo.LineDataCount);
                    //    }
                    //    node->GraphBuffers.Valid = TRUE;
                    //}

                    PhpNeedGraphContext(customDraw->Dc, drawInfo.Width, drawInfo.Height);

                    if (PhGetIntegerSetting(L"GraphShowText"))
                    {
                        PhSetGraphText(customDraw->Dc, &drawInfo, &node->ProcessItem->ProcessName->sr,
                            &NormalGraphTextMargin, &NormalGraphTextPadding, PH_ALIGN_TOP | PH_ALIGN_LEFT);
                        //PhSetGraphText(customDraw->Dc, &drawInfo, &node->CounterEntry->Id.PerfCounterPath->sr,
                        //    &NormalGraphTextMargin, &NormalGraphTextPadding, PH_ALIGN_TOP | PH_ALIGN_LEFT);
                    }

                    if (GraphBits)
                    {
                        PhDrawGraphDirect(GraphContext, GraphBits, &drawInfo);

                        // draw border
                        rect.left += 4;
                        rect.top += 4;
                        rect.right -= 4;
                        rect.bottom -= 5; // graph draws 1px line at bottom. draw under it.
                        SetDCBrushColor(customDraw->Dc, RGB(65, 65, 65));
                        FrameRect(customDraw->Dc, &rect, GetStockBrush(DC_BRUSH));
                        rect.bottom += 5;
                        rect.right += 4;
                        rect.top -= 4;
                        rect.left -= 4;

                        BitBlt(
                            customDraw->Dc,
                            rect.left + 5,
                            rect.top + 5,
                            drawInfo.Width,
                            drawInfo.Height,
                            GraphContext,
                            0,
                            0,
                            SRCCOPY
                            );
                    }

                }
                break;
            }
        }
        return TRUE;
    case TreeNewColumnResized:
        {
            PPH_TREENEW_COLUMN column = Parameter1;
            ULONG i;

            if (!column)
                break;

            if (column->Id == PH_GRAPH_TREE_COLUMN_ITEM_NAME)
            {
                for (i = 0; i < context->NodeList->Count; i++)
                {
                    node = context->NodeList->Items[i];

                    if (column->Id == PH_GRAPH_TREE_COLUMN_ITEM_NAME)
                        node->GraphBuffers.Valid = FALSE;
                }
            }
        }
        return TRUE;
    }

    return FALSE;
}

VOID ClearGraphTree(
    _In_ PPH_GRAPHTREE_CONTEXT Context
    )
{
    for (ULONG i = 0; i < Context->NodeList->Count; i++)
        DestroyGraphNode(Context->NodeList->Items[i]);

    PhClearHashtable(Context->NodeHashtable);
    PhClearList(Context->NodeList);

    TreeNew_NodesStructured(Context->TreeNewHandle);
}

PPH_GRAPH_TREE_ROOT_NODE GetSelectedGraphNode(
    _In_ PPH_GRAPHTREE_CONTEXT Context
    )
{
    PPH_GRAPH_TREE_ROOT_NODE windowNode = NULL;

    for (ULONG i = 0; i < Context->NodeList->Count; i++)
    {
        windowNode = Context->NodeList->Items[i];

        if (windowNode->Node.Selected)
            return windowNode;
    }

    return NULL;
}

VOID GetSelectedGraphNodes(
    _In_ PPH_GRAPHTREE_CONTEXT Context,
    _Out_ PPH_GRAPH_TREE_ROOT_NODE** GraphNodes,
    _Out_ PULONG NumberOfGraphNodes
    )
{
    PPH_LIST list;

    list = PhCreateList(2);

    for (ULONG i = 0; i < Context->NodeList->Count; i++)
    {
        PPH_GRAPH_TREE_ROOT_NODE node = (PPH_GRAPH_TREE_ROOT_NODE)Context->NodeList->Items[i];

        if (node->Node.Selected)
        {
            PhAddItemList(list, node);
        }
    }

    *GraphNodes = PhAllocateCopy(list->Items, sizeof(PVOID) * list->Count);
    *NumberOfGraphNodes = list->Count;

    PhDereferenceObject(list);
}

VOID InitializeGraphTree(
    _Inout_ PPH_GRAPHTREE_CONTEXT Context
    )
{
    Context->NodeList = PhCreateList(100);
    Context->NodeHashtable = PhCreateHashtable(
        sizeof(PPH_GRAPH_TREE_ROOT_NODE),
        GraphNodeHashtableEqualFunction,
        GraphNodeHashtableHashFunction,
        100
        );

    //Context->NormalFontHandle = PhCreateFont(L"Microsoft Sans Serif", 9, FW_NORMAL);
    //Context->NormalFontHandle = PhCreateCommonFont(8, FW_NORMAL, Context->TreeNewHandle);
    Context->NormalFontHandle = PhCreateCommonFont(-14, FW_SEMIBOLD, Context->TreeNewHandle);

    PhSetControlTheme(Context->TreeNewHandle, L"explorer");
    TreeNew_SetCallback(Context->TreeNewHandle, GraphTreeNewCallback, Context);
    TreeNew_SetRowHeight(Context->TreeNewHandle, PH_SCALE_DPI(120));

    PhAddTreeNewColumnEx2(Context->TreeNewHandle, PH_GRAPH_TREE_COLUMN_ITEM_NAME, TRUE, L"Processes", 80, PH_ALIGN_LEFT, 0, 0, TN_COLUMN_FLAG_CUSTOMDRAW);

    TreeNew_SetSort(Context->TreeNewHandle, PH_GRAPH_TREE_COLUMN_ITEM_NAME, AscendingSortOrder);
    //GraphLoadSettingsTreeList(Context);

    PhInitializeTreeNewFilterSupport(&Context->FilterSupport, Context->TreeNewHandle, Context->NodeList);
}

VOID DeleteGraphTree(
    _In_ PPH_GRAPHTREE_CONTEXT Context
    )
{
    if (Context->TitleFontHandle)
        DeleteFont(Context->TitleFontHandle);
    if (Context->NormalFontHandle)
        DeleteFont(Context->NormalFontHandle);

    PhDeleteTreeNewFilterSupport(&Context->FilterSupport);
    //GraphSaveSettingsTreeList(Context);

    for (ULONG i = 0; i < Context->NodeList->Count; i++)
        DestroyGraphNode(Context->NodeList->Items[i]);

    PhClearReference(&Context->NodeHashtable);
    PhClearReference(&Context->NodeList);
}

#pragma endregion

LRESULT CALLBACK MainWndSubclassProc(
    _In_ HWND hWnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    PPH_GRAPHTREE_CONTEXT context;
    WNDPROC oldWndProc = NULL;

    if (!(context = PhGetWindowContext(hWnd, _I16_MAX)))
        return 0;

    oldWndProc = context->WindowProc;

    switch (uMsg)
    {
    case WM_DESTROY:
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)oldWndProc);
        break;
    case WM_MOUSEMOVE:
        {
            HWND tooltipHandle = TreeNew_GetTooltips(hWnd);

            if (tooltipHandle)
            {
                POINT point;

                GetCursorPos(&point);
                ScreenToClient(hWnd, &point);

                if (context->LastCursorLocation.x != point.x || context->LastCursorLocation.y != point.y)
                {
                    SendMessage(tooltipHandle, TTM_UPDATE, 0, 0);
                    context->LastCursorLocation = point;
                }
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR header = (LPNMHDR)lParam;

            if (header->hwndFrom == TreeNew_GetTooltips(hWnd))
            {
                switch (header->code)
                {
                case TTN_GETDISPINFO:
                    {
                        LPNMTTDISPINFO dispInfo = (LPNMTTDISPINFO)header;
                        POINT point;
                        RECT clientRect;
                        PPH_GRAPH_TREE_ROOT_NODE node = NULL;
                        PH_TREENEW_HIT_TEST hitTest;
                        ULONG index;
                        ULONG64 ioRead;
                        ULONG64 ioWrite;
                        ULONG64 ioOther;
                        PH_FORMAT format[8];

                        GetCursorPos(&point);
                        ScreenToClient(hWnd, &point);
                        GetClientRect(hWnd, &clientRect);

                        //getTooltipText.Index = (clientRect.right - point.x - 1) / context->DrawInfo.Step;
                        index = (clientRect.right - 4 - GetSystemMetrics(SM_CXVSCROLL) - point.x - 1) / 2; // context->DrawInfo.Step; // 4 (graph padding/border)

                        if (index == 0)
                            break;

                        hitTest.Point = point;
                        hitTest.InFlags = TN_TEST_COLUMN;
                        TreeNew_HitTest(hWnd, &hitTest);

                        node = (PPH_GRAPH_TREE_ROOT_NODE)hitTest.Node;

                        if (!node)
                            break;

                        if (
                            index == node->ProcessItem->IoReadHistory.Count ||
                            index == node->ProcessItem->IoWriteHistory.Count ||
                            index == node->ProcessItem->IoOtherHistory.Count
                            )
                        {
                            break;
                        }

                        if (
                            index > node->ProcessItem->IoReadHistory.Count ||
                            index > node->ProcessItem->IoWriteHistory.Count ||
                            index > node->ProcessItem->IoOtherHistory.Count
                            )
                        {
                            break;
                        }

                        ioRead = PhGetItemCircularBuffer_ULONG64(&node->ProcessItem->IoReadHistory, index);
                        ioWrite = PhGetItemCircularBuffer_ULONG64(&node->ProcessItem->IoWriteHistory, index);
                        ioOther = PhGetItemCircularBuffer_ULONG64(&node->ProcessItem->IoOtherHistory, index);

                        // R: %s\nW: %s\nO: %s\n%s
                        PhInitFormatS(&format[0], L"R: ");
                        PhInitFormatSize(&format[1], ioRead);
                        PhInitFormatS(&format[2], L"\nW: ");
                        PhInitFormatSize(&format[3], ioWrite);
                        PhInitFormatS(&format[4], L"\nO: ");
                        PhInitFormatSize(&format[5], ioOther);
                        PhInitFormatC(&format[6], L'\n');
                        PhInitFormatSR(&format[7], PH_AUTO_T(PH_STRING, PhGetStatisticsTimeString(node->ProcessItem, index))->sr);

                        dispInfo->lpszText = PH_AUTO_T(PH_STRING, PhFormat(format, RTL_NUMBER_OF(format), 64))->Buffer;
                    }
                    break;
                }
            }
        }
        break;
    }

    return CallWindowProc(oldWndProc, hWnd, uMsg, wParam, lParam);
}

VOID NTAPI PerfCounterTreeProcessesUpdatedCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_GRAPHTREE_CONTEXT context = Context;

    if (!context)
        return;

    if (context->ProcessesUpdatedCount < 2)
    {
        context->ProcessesUpdatedCount++;
    }
    else if (context->WindowHandle)
    {
        PostMessage(context->WindowHandle, MSG_UPDATE, 0, 0);
    }
}

BOOLEAN WordMatchStringRef(
    _Inout_ PPH_GRAPHTREE_CONTEXT Context,
    _In_ PPH_STRINGREF Text
    )
{
    PH_STRINGREF part;
    PH_STRINGREF remainingPart;

    remainingPart = Context->SearchboxText->sr;

    while (remainingPart.Length != 0)
    {
        PhSplitStringRefAtChar(&remainingPart, L'|', &part, &remainingPart);

        if (part.Length != 0)
        {
            if (PhFindStringInStringRef(Text, &part, TRUE) != -1)
                return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN WordMatchStringZ(
    _Inout_ PPH_GRAPHTREE_CONTEXT Context,
    _In_ PWSTR Text
    )
{
    PH_STRINGREF text;

    PhInitializeStringRef(&text, Text);
    return WordMatchStringRef(Context, &text);
}

BOOLEAN GraphTreeFilterCallback(
    _In_ PPH_TREENEW_NODE Node,
    _In_opt_ PVOID Context
    )
{
    PPH_GRAPHTREE_CONTEXT context = Context;
    PPH_GRAPH_TREE_ROOT_NODE poolNode = (PPH_GRAPH_TREE_ROOT_NODE)Node;
    BOOLEAN UseSearchPointer;
    ULONG64 SearchPointer;

    if (!context)
        return TRUE;
    if (PhIsNullOrEmptyString(context->SearchboxText))
        return TRUE;

    UseSearchPointer = PhStringToInteger64(&context->SearchboxText->sr, 0, &SearchPointer);

    if (UseSearchPointer)
    {
        if (poolNode->ProcessItem->ProcessId == (HANDLE)SearchPointer)
            return TRUE;
    }

    if (!PhIsNullOrEmptyString(poolNode->ProcessItem->ProcessName))
    {
        if (WordMatchStringRef(context, &poolNode->ProcessItem->ProcessName->sr))
            return TRUE;
    }

    if (!PhIsNullOrEmptyString(poolNode->ProcessItem->FileNameWin32))
    {
        if (WordMatchStringRef(context, &poolNode->ProcessItem->FileNameWin32->sr))
            return TRUE;
    }

    return FALSE;
}

INT_PTR CALLBACK PerfCounterTreeDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PPH_GRAPHTREE_CONTEXT context;

    if (uMsg == WM_INITDIALOG)
    {
        context = WindowContext = PhAllocateZero(sizeof(PH_GRAPHTREE_CONTEXT));

        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
    }

    if (!context)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            context->WindowHandle = hwndDlg;
            context->TreeNewHandle = GetDlgItem(hwndDlg, IDC_GRAPHTREE);
            context->SearchHandle = GetDlgItem(hwndDlg, IDC_SEARCH);

            context->WindowProc = (WNDPROC)GetWindowLongPtr(context->TreeNewHandle, GWLP_WNDPROC);
            PhSetWindowContext(context->TreeNewHandle, _I16_MAX, context);
            SetWindowLongPtr(context->TreeNewHandle, GWLP_WNDPROC, (LONG_PTR)MainWndSubclassProc);

            PhCreateSearchControl(hwndDlg, context->SearchHandle, L"Search Graphs");

            PhSetWindowStyle(context->TreeNewHandle, WS_BORDER, 0);
            PhSetWindowExStyle(context->TreeNewHandle, WS_EX_CLIENTEDGE, 0);
            SetWindowPos(context->TreeNewHandle, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

            PhInitializeWindowTheme(hwndDlg, !!PhGetIntegerSetting(L"EnableThemeSupport"));

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, context->TreeNewHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&context->LayoutManager, context->SearchHandle, NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDCANCEL), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);

            if (PhGetIntegerPairSetting(SETTING_NAME_WINDOW_POSITION).X != 0)
                PhLoadWindowPlacementFromSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            else
                PhCenterWindow(hwndDlg, NULL);

            InitializeGraphTree(context);

            context->SearchboxText = PhReferenceEmptyString();
            context->TreeFilterEntry = PhAddTreeNewFilter(
                &context->FilterSupport,
                GraphTreeFilterCallback,
                context);

            SendMessage(TreeNew_GetTooltips(context->TreeNewHandle), TTM_SETDELAYTIME, TTDT_INITIAL, 0);
            SendMessage(TreeNew_GetTooltips(context->TreeNewHandle), TTM_SETDELAYTIME, TTDT_AUTOPOP, MAXSHORT);
            SendMessage(TreeNew_GetTooltips(context->TreeNewHandle), TTM_SETMAXTIPWIDTH, 0, MAXSHORT);

            //PhAcquireQueuedLockShared(&PerfCounterListLock);
            //for (ULONG i = 0; i < PerfCounterList->Count; i++)
            //{
            //    PPERF_COUNTER_ENTRY entry = PhReferenceObjectSafe(PerfCounterList->Items[i]);
            //    if (!entry)
            //        continue;
            //    if (entry->UserReference)
            //    {
            //        AddPerfCounterNode(context, entry);
            //    }
            //}
            //PhReleaseQueuedLockShared(&PerfCounterListLock);

            // TODO: Show thread cpu graphs.
            PPH_PROCESS_ITEM* processes;
            ULONG numberOfProcesses;

            PhEnumProcessItems(&processes, &numberOfProcesses);

            for (ULONG i = 0; i < numberOfProcesses; i++)
            {
                PPH_PROCESS_ITEM process = processes[i];

                if (PH_IS_REAL_PROCESS_ID(process->ProcessId))
                {
                    PhReferenceObject(process);

                    AddProcessNode(context, process);
                }
            }

            PhDereferenceObjects(processes, numberOfProcesses);

            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackProcessProviderUpdatedEvent),
                PerfCounterTreeProcessesUpdatedCallback,
                context,
                &context->ProcessesUpdatedCallbackRegistration);

            //TreeNew_NodesStructured(context->TreeNewHandle);
            TreeNew_AutoSizeColumn(context->TreeNewHandle, PH_GRAPH_TREE_COLUMN_ITEM_NAME, TN_AUTOSIZE_REMAINING_SPACE);

            PhSetDialogFocus(hwndDlg, GetDlgItem(hwndDlg, IDCANCEL));
        }
        break;
    case WM_DESTROY:
        {
            PhUnregisterCallback(PhGetGeneralCallback(GeneralCallbackProcessProviderUpdatedEvent), &context->ProcessesUpdatedCallbackRegistration);

            PhSaveWindowPlacementToSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            PhDeleteLayoutManager(&context->LayoutManager);

            DeleteGraphTree(context);

            PhRemoveWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);

            PhFree(context);

            PostQuitMessage(0);
        }
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
                case IDCANCEL:
                    DestroyWindow(hwndDlg);
                    break;
                case WM_MSG_WINDOW_PROPERTIES:
                    {
                        PPH_GRAPH_TREE_ROOT_NODE selectedNode;

                        if (selectedNode = GetSelectedGraphNode(context))
                        {
                            PPH_PROCESS_PROPCONTEXT propContext;

                            if (propContext = PhCreateProcessPropContext(hwndDlg, selectedNode->ProcessItem))
                            {
                                PhShowProcessProperties(propContext);
                                PhDereferenceObject(propContext);
                            }
                        }
                    }
                    break;
                case IDC_SEARCH:
                    {
                        PPH_STRING newSearchboxText;

                        if (GET_WM_COMMAND_CMD(wParam, lParam) != EN_CHANGE)
                            break;

                        newSearchboxText = PH_AUTO(PhGetWindowText(context->SearchHandle));

                        if (!PhEqualString(context->SearchboxText, newSearchboxText, FALSE))
                        {
                            PhSwapReference(&context->SearchboxText, newSearchboxText);

                            PhApplyTreeNewFilters(&context->FilterSupport);

                            TreeNew_AutoSizeColumn(context->TreeNewHandle, PH_GRAPH_TREE_COLUMN_ITEM_NAME, TN_AUTOSIZE_REMAINING_SPACE);
                        }
                    }
                    break;
            }
        }
        break;
    case WM_SIZE:
        {
            PhLayoutManagerLayout(&context->LayoutManager);

            TreeNew_AutoSizeColumn(context->TreeNewHandle, PH_GRAPH_TREE_COLUMN_ITEM_NAME, TN_AUTOSIZE_REMAINING_SPACE);
        }
        break;
    case MSG_UPDATE:
        {
            static ULONG sequence = 0;

            if (context->NodeList)
            {
                for (ULONG i = 0; i < context->NodeList->Count; i++)
                {
                    PPH_GRAPH_TREE_ROOT_NODE node = context->NodeList->Items[i];

                    node->GraphBuffers.Valid = FALSE;
                    node->Sequence = sequence;
                }

                sequence++;

                TreeNew_NodesStructured(context->TreeNewHandle);

                SendMessage(TreeNew_GetTooltips(context->TreeNewHandle), TTM_UPDATE, 0, 0);
            }
        }
        break;
    }

    return FALSE;
}

static HWND GraphDialogHandle = NULL;
static HANDLE GraphDialogThreadHandle = NULL;
static PH_EVENT GraphDialogInitializedEvent = PH_EVENT_INIT;

NTSTATUS ShowGraphDialogThread(
    _In_ PVOID Parameter
    )
{
    BOOL result;
    MSG message;
    PH_AUTO_POOL autoPool;

    PhInitializeAutoPool(&autoPool);

    GraphDialogHandle = CreateDialog(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_DIALOG1),
        NULL,
        PerfCounterTreeDlgProc
        );

    //PhSetEvent(&GraphDialogInitializedEvent);

    //PostMessage(GraphDialogHandle, SHOWDIALOG, 0, 0);
    if (IsMinimized(GraphDialogHandle))
        ShowWindow(GraphDialogHandle, SW_RESTORE);
    else
        ShowWindow(GraphDialogHandle, SW_SHOW);

    SetForegroundWindow(GraphDialogHandle);

    while (result = GetMessage(&message, NULL, 0, 0))
    {
        if (result == -1)
            break;

        if (!IsDialogMessage(GraphDialogHandle, &message))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        PhDrainAutoPool(&autoPool);
    }

    PhDeleteAutoPool(&autoPool);

    if (GraphDialogThreadHandle)
    {
        NtClose(GraphDialogThreadHandle);
        GraphDialogThreadHandle = NULL;
    }

    //PhResetEvent(&GraphDialogInitializedEvent);

    return STATUS_SUCCESS;
}


VOID ShowGraphDialog(
    VOID
    )
{
    //if (!GraphDialogThreadHandle)
    {
        if (!NT_SUCCESS(PhCreateThreadEx(&GraphDialogThreadHandle, ShowGraphDialogThread, NULL)))
        {
            PhShowError(PhMainWndHandle, L"%s", L"Unable to create the window.");
            return;
        }

        //PhWaitForEvent(&GraphDialogInitializedEvent, NULL);
    }

    //PostMessage(GraphDialogHandle, SHOWDIALOG, 0, 0);
}
