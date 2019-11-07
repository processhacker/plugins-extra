/*
 * Process Hacker Extra Plugins -
 *   Firmware Plugin
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
#include "Efi\EfiTypes.h"
#include "Efi\EfiDevicePath.h"

NTSTATUS EnumerateFirmwareValues(
    _Out_ PVOID *Values
    )
{
    NTSTATUS status;
    PVOID buffer;
    ULONG bufferLength;

    bufferLength = PAGE_SIZE;
    buffer = PhAllocate(bufferLength);

    while (TRUE)
    {
        status = NtEnumerateSystemEnvironmentValuesEx(
            SystemEnvironmentValueInformation,
            buffer,
            &bufferLength
            );

        if (status == STATUS_BUFFER_TOO_SMALL || status == STATUS_INFO_LENGTH_MISMATCH)
        {
            PhFree(buffer);
            buffer = PhAllocate(bufferLength);
        }
        else
        {
            break;
        }
    }

    if (!NT_SUCCESS(status))
    {
        PhFree(buffer);
        return status;
    }

    *Values = buffer;

    return status;
}

NTSTATUS EfiEnumerateBootEntries(
    _Out_ PVOID *Entries
    )
{
    NTSTATUS status;
    PVOID buffer;
    ULONG bufferLength;

    bufferLength = PAGE_SIZE;
    buffer = PhAllocate(bufferLength);

    while (TRUE)
    {
        status = NtEnumerateBootEntries(
            buffer,
            &bufferLength
            );

        if (status == STATUS_BUFFER_TOO_SMALL || status == STATUS_INFO_LENGTH_MISMATCH)
        {
            PhFree(buffer);
            buffer = PhAllocate(bufferLength);
        }
        else
        {
            break;
        }
    }

    if (!NT_SUCCESS(status))
    {
        PhFree(buffer);
        return status;
    }

    *Entries = buffer;

    return status;
}

BOOLEAN EfiSupported(
    VOID
    )
{
    // The GetFirmwareEnvironmentVariable function will always fail on a legacy BIOS-based system, 
    //  or if Windows was installed using legacy BIOS on a system that supports both legacy BIOS and UEFI. 
    // To identify these conditions, call the function with a dummy firmware environment name such as an empty string ("") 
    //  for the lpName parameter and a dummy GUID such as "{00000000-0000-0000-0000-000000000000}" for the lpGuid parameter. 
    // On a legacy BIOS-based system, or on a system that supports both legacy BIOS and UEFI where Windows was installed using legacy BIOS, 
    //  the function will fail with ERROR_INVALID_FUNCTION. 
    // On a UEFI-based system, the function will fail with an error specific to the firmware, such as ERROR_NOACCESS, 
    //  to indicate that the dummy GUID namespace does not exist.

    UNICODE_STRING variableName = RTL_CONSTANT_STRING(L" ");
    PVOID variableValue = NULL; 
    ULONG variableValueLength = 0;

    //GetFirmwareEnvironmentVariable(
    //    L"", 
    //    L"{00000000-0000-0000-0000-000000000000}", 
    //    NULL, 
    //    0
    //    );
    //if (GetLastError() == ERROR_INVALID_FUNCTION)

    if (NtQuerySystemEnvironmentValueEx(
        &variableName, 
        (PGUID)&GUID_NULL, 
        variableValue,
        &variableValueLength,
        NULL
        ) == STATUS_VARIABLE_NOT_FOUND)
    {
        return TRUE;
    }

    return FALSE;
}
