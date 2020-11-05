/*
 * Process Hacker Extra Plugins -
 *   Nvidia GPU Plugin
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

#include "main.h"

static RECT NormalGraphTextMargin = { 5, 5, 5, 5 };
static RECT NormalGraphTextPadding = { 3, 3, 3, 3 };

INT_PTR CALLBACK NvGpuPanelDialogProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PPH_NVGPU_SYSINFO_CONTEXT context = NULL;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PPH_NVGPU_SYSINFO_CONTEXT)lParam;
        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);

        if (uMsg == WM_NCDESTROY)
        {
            PhRemoveWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
        }
    }

    if (context == NULL)
        return FALSE;

    switch (uMsg)
    {
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDC_DETAILS:
                {
                    ShowDetailsDialog(GetParent(hwndDlg), context);
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}

VOID NvGpuCreateGraphs(
    _Inout_ PPH_NVGPU_SYSINFO_CONTEXT Context
    )
{
    Context->GpuGraphHandle = CreateWindow(
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
    Graph_SetTooltip(Context->GpuGraphHandle, TRUE);

    Context->MemGraphHandle = CreateWindow(
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
    Graph_SetTooltip(Context->MemGraphHandle, TRUE);

    Context->SharedGraphHandle = CreateWindow(
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
    Graph_SetTooltip(Context->SharedGraphHandle, TRUE);

    Context->BusGraphHandle = CreateWindow(
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
    Graph_SetTooltip(Context->BusGraphHandle, TRUE);
}

VOID NvGpuLayoutGraphs(
    _Inout_ PPH_NVGPU_SYSINFO_CONTEXT Context
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
    GetClientRect(Context->GpuLabelHandle, &labelRect);

    graphWidth = clientRect.right - Context->GpuGraphMargin.left - Context->GpuGraphMargin.right;
    graphHeight = (clientRect.bottom - Context->GpuGraphMargin.top - Context->GpuGraphMargin.bottom - labelRect.bottom * 4 - ET_GPU_PADDING * 5) / 4;

    deferHandle = BeginDeferWindowPos(8);
    y = Context->GpuGraphMargin.top;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->GpuLabelHandle,
        NULL,
        Context->GpuGraphMargin.left,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += labelRect.bottom + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->GpuGraphHandle,
        NULL,
        Context->GpuGraphMargin.left,
        y,
        graphWidth,
        graphHeight,
        SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += graphHeight + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->MemLabelHandle,
        NULL,
        Context->GpuGraphMargin.left,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += labelRect.bottom + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->MemGraphHandle,
        NULL,
        Context->GpuGraphMargin.left,
        y,
        graphWidth,
        graphHeight,
        SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += graphHeight + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->SharedLabelHandle,
        NULL,
        Context->GpuGraphMargin.left,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += labelRect.bottom + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->SharedGraphHandle,
        NULL,
        Context->GpuGraphMargin.left,
        y,
        graphWidth,
        graphHeight,
        SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += graphHeight + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->BusLabelHandle,
        NULL,
        Context->GpuGraphMargin.left,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER
        );
    y += labelRect.bottom + ET_GPU_PADDING;

    deferHandle = DeferWindowPos(
        deferHandle,
        Context->BusGraphHandle,
        NULL,
        Context->GpuGraphMargin.left,
        y,
        graphWidth,
        clientRect.bottom - Context->GpuGraphMargin.bottom - y,
        SWP_NOACTIVATE | SWP_NOZORDER
        );

    EndDeferWindowPos(deferHandle);
}

VOID NvGpuNotifyUsageGraph(
    _Inout_ PPH_NVGPU_SYSINFO_CONTEXT Context,
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
            PhGraphStateGetDrawInfo(&Context->GpuGraphState, getDrawInfo, GpuUtilizationHistory.Count);

            if (PhGetIntegerSetting(L"GraphShowText"))
            {
                HDC hdc = Graph_GetBufferedContext(Context->GpuGraphHandle);

                PhMoveReference(&Context->GpuGraphState.Text,
                    PhFormatString(L"%.0f%%", (DOUBLE)GpuCurrentGpuUsage * 100)
                    );

                SelectObject(hdc, PhApplicationFont);
                PhSetGraphText(hdc, drawInfo, &Context->GpuGraphState.Text->sr,
                    &NormalGraphTextMargin, &NormalGraphTextPadding, PH_ALIGN_TOP | PH_ALIGN_LEFT);
            }
            else
            {
                drawInfo->Text.Buffer = NULL;
            }

            if (!Context->GpuGraphState.Valid)
            {
                PhCopyCircularBuffer_FLOAT(&GpuUtilizationHistory, Context->GpuGraphState.Data1, drawInfo->LineDataCount);
                Context->GpuGraphState.Valid = TRUE;
            }
        }
        break;
    case GCN_GETTOOLTIPTEXT:
        {
            PPH_GRAPH_GETTOOLTIPTEXT getTooltipText = (PPH_GRAPH_GETTOOLTIPTEXT)Header;

            if (getTooltipText->Index < getTooltipText->TotalCount)
            {
                if (Context->GpuGraphState.TooltipIndex != getTooltipText->Index)
                {
                    FLOAT gpuUsageValue;

                    gpuUsageValue = PhGetItemCircularBuffer_FLOAT(&GpuUtilizationHistory, getTooltipText->Index);

                    PhMoveReference(&Context->GpuGraphState.TooltipText, PhFormatString(
                        L"%.0f%%\n%s",
                        (DOUBLE)gpuUsageValue * 100,
                        ((PPH_STRING)PH_AUTO(PhGetStatisticsTimeString(NULL, getTooltipText->Index)))->Buffer
                        ));
                }

                getTooltipText->Text = Context->GpuGraphState.TooltipText->sr;
            }
        }
        break;
    }
}

VOID NvGpuNotifyMemoryGraph(
    _Inout_ PPH_NVGPU_SYSINFO_CONTEXT Context,
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
            PhGraphStateGetDrawInfo(&Context->MemGraphState, getDrawInfo, GpuMemoryHistory.Count);

            if (PhGetIntegerSetting(L"GraphShowText"))
            {
                HDC hdc = Graph_GetBufferedContext(Context->MemGraphHandle);

                PhMoveReference(&Context->MemGraphState.Text, PhFormatString(
                    L"%s / %s (%.2f%%)", 
                    PhaFormatSize(UInt32x32To64(GpuCurrentMemUsage, 1024), -1)->Buffer,
                    PhaFormatSize(UInt32x32To64(GpuMemoryLimit, 1024), -1)->Buffer,
                    (DOUBLE)GpuCurrentMemUsage / GpuMemoryLimit * 100
                    ));

                SelectObject(hdc, PhApplicationFont);
                PhSetGraphText(hdc, drawInfo, &Context->MemGraphState.Text->sr,
                    &NormalGraphTextMargin, &NormalGraphTextPadding, PH_ALIGN_TOP | PH_ALIGN_LEFT);
            }
            else
            {
                drawInfo->Text.Buffer = NULL;
            }

            if (!Context->MemGraphState.Valid)
            {
                for (ULONG i = 0; i < drawInfo->LineDataCount; i++)
                {
                    Context->MemGraphState.Data1[i] = (FLOAT)PhGetItemCircularBuffer_ULONG(&GpuMemoryHistory, i);
                }

                if (GpuMemoryLimit != 0)
                {
                    // Scale the data.
                    PhDivideSinglesBySingle(
                        Context->MemGraphState.Data1,
                        (FLOAT)GpuMemoryLimit,
                        drawInfo->LineDataCount
                        );
                }

                Context->MemGraphState.Valid = TRUE;
            }
        }
        break;
    case GCN_GETTOOLTIPTEXT:
        {
            PPH_GRAPH_GETTOOLTIPTEXT getTooltipText = (PPH_GRAPH_GETTOOLTIPTEXT)Header;

            if (getTooltipText->Index < getTooltipText->TotalCount)
            {
                if (Context->MemGraphState.TooltipIndex != getTooltipText->Index)
                {
                    ULONG usedPages;

                    usedPages = PhGetItemCircularBuffer_ULONG(&GpuMemoryHistory, getTooltipText->Index);

                    PhMoveReference(&Context->MemGraphState.TooltipText, PhFormatString(
                        L"%s / %s (%.2f%%)\n%s",
                        PhaFormatSize(UInt32x32To64(usedPages, 1024), -1)->Buffer,
                        PhaFormatSize(UInt32x32To64(GpuMemoryLimit, 1024), -1)->Buffer,
                        (DOUBLE)usedPages / GpuMemoryLimit * 100,
                        ((PPH_STRING)PH_AUTO(PhGetStatisticsTimeString(NULL, getTooltipText->Index)))->Buffer
                        ));
                }

                getTooltipText->Text = Context->MemGraphState.TooltipText->sr;
            }
        }
        break;
    }
}

VOID NvGpuNotifySharedGraph(
    _Inout_ PPH_NVGPU_SYSINFO_CONTEXT Context,
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
            PhGraphStateGetDrawInfo(&Context->SharedGraphState, getDrawInfo, GpuBoardHistory.Count);

            if (PhGetIntegerSetting(L"GraphShowText"))
            {
                HDC hdc = Graph_GetBufferedContext(Context->SharedGraphHandle);

                PhMoveReference(&Context->SharedGraphState.Text, PhFormatString(
                    L"%.0f%%",
                    (DOUBLE)GpuCurrentCoreUsage * 100
                    ));

                SelectObject(hdc, PhApplicationFont);
                PhSetGraphText(hdc, drawInfo, &Context->SharedGraphState.Text->sr,
                    &NormalGraphTextMargin, &NormalGraphTextPadding, PH_ALIGN_TOP | PH_ALIGN_LEFT);
            }
            else
            {
                drawInfo->Text.Buffer = NULL;
            }

            if (!Context->SharedGraphState.Valid)
            {
                PhCopyCircularBuffer_FLOAT(&GpuBoardHistory, Context->SharedGraphState.Data1, drawInfo->LineDataCount);
                Context->SharedGraphState.Valid = TRUE;
            }
        }
        break;
    case GCN_GETTOOLTIPTEXT:
        {
            PPH_GRAPH_GETTOOLTIPTEXT getTooltipText = (PPH_GRAPH_GETTOOLTIPTEXT)Header;

            if (getTooltipText->Index < getTooltipText->TotalCount)
            {
                if (Context->SharedGraphState.TooltipIndex != getTooltipText->Index)
                {
                    FLOAT usedPages;

                    usedPages = PhGetItemCircularBuffer_FLOAT(&GpuBoardHistory, getTooltipText->Index);

                    PhMoveReference(&Context->SharedGraphState.TooltipText, PhFormatString(
                        L"%.0f%%\n%s",
                        (DOUBLE)usedPages * 100,
                        ((PPH_STRING)PH_AUTO(PhGetStatisticsTimeString(NULL, getTooltipText->Index)))->Buffer
                        ));
                }

                getTooltipText->Text = Context->SharedGraphState.TooltipText->sr;
            }
        }
        break;
    }
}

VOID NvGpuNotifyBusGraph(
    _Inout_ PPH_NVGPU_SYSINFO_CONTEXT Context,
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
            PhGraphStateGetDrawInfo(&Context->BusGraphState, getDrawInfo, GpuBusHistory.Count);

            if (PhGetIntegerSetting(L"GraphShowText"))
            {
                HDC hdc = Graph_GetBufferedContext(Context->BusGraphHandle);

                PhMoveReference(&Context->BusGraphState.Text, PhFormatString(
                    L"%.0f%%",
                    (DOUBLE)GpuCurrentBusUsage * 100
                    ));

                SelectObject(hdc, PhApplicationFont);
                PhSetGraphText(hdc, drawInfo, &Context->BusGraphState.Text->sr,
                    &NormalGraphTextMargin, &NormalGraphTextPadding, PH_ALIGN_TOP | PH_ALIGN_LEFT);
            }
            else
            {
                drawInfo->Text.Buffer = NULL;
            }

            if (!Context->BusGraphState.Valid)
            {
                PhCopyCircularBuffer_FLOAT(&GpuBusHistory, Context->BusGraphState.Data1, drawInfo->LineDataCount);
                Context->BusGraphState.Valid = TRUE;
            }
        }
        break;
    case GCN_GETTOOLTIPTEXT:
        {
            PPH_GRAPH_GETTOOLTIPTEXT getTooltipText = (PPH_GRAPH_GETTOOLTIPTEXT)Header;

            if (getTooltipText->Index < getTooltipText->TotalCount)
            {
                if (Context->BusGraphState.TooltipIndex != getTooltipText->Index)
                {
                    FLOAT busUsage;

                    busUsage = PhGetItemCircularBuffer_FLOAT(&GpuBusHistory, getTooltipText->Index);

                    PhMoveReference(&Context->BusGraphState.TooltipText, PhFormatString(
                        L"%.0f%%\n%s",
                        (DOUBLE)busUsage * 100,
                        ((PPH_STRING)PH_AUTO(PhGetStatisticsTimeString(NULL, getTooltipText->Index)))->Buffer
                        ));
                }

                getTooltipText->Text = Context->BusGraphState.TooltipText->sr;
            }
        }
        break;
    }
}

VOID NvGpuUpdateGraphs(
    _Inout_ PPH_NVGPU_SYSINFO_CONTEXT Context
    )
{
    Context->GpuGraphState.Valid = FALSE;
    Context->GpuGraphState.TooltipIndex = -1;
    Graph_MoveGrid(Context->GpuGraphHandle, 1);
    Graph_Draw(Context->GpuGraphHandle);
    Graph_UpdateTooltip(Context->GpuGraphHandle);
    InvalidateRect(Context->GpuGraphHandle, NULL, FALSE);

    Context->MemGraphState.Valid = FALSE;
    Context->MemGraphState.TooltipIndex = -1;
    Graph_MoveGrid(Context->MemGraphHandle, 1);
    Graph_Draw(Context->MemGraphHandle);
    Graph_UpdateTooltip(Context->MemGraphHandle);
    InvalidateRect(Context->MemGraphHandle, NULL, FALSE);

    Context->SharedGraphState.Valid = FALSE;
    Context->SharedGraphState.TooltipIndex = -1;
    Graph_MoveGrid(Context->SharedGraphHandle, 1);
    Graph_Draw(Context->SharedGraphHandle);
    Graph_UpdateTooltip(Context->SharedGraphHandle);
    InvalidateRect(Context->SharedGraphHandle, NULL, FALSE);

    Context->BusGraphState.Valid = FALSE;
    Context->BusGraphState.TooltipIndex = -1;
    Graph_MoveGrid(Context->BusGraphHandle, 1);
    Graph_Draw(Context->BusGraphHandle);
    Graph_UpdateTooltip(Context->BusGraphHandle);
    InvalidateRect(Context->BusGraphHandle, NULL, FALSE);
}

VOID NvGpuUpdatePanel(
    _Inout_ PPH_NVGPU_SYSINFO_CONTEXT Context
    )
{
    NvGpuUpdateVoltage();

    PhSetDialogItemText(Context->GpuPanel, IDC_CLOCK_CORE, PhaFormatString(L"%lu MHz", GpuCurrentCoreClock)->Buffer);
    PhSetDialogItemText(Context->GpuPanel, IDC_CLOCK_MEMORY, PhaFormatString(L"%lu MHz", GpuCurrentMemoryClock)->Buffer);
    PhSetDialogItemText(Context->GpuPanel, IDC_CLOCK_SHADER, PhaFormatString(L"%lu MHz", GpuCurrentShaderClock)->Buffer);
    PhSetDialogItemText(Context->GpuPanel, IDC_FAN_PERCENT, ((PPH_STRING)PH_AUTO(NvGpuQueryFanSpeed()))->Buffer);

    if (PhGetIntegerSetting(SETTING_NAME_ENABLE_FAHRENHEIT))
    {
        FLOAT fahrenheit = (FLOAT)(GpuCurrentCoreTemp * 1.8 + 32);

        PhSetDialogItemText(Context->GpuPanel, IDC_TEMP_VALUE, PhaFormatString(L"%.1f\u00b0F", fahrenheit)->Buffer);
    }
    else
    {
        PhSetDialogItemText(Context->GpuPanel, IDC_TEMP_VALUE, PhaFormatString(L"%lu\u00b0C", GpuCurrentCoreTemp)->Buffer);
    }

    //PhSetDialogItemText(Context->GpuPanel, IDC_TEMP_VALUE, PhaFormatString(L"%s\u00b0C", PhaFormatUInt64(GpuCurrentBoardTemp, TRUE)->Buffer)->Buffer);
    PhSetDialogItemText(Context->GpuPanel, IDC_VOLTAGE, PhaFormatString(L"%lu mV", GpuCurrentVoltage)->Buffer);
    PhSetDialogItemText(Context->GpuPanel, IDC_TX, PhaFormatString(L"%s/s", PhaFormatSize(GpuPcieThroughputTx, ULONG_MAX)->Buffer)->Buffer);
    PhSetDialogItemText(Context->GpuPanel, IDC_RX, PhaFormatString(L"%s/s", PhaFormatSize(GpuPcieThroughputRx, ULONG_MAX)->Buffer)->Buffer);
    PhSetDialogItemText(Context->GpuPanel, IDC_POWER, PhaFormatString(L"%.2f mW\n", (DOUBLE)GpuPciePowerUsage * 0.001f)->Buffer);
}

INT_PTR CALLBACK NvGpuDialogProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PPH_NVGPU_SYSINFO_CONTEXT context = NULL;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PPH_NVGPU_SYSINFO_CONTEXT)lParam;

        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);

        if (uMsg == WM_NCDESTROY)
        {
            PhDeleteLayoutManager(&context->LayoutManager);

            PhDeleteGraphState(&context->GpuGraphState);
            PhDeleteGraphState(&context->MemGraphState);
            PhDeleteGraphState(&context->SharedGraphState);
            PhDeleteGraphState(&context->BusGraphState);

            if (context->GpuGraphHandle)
                DestroyWindow(context->GpuGraphHandle);
            if (context->MemGraphHandle)
                DestroyWindow(context->MemGraphHandle);
            if (context->SharedGraphHandle)
                DestroyWindow(context->SharedGraphHandle);
            if (context->BusGraphHandle)
                DestroyWindow(context->BusGraphHandle);
            if (context->GpuPanel)
                DestroyWindow(context->GpuPanel);

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
            PPH_LAYOUT_ITEM panelItem;

            context->WindowHandle = hwndDlg;

            context->GpuLabelHandle = GetDlgItem(hwndDlg, IDC_GPU_L);
            context->MemLabelHandle = GetDlgItem(hwndDlg, IDC_MEMORY_L);
            context->SharedLabelHandle = GetDlgItem(hwndDlg, IDC_SHARED_L);
            context->BusLabelHandle = GetDlgItem(hwndDlg, IDC_BUS_L);

            PhInitializeGraphState(&context->GpuGraphState);
            PhInitializeGraphState(&context->MemGraphState);
            PhInitializeGraphState(&context->SharedGraphState);
            PhInitializeGraphState(&context->BusGraphState);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_GPUNAME), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_RIGHT | PH_LAYOUT_FORCE_INVALIDATE);
            graphItem = PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_GRAPH_LAYOUT), NULL, PH_ANCHOR_ALL);
            context->GpuGraphMargin = graphItem->Margin;
            panelItem = PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_PANEL_LAYOUT), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_RIGHT | PH_ANCHOR_BOTTOM);

            SetWindowFont(GetDlgItem(hwndDlg, IDC_TITLE), context->Section->Parameters->LargeFont, FALSE);
            SetWindowFont(GetDlgItem(hwndDlg, IDC_GPUNAME), context->Section->Parameters->MediumFont, FALSE);
            PhSetDialogItemText(hwndDlg, IDC_GPUNAME, context->GpuName->Buffer);

            context->GpuPanel = CreateDialogParam(PluginInstance->DllBase, MAKEINTRESOURCE(IDD_GPU_PANEL), hwndDlg, NvGpuPanelDialogProc, (LPARAM)context);
            ShowWindow(context->GpuPanel, SW_SHOW);
            PhAddLayoutItemEx(&context->LayoutManager, context->GpuPanel, NULL, PH_ANCHOR_LEFT | PH_ANCHOR_RIGHT | PH_ANCHOR_BOTTOM, panelItem->Margin);

            NvGpuCreateGraphs(context);

            NvGpuUpdateValues();
            NvGpuUpdateGraphs(context);
            NvGpuUpdatePanel(context);
        }
        break;
    case WM_SIZE:
        NvGpuLayoutGraphs(context);
        break;
    case WM_NOTIFY:
        {
            NMHDR* header = (NMHDR*)lParam;

            if (header->hwndFrom == context->GpuGraphHandle)
            {
                NvGpuNotifyUsageGraph(context, header);
            }
            else if (header->hwndFrom == context->MemGraphHandle)
            {
                NvGpuNotifyMemoryGraph(context, header);
            }
            else if (header->hwndFrom == context->SharedGraphHandle)
            {
                NvGpuNotifySharedGraph(context, header);
            }
            else if (header->hwndFrom == context->BusGraphHandle)
            {
                NvGpuNotifyBusGraph(context, header);
            }
        }
        break;
    case MSG_UPDATE:
        {
            NvGpuUpdateGraphs(context);
            NvGpuUpdatePanel(context);
        }
        break;
    }

    return FALSE;
}

BOOLEAN NvGpuSectionCallback(
    _In_ PPH_SYSINFO_SECTION Section,
    _In_ PH_SYSINFO_SECTION_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2
    )
{
    PPH_NVGPU_SYSINFO_CONTEXT context = (PPH_NVGPU_SYSINFO_CONTEXT)Section->Context;

    switch (Message)
    {
    case SysInfoCreate:
        return TRUE;
    case SysInfoDestroy:
        {
            if (context->GpuName)
                PhDereferenceObject(context->GpuName);

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
            createDialog->DialogProc = NvGpuDialogProc;
            createDialog->Parameter = context;
        }
        return TRUE;
    case SysInfoGraphGetDrawInfo:
        {
            PPH_GRAPH_DRAW_INFO drawInfo = (PPH_GRAPH_DRAW_INFO)Parameter1;

            if (!drawInfo)
                break;

            drawInfo->Flags = PH_GRAPH_USE_GRID_X | PH_GRAPH_USE_GRID_Y;
            Section->Parameters->ColorSetupFunction(drawInfo, PhGetIntegerSetting(L"ColorCpuKernel"), 0);
            PhGetDrawInfoGraphBuffers(&Section->GraphState.Buffers, drawInfo, GpuUtilizationHistory.Count);

            if (!Section->GraphState.Valid)
            {
                PhCopyCircularBuffer_FLOAT(&GpuUtilizationHistory, Section->GraphState.Data1, drawInfo->LineDataCount);
                Section->GraphState.Valid = TRUE;
            }
        }
        return TRUE;
    case SysInfoGraphGetTooltipText:
        {
            FLOAT gpuUsageValue;
            PPH_SYSINFO_GRAPH_GET_TOOLTIP_TEXT getTooltipText = (PPH_SYSINFO_GRAPH_GET_TOOLTIP_TEXT)Parameter1;

            if (!getTooltipText)
                break;

            gpuUsageValue = PhGetItemCircularBuffer_FLOAT(&GpuUtilizationHistory, getTooltipText->Index);

            PhMoveReference(&Section->GraphState.TooltipText, PhFormatString(
                L"%.0f%%\n%s",
                (DOUBLE)gpuUsageValue * 100,
                ((PPH_STRING)PH_AUTO(PhGetStatisticsTimeString(NULL, getTooltipText->Index)))->Buffer
                ));

            getTooltipText->Text = Section->GraphState.TooltipText->sr;
        }
        return TRUE;
    case SysInfoGraphDrawPanel:
        {
            PPH_SYSINFO_DRAW_PANEL drawPanel = (PPH_SYSINFO_DRAW_PANEL)Parameter1;

            if (!drawPanel)
                break;

            drawPanel->Title = PhCreateString(Section->Name.Buffer);
            drawPanel->SubTitle = PhFormatString(
                L"%.0f%%",
                (DOUBLE)GpuCurrentGpuUsage * 100
                );
        }
        return TRUE;
    }

    return FALSE;
}

VOID NvGpuSysInfoInitializing(
    _In_ PPH_PLUGIN_SYSINFO_POINTERS Pointers
    )
{
    PH_SYSINFO_SECTION section;
    PPH_NVGPU_SYSINFO_CONTEXT context;

    context = (PPH_NVGPU_SYSINFO_CONTEXT)PhAllocate(sizeof(PH_NVGPU_SYSINFO_CONTEXT));
    memset(context, 0, sizeof(PH_NVGPU_SYSINFO_CONTEXT));
    memset(&section, 0, sizeof(PH_SYSINFO_SECTION));

    section.Context = context;
    section.Callback = NvGpuSectionCallback;

    context->GpuName = NvGpuQueryName();
    PhInitializeStringRef(&section.Name, context->GpuName->Buffer);

    context->Section = Pointers->CreateSection(&section);
}
