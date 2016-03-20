/*
 * Process Hacker Extra Plugins -
 *   Environment Edit Plugin
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

#include "main.h"

// This function is available starting with XP-SP2 (32bit and 64bit).
typedef NTSTATUS (NTAPI* _RtlQueueApcWow64Thread)(
    _In_ HANDLE ThreadHandle,
    _In_ PPS_APC_ROUTINE ApcRoutine,
    _In_opt_ PVOID ApcArgument1,
    _In_opt_ PVOID ApcArgument2,
    _In_opt_ PVOID ApcArgument3
    ); 

typedef NTSTATUS (NTAPI* _NtQueueApcThread)(
    _In_ HANDLE ThreadHandle,
    _In_ PPS_APC_ROUTINE ApcRoutine,
    _In_opt_ PVOID ApcArgument1,
    _In_opt_ PVOID ApcArgument2,
    _In_opt_ PVOID ApcArgument3
    );

// We can statically link against both functions if they were included in the PHNT headers.
static _RtlQueueApcWow64Thread RtlQueueApcWow64Thread_I = NULL;
static _NtQueueApcThread NtQueueApcThread_I = NULL;

NTSTATUS SetRemoteEnvironmentVariable(
    _In_ HANDLE ProcessHandle,
    _In_ PWSTR EnvironmentName,
    _In_opt_ PWSTR EnvironmentValue
    )
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    NTSTATUS status = STATUS_TIMEOUT;
    LARGE_INTEGER timeout;
    PH_STRINGREF environmentName;
    PH_STRINGREF environmentValue;
    PVOID environmentNameBaseAddress = NULL;
    PVOID environmentValueBaseAddress = NULL;
    SIZE_T environmentNameAllocationSize = 0;
    SIZE_T environmentValueAllocationSize = 0;
    PVOID exitThreadStart = NULL;
    PVOID setVariableApcStart = NULL;
    HANDLE remoteThreadHandle = NULL;
    PPH_STRING kernel32FileName = NULL;
    PPH_STRING ntdllFileName = NULL;
    BOOLEAN isWow64 = FALSE;

    if (PhBeginInitOnce(&initOnce))
    {
        PVOID ntdll;

        ntdll = PhGetDllHandle(L"ntdll.dll");
        RtlQueueApcWow64Thread_I = PhGetProcedureAddress(ntdll, "RtlQueueApcWow64Thread", 0);
        NtQueueApcThread_I = PhGetProcedureAddress(ntdll, "NtQueueApcThread", 0);

        PhEndInitOnce(&initOnce);
    }

    PhInitializeStringRefLongHint(&environmentName, EnvironmentName);
    environmentNameAllocationSize = environmentName.Length + sizeof(WCHAR);

    if (EnvironmentValue)
    {
        PhInitializeStringRefLongHint(&environmentValue, EnvironmentValue);
        environmentValueAllocationSize = environmentValue.Length + sizeof(WCHAR);
    }

    __try
    {
#ifdef _WIN64
        if (!NT_SUCCESS(status = PhGetProcessIsWow64(
            ProcessHandle, 
            &isWow64
            )))
        {
            __leave;
        }
#endif
        if (isWow64)
        {
            ntdllFileName = PhConcatStrings2(USER_SHARED_DATA->NtSystemRoot, L"\\SysWow64\\ntdll.dll");
            kernel32FileName = PhConcatStrings2(USER_SHARED_DATA->NtSystemRoot, L"\\SysWow64\\kernel32.dll");
        }
        else
        {
            ntdllFileName = PhConcatStrings2(USER_SHARED_DATA->NtSystemRoot, L"\\System32\\ntdll.dll");
            kernel32FileName = PhConcatStrings2(USER_SHARED_DATA->NtSystemRoot, L"\\System32\\kernel32.dll");
        }

        if (!NT_SUCCESS(status = PhGetProcedureAddressRemote(
            ProcessHandle,
            ntdllFileName->Buffer,
            "RtlExitUserThread",
            0,
            &exitThreadStart,
            NULL
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = PhGetProcedureAddressRemote(
            ProcessHandle,
            kernel32FileName->Buffer,
            "SetEnvironmentVariableW", // TODO: We can use RtlSetEnvironmentVariable from ntdll...
            0,
            &setVariableApcStart,
            NULL
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = NtAllocateVirtualMemory(
            ProcessHandle,
            &environmentNameBaseAddress,
            0,
            &environmentNameAllocationSize,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_READWRITE
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = NtWriteVirtualMemory(
            ProcessHandle,
            environmentNameBaseAddress,
            environmentName.Buffer,
            environmentName.Length + sizeof(WCHAR),
            NULL
            )))
        {
            __leave;
        }

        if (EnvironmentValue)
        {
            if (!NT_SUCCESS(status = NtAllocateVirtualMemory(
                ProcessHandle,
                &environmentValueBaseAddress,
                0,
                &environmentValueAllocationSize,
                MEM_RESERVE | MEM_COMMIT,
                PAGE_READWRITE
                )))
            {
                __leave;
            }

            if (!NT_SUCCESS(status = NtWriteVirtualMemory(
                ProcessHandle,
                environmentValueBaseAddress,
                environmentValue.Buffer,
                environmentValue.Length + sizeof(WCHAR),
                NULL
                )))
            {
                __leave;
            }
        }

        if (WindowsVersion >= WINDOWS_VISTA)
        {
            if (!NT_SUCCESS(status = RtlCreateUserThread(
                ProcessHandle,
                NULL,
                TRUE,
                0,
                0,
                0,
                (PUSER_THREAD_START_ROUTINE)exitThreadStart,
                NULL,
                &remoteThreadHandle,
                NULL
                )))
            {
                __leave;
            }
        }
        else
        {
            if (!(remoteThreadHandle = CreateRemoteThread(
                ProcessHandle,
                NULL,
                0,
                (PTHREAD_START_ROUTINE)exitThreadStart,
                NULL,
                CREATE_SUSPENDED,
                NULL
                )))
            {
                status = PhGetLastWin32ErrorAsNtStatus();
                __leave;
            }
        }

        // If an application queues an APC(s) before the thread begins running, 
        // the thread begins by first calling the pending APC(s) before calling the start routine.
        //
        // In this implementation it'll first execute the pending APC call to SetEnvironmentVariableW,
        // then finishes by calling the start routine which is RtlExitUserThread :P

        if (isWow64)
        {
            // NtQueueApcThread doesn't work on Wow64 processes... Use the runtime library instead.
            if (!NT_SUCCESS(status = RtlQueueApcWow64Thread_I(
                remoteThreadHandle,
                setVariableApcStart,
                environmentNameBaseAddress,
                environmentValueBaseAddress,
                NULL
                )))
            {
                __leave;
            }
        }
        else
        {
            if (!NT_SUCCESS(status = NtQueueApcThread_I(
                remoteThreadHandle,
                setVariableApcStart,
                environmentNameBaseAddress,
                environmentValueBaseAddress,
                NULL
                )))
            {
                __leave;
            }
        }

        // Execute the pending APCs
        if (!NT_SUCCESS(status = NtResumeThread(remoteThreadHandle, NULL)))
        {
            __leave;
        }

        // Wait for the thread to exit or timeout (10s).
        if (NtWaitForSingleObject(remoteThreadHandle, FALSE, PhTimeoutFromMilliseconds(&timeout, 10000)) == STATUS_TIMEOUT)
        {
            // Timed out... Terminate the thread.
            NtTerminateThread(remoteThreadHandle, STATUS_TIMEOUT);
        }

        // TODO: Flush Peb()->ProcessParameters->Environment pointer address and/or every thread base address?
        NtFlushInstructionCache(ProcessHandle, NULL, 0);
    }
    __finally
    {
        if (remoteThreadHandle)
        {
            NtClose(remoteThreadHandle);
        }

        if (environmentNameBaseAddress)
        {
            environmentNameAllocationSize = 0;
            NtFreeVirtualMemory(
                ProcessHandle,
                &environmentNameBaseAddress,
                &environmentNameAllocationSize,
                MEM_RELEASE
                );
        }

        if (environmentValueBaseAddress)
        {
            environmentValueAllocationSize = 0;
            NtFreeVirtualMemory(
                ProcessHandle,
                &environmentValueBaseAddress,
                &environmentValueAllocationSize,
                MEM_RELEASE
                );
        }

        if (ntdllFileName)
        {
            PhDereferenceObject(ntdllFileName);
        }

        if (kernel32FileName)
        {
            PhDereferenceObject(kernel32FileName);
        }
    }

    return status;
}