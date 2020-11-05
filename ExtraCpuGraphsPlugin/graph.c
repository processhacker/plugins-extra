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

#include "main.h"

static RECT NormalGraphTextMargin = { 5, 5, 5, 5 };
static RECT NormalGraphTextPadding = { 3, 3, 3, 3 };

VOID CpuPluginCreateGraphs(
    _Inout_ PPH_CPUPLUGIN_SYSINFO_CONTEXT Context
    )
{
    Context->ContextSwitchesGraphHandle = CreateWindow(
        PH_GRAPH_CLASSNAME,
        NULL,
        WS_VISIBLE | WS_CHILD | WS_BORDER,
        0,
        0,
        3,
        3,
        Context->WindowHandle,
        NULL,
        NULL,
        NULL
        );
    Graph_SetTooltip(Context->ContextSwitchesGraphHandle, TRUE);

    Context->InterruptsGraphHandle = CreateWindow(
        PH_GRAPH_CLASSNAME,
        NULL,
        WS_VISIBLE | WS_CHILD | WS_BORDER,
        0,
        0,
        3,
        3,
        Context->WindowHandle,
        NULL,
        NULL,
        NULL
        );
    Graph_SetTooltip(Context->InterruptsGraphHandle, TRUE);

    Context->DpcsGraphHandle = CreateWindow(
        PH_GRAPH_CLASSNAME,
        NULL,
        WS_VISIBLE | WS_CHILD | WS_BORDER,
        0,
        0,
        3,
        3,
        Context->WindowHandle,
        NULL,
        NULL,
        NULL
        );
    Graph_SetTooltip(Context->DpcsGraphHandle, TRUE);

    Context->SystemCallsGraphHandle = CreateWindow(
        PH_GRAPH_CLASSNAME,
        NULL,
        WS_VISIBLE | WS_CHILD | WS_BORDER,
        0,
        0,
        3,
        3,
        Context->WindowHandle,
        NULL,
        NULL,
        NULL
        );
    Graph_SetTooltip(Context->SystemCallsGraphHandle, TRUE);
}

VOID CpuPluginLayoutGraphs(
    _Inout_ PPH_CPUPLUGIN_SYSINFO_CONTEXT Context
    )
{
    RECT clientRect;
    RECT labelRect;
    ULONG graphWidth;
    ULONG graphHeight;
    HDWP deferHandle;
    ULONG y;

#define ET_GPU_PADDING 3

    PhLayoutManagerLayout(&Context->LayoutManager);

    GetClientRect(Context->WindowHandle, &clientRect);
    GetClientRect(Context->ContextSwitchesLabelHandle, &labelRect);

    graphWidth = clientRect.right - Context->ContextSwitchesGraphMargin.left - Context->ContextSwitchesGraphMargin.right;
    graphHeight = (clientRect.bottom - Context->ContextSwitchesGraphMargin.top - Context->ContextSwitchesGraphMargin.bottom - labelRect.bottom * 4 - ET_GPU_PADDING * 5) / 4;

    deferHandle = BeginDeferWindowPos(8);
    y = Context->ContextSwitchesGraphMargin.top;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->ContextSwitchesLabelHandle,
        NULL,
        Context->ContextSwitchesGraphMargin.left,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += labelRect.bottom + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->ContextSwitchesGraphHandle,
        NULL,
        Context->ContextSwitchesGraphMargin.left,
        y,
        graphWidth,
        graphHeight,
        SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += graphHeight + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->InterruptsLabelHandle,
        NULL,
        Context->ContextSwitchesGraphMargin.left,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += labelRect.bottom + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->InterruptsGraphHandle,
        NULL,
        Context->ContextSwitchesGraphMargin.left,
        y,
        graphWidth,
        graphHeight,
        SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += graphHeight + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->DpcsLabelHandle,
        NULL,
        Context->ContextSwitchesGraphMargin.left,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += labelRect.bottom + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->DpcsGraphHandle,
        NULL,
        Context->ContextSwitchesGraphMargin.left,
        y,
        graphWidth,
        graphHeight,
        SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += graphHeight + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->SystemCallsLabelHandle,
        NULL,
        Context->ContextSwitchesGraphMargin.left,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += labelRect.bottom + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->SystemCallsGraphHandle,
        NULL,
        Context->ContextSwitchesGraphMargin.left,
        y,
        graphWidth,
        clientRect.bottom - Context->ContextSwitchesGraphMargin.bottom - y,
        SWP_NOACTIVATE | SWP_NOZORDER
        );

    EndDeferWindowPos(deferHandle);
}

VOID CpuPluginNotifyContextSwitchesGraph(
    _Inout_ PPH_CPUPLUGIN_SYSINFO_CONTEXT Context,
    _In_ NMHDR *Header
    )
{
    switch (Header->code)
    {
    case GCN_GETDRAWINFO:
        {
            PPH_GRAPH_GETDRAWINFO getDrawInfo = (PPH_GRAPH_GETDRAWINFO)Header;
            PPH_GRAPH_DRAW_INFO drawInfo = getDrawInfo->DrawInfo;

            drawInfo->Flags = PH_GRAPH_USE_GRID_X | PH_GRAPH_USE_GRID_Y;
            Context->Section->Parameters->ColorSetupFunction(drawInfo, PhGetIntegerSetting(L"ColorIoWrite"), 0);
            PhGraphStateGetDrawInfo(&Context->ContextSwitchesGraphState, getDrawInfo, ContextSwitchesHistory.Count);

            if (!Context->ContextSwitchesGraphState.Valid)
            {
                FLOAT max = 40000;

                for (ULONG i = 0; i < drawInfo->LineDataCount; i++)
                {
                    FLOAT data1;

                    Context->ContextSwitchesGraphState.Data1[i] = data1 = (FLOAT)PhGetItemCircularBuffer_ULONG64(&ContextSwitchesHistory, i);

                    if (max < data1)
                        max = data1;
                }

                if (max != 0)
                {
                    PhDivideSinglesBySingle(Context->ContextSwitchesGraphState.Data1, max, drawInfo->LineDataCount);
                }

                Context->ContextSwitchesGraphState.Valid = TRUE;
            }

            if (PhGetIntegerSetting(L"GraphShowText"))
            {
                HDC hdc = Graph_GetBufferedContext(Context->ContextSwitchesGraphHandle);

                PhMoveReference(&Context->ContextSwitchesGraphState.Text, PhFormatUInt64(ContextSwitchesDelta.Delta, TRUE));

                SelectObject(hdc, PhApplicationFont);
                PhSetGraphText(hdc, drawInfo, &Context->ContextSwitchesGraphState.Text->sr,
                    &NormalGraphTextMargin, &NormalGraphTextPadding, PH_ALIGN_TOP | PH_ALIGN_LEFT);
            }
            else
            {
                drawInfo->Text.Buffer = NULL;
            }
        }
        break;
    case GCN_GETTOOLTIPTEXT:
        {
            PPH_GRAPH_GETTOOLTIPTEXT getTooltipText = (PPH_GRAPH_GETTOOLTIPTEXT)Header;

            if (getTooltipText->Index < getTooltipText->TotalCount)
            {
                if (Context->ContextSwitchesGraphState.TooltipIndex != getTooltipText->Index)
                {
                    ULONG64 value;
                    PH_FORMAT format[3];

                    value = PhGetItemCircularBuffer_ULONG64(&ContextSwitchesHistory, getTooltipText->Index);

                    PhInitFormatI64UGroupDigits(&format[0], value);
                    PhInitFormatC(&format[1], L'\n');
                    PhInitFormatSR(&format[2], ((PPH_STRING)PH_AUTO(PhGetStatisticsTimeString(NULL, getTooltipText->Index)))->sr);

                    PhMoveReference(&Context->ContextSwitchesGraphState.TooltipText, PhFormat(format, RTL_NUMBER_OF(format), 0));
                }

                getTooltipText->Text = PhGetStringRef(Context->ContextSwitchesGraphState.TooltipText);
            }
        }
        break;
    }
}

VOID CpuPluginNotifyInterruptsGraph(
    _Inout_ PPH_CPUPLUGIN_SYSINFO_CONTEXT Context,
    _In_ NMHDR *Header
    )
{
    switch (Header->code)
    {
    case GCN_GETDRAWINFO:
        {
            PPH_GRAPH_GETDRAWINFO getDrawInfo = (PPH_GRAPH_GETDRAWINFO)Header;
            PPH_GRAPH_DRAW_INFO drawInfo = getDrawInfo->DrawInfo;

            drawInfo->Flags = PH_GRAPH_USE_GRID_X | PH_GRAPH_USE_GRID_Y;
            Context->Section->Parameters->ColorSetupFunction(drawInfo, PhGetIntegerSetting(L"ColorPrivate"), 0);
            PhGraphStateGetDrawInfo(&Context->InterruptsGraphState, getDrawInfo, InterruptsHistory.Count);

            if (!Context->InterruptsGraphState.Valid)
            {
                FLOAT max = 30000;

                for (ULONG i = 0; i < drawInfo->LineDataCount; i++)
                {
                    FLOAT data1;

                    Context->InterruptsGraphState.Data1[i] = data1 = (FLOAT)PhGetItemCircularBuffer_ULONG64(&InterruptsHistory, i);

                    if (max < data1)
                        max = data1;
                }

                if (max != 0)
                {
                    PhDivideSinglesBySingle(Context->InterruptsGraphState.Data1, max, drawInfo->LineDataCount);
                }

                Context->InterruptsGraphState.Valid = TRUE;
            }

            if (PhGetIntegerSetting(L"GraphShowText"))
            {
                HDC hdc = Graph_GetBufferedContext(Context->InterruptsGraphHandle);

                PhMoveReference(&Context->InterruptsGraphState.Text, PhFormatUInt64(InterruptsDelta.Delta, TRUE));

                SelectObject(hdc, PhApplicationFont);
                PhSetGraphText(hdc, drawInfo, &Context->InterruptsGraphState.Text->sr,
                    &NormalGraphTextMargin, &NormalGraphTextPadding, PH_ALIGN_TOP | PH_ALIGN_LEFT);
            }
            else
            {
                drawInfo->Text.Buffer = NULL;
            }
        }
        break;
    case GCN_GETTOOLTIPTEXT:
        {
            PPH_GRAPH_GETTOOLTIPTEXT getTooltipText = (PPH_GRAPH_GETTOOLTIPTEXT)Header;

            if (getTooltipText->Index < getTooltipText->TotalCount)
            {
                if (Context->InterruptsGraphState.TooltipIndex != getTooltipText->Index)
                {
                    ULONG64 value;
                    PH_FORMAT format[3];

                    value = PhGetItemCircularBuffer_ULONG64(&InterruptsHistory, getTooltipText->Index);

                    PhInitFormatI64UGroupDigits(&format[0], value);
                    PhInitFormatC(&format[1], L'\n');
                    PhInitFormatSR(&format[2], ((PPH_STRING)PH_AUTO(PhGetStatisticsTimeString(NULL, getTooltipText->Index)))->sr);

                    PhMoveReference(&Context->InterruptsGraphState.TooltipText, PhFormat(format, RTL_NUMBER_OF(format), 0));
                }

                getTooltipText->Text = PhGetStringRef(Context->InterruptsGraphState.TooltipText);
            }
        }
        break;
    }
}

VOID CpuPluginNotifyDpcsGraph(
    _Inout_ PPH_CPUPLUGIN_SYSINFO_CONTEXT Context,
    _In_ NMHDR *Header
    )
{
    switch (Header->code)
    {
    case GCN_GETDRAWINFO:
        {
            PPH_GRAPH_GETDRAWINFO getDrawInfo = (PPH_GRAPH_GETDRAWINFO)Header;
            PPH_GRAPH_DRAW_INFO drawInfo = getDrawInfo->DrawInfo;

            drawInfo->Flags = PH_GRAPH_USE_GRID_X | PH_GRAPH_USE_GRID_Y;
            Context->Section->Parameters->ColorSetupFunction(drawInfo, PhGetIntegerSetting(L"ColorPhysical"), 0);
            PhGraphStateGetDrawInfo(&Context->DpcsGraphState, getDrawInfo, DpcsHistory.Count);

            if (!Context->DpcsGraphState.Valid)
            {
                FLOAT max = 1000;

                for (ULONG i = 0; i < drawInfo->LineDataCount; i++)
                {
                    FLOAT data1;

                    Context->DpcsGraphState.Data1[i] = data1 = (FLOAT)PhGetItemCircularBuffer_ULONG64(&DpcsHistory, i);

                    if (max < data1)
                        max = data1;
                }

                if (max != 0)
                {
                    PhDivideSinglesBySingle(Context->DpcsGraphState.Data1, max, drawInfo->LineDataCount);
                }

                Context->DpcsGraphState.Valid = TRUE;
            }

            if (PhGetIntegerSetting(L"GraphShowText"))
            {
                HDC hdc = Graph_GetBufferedContext(Context->DpcsGraphHandle);

                PhMoveReference(&Context->DpcsGraphState.Text, PhFormatUInt64(DpcsDelta.Delta, TRUE));

                SelectObject(hdc, PhApplicationFont);
                PhSetGraphText(hdc, drawInfo, &Context->DpcsGraphState.Text->sr,
                    &NormalGraphTextMargin, &NormalGraphTextPadding, PH_ALIGN_TOP | PH_ALIGN_LEFT);
            }
            else
            {
                drawInfo->Text.Buffer = NULL;
            }
        }
        break;
    case GCN_GETTOOLTIPTEXT:
        {
            PPH_GRAPH_GETTOOLTIPTEXT getTooltipText = (PPH_GRAPH_GETTOOLTIPTEXT)Header;

            if (getTooltipText->Index < getTooltipText->TotalCount)
            {
                if (Context->DpcsGraphState.TooltipIndex != getTooltipText->Index)
                {
                    ULONG64 value;
                    PH_FORMAT format[3];

                    value = PhGetItemCircularBuffer_ULONG64(&DpcsHistory, getTooltipText->Index);

                    PhInitFormatI64UGroupDigits(&format[0], value);
                    PhInitFormatC(&format[1], L'\n');
                    PhInitFormatSR(&format[2], ((PPH_STRING)PH_AUTO(PhGetStatisticsTimeString(NULL, getTooltipText->Index)))->sr);

                    PhMoveReference(&Context->DpcsGraphState.TooltipText, PhFormat(format, RTL_NUMBER_OF(format), 0));
                }

                getTooltipText->Text = PhGetStringRef(Context->DpcsGraphState.TooltipText);
            }
        }
        break;
    }
}

VOID CpuPluginNotifySystemCallsGraph(
    _Inout_ PPH_CPUPLUGIN_SYSINFO_CONTEXT Context,
    _In_ NMHDR *Header
    )
{
    switch (Header->code)
    {
    case GCN_GETDRAWINFO:
        {
            PPH_GRAPH_GETDRAWINFO getDrawInfo = (PPH_GRAPH_GETDRAWINFO)Header;
            PPH_GRAPH_DRAW_INFO drawInfo = getDrawInfo->DrawInfo;

            drawInfo->Flags = PH_GRAPH_USE_GRID_X | PH_GRAPH_USE_GRID_Y;
            Context->Section->Parameters->ColorSetupFunction(drawInfo, PhGetIntegerSetting(L"ColorCpuKernel"), 0);
            PhGraphStateGetDrawInfo(&Context->SystemCallsGraphState, getDrawInfo, SystemCallsHistory.Count);

            if (!Context->SystemCallsGraphState.Valid)
            {
                FLOAT max = 1000;

                for (ULONG i = 0; i < drawInfo->LineDataCount; i++)
                {
                    FLOAT data1;

                    Context->SystemCallsGraphState.Data1[i] = data1 = (FLOAT)PhGetItemCircularBuffer_ULONG64(&SystemCallsHistory, i);

                    if (max < data1)
                        max = data1;
                }

                if (max != 0)
                {
                    PhDivideSinglesBySingle(Context->SystemCallsGraphState.Data1, max, drawInfo->LineDataCount);
                }

                Context->SystemCallsGraphState.Valid = TRUE;
            }

            if (PhGetIntegerSetting(L"GraphShowText"))
            {
                HDC hdc = Graph_GetBufferedContext(Context->SystemCallsGraphHandle);

                PhMoveReference(&Context->SystemCallsGraphState.Text, PhFormatUInt64(SystemCallsDelta.Delta, TRUE));

                SelectObject(hdc, PhApplicationFont);
                PhSetGraphText(hdc, drawInfo, &Context->SystemCallsGraphState.Text->sr,
                    &NormalGraphTextMargin, &NormalGraphTextPadding, PH_ALIGN_TOP | PH_ALIGN_LEFT);
            }
            else
            {
                drawInfo->Text.Buffer = NULL;
            }
        }
        break;
    case GCN_GETTOOLTIPTEXT:
        {
            PPH_GRAPH_GETTOOLTIPTEXT getTooltipText = (PPH_GRAPH_GETTOOLTIPTEXT)Header;

            if (getTooltipText->Index < getTooltipText->TotalCount)
            {
                if (Context->SystemCallsGraphState.TooltipIndex != getTooltipText->Index)
                {
                    ULONG64 value;
                    PH_FORMAT format[3];

                    value = PhGetItemCircularBuffer_ULONG64(&SystemCallsHistory, getTooltipText->Index);

                    PhInitFormatI64UGroupDigits(&format[0], value);
                    PhInitFormatC(&format[1], L'\n');
                    PhInitFormatSR(&format[2], ((PPH_STRING)PH_AUTO(PhGetStatisticsTimeString(NULL, getTooltipText->Index)))->sr);

                    PhMoveReference(&Context->SystemCallsGraphState.TooltipText, PhFormat(format, RTL_NUMBER_OF(format), 0));
                }

                getTooltipText->Text = PhGetStringRef(Context->SystemCallsGraphState.TooltipText);
            }
        }
        break;
    }
}

VOID CpuPluginUpdateGraphs(
    _Inout_ PPH_CPUPLUGIN_SYSINFO_CONTEXT Context
    )
{
    Context->ContextSwitchesGraphState.Valid = FALSE;
    Context->ContextSwitchesGraphState.TooltipIndex = -1;
    Graph_MoveGrid(Context->ContextSwitchesGraphHandle, 1);
    Graph_Draw(Context->ContextSwitchesGraphHandle);
    Graph_UpdateTooltip(Context->ContextSwitchesGraphHandle);
    InvalidateRect(Context->ContextSwitchesGraphHandle, NULL, FALSE);

    Context->InterruptsGraphState.Valid = FALSE;
    Context->InterruptsGraphState.TooltipIndex = -1;
    Graph_MoveGrid(Context->InterruptsGraphHandle, 1);
    Graph_Draw(Context->InterruptsGraphHandle);
    Graph_UpdateTooltip(Context->InterruptsGraphHandle);
    InvalidateRect(Context->InterruptsGraphHandle, NULL, FALSE);

    Context->DpcsGraphState.Valid = FALSE;
    Context->DpcsGraphState.TooltipIndex = -1;
    Graph_MoveGrid(Context->DpcsGraphHandle, 1);
    Graph_Draw(Context->DpcsGraphHandle);
    Graph_UpdateTooltip(Context->DpcsGraphHandle);
    InvalidateRect(Context->DpcsGraphHandle, NULL, FALSE);

    Context->SystemCallsGraphState.Valid = FALSE;
    Context->SystemCallsGraphState.TooltipIndex = -1;
    Graph_MoveGrid(Context->SystemCallsGraphHandle, 1);
    Graph_Draw(Context->SystemCallsGraphHandle);
    Graph_UpdateTooltip(Context->SystemCallsGraphHandle);
    InvalidateRect(Context->SystemCallsGraphHandle, NULL, FALSE);
}

INT_PTR CALLBACK CpuPluginDialogProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PPH_CPUPLUGIN_SYSINFO_CONTEXT context = NULL;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PPH_CPUPLUGIN_SYSINFO_CONTEXT)lParam;
        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);

        if (uMsg == WM_NCDESTROY)
        {
            PhDeleteLayoutManager(&context->LayoutManager);

            PhDeleteGraphState(&context->ContextSwitchesGraphState);
            PhDeleteGraphState(&context->InterruptsGraphState);
            PhDeleteGraphState(&context->DpcsGraphState);
            PhDeleteGraphState(&context->SystemCallsGraphState);

            if (context->ContextSwitchesGraphHandle)
                DestroyWindow(context->ContextSwitchesGraphHandle);
            if (context->InterruptsGraphHandle)
                DestroyWindow(context->InterruptsGraphHandle);
            if (context->DpcsGraphHandle)
                DestroyWindow(context->DpcsGraphHandle);
            if (context->SystemCallsGraphHandle)
                DestroyWindow(context->SystemCallsGraphHandle);
            //if (context->GpuPanel)
            //    DestroyWindow(context->GpuPanel);

            PhRemoveWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
        }
    }

    if (context == NULL)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PPH_LAYOUT_ITEM graphItem;

            context->WindowHandle = hwndDlg;
            context->ContextSwitchesLabelHandle = GetDlgItem(hwndDlg, IDC_GPU_L);
            context->InterruptsLabelHandle = GetDlgItem(hwndDlg, IDC_MEMORY_L);
            context->DpcsLabelHandle = GetDlgItem(hwndDlg, IDC_SHARED_L);
            context->SystemCallsLabelHandle = GetDlgItem(hwndDlg, IDC_BUS_L);

            PhInitializeGraphState(&context->ContextSwitchesGraphState);
            PhInitializeGraphState(&context->InterruptsGraphState);
            PhInitializeGraphState(&context->DpcsGraphState);
            PhInitializeGraphState(&context->SystemCallsGraphState);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            graphItem = PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_GRAPH_LAYOUT), NULL, PH_ANCHOR_ALL);
            context->ContextSwitchesGraphMargin = graphItem->Margin;

            CpuPluginCreateGraphs(context);
            CpuPluginUpdateGraphs(context);
        }
        break;
    case WM_SIZE:
        CpuPluginLayoutGraphs(context);
        break;
    case WM_NOTIFY:
        {
            NMHDR* header = (NMHDR*)lParam;

            if (header->hwndFrom == context->ContextSwitchesGraphHandle)
            {
                CpuPluginNotifyContextSwitchesGraph(context, header);
            }
            else if (header->hwndFrom == context->InterruptsGraphHandle)
            {
                CpuPluginNotifyInterruptsGraph(context, header);
            }
            else if (header->hwndFrom == context->DpcsGraphHandle)
            {
                CpuPluginNotifyDpcsGraph(context, header);
            }
            else if (header->hwndFrom == context->SystemCallsGraphHandle)
            {
                CpuPluginNotifySystemCallsGraph(context, header);
            }
        }
        break;
    case MSG_UPDATE:
        {
            CpuPluginUpdateGraphs(context);
        }
        break;
    }

    return FALSE;
}

BOOLEAN CpuPluginSectionCallback(
    _In_ PPH_SYSINFO_SECTION Section,
    _In_ PH_SYSINFO_SECTION_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2
    )
{
    PPH_CPUPLUGIN_SYSINFO_CONTEXT context = (PPH_CPUPLUGIN_SYSINFO_CONTEXT)Section->Context;

    switch (Message)
    {
    case SysInfoCreate:
        return TRUE;
    case SysInfoDestroy:
        {
            PhFree(context);
        }
        return TRUE;
    case SysInfoTick:
        {
            if (context->WindowHandle)
            {
                PostMessage(context->WindowHandle, MSG_UPDATE, 0, 0);
            }
        }
        return TRUE;
    case SysInfoCreateDialog:
        {
            PPH_SYSINFO_CREATE_DIALOG createDialog = (PPH_SYSINFO_CREATE_DIALOG)Parameter1;

            if (!createDialog)
                break;

            createDialog->Instance = PluginInstance->DllBase;
            createDialog->Template = MAKEINTRESOURCE(IDD_GPU_DIALOG);
            createDialog->DialogProc = CpuPluginDialogProc;
            createDialog->Parameter = context;
        }
        return TRUE;
    case SysInfoGraphGetDrawInfo:
        {
            PPH_GRAPH_DRAW_INFO drawInfo = (PPH_GRAPH_DRAW_INFO)Parameter1;

            if (!drawInfo)
                break;

            drawInfo->Flags = PH_GRAPH_USE_GRID_X | PH_GRAPH_USE_GRID_Y;
            Section->Parameters->ColorSetupFunction(drawInfo, PhGetIntegerSetting(L"ColorIoWrite"), 0);
            PhGetDrawInfoGraphBuffers(&Section->GraphState.Buffers, drawInfo, ContextSwitchesHistory.Count);

            if (!Section->GraphState.Valid)
            {
                FLOAT max = 1000;

                for (ULONG i = 0; i < drawInfo->LineDataCount; i++)
                {
                    FLOAT data1;

                    Section->GraphState.Data1[i] = data1 = (FLOAT)PhGetItemCircularBuffer_ULONG64(&ContextSwitchesHistory, i);

                    if (max < data1)
                        max = data1;
                }

                if (max != 0)
                {
                    PhDivideSinglesBySingle(Section->GraphState.Data1, max, drawInfo->LineDataCount);
                }

                Section->GraphState.Valid = TRUE;
            }
        }
        return TRUE;
    case SysInfoGraphGetTooltipText:
        {
            PPH_SYSINFO_GRAPH_GET_TOOLTIP_TEXT getTooltipText = (PPH_SYSINFO_GRAPH_GET_TOOLTIP_TEXT)Parameter1;
            ULONG64 value;
            PH_FORMAT format[3];

            if (!getTooltipText)
                break;

            value = PhGetItemCircularBuffer_ULONG64(&ContextSwitchesHistory, getTooltipText->Index);

            PhInitFormatI64UGroupDigits(&format[0], value);
            PhInitFormatC(&format[1], L'\n');
            PhInitFormatSR(&format[2], ((PPH_STRING)PH_AUTO(PhGetStatisticsTimeString(NULL, getTooltipText->Index)))->sr);

            PhMoveReference(&Section->GraphState.TooltipText, PhFormat(format, RTL_NUMBER_OF(format), 0));
            getTooltipText->Text = Section->GraphState.TooltipText->sr;
        }
        return TRUE;
    case SysInfoGraphDrawPanel:
        {
            PPH_SYSINFO_DRAW_PANEL drawPanel = (PPH_SYSINFO_DRAW_PANEL)Parameter1;

            if (!drawPanel)
                break;

            drawPanel->Title = PhCreateString(L"CPU");
        }
        return TRUE;
    }

    return FALSE;
}

VOID CpuPluginSysInfoInitializing(
    _In_ PPH_PLUGIN_SYSINFO_POINTERS Pointers
    )
{
    PH_SYSINFO_SECTION section;
    PPH_CPUPLUGIN_SYSINFO_CONTEXT context;

    context = PhAllocate(sizeof(PH_CPUPLUGIN_SYSINFO_CONTEXT));
    memset(context, 0, sizeof(PH_CPUPLUGIN_SYSINFO_CONTEXT));

    memset(&section, 0, sizeof(PH_SYSINFO_SECTION));
    section.Context = context;
    section.Callback = CpuPluginSectionCallback;
    PhInitializeStringRef(&section.Name, L"CpuPlugin");

    context->Section = Pointers->CreateSection(&section);
}
