/*
 * Process Hacker Extra Plugins -
 *   Trusted Installer Plugin
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

NTSTATUS RunAsCreateProcessThread(
    _In_ PVOID Parameter
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    BOOLEAN success = FALSE;
    ULONG bytesNeeded = 0;
    SERVICE_STATUS_PROCESS serviceStatus = { 0 };
    SC_HANDLE serviceHandle = NULL;
    HANDLE processHandle = NULL;
    HANDLE tokenHandle = NULL;
    PTOKEN_USER tokenUser = NULL;
    PPH_STRING userName = NULL;
    PPH_STRING program = Parameter;

    __try
    {
        if (!(serviceHandle = PhOpenService(L"TrustedInstaller", SERVICE_QUERY_STATUS | SERVICE_START)))
        {
            status = PhGetLastWin32ErrorAsNtStatus();
            __leave;
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
            __leave;
        }

        if (serviceStatus.dwCurrentState == SERVICE_RUNNING)
        {
            success = TRUE;
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
                        success = TRUE;
                        break;
                    }
                }

                Sleep(1000);

            } while (--attempts != 0);
        }

        if (!success)
        {
            // One or more services failed to start.
            status = STATUS_SERVICES_FAILED_AUTOSTART;
            __leave;
        }

        if (!NT_SUCCESS(status = PhOpenProcess(
            &processHandle,
            ProcessQueryAccess,
            UlongToHandle(serviceStatus.dwProcessId)
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = NtOpenProcessToken(
            processHandle, 
            TOKEN_QUERY, 
            &tokenHandle
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = PhGetTokenUser(tokenHandle, &tokenUser)))
        {
            __leave;
        }

        if (!(userName = PhGetSidFullName(tokenUser->User.Sid, TRUE, NULL)))
        {
            // the SID structure is not valid.
            status = STATUS_INVALID_SID;
            __leave;
        }

        status = PhExecuteRunAsCommand2(
            PhMainWndHandle,
            program->Buffer,
            userName->Buffer,
            L"",
            LOGON32_LOGON_SERVICE,
            UlongToHandle(serviceStatus.dwProcessId),
            NtCurrentPeb()->SessionId,
            NULL,
            FALSE
            );
    }
    __finally
    {
        if (program)
        {
            PhDereferenceObject(program);
        }

        if (userName)
        {
            PhDereferenceObject(userName);
        }

        if (tokenUser)
        {
            PhFree(tokenUser);
        }

        if (tokenHandle)
        {
            NtClose(tokenHandle);
        }

        if (processHandle)
        {
            NtClose(processHandle);
        }

        if (serviceHandle)
        {
            CloseServiceHandle(serviceHandle);
        }
    }

    return status;
}
