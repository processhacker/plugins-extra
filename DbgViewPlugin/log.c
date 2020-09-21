/*
 * Process Hacker Extra Plugins -
 *   Debug View Plugin
 *
 * Copyright (C) 2015 dmex
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

PH_CALLBACK_DECLARE(DbgLoggedCallback);

VOID DbgFreeLogEntry(
    _In_ PDEBUG_LOG_ENTRY Entry
    )
{
    PhClearReference(&Entry->FilePath);
    PhClearReference(&Entry->ProcessName);
    PhClearReference(&Entry->Message);

    PhFree(Entry);
}

VOID DbgAddLogEntry(
    _Inout_ PPH_DBGEVENTS_CONTEXT Context,
    _In_ PDEBUG_LOG_ENTRY Entry
    )
{
    if (Context->LogMessageList->Count > PhGetIntegerSetting(SETTING_NAME_MAX_ENTRIES))
    {
        DbgFreeLogEntry(Context->LogMessageList->Items[0]);
        PhRemoveItemList(Context->LogMessageList, 0);
    }

    PhAddItemList(Context->LogMessageList, Entry);
    PhInvokeCallback(&DbgLoggedCallback, Entry);
}

VOID DbgClearLogEntries(
    _Inout_ PPH_DBGEVENTS_CONTEXT Context
    )
{
    for (ULONG i = 0; i < Context->LogMessageList->Count; i++)
    {
        if (Context->LogMessageList->Items[i])
        {
            DbgFreeLogEntry(Context->LogMessageList->Items[i]);
        }
    }

    PhClearList(Context->LogMessageList);
}

VOID DbgShowErrorMessage(
    _Inout_ PPH_DBGEVENTS_CONTEXT Context,
    _In_ PWSTR Type
    )
{
    ULONG errorCode = GetLastError();
    PPH_STRING errorMessage = PhGetWin32Message(errorCode);

    if (errorMessage)
    {
        PhShowError(Context->DialogHandle, L"%s: [%u] %s", Type, errorCode, errorMessage->Buffer);
        PhDereferenceObject(errorMessage);
    }
}

VOID DbgFormatObjectName(
    _In_ BOOLEAN LocalName,
    _In_ PWSTR OriginalName,
    _Out_ PUNICODE_STRING ObjectName
    )
{
    SIZE_T length;
    SIZE_T originalNameLength;

    // Sessions other than session 0 require SeCreateGlobalPrivilege.
    if (LocalName && NtCurrentPeb()->SessionId != 0)
    {
        WCHAR buffer[256] = L"";

        memcpy(buffer, L"\\Sessions\\", 10 * sizeof(WCHAR));
        _ultow(NtCurrentPeb()->SessionId, buffer + 10, 10);
        length = PhCountStringZ(buffer);
        originalNameLength = PhCountStringZ(OriginalName);
        memcpy(buffer + length, OriginalName, (originalNameLength + 1) * sizeof(WCHAR));
        length += originalNameLength;

        ObjectName->Buffer = buffer;
        ObjectName->MaximumLength = (ObjectName->Length = (USHORT)(length * sizeof(WCHAR))) + sizeof(WCHAR);
    }
    else
    {
        RtlInitUnicodeString(ObjectName, OriginalName);
    }
}

VOID DbgProcessLogMessageEntry(
    _Inout_ PPH_DBGEVENTS_CONTEXT Context,
    _In_ BOOLEAN GlobalEvents
    )
{
    PDBWIN_PAGE_BUFFER debugMessageBuffer;
    PDEBUG_LOG_ENTRY entry = NULL;
    HANDLE processHandle = NULL;
    PPH_STRING fileName = NULL;
    HICON icon = NULL;

    debugMessageBuffer = GlobalEvents ? Context->GlobalDebugBuffer : Context->LocalDebugBuffer;

    entry = PhAllocate(sizeof(DEBUG_LOG_ENTRY));
    memset(entry, 0, sizeof(DEBUG_LOG_ENTRY));

    PhQuerySystemTime(&entry->Time);
    entry->ProcessId = UlongToHandle(debugMessageBuffer->ProcessId);
    entry->Message = PhConvertMultiByteToUtf16(debugMessageBuffer->Buffer);

    PhGetProcessImageFileNameByProcessId(entry->ProcessId, &fileName);

    if (PhIsNullOrEmptyString(fileName))
        PhMoveReference(&fileName, PhGetKernelFileName());

    PhMoveReference(&fileName, PhGetFileName(fileName));

    icon = PhGetFileShellIcon(PhGetString(fileName), L".exe", TRUE);

    if (icon)
    {
        entry->ImageIndex = ImageList_AddIcon(Context->ListViewImageList, icon);
        DestroyIcon(icon);
    }

    entry->FilePath = fileName;
    entry->ProcessName = PhGetBaseName(fileName);

    // Drop event if it matches a filter
    for (ULONG i = 0; i < Context->ExcludeList->Count; i++)
    {
        PDBG_FILTER_TYPE filterEntry = Context->ExcludeList->Items[i];

        if (filterEntry->Type == FilterByName)
        {
            if (PhEqualString(filterEntry->ProcessName, entry->ProcessName, TRUE))
            {
                DbgFreeLogEntry(entry);
                return;
            }
        }
        else if (filterEntry->Type == FilterByPid)
        {
            if (filterEntry->ProcessId == entry->ProcessId)
            {
                DbgFreeLogEntry(entry);
                return;
            }
        }
    }

    DbgAddLogEntry(Context, entry);
}

NTSTATUS DbgEventsLocalThread(
    _In_ PVOID Parameter
    )
{
    LARGE_INTEGER timeout;
    NTSTATUS status;
    PPH_DBGEVENTS_CONTEXT context = (PPH_DBGEVENTS_CONTEXT)Parameter;

    while (TRUE)
    {
        NtSetEvent(context->LocalBufferReadyEvent, NULL);

        status = NtWaitForSingleObject(
            context->LocalDataReadyEvent,
            FALSE,
            PhTimeoutFromMilliseconds(&timeout, 100)
            );

        if (status == STATUS_TIMEOUT)
            continue;
        if (status != STATUS_SUCCESS)
            break;

        // The process calling OutputDebugString is blocked here...
        // This gives us some time to extract information without the process exiting.
        DbgProcessLogMessageEntry(context, FALSE);
    }

    return STATUS_SUCCESS;
}

NTSTATUS DbgEventsGlobalThread(
    _In_ PVOID Parameter
    )
{
    LARGE_INTEGER timeout;
    NTSTATUS status;
    PPH_DBGEVENTS_CONTEXT context = (PPH_DBGEVENTS_CONTEXT)Parameter;

    while (TRUE)
    {
        NtSetEvent(context->GlobalBufferReadyEvent, NULL);

        status = NtWaitForSingleObject(
            context->GlobalDataReadyEvent,
            FALSE,
            PhTimeoutFromMilliseconds(&timeout, 100)
            );

        if (status == STATUS_TIMEOUT)
            continue;
        if (status != STATUS_SUCCESS)
            break;

        // The process calling OutputDebugString is blocked here...
        // This gives us some time to extract information without the process exiting.
        DbgProcessLogMessageEntry(context, TRUE);
    }

    return STATUS_SUCCESS;
}

BOOLEAN DbgCreateSecurityAttributes(
    _Inout_ PPH_DBGEVENTS_CONTEXT Context
    )
{
    Context->SecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    Context->SecurityAttributes.bInheritHandle = FALSE;

    if (ConvertStringSecurityDescriptorToSecurityDescriptor(
        L"D:(A;;GRGWGX;;;WD)(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGWGX;;;AN)(A;;GRGWGX;;;RC)(A;;GRGWGX;;;S-1-15-2-1)S:(ML;;NW;;;LW)",
        SDDL_REVISION,
        &Context->SecurityAttributes.lpSecurityDescriptor,
        NULL
        ))
    {
        return TRUE;
    }

    return FALSE;
}

BOOLEAN DbgCleanupSecurityAttributes(
    _Inout_ PPH_DBGEVENTS_CONTEXT Context
    )
{
    if (Context->SecurityAttributes.lpSecurityDescriptor)
    {
        LocalFree(Context->SecurityAttributes.lpSecurityDescriptor);
        Context->SecurityAttributes.lpSecurityDescriptor = NULL;
        return TRUE;
    }

    return FALSE;
}

BOOLEAN DbgEventsCreate(
    _Inout_ PPH_DBGEVENTS_CONTEXT Context,
    _In_ BOOLEAN GlobalEvents
    )
{
    if (GlobalEvents)
    {
        SIZE_T viewSize;
        LARGE_INTEGER maximumSize;
        OBJECT_ATTRIBUTES objectAttributes;
        UNICODE_STRING objectName;

        maximumSize.QuadPart = PAGE_SIZE;
        viewSize = sizeof(DBWIN_PAGE_BUFFER);

        if (!(Context->GlobalBufferReadyEvent = CreateEvent(&Context->SecurityAttributes, FALSE, FALSE, L"Global\\" DBWIN_BUFFER_READY)))
        {
            DbgShowErrorMessage(Context, L"DBWIN_BUFFER_READY");
            return FALSE;
        }

        if (!(Context->GlobalDataReadyEvent = CreateEvent(&Context->SecurityAttributes, FALSE, FALSE, L"Global\\" DBWIN_DATA_READY)))
        {
            DbgShowErrorMessage(Context, L"DBWIN_DATA_READY");
            return FALSE;
        }

        DbgFormatObjectName(FALSE, DBWIN_BUFFER_SECTION_NAME, &objectName);
        InitializeObjectAttributes(
            &objectAttributes,
            &objectName,
            OBJ_CASE_INSENSITIVE,
            NULL,
            Context->SecurityAttributes.lpSecurityDescriptor
            );

        if (!NT_SUCCESS(NtCreateSection(
            &Context->GlobalDataBufferHandle,
            STANDARD_RIGHTS_REQUIRED | SECTION_QUERY | SECTION_MAP_READ | SECTION_MAP_WRITE,
            &objectAttributes,
            &maximumSize,
            PAGE_READWRITE,
            SEC_COMMIT,
            NULL
            )))
        {
            DbgShowErrorMessage(Context, L"NtCreateSection");
            return FALSE;
        }

        if (!NT_SUCCESS(NtMapViewOfSection(
            Context->GlobalDataBufferHandle,
            NtCurrentProcess(),
            &Context->GlobalDebugBuffer,
            0,
            0,
            NULL,
            &viewSize,
            ViewShare,
            0,
            PAGE_READONLY
            )))
        {
            DbgShowErrorMessage(Context, L"NtMapViewOfSection");
            return FALSE;
        }

        Context->CaptureGlobalEnabled = TRUE;

        PhCreateThread2(DbgEventsGlobalThread, Context);
    }
    else
    {
        SIZE_T viewSize;
        LARGE_INTEGER maximumSize;
        OBJECT_ATTRIBUTES objectAttributes;
        UNICODE_STRING objectName;
        HANDLE threadHandle = NULL;

        maximumSize.QuadPart = PAGE_SIZE;
        viewSize = sizeof(DBWIN_PAGE_BUFFER);

        if (!(Context->LocalBufferReadyEvent = CreateEvent(&Context->SecurityAttributes, FALSE, FALSE, L"Local\\" DBWIN_BUFFER_READY)))
        {
            DbgShowErrorMessage(Context, L"DBWIN_BUFFER_READY");
            return FALSE;
        }

        if (!(Context->LocalDataReadyEvent = CreateEvent(&Context->SecurityAttributes, FALSE, FALSE, L"Local\\" DBWIN_DATA_READY)))
        {
            DbgShowErrorMessage(Context, L"DBWIN_DATA_READY");
            return FALSE;
        }

        DbgFormatObjectName(TRUE, DBWIN_BUFFER_SECTION_NAME, &objectName);
        InitializeObjectAttributes(
            &objectAttributes,
            &objectName,
            OBJ_CASE_INSENSITIVE,
            NULL,
            Context->SecurityAttributes.lpSecurityDescriptor
            );

        if (!NT_SUCCESS(NtCreateSection(
            &Context->LocalDataBufferHandle,
            STANDARD_RIGHTS_REQUIRED | SECTION_QUERY | SECTION_MAP_READ | SECTION_MAP_WRITE,
            &objectAttributes,
            &maximumSize,
            PAGE_READWRITE,
            SEC_COMMIT,
            NULL
            )))
        {
            DbgShowErrorMessage(Context, L"NtCreateSection");
            return FALSE;
        }

        if (!NT_SUCCESS(NtMapViewOfSection(
            Context->LocalDataBufferHandle,
            NtCurrentProcess(),
            &Context->LocalDebugBuffer,
            0,
            0,
            NULL,
            &viewSize,
            ViewShare,
            0,
            PAGE_READONLY
            )))
        {
            DbgShowErrorMessage(Context, L"NtMapViewOfSection");
            return FALSE;
        }

        Context->CaptureLocalEnabled = TRUE;

        PhCreateThread2(DbgEventsLocalThread, Context);
    }

    return TRUE;
}

VOID DbgEventsCleanup(
    _Inout_ PPH_DBGEVENTS_CONTEXT Context,
    _In_ BOOLEAN CleanupGlobal
    )
{
    if (CleanupGlobal)
    {
        Context->CaptureGlobalEnabled = FALSE;

        if (Context->GlobalDebugBuffer)
        {
            NtUnmapViewOfSection(NtCurrentProcess(), Context->GlobalDebugBuffer);
            Context->GlobalDebugBuffer = NULL;
        }

        if (Context->GlobalDataBufferHandle)
        {
            NtClose(Context->GlobalDataBufferHandle);
            Context->GlobalDataBufferHandle = NULL;
        }

        if (Context->GlobalBufferReadyEvent)
        {
            NtClose(Context->GlobalBufferReadyEvent);
            Context->GlobalBufferReadyEvent = NULL;
        }

        if (Context->GlobalDataReadyEvent)
        {
            NtClose(Context->GlobalDataReadyEvent);
            Context->GlobalDataReadyEvent = NULL;
        }
    }
    else
    {
        Context->CaptureLocalEnabled = FALSE;

        if (Context->LocalDebugBuffer)
        {
            NtUnmapViewOfSection(NtCurrentProcess(), Context->LocalDebugBuffer);
            Context->LocalDebugBuffer = NULL;
        }

        if (Context->LocalDataBufferHandle)
        {
            NtClose(Context->LocalDataBufferHandle);
            Context->LocalDataBufferHandle = NULL;
        }

        if (Context->LocalBufferReadyEvent)
        {
            NtClose(Context->LocalBufferReadyEvent);
            Context->LocalBufferReadyEvent = NULL;
        }

        if (Context->LocalDataReadyEvent)
        {
            NtClose(Context->LocalDataReadyEvent);
            Context->LocalDataReadyEvent = NULL;
        }
    }
}
