/*
 * Process Hacker Extra Plugins -
 *   Trusted Installer Plugin
 *
 * Copyright (C) 2016-2019 dmex
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

BOOL (WINAPI* InitializeProcThreadAttributeList_I)(
    _Out_writes_bytes_to_opt_(*Size, *Size) LPPROC_THREAD_ATTRIBUTE_LIST AttributeList,
    _In_ ULONG AttributeCount,
    _Reserved_ ULONG Flags,
    _When_(AttributeList == NULL, _Out_) _When_(AttributeList != NULL, _Inout_) PSIZE_T Size
    ) = NULL;

BOOL (WINAPI* UpdateProcThreadAttribute_I)(
    _Inout_ LPPROC_THREAD_ATTRIBUTE_LIST AttributeList,
    _In_ ULONG Flags,
    _In_ ULONG_PTR Attribute,
    _In_reads_bytes_opt_(Size) PVOID Value,
    _In_ SIZE_T Size,
    _Out_writes_bytes_opt_(Size) PVOID PreviousValue,
    _In_opt_ PSIZE_T ReturnSize
    ) = NULL;

VOID (WINAPI* DeleteProcThreadAttributeList_I)(
    _Inout_ LPPROC_THREAD_ATTRIBUTE_LIST AttributeList
    ) = NULL;

NTSTATUS RunAsCreateProcessThread(
    _In_ PVOID Parameter
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    SERVICE_STATUS_PROCESS serviceStatus = { 0 };
    SC_HANDLE serviceHandle = NULL;
    HANDLE processHandle = NULL;
    HANDLE newProcessHandle = NULL;
    STARTUPINFOEX startupInfo;
    SIZE_T attributeListLength;
    PPH_STRING systemDirectory;
    PPH_STRING commandLine;
    ULONG bytesNeeded = 0;

    InitializeProcThreadAttributeList_I = PhGetModuleProcAddress(L"kernelbase.dll", "InitializeProcThreadAttributeList");
    UpdateProcThreadAttribute_I = PhGetModuleProcAddress(L"kernelbase.dll", "UpdateProcThreadAttribute");
    DeleteProcThreadAttributeList_I = PhGetModuleProcAddress(L"kernelbase.dll", "DeleteProcThreadAttributeList");

    if (!(InitializeProcThreadAttributeList_I && UpdateProcThreadAttribute_I && DeleteProcThreadAttributeList_I))
        return STATUS_UNSUCCESSFUL;

    systemDirectory = PhGetSystemDirectory();
    commandLine = PhConcatStringRefZ(&systemDirectory->sr, L"\\cmd.exe");

    memset(&startupInfo, 0, sizeof(STARTUPINFOEX));
    startupInfo.StartupInfo.cb = sizeof(STARTUPINFOEX);
    startupInfo.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.StartupInfo.wShowWindow = SW_SHOWNORMAL;

    if (!(serviceHandle = PhOpenService(L"TrustedInstaller", SERVICE_QUERY_STATUS | SERVICE_START)))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        goto CleanupExit;
    }

    if (!QueryServiceStatusEx(
        serviceHandle,
        SC_STATUS_PROCESS_INFO,
        (PBYTE)&serviceStatus,
        sizeof(SERVICE_STATUS_PROCESS),
        &bytesNeeded
        ))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        goto CleanupExit;
    }

    if (serviceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        status = STATUS_SUCCESS;
    }
    else
    {
        ULONG attempts = 10;

        StartService(serviceHandle, 0, NULL);

        do
        {
            if (QueryServiceStatusEx(
                serviceHandle,
                SC_STATUS_PROCESS_INFO,
                (PBYTE)&serviceStatus,
                sizeof(SERVICE_STATUS_PROCESS),
                &bytesNeeded
                ))
            {
                if (serviceStatus.dwCurrentState == SERVICE_RUNNING)
                {
                    status = STATUS_SUCCESS;
                    break;
                }
            }

            PhDelayExecution(1000);

        } while (--attempts != 0);
    }

    if (!NT_SUCCESS(status))
    {
        status = STATUS_SERVICES_FAILED_AUTOSTART; // One or more services failed to start.
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_CREATE_PROCESS, UlongToHandle(serviceStatus.dwProcessId))))
        goto CleanupExit;


    if (!InitializeProcThreadAttributeList_I(NULL, 1, 0, &attributeListLength) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        goto CleanupExit;
    }

    startupInfo.lpAttributeList = PhAllocate(attributeListLength);

    if (!InitializeProcThreadAttributeList_I(startupInfo.lpAttributeList, 1, 0, &attributeListLength))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        goto CleanupExit;
    }

    if (!UpdateProcThreadAttribute_I(startupInfo.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &processHandle, sizeof(HANDLE), NULL, NULL))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        goto CleanupExit;
    }

    status = PhCreateProcessWin32Ex(
        NULL,
        PhGetString(commandLine),
        NULL,
        NULL,
        &startupInfo.StartupInfo,
        PH_CREATE_PROCESS_SUSPENDED | PH_CREATE_PROCESS_NEW_CONSOLE | PH_CREATE_PROCESS_EXTENDED_STARTUPINFO,
        NULL,
        NULL,
        &newProcessHandle,
        NULL
        );

    if (NT_SUCCESS(status))
    {
        PROCESS_BASIC_INFORMATION basicInfo;

        if (NT_SUCCESS(PhGetProcessBasicInformation(newProcessHandle, &basicInfo)))
        {
            AllowSetForegroundWindow(HandleToUlong(basicInfo.UniqueProcessId)); // ASFW_ANY
        }

        NtResumeProcess(newProcessHandle);

        NtClose(newProcessHandle);
    }

CleanupExit:

    if (processHandle)
        NtClose(processHandle);
    if (serviceHandle)
        CloseServiceHandle(serviceHandle);

    if (startupInfo.lpAttributeList)
    {
        DeleteProcThreadAttributeList_I(startupInfo.lpAttributeList);
        PhFree(startupInfo.lpAttributeList);
    }

    if (commandLine)
        PhDereferenceObject(commandLine);
    if (systemDirectory)
        PhDereferenceObject(systemDirectory);

    return status;
}

NTSTATUS RunAsCreateProcessThreadLegacy(
    _In_ PVOID Parameter
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    SERVICE_STATUS_PROCESS serviceStatus = { 0 };
    SC_HANDLE serviceHandle = NULL;
    HANDLE processHandle = NULL;
    HANDLE tokenHandle = NULL;
    PTOKEN_USER tokenUser = NULL;
    PPH_STRING userName = NULL;
    PPH_STRING systemDirectory;
    PPH_STRING commandLine;
    ULONG bytesNeeded = 0;

    systemDirectory = PhGetSystemDirectory();
    commandLine = PhConcatStringRefZ(&systemDirectory->sr, L"\\cmd.exe");

    if (!(serviceHandle = PhOpenService(L"TrustedInstaller", SERVICE_QUERY_STATUS | SERVICE_START)))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        goto CleanupExit;
    }

    if (!QueryServiceStatusEx(
        serviceHandle,
        SC_STATUS_PROCESS_INFO,
        (PBYTE)&serviceStatus,
        sizeof(SERVICE_STATUS_PROCESS),
        &bytesNeeded
        ))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        goto CleanupExit;
    }

    if (serviceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        status = STATUS_SUCCESS;
    }
    else
    {
        ULONG attempts = 5;

        StartService(serviceHandle, 0, NULL);

        do
        {
            if (QueryServiceStatusEx(
                serviceHandle,
                SC_STATUS_PROCESS_INFO,
                (PBYTE)&serviceStatus,
                sizeof(SERVICE_STATUS_PROCESS),
                &bytesNeeded
                ))
            {
                if (serviceStatus.dwCurrentState == SERVICE_RUNNING)
                {
                    status = STATUS_SUCCESS;
                    break;
                }
            }

            Sleep(1000);

        } while (--attempts != 0);
    }

    if (!NT_SUCCESS(status))
    {
        status = STATUS_SERVICES_FAILED_AUTOSTART; // One or more services failed to start.
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, UlongToHandle(serviceStatus.dwProcessId))))
        goto CleanupExit;

    if (!NT_SUCCESS(status = NtOpenProcessToken(processHandle, TOKEN_QUERY, &tokenHandle)))
        goto CleanupExit;

    if (!NT_SUCCESS(status = PhGetTokenUser(tokenHandle, &tokenUser)))
        goto CleanupExit;

    if (!(userName = PhGetSidFullName(tokenUser->User.Sid, TRUE, NULL)))
    {
        status = STATUS_INVALID_SID; // the SID structure is not valid.
        goto CleanupExit;
    }

    status = PhExecuteRunAsCommand2(
        PhMainWndHandle,
        PhGetStringOrEmpty(commandLine),
        PhGetStringOrEmpty(userName),
        L"",
        LOGON32_LOGON_SERVICE,
        UlongToHandle(serviceStatus.dwProcessId),
        NtCurrentPeb()->SessionId,
        NULL,
        FALSE
        );

CleanupExit:
    if (tokenHandle)
        NtClose(tokenHandle);
    if (processHandle)
        NtClose(processHandle);
    if (serviceHandle)
        CloseServiceHandle(serviceHandle);
    if (systemDirectory)
        PhDereferenceObject(systemDirectory);
    if (commandLine)
        PhDereferenceObject(commandLine);
    if (userName)
        PhDereferenceObject(userName);

    if (tokenUser)
        PhFree(tokenUser);

    return status;
}

