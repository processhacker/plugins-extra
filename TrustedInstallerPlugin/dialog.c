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

NTSTATUS RunAsTrustedInstallerThread(
    _In_ PVOID Parameter
    )
{
    NTSTATUS status;
    HANDLE threadHandle;
    THREAD_BASIC_INFORMATION basicInfo;

    if (NT_SUCCESS(PhCreateThreadEx(&threadHandle, RunAsCreateProcessThread, NULL)))
    {
        NtWaitForSingleObject(threadHandle, FALSE, NULL);
        
        status = PhGetThreadBasicInformation(threadHandle, &basicInfo);

        if (NT_SUCCESS(status))
        {
            status = basicInfo.ExitStatus;
        }

        if (!NT_SUCCESS(status))
        {
            PhShowStatus(
                PhMainWndHandle,
                L"Error creating process with TrustedInstaller privileges",
                basicInfo.ExitStatus,
                0
                );
        }

        NtClose(threadHandle);
    }

    return STATUS_SUCCESS;
}

VOID ShowRunAsDialog(
    _In_opt_ HWND Parent
    )
{
    PhCreateThread2(RunAsTrustedInstallerThread, NULL);
}
