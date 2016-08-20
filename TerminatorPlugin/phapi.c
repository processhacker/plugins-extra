/*
 * Process Hacker Extra Plugins -
 *   Terminator Plugin
 *
 * Copyright (C) 2009-2016 wj32
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
#include "kph2user.h"

/**
 * Opens a process.
 *
 * \param ProcessHandle A variable which receives a handle to the process.
 * \param DesiredAccess The desired access to the process.
 * \param ProcessId The ID of the process.
 */
NTSTATUS Ph2OpenProcess(
    _Out_ PHANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ HANDLE ProcessId
    )
{
    CLIENT_ID clientId;

    clientId.UniqueProcess = ProcessId;
    clientId.UniqueThread = NULL;

    if (Kph2IsConnected())
    {
        return Kph2OpenProcess(
            ProcessHandle,
            DesiredAccess,
            &clientId
            );
    }
    else
    {
        return PhOpenProcess(
            ProcessHandle,
            DesiredAccess,
            ProcessId
            );
    }
}

/**
 * Opens a thread.
 *
 * \param ThreadHandle A variable which receives a handle to the thread.
 * \param DesiredAccess The desired access to the thread.
 * \param ThreadId The ID of the thread.
 */
NTSTATUS Ph2OpenThread(
    _Out_ PHANDLE ThreadHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ HANDLE ThreadId
    )
{
    CLIENT_ID clientId;

    clientId.UniqueProcess = NULL;
    clientId.UniqueThread = ThreadId;

    if (Kph2IsConnected())
    {
        return Kph2OpenThread(
            ThreadHandle,
            DesiredAccess,
            &clientId
            );
    }
    else
    {
        return PhOpenThread(
            ThreadHandle,
            DesiredAccess,
            ThreadId
            );
    }
}

/**
 * Duplicates a handle.
 *
 * \param SourceProcessHandle A handle to the source process. The handle must have
 * PROCESS_DUP_HANDLE access.
 * \param SourceHandle The handle to duplicate from the source process.
 * \param TargetProcessHandle A handle to the target process. If DUPLICATE_CLOSE_SOURCE is specified
 * in the \a Options parameter, this parameter can be NULL.
 * \param TargetHandle A variable which receives the new handle in the target process. If
 * DUPLICATE_CLOSE_SOURCE is specified in the \a Options parameter, this parameter can be NULL.
 * \param DesiredAccess The desired access to the object referenced by the source handle.
 * \param HandleAttributes The attributes to apply to the new handle.
 * \param Options The options to use when duplicating the handle.
 */
NTSTATUS Ph2DuplicateObject(
    _In_ HANDLE SourceProcessHandle,
    _In_ HANDLE SourceHandle,
    _In_opt_ HANDLE TargetProcessHandle,
    _Out_opt_ PHANDLE TargetHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ ULONG HandleAttributes,
    _In_ ULONG Options
    )
{
    NTSTATUS status;

    if (Kph2IsConnected())
    {
        status = Kph2DuplicateObject(
            SourceProcessHandle,
            SourceHandle,
            TargetProcessHandle,
            TargetHandle,
            DesiredAccess,
            HandleAttributes,
            Options
            );

        // If KPH couldn't duplicate the handle, pass through to NtDuplicateObject. This is for
        // special objects like ALPC ports.
        if (status != STATUS_NOT_SUPPORTED)
            return status;
    }

    return NtDuplicateObject(
        SourceProcessHandle,
        SourceHandle,
        TargetProcessHandle,
        TargetHandle,
        DesiredAccess,
        HandleAttributes,
        Options
        );
}

/**
 * Terminates a thread.
 *
 * \param ThreadHandle A handle to a thread. The handle must have THREAD_TERMINATE access.
 * \param ExitStatus A status value that indicates why the thread is being terminated.
 */
NTSTATUS Ph2TerminateThread(
    _In_ HANDLE ThreadHandle,
    _In_ NTSTATUS ExitStatus
    )
{
#ifndef _WIN64
    NTSTATUS status;

    if (Kph2IsConnected())
    {
        status = Kph2TerminateThread(
            ThreadHandle,
            ExitStatus
            );

        if (status != STATUS_NOT_SUPPORTED)
            return status;
    }
#endif

    return NtTerminateThread(
        ThreadHandle,
        ExitStatus
        );
}

/**
 * Gets the processor context of a thread.
 *
 * \param ThreadHandle A handle to a thread. The handle must have THREAD_GET_CONTEXT access.
 * \param Context A variable which receives the context structure.
 */
NTSTATUS Ph2GetThreadContext(
    _In_ HANDLE ThreadHandle,
    _Inout_ PCONTEXT Context
    )
{
    if (Kph2IsConnected())
    {
        return Kph2GetContextThread(ThreadHandle, Context);
    }
    else
    {
        return NtGetContextThread(ThreadHandle, Context);
    }
}

/**
 * Sets the processor context of a thread.
 *
 * \param ThreadHandle A handle to a thread. The handle must have THREAD_SET_CONTEXT access.
 * \param Context The new context structure.
 */
NTSTATUS Ph2SetThreadContext(
    _In_ HANDLE ThreadHandle,
    _In_ PCONTEXT Context
    )
{
    if (Kph2IsConnected())
    {
        return Kph2SetContextThread(ThreadHandle, Context);
    }
    else
    {
        return NtSetContextThread(ThreadHandle, Context);
    }
}

/**
 * Copies memory from the current process into another process.
 *
 * \param ProcessHandle A handle to a process. The handle must have PROCESS_VM_WRITE access.
 * \param BaseAddress The address to which memory is to be copied.
 * \param Buffer A buffer which contains the memory to copy.
 * \param BufferSize The number of bytes to copy.
 * \param NumberOfBytesWritten A variable which receives the number of bytes copied from the buffer.
 */
NTSTATUS Ph2WriteVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_ PVOID BaseAddress,
    _In_reads_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesWritten
    )
{
    NTSTATUS status;

    status = NtWriteVirtualMemory(
        ProcessHandle,
        BaseAddress,
        Buffer,
        BufferSize,
        NumberOfBytesWritten
        );

    if (status == STATUS_ACCESS_DENIED && Kph2IsConnected())
    {
        status = Kph2WriteVirtualMemory(
            ProcessHandle,
            BaseAddress,
            Buffer,
            BufferSize,
            NumberOfBytesWritten
            );
    }

    return status;
}

/**
 * Terminates a process.
 *
 * \param ProcessHandle A handle to a process. The handle must have PROCESS_TERMINATE access.
 * \param ExitStatus A status value that indicates why the process is being terminated.
 */
NTSTATUS Ph2TerminateProcess(
    _In_ HANDLE ProcessHandle,
    _In_ NTSTATUS ExitStatus
    )
{
    NTSTATUS status;

    if (Kph2IsConnected())
    {
        status = Kph2TerminateProcess(
            ProcessHandle,
            ExitStatus
            );

        if (status != STATUS_NOT_SUPPORTED)
            return status;
    }

    return NtTerminateProcess(
        ProcessHandle,
        ExitStatus
        );
}