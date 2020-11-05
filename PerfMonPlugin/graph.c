/*
 * Process Hacker Extra Plugins -
 *   Performance Monitor Plugin
 *
 * Copyright (C) 2015-2019 dmex
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

#include "perfmon.h"

#define MSG_UPDATE (WM_APP + 1)

PPH_STRING PerfCounterLabelYFunction(
    _In_ PPH_GRAPH_DRAW_INFO DrawInfo,
    _In_ ULONG DataIndex,
    _In_ FLOAT Value,
    _In_ FLOAT Parameter
    )
{
    ULONG64 size;

    size = (ULONG64)((DOUBLE)Value * (DOUBLE)Parameter);

    if (size != 0)
    {
        return PhFormatUInt64(size, TRUE);
    }
    else
    {
        return PhReferenceEmptyString();
    }
}

VOID NTAPI ProcessesUpdatedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PERFMON_SYSINFO_CONTEXT context = Context;

    if (context && context->WindowHandle)
    {
        PostMessage(context->WindowHandle, MSG_UPDATE, 0, 0);
    }
}

VOID PerfCounterUpdateGraphs(
    _Inout_ PPH_PERFMON_SYSINFO_CONTEXT Context
    )
{
    Context->GraphState.Valid = FALSE;
    Context->GraphState.TooltipIndex = ULONG_MAX;
    Graph_MoveGrid(Context->GraphHandle, 1);
    Graph_Draw(Context->GraphHandle);
    Graph_UpdateTooltip(Context->GraphHandle);
    InvalidateRect(Context->GraphHandle, NULL, FALSE);
}

INT_PTR CALLBACK PerfCounterDialogProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PPH_PERFMON_SYSINFO_CONTEXT context = NULL;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PPH_PERFMON_SYSINFO_CONTEXT)lParam;
        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);

        if (uMsg == WM_DESTROY)
        {
            PhDeleteLayoutManager(&context->LayoutManager);
            PhDeleteGraphState(&context->GraphState);

            if (context->GraphHandle)
                DestroyWindow(context->GraphHandle);

            PhUnregisterCallback(PhGetGeneralCallback(GeneralCallbackProcessProviderUpdatedEvent), &context->ProcessesUpdatedRegistration);
            PhRemoveWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
        }
    }

    if (context == NULL)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PPH_LAYOUT_ITEM panelItem;

            context->WindowHandle = hwndDlg;
            context->GraphHandle = CreateWindow(
                PH_GRAPH_CLASSNAME,
                NULL,
                WS_VISIBLE | WS_CHILD | WS_BORDER,
                0,
                0,
                3,
                3,
                hwndDlg,
                NULL,
                NULL,
                NULL
                );
            Graph_SetTooltip(context->GraphHandle, TRUE);

            PhInitializeGraphState(&context->GraphState);
            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);

            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_COUNTERNAME), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_RIGHT | PH_LAYOUT_FORCE_INVALIDATE);
            panelItem = PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_GRAPH_LAYOUT), NULL, PH_ANCHOR_ALL);
            PhAddLayoutItemEx(&context->LayoutManager, context->GraphHandle, NULL, PH_ANCHOR_ALL, panelItem->Margin);

            SendMessage(GetDlgItem(hwndDlg, IDC_COUNTERNAME), WM_SETFONT, (WPARAM)context->SysinfoSection->Parameters->LargeFont, FALSE);
            PhSetDialogItemText(hwndDlg, IDC_COUNTERNAME, context->SysinfoSection->Name.Buffer);

            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackProcessProviderUpdatedEvent),
                ProcessesUpdatedHandler,
                context,
                &context->ProcessesUpdatedRegistration
                );

            PerfCounterUpdateGraphs(context);
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&context->LayoutManager);
        break;
    case WM_NOTIFY:
        {
            NMHDR* header = (NMHDR*)lParam;

            if (header->hwndFrom == context->GraphHandle)
            {
                switch (header->code)
                {
                case GCN_GETDRAWINFO:
                    {
                        PPH_GRAPH_GETDRAWINFO getDrawInfo = (PPH_GRAPH_GETDRAWINFO)header;
                        PPH_GRAPH_DRAW_INFO drawInfo = getDrawInfo->DrawInfo;

                        drawInfo->Flags = PH_GRAPH_USE_GRID_X | PH_GRAPH_USE_GRID_Y | PH_GRAPH_LABEL_MAX_Y;
                        context->SysinfoSection->Parameters->ColorSetupFunction(drawInfo, PhGetIntegerSetting(L"ColorCpuKernel"), 0);
                        PhGraphStateGetDrawInfo(&context->GraphState, getDrawInfo, context->Entry->HistoryBuffer.Count);

                        if (!context->GraphState.Valid)
                        {
                            FLOAT max = 0;

                            for (ULONG i = 0; i < drawInfo->LineDataCount; i++)
                            {
                                context->GraphState.Data1[i] = (FLOAT)PhGetItemCircularBuffer_ULONG64(&context->Entry->HistoryBuffer, i);

                                if (context->GraphState.Data1[i] > max)
                                    max = context->GraphState.Data1[i];
                            }


                            if (max != 0)
                            {
                                // Scale the data.
                                PhDivideSinglesBySingle(
                                    context->GraphState.Data1,
                                    max,
                                    drawInfo->LineDataCount
                                    );
                            }

                            drawInfo->LabelYFunction = PerfCounterLabelYFunction;
                            drawInfo->LabelYFunctionParameter = max;

                            context->GraphState.Valid = TRUE;
                        }
                    }
                    break;
                case GCN_GETTOOLTIPTEXT:
                    {
                        PPH_GRAPH_GETTOOLTIPTEXT getTooltipText = (PPH_GRAPH_GETTOOLTIPTEXT)header;

                        if (getTooltipText->Index < getTooltipText->TotalCount)
                        {
                            if (context->GraphState.TooltipIndex != getTooltipText->Index)
                            {
                                ULONG64 counterValue = PhGetItemCircularBuffer_ULONG64(
                                    &context->Entry->HistoryBuffer,
                                    getTooltipText->Index
                                    );

                                PhMoveReference(&context->GraphState.TooltipText, PhFormatString(
                                    L"%s\n%s",
                                    PhaFormatUInt64(counterValue, TRUE)->Buffer,
                                    ((PPH_STRING)PH_AUTO(PhGetStatisticsTimeString(NULL, getTooltipText->Index)))->Buffer
                                    ));
                            }

                            getTooltipText->Text = context->GraphState.TooltipText->sr;
                        }
                    }
                    break;
                }
            }
        }
        break;
    case MSG_UPDATE:
        PerfCounterUpdateGraphs(context);
        break;
    }

    return FALSE;
}

BOOLEAN PerfCounterSectionCallback(
    _In_ PPH_SYSINFO_SECTION Section,
    _In_ PH_SYSINFO_SECTION_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2
    )
{
    PPH_PERFMON_SYSINFO_CONTEXT context = (PPH_PERFMON_SYSINFO_CONTEXT)Section->Context;

    switch (Message)
    {
    case SysInfoCreate:
        return TRUE;
    case SysInfoDestroy:
        return TRUE;
    case SysInfoTick:
        return TRUE;
    case SysInfoCreateDialog:
        {
            PPH_SYSINFO_CREATE_DIALOG createDialog = (PPH_SYSINFO_CREATE_DIALOG)Parameter1;

            if (!createDialog)
                break;

            createDialog->Instance = PluginInstance->DllBase;
            createDialog->Template = MAKEINTRESOURCE(IDD_PERFMON_DIALOG);
            createDialog->DialogProc = PerfCounterDialogProc;
            createDialog->Parameter = context;
        }
        return TRUE;
    case SysInfoGraphGetDrawInfo:
        {
            PPH_GRAPH_DRAW_INFO drawInfo = (PPH_GRAPH_DRAW_INFO)Parameter1;

            if (!drawInfo)
                break;

            drawInfo->Flags = PH_GRAPH_USE_GRID_X | PH_GRAPH_USE_GRID_Y | PH_GRAPH_LABEL_MAX_Y;
            Section->Parameters->ColorSetupFunction(drawInfo, PhGetIntegerSetting(L"ColorCpuKernel"), 0);
            PhGetDrawInfoGraphBuffers(&Section->GraphState.Buffers, drawInfo, context->Entry->HistoryBuffer.Count);

            if (!Section->GraphState.Valid)
            {
                FLOAT max = 0;

                for (ULONG i = 0; i < drawInfo->LineDataCount; i++)
                {
                    Section->GraphState.Data1[i] = (FLOAT)PhGetItemCircularBuffer_ULONG64(&context->Entry->HistoryBuffer, i);

                    if (Section->GraphState.Data1[i] > max)
                        max = Section->GraphState.Data1[i];
                }

                if (max != 0)
                {
                    // Scale the data.

                    PhDivideSinglesBySingle(
                        Section->GraphState.Data1,
                        max,
                        drawInfo->LineDataCount
                        );
                }

                drawInfo->LabelYFunction = PerfCounterLabelYFunction;
                drawInfo->LabelYFunctionParameter = max;

                Section->GraphState.Valid = TRUE;
            }
        }
        return TRUE;
    case SysInfoGraphGetTooltipText:
        {
            PPH_SYSINFO_GRAPH_GET_TOOLTIP_TEXT getTooltipText = (PPH_SYSINFO_GRAPH_GET_TOOLTIP_TEXT)Parameter1;
            ULONG64 counterValue;

            if (!getTooltipText)
                break;

            counterValue = PhGetItemCircularBuffer_ULONG64(
                &context->Entry->HistoryBuffer,
                getTooltipText->Index
                );

            PhMoveReference(&Section->GraphState.TooltipText, PhFormatString(
                L"%s\n%s",
                PhaFormatUInt64(counterValue, TRUE)->Buffer,
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

            drawPanel->Title = PhCreateString2(&Section->Name);
            drawPanel->SubTitle = PhFormatUInt64(context->Entry->HistoryDelta.Value, TRUE);
        }
        return TRUE;
    }

    return FALSE;
}

VOID PerfMonCounterSysInfoInitializing(
    _In_ PPH_PLUGIN_SYSINFO_POINTERS Pointers,
    _In_ PPERF_COUNTER_ENTRY Entry
    )
{
    PH_SYSINFO_SECTION section;
    PPH_PERFMON_SYSINFO_CONTEXT context;

    context = PhAllocateZero(sizeof(PH_PERFMON_SYSINFO_CONTEXT));
    context->Entry = Entry;

    section.Context = context;
    section.Callback = PerfCounterSectionCallback;

    PhInitializeStringRef(&section.Name, PhGetStringOrEmpty(Entry->Id.PerfCounterPath));

    context->SysinfoSection = Pointers->CreateSection(&section);
}
