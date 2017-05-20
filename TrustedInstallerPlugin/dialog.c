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

NTSTATUS RunAsTrustedInstallerThread(
    _In_ PVOID Parameter
    )
{
    HANDLE threadHandle;
    THREAD_BASIC_INFORMATION basicInfo;

    if (threadHandle = PhCreateThread(0, RunAsCreateProcessThread, NULL))
    {
        NtWaitForSingleObject(threadHandle, FALSE, NULL);
        
        if (NT_SUCCESS(PhGetThreadBasicInformation(threadHandle, &basicInfo)))
        {
            if (basicInfo.ExitStatus != STATUS_SUCCESS)
            {
                PhShowStatus(
                    PhMainWindowHandle, 
                    L"Error creating process with TrustedInstaller privileges", 
                    basicInfo.ExitStatus, 
                    0
                    );
            }
        }

        NtClose(threadHandle);
    }

    return STATUS_SUCCESS;
}

VOID ShowRunAsDialog(
    _In_opt_ HWND Parent
    )
{
    HANDLE threadHandle;

    if (threadHandle = PhCreateThread(0, RunAsTrustedInstallerThread, NULL))
        NtClose(threadHandle);
}