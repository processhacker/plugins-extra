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

PPH_PLUGIN PluginInstance = NULL;
PH_CALLBACK_REGISTRATION PluginLoadCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginUnloadCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginShowOptionsCallbackRegistration;
PH_CALLBACK_REGISTRATION ProcessesUpdatedCallbackRegistration;
PH_CALLBACK_REGISTRATION SystemInformationInitializingCallbackRegistration;

ULONG NumberOfProcessors;
PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION CpuInformation;
PSYSTEM_INTERRUPT_INFORMATION InterruptInformation;
PH_UINT64_DELTA ContextSwitchesDelta;
PH_UINT64_DELTA InterruptsDelta;
PH_UINT64_DELTA DpcsDelta;
PH_UINT64_DELTA SystemCallsDelta;
PH_CIRCULAR_BUFFER_ULONG64 ContextSwitchesHistory;
PH_CIRCULAR_BUFFER_ULONG64 InterruptsHistory;
PH_CIRCULAR_BUFFER_ULONG64 DpcsHistory;
PH_CIRCULAR_BUFFER_ULONG64 SystemCallsHistory;
PH_PLUGIN_SYSTEM_STATISTICS PluginStatistics = { 0 };

VOID NTAPI LoadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    ULONG sampleCount;

    NumberOfProcessors = (ULONG)PhSystemBasicInformation.NumberOfProcessors;
    CpuInformation = PhAllocate(sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * NumberOfProcessors);
    InterruptInformation = PhAllocate(sizeof(SYSTEM_INTERRUPT_INFORMATION) * NumberOfProcessors);

    PhInitializeDelta(&ContextSwitchesDelta);
    PhInitializeDelta(&InterruptsDelta);
    PhInitializeDelta(&DpcsDelta);
    PhInitializeDelta(&SystemCallsDelta);

    sampleCount = PhGetIntegerSetting(L"SampleCount");
    PhInitializeCircularBuffer_ULONG64(&ContextSwitchesHistory, sampleCount);
    PhInitializeCircularBuffer_ULONG64(&InterruptsHistory, sampleCount);
    PhInitializeCircularBuffer_ULONG64(&DpcsHistory, sampleCount);
    PhInitializeCircularBuffer_ULONG64(&SystemCallsHistory, sampleCount);
}

VOID NTAPI UnloadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    NOTHING;
}

VOID NTAPI ShowOptionsCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    NOTHING;
}

VOID NTAPI ProcessesUpdatedCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    static ULONG runCount = 0; // MUST keep in sync with runCount in process provider
    ULONG64 dpcCount = 0;
    ULONG64 interruptCount = 0;

    if (NT_SUCCESS(NtQuerySystemInformation(
        SystemInterruptInformation,
        InterruptInformation,
        sizeof(SYSTEM_INTERRUPT_INFORMATION) * NumberOfProcessors,
        NULL
        )))
    {
        for (ULONG i = 0; i < NumberOfProcessors; i++)
            dpcCount += InterruptInformation[i].DpcCount;
    }

    if (NT_SUCCESS(NtQuerySystemInformation(
        SystemProcessorPerformanceInformation,
        CpuInformation,
        sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * NumberOfProcessors,
        NULL
        )))
    {
        for (ULONG i = 0; i < NumberOfProcessors; i++)
        {
            PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION cpuInfo = &CpuInformation[i];
            interruptCount += cpuInfo->InterruptCount;
        }
    }

    PhPluginGetSystemStatistics(&PluginStatistics);
    PhUpdateDelta(&ContextSwitchesDelta, PluginStatistics.Performance->ContextSwitches);
    PhUpdateDelta(&InterruptsDelta, interruptCount);
    PhUpdateDelta(&DpcsDelta, dpcCount);
    PhUpdateDelta(&SystemCallsDelta, PluginStatistics.Performance->SystemCalls);

    if (runCount != 0)
    {
        PhAddItemCircularBuffer_ULONG64(&ContextSwitchesHistory, ContextSwitchesDelta.Delta);
        PhAddItemCircularBuffer_ULONG64(&InterruptsHistory, InterruptsDelta.Delta);
        PhAddItemCircularBuffer_ULONG64(&DpcsHistory, DpcsDelta.Delta);
        PhAddItemCircularBuffer_ULONG64(&SystemCallsHistory, SystemCallsDelta.Delta);
    }

    runCount++;
}

VOID NTAPI SystemInformationInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_SYSINFO_POINTERS pluginEntry = (PPH_PLUGIN_SYSINFO_POINTERS)Parameter;

    if (pluginEntry)
    {
        CpuPluginSysInfoInitializing(pluginEntry);
    }
}

LOGICAL DllMain(
    _In_ HINSTANCE Instance,
    _In_ ULONG Reason,
    _Reserved_ PVOID Reserved
    )
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        {
            PPH_PLUGIN_INFORMATION info;

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->Author = L"dmex";
            info->DisplayName = L"Extra CPU graphs";

            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackLoad),
                LoadCallback,
                NULL,
                &PluginLoadCallbackRegistration
                );
            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackUnload),
                UnloadCallback,
                NULL,
                &PluginUnloadCallbackRegistration
                );
            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackShowOptions),
                ShowOptionsCallback,
                NULL,
                &PluginShowOptionsCallbackRegistration
                );
            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackProcessProviderUpdatedEvent),
                ProcessesUpdatedCallback,
                NULL,
                &ProcessesUpdatedCallbackRegistration
                );
            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackSystemInformationInitializing),
                SystemInformationInitializingCallback,
                NULL,
                &SystemInformationInitializingCallbackRegistration
                );
        }
        break;
    }

    return TRUE;
}
