/*
 * Process Hacker Extra Plugins -
 *   Terminator Plugin
 *
 * Copyright (C) 2010-2011 wj32
 * Copyright (C) 2016-2017 dmex
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
#include "phapi.h"
#include "kph2user.h"

NTSTATUS NTAPI TerminatorTP1(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    HANDLE processHandle;

    if (NT_SUCCESS(status = Ph2OpenProcess(
        &processHandle,
        PROCESS_TERMINATE,
        ProcessId
        )))
    {
        // Don't use KPH.
        status = NtTerminateProcess(processHandle, STATUS_SUCCESS);

        NtClose(processHandle);
    }

    return status;
}

NTSTATUS NTAPI TerminatorTP2(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    HANDLE processHandle;

    if (NT_SUCCESS(status = Ph2OpenProcess(
        &processHandle,
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
        ProcessId
        )))
    {
        status = RtlCreateUserThread(
            processHandle,
            NULL,
            FALSE,
            0,
            0,
            0,
            PhGetModuleProcAddress(L"ntdll.dll", "RtlExitUserProcess"),
            NULL,
            NULL,
            NULL
            );

        NtClose(processHandle);
    }

    return status;
}

NTSTATUS NTAPI TerminatorTTGeneric(
    _In_ HANDLE ProcessId,
    _In_ BOOLEAN UseKph
    )
{
    NTSTATUS status;
    PVOID processes;
    PSYSTEM_PROCESS_INFORMATION process;

    if (UseKph && !Kph2IsConnected())
        return STATUS_NOT_SUPPORTED;

    if (!NT_SUCCESS(status = PhEnumProcesses(&processes)))
        return status;

    process = PhFindProcessInformation(processes, ProcessId);

    if (!process)
    {
        PhFree(processes);
        return STATUS_INVALID_CID;
    }

    for (ULONG i = 0; i < process->NumberOfThreads; i++)
    {
        HANDLE threadHandle;

        if (NT_SUCCESS(Ph2OpenThread(
            &threadHandle,
            THREAD_TERMINATE,
            process->Threads[i].ClientId.UniqueThread
            )))
        {
            if (UseKph)
                Kph2TerminateThread(threadHandle, STATUS_SUCCESS);
            else
                NtTerminateThread(threadHandle, STATUS_SUCCESS);

            NtClose(threadHandle);
        }
    }

    PhFree(processes);

    return STATUS_SUCCESS;
}

NTSTATUS NTAPI TerminatorTT1(
    _In_ HANDLE ProcessId
    )
{
    return TerminatorTTGeneric(ProcessId, FALSE);
}

NTSTATUS NTAPI TerminatorTT2(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    PVOID processes;
    PSYSTEM_PROCESS_INFORMATION process;
    CONTEXT context;

    if (!NT_SUCCESS(status = PhEnumProcesses(&processes)))
        return status;

    process = PhFindProcessInformation(processes, ProcessId);

    if (!process)
    {
        PhFree(processes);
        return STATUS_INVALID_CID;
    }

    for (ULONG i = 0; i < process->NumberOfThreads; i++)
    {
        HANDLE threadHandle;

        if (NT_SUCCESS(Ph2OpenThread(
            &threadHandle,
            THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
            process->Threads[i].ClientId.UniqueThread
            )))
        {
            context.ContextFlags = CONTEXT_CONTROL;
            Ph2GetThreadContext(threadHandle, &context);

#ifdef _M_IX86
            context.Eip = (DWORD)PhGetModuleProcAddress(L"ntdll.dll", "RtlExitUserProcess");
#endif
#ifdef _M_AMD64
            context.Rip = (DWORD64)PhGetModuleProcAddress(L"ntdll.dll", "RtlExitUserProcess");
#endif
#ifdef _M_ARM64
            context.Pc = (DWORD64)PhGetModuleProcAddress(L"ntdll.dll", "RtlExitUserProcess");
#endif

            Ph2SetThreadContext(threadHandle, &context);
            NtClose(threadHandle);
        }
    }

    PhFree(processes);

    return STATUS_SUCCESS;
}

NTSTATUS NTAPI TerminatorTP1a(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    HANDLE processHandle = NULL;

    if (!NT_SUCCESS(status = NtGetNextProcess(
        NULL,
        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE,
        0,
        0,
        &processHandle
        )))
    {
        return status;
    }

    while (TRUE)
    {
        HANDLE newProcessHandle;
        PROCESS_BASIC_INFORMATION basicInfo;

        if (NT_SUCCESS(PhGetProcessBasicInformation(processHandle, &basicInfo)))
        {
            if (basicInfo.UniqueProcessId == ProcessId)
            {
                Ph2TerminateProcess(processHandle, STATUS_SUCCESS);
                NtClose(processHandle);
                break;
            }
        }

        if (NT_SUCCESS(status = NtGetNextProcess(
            processHandle,
            PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE,
            0,
            0,
            &newProcessHandle
            )))
        {
            NtClose(processHandle);
            processHandle = newProcessHandle;
        }
        else
        {
            NtClose(processHandle);
            break;
        }
    }

    return status;
}

NTSTATUS NTAPI TerminatorTT1a(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    HANDLE processHandle;
    HANDLE threadHandle;
    HANDLE newThreadHandle;

    if (NT_SUCCESS(status = Ph2OpenProcess(
        &processHandle,
        PROCESS_QUERY_INFORMATION, // NtGetNextThread actually requires this access for some reason
        ProcessId
        )))
    {
        if (!NT_SUCCESS(status = NtGetNextThread(
            processHandle,
            NULL,
            THREAD_TERMINATE,
            0,
            0,
            &threadHandle
            )))
        {
            NtClose(processHandle);
            return status;
        }

        while (TRUE)
        {
            Ph2TerminateThread(threadHandle, STATUS_SUCCESS);

            if (NT_SUCCESS(NtGetNextThread(
                processHandle,
                threadHandle,
                THREAD_TERMINATE,
                0,
                0,
                &newThreadHandle
                )))
            {
                NtClose(threadHandle);
                threadHandle = newThreadHandle;
            }
            else
            {
                NtClose(threadHandle);
                break;
            }
        }

        NtClose(processHandle);
    }

    return status;
}

NTSTATUS NTAPI TerminatorCH1(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    HANDLE processHandle;

    if (NT_SUCCESS(status = Ph2OpenProcess(
        &processHandle,
        PROCESS_DUP_HANDLE,
        ProcessId
        )))
    {
        for (ULONG i = 0; i < 0x1000; i += 4)
        {
            Ph2DuplicateObject(
                processHandle,
                UlongToHandle(i),
                NULL,
                NULL,
                0,
                0,
                DUPLICATE_CLOSE_SOURCE
                );
        }

        NtClose(processHandle);
    }

    return status;
}

static BOOL CALLBACK DestroyProcessWindowsProc(
    _In_ HWND hwnd,
    _In_ LPARAM lParam
    )
{
    ULONG processId;

    GetWindowThreadProcessId(hwnd, &processId);

    if (processId == (ULONG)lParam)
    {
        PostMessage(hwnd, WM_DESTROY, 0, 0);
    }

    return TRUE;
}

NTSTATUS NTAPI TerminatorW1(
    _In_ HANDLE ProcessId
    )
{
    EnumWindows(DestroyProcessWindowsProc, (LPARAM)ProcessId);
    return STATUS_SUCCESS;
}

static BOOL CALLBACK QuitProcessWindowsProc(
    _In_ HWND hwnd,
    _In_ LPARAM lParam
    )
{
    ULONG processId;

    GetWindowThreadProcessId(hwnd, &processId);

    if (processId == (ULONG)lParam)
    {
        PostMessage(hwnd, WM_QUIT, 0, 0);
    }

    return TRUE;
}

NTSTATUS NTAPI TerminatorW2(
    _In_ HANDLE ProcessId
    )
{
    EnumWindows(QuitProcessWindowsProc, (LPARAM)ProcessId);
    return STATUS_SUCCESS;
}

static BOOL CALLBACK CloseProcessWindowsProc(
    _In_ HWND hwnd,
    _In_ LPARAM lParam
    )
{
    ULONG processId;

    GetWindowThreadProcessId(hwnd, &processId);

    if (processId == (ULONG)lParam)
    {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }

    return TRUE;
}

NTSTATUS NTAPI TerminatorW3(
    _In_ HANDLE ProcessId
    )
{
    EnumWindows(CloseProcessWindowsProc, (LPARAM)ProcessId);
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI TerminatorTJ1(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    HANDLE processHandle;

    // TODO: Check if the process is already in a job.

    if (NT_SUCCESS(status = Ph2OpenProcess(
        &processHandle,
        PROCESS_SET_QUOTA | PROCESS_TERMINATE,
        ProcessId
        )))
    {
        HANDLE jobHandle;

        status = NtCreateJobObject(&jobHandle, JOB_OBJECT_ALL_ACCESS, NULL);

        if (NT_SUCCESS(status))
        {
            status = NtAssignProcessToJobObject(jobHandle, processHandle);

            if (NT_SUCCESS(status))
                status = NtTerminateJobObject(jobHandle, STATUS_SUCCESS);

            NtClose(jobHandle);
        }

        NtClose(processHandle);
    }

    return status;
}

NTSTATUS NTAPI TerminatorTD1(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    HANDLE processHandle;

    if (NT_SUCCESS(status = Ph2OpenProcess(
        &processHandle,
        PROCESS_SUSPEND_RESUME,
        ProcessId
        )))
    {
        HANDLE debugObjectHandle;
        OBJECT_ATTRIBUTES objectAttributes;

        InitializeObjectAttributes(
            &objectAttributes,
            NULL,
            0,
            NULL,
            NULL
            );

        if (NT_SUCCESS(NtCreateDebugObject(
            &debugObjectHandle,
            DEBUG_PROCESS_ASSIGN,
            &objectAttributes,
            DEBUG_KILL_ON_CLOSE
            )))
        {
            NtDebugActiveProcess(processHandle, debugObjectHandle);
            NtClose(debugObjectHandle);
        }

        NtClose(processHandle);
    }

    return status;
}

NTSTATUS NTAPI TerminatorTP3(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    HANDLE processHandle;

    if (!Kph2IsConnected())
        return STATUS_NOT_SUPPORTED;

    if (NT_SUCCESS(status = Ph2OpenProcess(
        &processHandle,
        SYNCHRONIZE, // KPH doesn't require any access for this operation
        ProcessId
        )))
    {
        status = Kph2TerminateProcess(processHandle, STATUS_SUCCESS);

        NtClose(processHandle);
    }

    return status;
}

NTSTATUS NTAPI TerminatorTT3(
    _In_ HANDLE ProcessId
    )
{
    return TerminatorTTGeneric(ProcessId, TRUE);
}

NTSTATUS NTAPI TerminatorM1(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    HANDLE processHandle;

    if (NT_SUCCESS(status = Ph2OpenProcess(
        &processHandle,
        PROCESS_QUERY_INFORMATION | PROCESS_VM_WRITE,
        ProcessId
        )))
    {
        PVOID pageOfGarbage;
        SIZE_T pageSize;
        PVOID baseAddress;
        MEMORY_BASIC_INFORMATION basicInfo;

        pageOfGarbage = NULL;
        pageSize = PAGE_SIZE;

        if (!NT_SUCCESS(NtAllocateVirtualMemory(
            NtCurrentProcess(),
            &pageOfGarbage,
            0,
            &pageSize,
            MEM_COMMIT,
            PAGE_READONLY
            )))
        {
            NtClose(processHandle);
            return STATUS_NO_MEMORY;
        }

        baseAddress = (PVOID)0;

        while (NT_SUCCESS(NtQueryVirtualMemory(
            processHandle,
            baseAddress,
            MemoryBasicInformation,
            &basicInfo,
            sizeof(MEMORY_BASIC_INFORMATION),
            NULL
            )))
        {
            ULONG i;

            // Make sure we don't write to views of mapped files. That
            // could possibly corrupt files!
            if (basicInfo.Type == MEM_PRIVATE)
            {
                for (i = 0; i < basicInfo.RegionSize; i += PAGE_SIZE)
                {
                    Ph2WriteVirtualMemory(
                        processHandle,
                        PTR_ADD_OFFSET(baseAddress, i),
                        pageOfGarbage,
                        PAGE_SIZE,
                        NULL
                        );
                }
            }

            baseAddress = PTR_ADD_OFFSET(baseAddress, basicInfo.RegionSize);
        }

        // Size needs to be zero if we're freeing.
        pageSize = 0;
        NtFreeVirtualMemory(
            NtCurrentProcess(),
            &pageOfGarbage,
            &pageSize,
            MEM_RELEASE
            );

        NtClose(processHandle);
    }

    return status;
}

NTSTATUS NTAPI TerminatorM2(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    HANDLE processHandle;

    if (NT_SUCCESS(status = Ph2OpenProcess(
        &processHandle,
        PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION,
        ProcessId
        )))
    {
        PVOID baseAddress;
        MEMORY_BASIC_INFORMATION basicInfo;
        ULONG oldProtect;

        baseAddress = (PVOID)0;

        while (NT_SUCCESS(NtQueryVirtualMemory(
            processHandle,
            baseAddress,
            MemoryBasicInformation,
            &basicInfo,
            sizeof(MEMORY_BASIC_INFORMATION),
            NULL
            )))
        {
            SIZE_T regionSize;

            regionSize = basicInfo.RegionSize;
            NtProtectVirtualMemory(
                processHandle,
                &basicInfo.BaseAddress,
                &regionSize,
                PAGE_NOACCESS,
                &oldProtect
                );
            baseAddress = PTR_ADD_OFFSET(baseAddress, basicInfo.RegionSize);
        }

        NtClose(processHandle);
    }

    return status;
}

NTSTATUS NTAPI TerminatorM3(
    _In_ HANDLE ProcessId
    )
{
    NTSTATUS status;
    HANDLE processHandle;

    if (NT_SUCCESS(status = Ph2OpenProcess(
        &processHandle,
        PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION,
        ProcessId
        )))
    {
        PVOID baseAddress;
        MEMORY_BASIC_INFORMATION basicInfo;

        baseAddress = (PVOID)0;

        while (NT_SUCCESS(NtQueryVirtualMemory(
            processHandle,
            baseAddress,
            MemoryBasicInformation,
            &basicInfo,
            sizeof(MEMORY_BASIC_INFORMATION),
            NULL
            )))
        {
            if (!(basicInfo.Type & (MEM_MAPPED | MEM_IMAGE)))
            {
                SIZE_T regionSize;

                regionSize = 0;

                status = NtFreeVirtualMemory(
                    processHandle,
                    &baseAddress,
                    &regionSize,
                    MEM_RELEASE
                    );
            }
            else
            {
                status = NtUnmapViewOfSection(processHandle, baseAddress);
            }

            baseAddress = PTR_ADD_OFFSET(baseAddress, basicInfo.RegionSize);
        }

        NtClose(processHandle);
    }

    return status;
}

TEST_ITEM PhTerminatorTests[] =
{
    { L"TP1", L"Terminates the process using NtTerminateProcess", TerminatorTP1 },
    { L"TP2", L"Creates a remote thread in the process which terminates the process", TerminatorTP2 },
    { L"TT1", L"Terminates the process' threads", TerminatorTT1 },
    { L"TT2", L"Modifies the process' threads with contexts which terminate the process", TerminatorTT2 },
    { L"TP1a", L"Terminates the process using NtTerminateProcess (alternative method)", TerminatorTP1a },
    { L"TT1a", L"Terminates the process' threads (alternative method)", TerminatorTT1a },
    { L"CH1", L"Closes the process' handles", TerminatorCH1 },
    { L"W1", L"Sends the WM_DESTROY message to the process' windows", TerminatorW1 },
    { L"W2", L"Sends the WM_QUIT message to the process' windows", TerminatorW2 },
    { L"W3", L"Sends the WM_CLOSE message to the process' windows", TerminatorW3 },
    { L"TJ1", L"Assigns the process to a job object and terminates the job", TerminatorTJ1 },
    { L"TD1", L"Debugs the process and closes the debug object", TerminatorTD1 },
    { L"TP3", L"Terminates the process in kernel-mode", TerminatorTP3 },
    { L"TT3", L"Terminates the process' threads in kernel-mode", TerminatorTT3 },
    { L"M1", L"Writes garbage to the process' memory regions", TerminatorM1 },
    { L"M2", L"Sets the page protection of the process' memory regions to PAGE_NOACCESS", TerminatorM2 },
    { L"M3", L"Frees the process' memory regions", TerminatorM3 }
};

BOOLEAN PhpRunTerminatorTest(
    _In_ HWND WindowHandle,
    _In_ PPH_PROCESS_ITEM ProcessItem,
    _In_ INT Index
    )
{
    NTSTATUS status;
    PTEST_ITEM testItem;
    HWND lvHandle;
    PVOID processes;
    BOOLEAN success = FALSE;

    lvHandle = GetDlgItem(WindowHandle, IDC_TERMINATOR_LIST);

    if (!PhGetListViewItemParam(
        lvHandle,
        Index,
        &testItem
        ))
        return FALSE;

    status = testItem->TestProc(ProcessItem->ProcessId);

    PhDelayExecution(1000);

    if (status == STATUS_NOT_SUPPORTED)
    {
        PPH_STRING concat;

        concat = PhConcatStrings2(L"(Not available) ", testItem->Description);
        PhSetListViewSubItem(lvHandle, Index, 1, concat->Buffer);
        PhDereferenceObject(concat);
    }

    if (!NT_SUCCESS(PhEnumProcesses(&processes)))
        return FALSE;

    // Check if the process exists.
    if (!PhFindProcessInformation(processes, ProcessItem->ProcessId))
    {
        PhSetListViewItemImageIndex(lvHandle, Index, 1);
        PhSetDialogItemText(WindowHandle, IDC_TERMINATOR_TEXT, L"The process was terminated.");
        success = TRUE;
    }
    else
    {
        PhSetListViewItemImageIndex(lvHandle, Index, 0);
    }

    PhFree(processes);

    UpdateWindow(WindowHandle);

    return success;
}
