#ifndef _PH_KPHUSER_H
#define _PH_KPHUSER_H

#include "kph2api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _KPH_PARAMETERS
{
    KPH_SECURITY_LEVEL SecurityLevel;
    BOOLEAN CreateDynamicConfiguration;
} KPH_PARAMETERS, *PKPH_PARAMETERS;

NTSTATUS
NTAPI
Kph2Connect(
    VOID
    );

NTSTATUS
NTAPI
Kph2Disconnect(
    VOID
    );

BOOLEAN
NTAPI
Kph2IsConnected(
    VOID
    );

NTSTATUS
NTAPI
Kph2SetParameters(
    _In_ PKPH_PARAMETERS Parameters
    );

NTSTATUS
NTAPI
Kph2Install(
    _In_ PKPH_PARAMETERS Parameters
    );

NTSTATUS
NTAPI
Kph2Uninstall(
    VOID
    );

NTSTATUS
NTAPI
Kph2GetFeatures(
    _Out_ PULONG Features
    );

NTSTATUS
NTAPI
Kph2OpenProcess(
    _Out_ PHANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PCLIENT_ID ClientId
    );

NTSTATUS
NTAPI
Kph2OpenProcessToken(
    _In_ HANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE TokenHandle
    );

NTSTATUS
NTAPI
Kph2OpenProcessJob(
    _In_ HANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE JobHandle
    );

NTSTATUS
NTAPI
Kph2SuspendProcess(
    _In_ HANDLE ProcessHandle
    );

NTSTATUS
NTAPI
Kph2ResumeProcess(
    _In_ HANDLE ProcessHandle
    );

NTSTATUS
NTAPI
Kph2TerminateProcess(
    _In_ HANDLE ProcessHandle,
    _In_ NTSTATUS ExitStatus
    );

NTSTATUS
NTAPI
Kph2ReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_ PVOID BaseAddress,
    _Out_writes_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead
    );

NTSTATUS
NTAPI
Kph2WriteVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _In_reads_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesWritten
    );

NTSTATUS
NTAPI
Kph2ReadVirtualMemoryUnsafe(
    _In_opt_ HANDLE ProcessHandle,
    _In_ PVOID BaseAddress,
    _Out_writes_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead
    );

NTSTATUS
NTAPI
Kph2QueryInformationProcess(
    _In_ HANDLE ProcessHandle,
    _In_ KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass,
    _Out_writes_bytes_(ProcessInformationLength) PVOID ProcessInformation,
    _In_ ULONG ProcessInformationLength,
    _Out_opt_ PULONG ReturnLength
    );

NTSTATUS
NTAPI
Kph2SetInformationProcess(
    _In_ HANDLE ProcessHandle,
    _In_ KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass,
    _In_reads_bytes_(ProcessInformationLength) PVOID ProcessInformation,
    _In_ ULONG ProcessInformationLength
    );

NTSTATUS
NTAPI
Kph2OpenThread(
    _Out_ PHANDLE ThreadHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PCLIENT_ID ClientId
    );

NTSTATUS
NTAPI
Kph2OpenThreadProcess(
    _In_ HANDLE ThreadHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE ProcessHandle
    );

NTSTATUS
NTAPI
Kph2TerminateThread(
    _In_ HANDLE ThreadHandle,
    _In_ NTSTATUS ExitStatus
    );

NTSTATUS
NTAPI
Kph2TerminateThreadUnsafe(
    _In_ HANDLE ThreadHandle,
    _In_ NTSTATUS ExitStatus
    );

NTSTATUS
NTAPI
Kph2GetContextThread(
    _In_ HANDLE ThreadHandle,
    _Inout_ PCONTEXT ThreadContext
    );

NTSTATUS
NTAPI
Kph2SetContextThread(
    _In_ HANDLE ThreadHandle,
    _In_ PCONTEXT ThreadContext
    );

NTSTATUS
NTAPI
Kph2CaptureStackBackTraceThread(
    _In_ HANDLE ThreadHandle,
    _In_ ULONG FramesToSkip,
    _In_ ULONG FramesToCapture,
    _Out_writes_(FramesToCapture) PVOID *BackTrace,
    _Out_opt_ PULONG CapturedFrames,
    _Out_opt_ PULONG BackTraceHash
    );

NTSTATUS
NTAPI
Kph2QueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ KPH_THREAD_INFORMATION_CLASS ThreadInformationClass,
    _Out_writes_bytes_(ThreadInformationLength) PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Out_opt_ PULONG ReturnLength
    );

NTSTATUS
NTAPI
Kph2SetInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ KPH_THREAD_INFORMATION_CLASS ThreadInformationClass,
    _In_reads_bytes_(ThreadInformationLength) PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength
    );

NTSTATUS
NTAPI
Kph2EnumerateProcessHandles(
    _In_ HANDLE ProcessHandle,
    _Out_writes_bytes_(BufferLength) PVOID Buffer,
    _In_opt_ ULONG BufferLength,
    _Out_opt_ PULONG ReturnLength
    );

NTSTATUS
NTAPI
Kph2EnumerateProcessHandles2(
    _In_ HANDLE ProcessHandle,
    _Out_ PKPH_PROCESS_HANDLE_INFORMATION *Handles
    );

NTSTATUS
NTAPI
Kph2QueryInformationObject(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE Handle,
    _In_ KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass,
    _Out_writes_bytes_(ObjectInformationLength) PVOID ObjectInformation,
    _In_ ULONG ObjectInformationLength,
    _Out_opt_ PULONG ReturnLength
    );

NTSTATUS
NTAPI
Kph2SetInformationObject(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE Handle,
    _In_ KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass,
    _In_reads_bytes_(ObjectInformationLength) PVOID ObjectInformation,
    _In_ ULONG ObjectInformationLength
    );

NTSTATUS
NTAPI
Kph2DuplicateObject(
    _In_ HANDLE SourceProcessHandle,
    _In_ HANDLE SourceHandle,
    _In_opt_ HANDLE TargetProcessHandle,
    _Out_opt_ PHANDLE TargetHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ ULONG HandleAttributes,
    _In_ ULONG Options
    );

NTSTATUS
NTAPI
Kph2OpenDriver(
    _Out_ PHANDLE DriverHandle,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes
    );

NTSTATUS
NTAPI
Kph2QueryInformationDriver(
    _In_ HANDLE DriverHandle,
    _In_ DRIVER_INFORMATION_CLASS DriverInformationClass,
    _Out_writes_bytes_(DriverInformationLength) PVOID DriverInformation,
    _In_ ULONG DriverInformationLength,
    _Out_opt_ PULONG ReturnLength
    );

// kphdata

NTSTATUS
NTAPI
Kph2InitializeDynamicPackage(
    _Out_ PKPH_DYN_PACKAGE Package
    );

#ifdef __cplusplus
}
#endif

#endif
