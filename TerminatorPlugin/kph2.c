/*
 * Process Hacker -
 *   KProcessHacker API
 *
 * Copyright (C) 2009-2011 wj32
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
#include <hndlinfo.h>
#include <shlobj.h>

HANDLE PhKph2Handle = NULL;

PPH_STRING Kph2GetPluginDirectory(
    VOID
    )
{
    PPH_STRING directory;
    PPH_STRING path;
    PPH_STRING kphFileName;

    directory = PhGetApplicationDirectory();
    path = PhIsExecutingInWow64() ? PhCreateString(KPH_PATH32) : PhCreateString(KPH_PATH64);

    kphFileName = PhConcatStringRef2(&directory->sr, &path->sr);
  
    PhDereferenceObject(path);
    PhDereferenceObject(directory);

    // Create the directory if it does not exist.
    {
        PPH_STRING fullPath;
        ULONG indexOfFileName;
        PPH_STRING directoryName;

        if (fullPath = PhGetFullPath(PhGetString(kphFileName), &indexOfFileName))
        {
            if (indexOfFileName != -1)
            {
                directoryName = PhSubstring(fullPath, 0, indexOfFileName);

                PhCreateDirectory(directoryName);

                PhDereferenceObject(directoryName);
            }

            PhDereferenceObject(fullPath);
        }
    }

    return kphFileName;
}

NTSTATUS Kph2Connect(
    VOID
    )
{
    NTSTATUS status;
    HANDLE kphHandle;
    UNICODE_STRING objectName;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK isb;
    OBJECT_HANDLE_FLAG_INFORMATION handleFlagInfo;

    if (PhKph2Handle)
        return STATUS_ADDRESS_ALREADY_EXISTS;

    RtlInitUnicodeString(&objectName, KPH_DEVICE_NAME);
    InitializeObjectAttributes(
        &objectAttributes,
        &objectName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    status = NtOpenFile(
        &kphHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        &objectAttributes,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_NON_DIRECTORY_FILE
        );

    if (NT_SUCCESS(status))
    {
        // Protect the handle from being closed.

        handleFlagInfo.Inherit = FALSE;
        handleFlagInfo.ProtectFromClose = TRUE;

        NtSetInformationObject(
            kphHandle,
            ObjectHandleFlagInformation,
            &handleFlagInfo,
            sizeof(OBJECT_HANDLE_FLAG_INFORMATION)
            );

        PhKph2Handle = kphHandle;
    }

    return status;
}

NTSTATUS Kph2Disconnect(
    VOID
    )
{
    NTSTATUS status;
    OBJECT_HANDLE_FLAG_INFORMATION handleFlagInfo;

    if (!PhKph2Handle)
        return STATUS_ALREADY_DISCONNECTED;

    // Unprotect the handle.

    handleFlagInfo.Inherit = FALSE;
    handleFlagInfo.ProtectFromClose = FALSE;

    NtSetInformationObject(
        PhKph2Handle,
        ObjectHandleFlagInformation,
        &handleFlagInfo,
        sizeof(OBJECT_HANDLE_FLAG_INFORMATION)
        );

    status = NtClose(PhKph2Handle);
    PhKph2Handle = NULL;

    return status;
}

BOOLEAN Kph2IsConnected(
    VOID
    )
{
    return PhKph2Handle != NULL;
}

NTSTATUS Kph2SetParameters(
    _In_ PKPH_PARAMETERS Parameters
    )
{
    static PH_STRINGREF keyName = PH_STRINGREF_INIT(L"System\\CurrentControlSet\\Services\\kph2\\Parameters");
    NTSTATUS status;
    HANDLE parametersKeyHandle = NULL;
    ULONG disposition;
    UNICODE_STRING valueName;

    if (!NT_SUCCESS(status = PhCreateKey(
        &parametersKeyHandle,
        KEY_WRITE | DELETE,
        PH_KEY_LOCAL_MACHINE,
        &keyName,
        0,
        0,
        &disposition
        )))
    {
        return status;
    }

    RtlInitUnicodeString(&valueName, L"SecurityLevel");
    if (!NT_SUCCESS(status = NtSetValueKey(
        parametersKeyHandle,
        &valueName,
        0,
        REG_DWORD,
        &Parameters->SecurityLevel,
        sizeof(ULONG)
        )))
    {
        goto SetValuesEnd;
    }

    if (Parameters->CreateDynamicConfiguration)
    {
        KPH_DYN_CONFIGURATION configuration;

        RtlInitUnicodeString(&valueName, L"DynamicConfiguration");

        configuration.Version = KPH_DYN_CONFIGURATION_VERSION;
        configuration.NumberOfPackages = 1;

        if (NT_SUCCESS(Kph2InitializeDynamicPackage(&configuration.Packages[0])))
        {
            status = NtSetValueKey(
                parametersKeyHandle, 
                &valueName, 
                0, 
                REG_BINARY, 
                &configuration, 
                sizeof(KPH_DYN_CONFIGURATION)
                );
        }
    }

    // Put more parameters here...

SetValuesEnd:
    if (!NT_SUCCESS(status))
    {
        // Delete the key if we created it.
        if (disposition == REG_CREATED_NEW_KEY)
            NtDeleteKey(parametersKeyHandle);
    }

    NtClose(parametersKeyHandle);

    return status;
}

NTSTATUS Kph2RemoveParameters(
    VOID
    )
{
    static PH_STRINGREF keyName = PH_STRINGREF_INIT(L"System\\CurrentControlSet\\Services\\kph2\\Parameters");
    NTSTATUS status;
    HANDLE parametersKeyHandle = NULL;

    if (NT_SUCCESS(status = PhOpenKey(
        &parametersKeyHandle,
        DELETE,
        PH_KEY_LOCAL_MACHINE,
        &keyName,
        0
        )))
    {
        status = NtDeleteKey(parametersKeyHandle);
        NtClose(parametersKeyHandle);
    }

    if (status == STATUS_OBJECT_NAME_NOT_FOUND)
        status = STATUS_SUCCESS; // Normalize special error codes

    return status;
}

static NTSTATUS Kph2Extract(
    _In_ PPH_STRING KphFileName
    )
{
    NTSTATUS status = STATUS_FAIL_CHECK;
    HANDLE fileHandle = NULL;
    HANDLE sectionHandle = NULL;
    PVOID viewBase = NULL;
    PVOID fileResource;
    HRSRC resourceHandle;
    HGLOBAL fileResourceHandle;
    SIZE_T viewSize = 0;
    SIZE_T resourceLength;
    LARGE_INTEGER maximumSize;

    if (!(resourceHandle = FindResource(
        PluginInstance->DllBase,
        PhIsExecutingInWow64() ? MAKEINTRESOURCE(IDR_KPH2_32) : MAKEINTRESOURCE(IDR_KPH2_64),
        RT_RCDATA
        )))
    {
        goto CleanupExit;
    }

    if (!(fileResourceHandle = LoadResource(PluginInstance->DllBase, resourceHandle)))
        goto CleanupExit;

    if (!(fileResource = LockResource(fileResourceHandle)))
        goto CleanupExit;

    resourceLength = (SIZE_T)SizeofResource(PluginInstance->DllBase, resourceHandle);
    maximumSize.QuadPart = resourceLength;

    if (!NT_SUCCESS(status = PhCreateFileWin32(
        &fileHandle,
        PhGetString(KphFileName),
        FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_OPEN_IF,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
        )))
    {
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = NtCreateSection(
        &sectionHandle,
        SECTION_ALL_ACCESS,
        NULL,
        &maximumSize,
        PAGE_READWRITE,
        SEC_COMMIT,
        fileHandle
        )))
    {
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = NtMapViewOfSection(
        sectionHandle,
        NtCurrentProcess(),
        &viewBase,
        0,
        0,
        NULL,
        &viewSize,
        ViewShare,
        0,
        PAGE_READWRITE
        )))
    {
        goto CleanupExit;
    }

    RtlCopyMemory(viewBase, fileResource, resourceLength);

CleanupExit:

    if (viewBase)
        NtUnmapViewOfSection(NtCurrentProcess(), viewBase);

    if (sectionHandle)
        NtClose(sectionHandle);

    if (fileHandle)
        NtClose(fileHandle);

    if (resourceHandle)
        FreeResource(resourceHandle);

    return status;
}

NTSTATUS Kph2Install(
    _In_ PKPH_PARAMETERS Parameters
    )
{
    static UNICODE_STRING objectName = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\kph2");
    NTSTATUS status;
    ULONG parameters;
    HANDLE tokenHandle = NULL;
    HANDLE keyHandle = NULL;
    PPH_STRING kphFileName;
    UNICODE_STRING fileName = { 0 };
    UNICODE_STRING valueName;
    OBJECT_ATTRIBUTES objectAttributes;

    kphFileName = Kph2GetPluginDirectory();

    if (!RtlDoesFileExists_U(kphFileName->Buffer))
    {
        if (!NT_SUCCESS(status = Kph2Extract(kphFileName)))
            goto CleanupExit;
    }
    
    if (NT_SUCCESS(status = NtOpenProcessToken(
        NtCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES,
        &tokenHandle
        )))
    {
        PhSetTokenPrivilege(tokenHandle, SE_LOAD_DRIVER_NAME, NULL, SE_PRIVILEGE_ENABLED);
        NtClose(tokenHandle);
    }

    InitializeObjectAttributes(
        &objectAttributes,
        &objectName,
        OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
        NULL,
        NULL
        );

    status = NtCreateKey(
        &keyHandle,
        MAXIMUM_ALLOWED,
        &objectAttributes,
        0,
        NULL,
        0,
        NULL
        );

    if (!NT_SUCCESS(status))
        goto CleanupExit;

    if (!NT_SUCCESS(status = RtlDosPathNameToNtPathName_U_WithStatus(
        kphFileName->Buffer,
        &fileName,
        NULL,
        NULL
        )))
    {
        goto CleanupExit;
    }

    RtlInitUnicodeString(&valueName, L"Type");
    parameters = SERVICE_KERNEL_DRIVER;
    if (!NT_SUCCESS(status = NtSetValueKey(
        keyHandle,
        &valueName,
        0,
        REG_DWORD,
        &parameters,
        sizeof(ULONG)
        )))
    {
        goto CleanupExit;
    }

    RtlInitUnicodeString(&valueName, L"ErrorControl");
    parameters = SERVICE_ERROR_NORMAL;
    if (!NT_SUCCESS(status = NtSetValueKey(
        keyHandle,
        &valueName,
        0,
        REG_DWORD,
        &parameters,
        sizeof(ULONG)
        )))
    {
        goto CleanupExit;
    }

    RtlInitUnicodeString(&valueName, L"Start");
    parameters = SERVICE_DEMAND_START;
    if (!NT_SUCCESS(status = NtSetValueKey(
        keyHandle,
        &valueName,
        0,
        REG_DWORD,
        &parameters,
        sizeof(ULONG)
        )))
    {
        goto CleanupExit;
    }

    RtlInitUnicodeString(&valueName, L"ImagePath");
    if (!NT_SUCCESS(status = NtSetValueKey(
        keyHandle,
        &valueName,
        0,
        REG_EXPAND_SZ,
        fileName.Buffer,
        fileName.Length + sizeof(UNICODE_NULL)
        )))
    {
        goto CleanupExit;
    }

    if (Parameters)
    {
        if (!NT_SUCCESS(status = Kph2SetParameters(Parameters)))
            goto CleanupExit;
    }

    if (!NT_SUCCESS(status = NtUnloadDriver(&objectName)))
        status = STATUS_SUCCESS;

    if ((status = NtLoadDriver(&objectName)) == STATUS_IMAGE_ALREADY_LOADED)
        status = STATUS_SUCCESS; // Normalize special error codes

CleanupExit:

    if (keyHandle)
        NtClose(keyHandle);

    if (kphFileName)
        PhDereferenceObject(kphFileName);

    if (fileName.Buffer)
        RtlFreeUnicodeString(&fileName);

    return status;
}

NTSTATUS Kph2Uninstall(
    VOID
    )
{
    static UNICODE_STRING objectName = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\kph2");
    NTSTATUS status;
    HANDLE keyHandle;
    OBJECT_ATTRIBUTES objectAttributes;
    PPH_STRING kphFileName;
    UNICODE_STRING fileName = { 0 };
    UNICODE_STRING valueName;

    kphFileName = Kph2GetPluginDirectory();

    if (!RtlDoesFileExists_U(kphFileName->Buffer))
    {
        if (!NT_SUCCESS(status = Kph2Extract(kphFileName)))
            return status;
    }
    
    InitializeObjectAttributes(
        &objectAttributes,
        &objectName,
        OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
        NULL,
        NULL
        );

    status = NtCreateKey(
        &keyHandle,
        KEY_READ | KEY_WRITE | DELETE,
        &objectAttributes,
        0,
        NULL,
        0,
        NULL
        );

    if (!NT_SUCCESS(status))
        goto CleanupExit;

    if (!NT_SUCCESS(status = RtlDosPathNameToNtPathName_U_WithStatus(
        kphFileName->Buffer,
        &fileName,
        NULL,
        NULL
        )))
    {
        goto CleanupExit;
    }

    RtlInitUnicodeString(&valueName, L"Type");
    if (!NT_SUCCESS(status = NtSetValueKey(
        keyHandle,
        &valueName,
        0,
        REG_DWORD,
        &(ULONG) { SERVICE_KERNEL_DRIVER },
        sizeof(ULONG)
        )))
    {
        goto CleanupExit;
    }

    RtlInitUnicodeString(&valueName, L"ErrorControl");
    if (!NT_SUCCESS(status = NtSetValueKey(
        keyHandle,
        &valueName,
        0,
        REG_DWORD,
        &(ULONG) { SERVICE_ERROR_NORMAL },
        sizeof(ULONG)
        )))
    {
        goto CleanupExit;
    }

    RtlInitUnicodeString(&valueName, L"Start");
    if (!NT_SUCCESS(status = NtSetValueKey(
        keyHandle,
        &valueName,
        0,
        REG_DWORD,
        &(ULONG) { SERVICE_DEMAND_START },
        sizeof(ULONG)
        )))
    {
        goto CleanupExit;
    }

    RtlInitUnicodeString(&valueName, L"ImagePath");
    if (!NT_SUCCESS(status = NtSetValueKey(
        keyHandle,
        &valueName,
        0,
        REG_EXPAND_SZ,
        fileName.Buffer,
        fileName.Length + sizeof(UNICODE_NULL)
        )))
    {
        goto CleanupExit;
    }

    if ((status = NtUnloadDriver(&objectName)) == STATUS_OBJECT_NAME_NOT_FOUND)
        status = STATUS_SUCCESS; // Normalize special error codes

    if (NT_SUCCESS(status))
        status = Kph2RemoveParameters();

    if (NT_SUCCESS(status))
        status = NtDeleteKey(keyHandle);

CleanupExit:

    if (keyHandle)
        NtClose(keyHandle);

    if (kphFileName)
        PhDereferenceObject(kphFileName);

    if (fileName.Buffer)
        RtlFreeUnicodeString(&fileName);

    return status;
}

NTSTATUS Kphp2DeviceIoControl(
    _In_ ULONG KphControlCode,
    _In_ PVOID InBuffer,
    _In_ ULONG InBufferLength
    )
{
    IO_STATUS_BLOCK isb;

    return NtDeviceIoControlFile(
        PhKph2Handle,
        NULL,
        NULL,
        NULL,
        &isb,
        KphControlCode,
        InBuffer,
        InBufferLength,
        NULL,
        0
        );
}

NTSTATUS Kph2GetFeatures(
    _Out_ PULONG Features
    )
{
    struct
    {
        PULONG Features;
    } input = { Features };

    return Kphp2DeviceIoControl(
        KPH_GETFEATURES,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2OpenProcess(
    _Out_ PHANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PCLIENT_ID ClientId
    )
{
    struct
    {
        PHANDLE ProcessHandle;
        ACCESS_MASK DesiredAccess;
        PCLIENT_ID ClientId;
    } input = { ProcessHandle, DesiredAccess, ClientId };

    return Kphp2DeviceIoControl(
        KPH_OPENPROCESS,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2OpenProcessToken(
    _In_ HANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE TokenHandle
    )
{
    struct
    {
        HANDLE ProcessHandle;
        ACCESS_MASK DesiredAccess;
        PHANDLE TokenHandle;
    } input = { ProcessHandle, DesiredAccess, TokenHandle };

    return Kphp2DeviceIoControl(
        KPH_OPENPROCESSTOKEN,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2OpenProcessJob(
    _In_ HANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE JobHandle
    )
{
    struct
    {
        HANDLE ProcessHandle;
        ACCESS_MASK DesiredAccess;
        PHANDLE JobHandle;
    } input = { ProcessHandle, DesiredAccess, JobHandle };

    return Kphp2DeviceIoControl(
        KPH_OPENPROCESSJOB,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2SuspendProcess(
    _In_ HANDLE ProcessHandle
    )
{
    struct
    {
        HANDLE ProcessHandle;
    } input = { ProcessHandle };

    return Kphp2DeviceIoControl(
        KPH_SUSPENDPROCESS,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2ResumeProcess(
    _In_ HANDLE ProcessHandle
    )
{
    struct
    {
        HANDLE ProcessHandle;
    } input = { ProcessHandle };

    return Kphp2DeviceIoControl(
        KPH_RESUMEPROCESS,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2TerminateProcess(
    _In_ HANDLE ProcessHandle,
    _In_ NTSTATUS ExitStatus
    )
{
    NTSTATUS status;
    struct
    {
        HANDLE ProcessHandle;
        NTSTATUS ExitStatus;
    } input = { ProcessHandle, ExitStatus };

    status = Kphp2DeviceIoControl(
        KPH_TERMINATEPROCESS,
        &input,
        sizeof(input)
        );

    // Check if we're trying to terminate the current process,
    // because kernel-mode can't do it.
    if (status == STATUS_CANT_TERMINATE_SELF)
    {
        RtlExitUserProcess(ExitStatus);
    }

    return status;
}

NTSTATUS Kph2ReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_ PVOID BaseAddress,
    _Out_writes_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead
    )
{
    struct
    {
        HANDLE ProcessHandle;
        PVOID BaseAddress;
        PVOID Buffer;
        SIZE_T BufferSize;
        PSIZE_T NumberOfBytesRead;
    } input = { ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesRead };

    return Kphp2DeviceIoControl(
        KPH_READVIRTUALMEMORY,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2WriteVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _In_reads_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesWritten
    )
{
    struct
    {
        HANDLE ProcessHandle;
        PVOID BaseAddress;
        PVOID Buffer;
        SIZE_T BufferSize;
        PSIZE_T NumberOfBytesWritten;
    } input = { ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesWritten };

    return Kphp2DeviceIoControl(
        KPH_WRITEVIRTUALMEMORY,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2ReadVirtualMemoryUnsafe(
    _In_opt_ HANDLE ProcessHandle,
    _In_ PVOID BaseAddress,
    _Out_writes_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead
    )
{
    struct
    {
        HANDLE ProcessHandle;
        PVOID BaseAddress;
        PVOID Buffer;
        SIZE_T BufferSize;
        PSIZE_T NumberOfBytesRead;
    } input = { ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesRead };

    return Kphp2DeviceIoControl(
        KPH_READVIRTUALMEMORYUNSAFE,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2QueryInformationProcess(
    _In_ HANDLE ProcessHandle,
    _In_ KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass,
    _Out_writes_bytes_(ProcessInformationLength) PVOID ProcessInformation,
    _In_ ULONG ProcessInformationLength,
    _Out_opt_ PULONG ReturnLength
    )
{
    struct
    {
        HANDLE ProcessHandle;
        KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass;
        PVOID ProcessInformation;
        ULONG ProcessInformationLength;
        PULONG ReturnLength;
    } input = { ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength };

    return Kphp2DeviceIoControl(
        KPH_QUERYINFORMATIONPROCESS,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2SetInformationProcess(
    _In_ HANDLE ProcessHandle,
    _In_ KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass,
    _In_reads_bytes_(ProcessInformationLength) PVOID ProcessInformation,
    _In_ ULONG ProcessInformationLength
    )
{
    struct
    {
        HANDLE ProcessHandle;
        KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass;
        PVOID ProcessInformation;
        ULONG ProcessInformationLength;
    } input = { ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength };

    return Kphp2DeviceIoControl(
        KPH_SETINFORMATIONPROCESS,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2OpenThread(
    _Out_ PHANDLE ThreadHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PCLIENT_ID ClientId
    )
{
    struct
    {
        PHANDLE ThreadHandle;
        ACCESS_MASK DesiredAccess;
        PCLIENT_ID ClientId;
    } input = { ThreadHandle, DesiredAccess, ClientId };

    return Kphp2DeviceIoControl(
        KPH_OPENTHREAD,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2OpenThreadProcess(
    _In_ HANDLE ThreadHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE ProcessHandle
    )
{
    struct
    {
        HANDLE ThreadHandle;
        ACCESS_MASK DesiredAccess;
        PHANDLE ProcessHandle;
    } input = { ThreadHandle, DesiredAccess, ProcessHandle };

    return Kphp2DeviceIoControl(
        KPH_OPENTHREADPROCESS,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2TerminateThread(
    _In_ HANDLE ThreadHandle,
    _In_ NTSTATUS ExitStatus
    )
{
    NTSTATUS status;
    struct
    {
        HANDLE ThreadHandle;
        NTSTATUS ExitStatus;
    } input = { ThreadHandle, ExitStatus };

    status = Kphp2DeviceIoControl(
        KPH_TERMINATETHREAD,
        &input,
        sizeof(input)
        );

    if (status == STATUS_CANT_TERMINATE_SELF)
    {
        RtlExitUserThread(ExitStatus);
    }

    return status;
}

NTSTATUS Kph2TerminateThreadUnsafe(
    _In_ HANDLE ThreadHandle,
    _In_ NTSTATUS ExitStatus
    )
{
    struct
    {
        HANDLE ThreadHandle;
        NTSTATUS ExitStatus;
    } input = { ThreadHandle, ExitStatus };

    return Kphp2DeviceIoControl(
        KPH_TERMINATETHREADUNSAFE,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2GetContextThread(
    _In_ HANDLE ThreadHandle,
    _Inout_ PCONTEXT ThreadContext
    )
{
    struct
    {
        HANDLE ThreadHandle;
        PCONTEXT ThreadContext;
    } input = { ThreadHandle, ThreadContext };

    return Kphp2DeviceIoControl(
        KPH_GETCONTEXTTHREAD,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2SetContextThread(
    _In_ HANDLE ThreadHandle,
    _In_ PCONTEXT ThreadContext
    )
{
    struct
    {
        HANDLE ThreadHandle;
        PCONTEXT ThreadContext;
    } input = { ThreadHandle, ThreadContext };

    return Kphp2DeviceIoControl(
        KPH_SETCONTEXTTHREAD,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2CaptureStackBackTraceThread(
    _In_ HANDLE ThreadHandle,
    _In_ ULONG FramesToSkip,
    _In_ ULONG FramesToCapture,
    _Out_writes_(FramesToCapture) PVOID *BackTrace,
    _Out_opt_ PULONG CapturedFrames,
    _Out_opt_ PULONG BackTraceHash
    )
{
    struct
    {
        HANDLE ThreadHandle;
        ULONG FramesToSkip;
        ULONG FramesToCapture;
        PVOID *BackTrace;
        PULONG CapturedFrames;
        PULONG BackTraceHash;
    } input = { ThreadHandle, FramesToSkip, FramesToCapture, BackTrace, CapturedFrames, BackTraceHash };

    return Kphp2DeviceIoControl(
        KPH_CAPTURESTACKBACKTRACETHREAD,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2QueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ KPH_THREAD_INFORMATION_CLASS ThreadInformationClass,
    _Out_writes_bytes_(ThreadInformationLength) PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Out_opt_ PULONG ReturnLength
    )
{
    struct
    {
        HANDLE ThreadHandle;
        KPH_THREAD_INFORMATION_CLASS ThreadInformationClass;
        PVOID ThreadInformation;
        ULONG ThreadInformationLength;
        PULONG ReturnLength;
    } input = { ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength, ReturnLength };

    return Kphp2DeviceIoControl(
        KPH_QUERYINFORMATIONTHREAD,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2SetInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ KPH_THREAD_INFORMATION_CLASS ThreadInformationClass,
    _In_reads_bytes_(ThreadInformationLength) PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength
    )
{
    struct
    {
        HANDLE ThreadHandle;
        KPH_THREAD_INFORMATION_CLASS ThreadInformationClass;
        PVOID ThreadInformation;
        ULONG ThreadInformationLength;
    } input = { ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength };

    return Kphp2DeviceIoControl(
        KPH_SETINFORMATIONTHREAD,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2EnumerateProcessHandles(
    _In_ HANDLE ProcessHandle,
    _Out_writes_bytes_(BufferLength) PVOID Buffer,
    _In_opt_ ULONG BufferLength,
    _Out_opt_ PULONG ReturnLength
    )
{
    struct
    {
        HANDLE ProcessHandle;
        PVOID Buffer;
        ULONG BufferLength;
        PULONG ReturnLength;
    } input = { ProcessHandle, Buffer, BufferLength, ReturnLength };

    return Kphp2DeviceIoControl(
        KPH_ENUMERATEPROCESSHANDLES,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2EnumerateProcessHandles2(
    _In_ HANDLE ProcessHandle,
    _Out_ PKPH_PROCESS_HANDLE_INFORMATION *Handles
    )
{
    NTSTATUS status;
    PVOID buffer;
    ULONG bufferSize = 2048;

    buffer = PhAllocate(bufferSize);

    while (TRUE)
    {
        status = Kph2EnumerateProcessHandles(
            ProcessHandle,
            buffer,
            bufferSize,
            &bufferSize
            );

        if (status == STATUS_BUFFER_TOO_SMALL)
        {
            PhFree(buffer);
            buffer = PhAllocate(bufferSize);
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

    *Handles = buffer;

    return status;
}

NTSTATUS Kph2QueryInformationObject(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE Handle,
    _In_ KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass,
    _Out_writes_bytes_(ObjectInformationLength) PVOID ObjectInformation,
    _In_ ULONG ObjectInformationLength,
    _Out_opt_ PULONG ReturnLength
    )
{
    struct
    {
        HANDLE ProcessHandle;
        HANDLE Handle;
        KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass;
        PVOID ObjectInformation;
        ULONG ObjectInformationLength;
        PULONG ReturnLength;
    } input = { ProcessHandle, Handle, ObjectInformationClass, ObjectInformation, ObjectInformationLength, ReturnLength };

    return Kphp2DeviceIoControl(
        KPH_QUERYINFORMATIONOBJECT,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2SetInformationObject(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE Handle,
    _In_ KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass,
    _In_reads_bytes_(ObjectInformationLength) PVOID ObjectInformation,
    _In_ ULONG ObjectInformationLength
    )
{
    struct
    {
        HANDLE ProcessHandle;
        HANDLE Handle;
        KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass;
        PVOID ObjectInformation;
        ULONG ObjectInformationLength;
    } input = { ProcessHandle, Handle, ObjectInformationClass, ObjectInformation, ObjectInformationLength };

    return Kphp2DeviceIoControl(
        KPH_SETINFORMATIONOBJECT,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2DuplicateObject(
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
    struct
    {
        HANDLE SourceProcessHandle;
        HANDLE SourceHandle;
        HANDLE TargetProcessHandle;
        PHANDLE TargetHandle;
        ACCESS_MASK DesiredAccess;
        ULONG HandleAttributes;
        ULONG Options;
    } input = { SourceProcessHandle, SourceHandle, TargetProcessHandle, TargetHandle, DesiredAccess, HandleAttributes, Options };

    status = Kphp2DeviceIoControl(
        KPH_DUPLICATEOBJECT,
        &input,
        sizeof(input)
        );

    if (status == STATUS_CANT_TERMINATE_SELF)
    {
        // We tried to close a handle in the current process.
        if (Options & DUPLICATE_CLOSE_SOURCE)
            status = NtClose(SourceHandle);
    }

    return status;
}

NTSTATUS Kph2OpenDriver(
    _Out_ PHANDLE DriverHandle,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes
    )
{
    struct
    {
        PHANDLE DriverHandle;
        POBJECT_ATTRIBUTES ObjectAttributes;
    } input = { DriverHandle, ObjectAttributes };

    return Kphp2DeviceIoControl(
        KPH_OPENDRIVER,
        &input,
        sizeof(input)
        );
}

NTSTATUS Kph2QueryInformationDriver(
    _In_ HANDLE DriverHandle,
    _In_ DRIVER_INFORMATION_CLASS DriverInformationClass,
    _Out_writes_bytes_(DriverInformationLength) PVOID DriverInformation,
    _In_ ULONG DriverInformationLength,
    _Out_opt_ PULONG ReturnLength
    )
{
    struct
    {
        HANDLE DriverHandle;
        DRIVER_INFORMATION_CLASS DriverInformationClass;
        PVOID DriverInformation;
        ULONG DriverInformationLength;
        PULONG ReturnLength;
    } input = { DriverHandle, DriverInformationClass, DriverInformation, DriverInformationLength, ReturnLength };

    return Kphp2DeviceIoControl(
        KPH_QUERYINFORMATIONDRIVER,
        &input,
        sizeof(input)
        );
}
