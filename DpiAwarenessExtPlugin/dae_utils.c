/*
 * Process Hacker Extra Plugins -
 *  DPI Awareness Extras Plugin
 *
 * Copyright (C) 2018 poizan42
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

#include <phdk.h>
#include "dae_utils.h"

typedef struct _GET_DLL_BASE_REMOTE_CONTEXT
{
    PH_STRINGREF BaseDllName;
    PVOID DllBase;
} GET_DLL_BASE_REMOTE_CONTEXT, *PGET_DLL_BASE_REMOTE_CONTEXT;

static BOOLEAN DaepGetDllBaseRemoteCallback(
    _In_ PLDR_DATA_TABLE_ENTRY Module,
    _In_opt_ PVOID Context
)
{
    PGET_DLL_BASE_REMOTE_CONTEXT context = Context;
    PH_STRINGREF baseDllName;

    PhUnicodeStringToStringRef(&Module->BaseDllName, &baseDllName);

    if (PhEqualStringRef(&baseDllName, &context->BaseDllName, TRUE))
    {
        context->DllBase = Module->DllBase;
        return FALSE;
    }

    return TRUE;
}

NTSTATUS DaeGetDllBaseRemote(
    _In_ HANDLE ProcessHandle,
    _In_ PPH_STRINGREF BaseDllName,
    _Out_ PVOID *DllBase
)
{
    NTSTATUS status;
    GET_DLL_BASE_REMOTE_CONTEXT context;
#ifdef _WIN64
    BOOLEAN isWow64 = FALSE;
#endif

    context.BaseDllName = *BaseDllName;
    context.DllBase = NULL;

#ifdef _WIN64
    PhGetProcessIsWow64(ProcessHandle, &isWow64);

    if (isWow64)
        status = PhEnumProcessModules32(ProcessHandle, DaepGetDllBaseRemoteCallback, &context);
    if (!context.DllBase)
#endif
        status = PhEnumProcessModules(ProcessHandle, DaepGetDllBaseRemoteCallback, &context);

    if (NT_SUCCESS(status))
        *DllBase = context.DllBase;

    return status;
}
