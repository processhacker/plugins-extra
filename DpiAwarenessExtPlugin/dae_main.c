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
#include <mapimg.h>
#include "resource.h"
#include "dae_utils.h"

#define DAE_DPIAWARENESSEXT_COLUMN_ID 1

#define DAE_DPI_AWARENESS_UNKNOWN 0
#define DAE_DPI_AWARENESS_UNAWARE 1
#define DAE_DPI_AWARENESS_SYSTEM 2
#define DAE_DPI_AWARENESS_PER_MONITOR 3

#define DAE_TRIS_UNKNOWN 0
#define DAE_TRIS_FALSE 1
#define DAE_TRIS_TRUE 2

#define DAE_TRIS_STR(x) ((x) == DAE_TRIS_UNKNOWN ? L"?" : ((x) == DAE_TRIS_TRUE ? L"+" : L"-"))

 /*#define DAE_DPI_AWARENESS_LOCKED 0x8
#define DAE_DPI_AWARENESS_PERTHREAD 0x10*/

// Some flags in CLIENTINFO.CI_flags
#define CI_INITTHREAD        0x00000008
// This is not in any symbol file so is probably not what Microsoft calls it.
#define CI_FORCEDPIAWARE 0x20000000

// The offset of ((PCLIENTINFO)TEB->Win32ClientInfo)->CI_flags (which is TEB->Win32ClientInfo[0])
#define CI_FLAGS_TEB64_OFFSET 0x800
#define CI_FLAGS_TEB32_OFFSET 0x6CC
#ifdef _WIN64
#define CI_FLAGS_TEB_OFFSET CI_FLAGS_TEB64_OFFSET
#else
#define CI_FLAGS_TEB_OFFSET CI_FLAGS_TEB32_OFFSET
#endif

typedef struct _PROCESS_EXTENSION
{
    LIST_ENTRY ListEntry;
    ULONG ValidAwareness : 1;
    ULONG ValidForced : 1;
    ULONG ValidDescription : 1;
    ULONG ValidSpare : 29;

    ULONG DpiAwareness : 2;
    ULONG DpiAwarenessForced : 2;
    ULONG DpiAwarenessExtSpare : 28;
    PPH_PROCESS_ITEM ProcessItem;
    PSYSTEM_PROCESS_INFORMATION ExtProcessInfo;
    WCHAR DpiAwarenessExtDescription[32];
} PROCESS_EXTENSION, *PPROCESS_EXTENSION;

static VOID DaepUpdateDpiAwareness(_In_ PPH_PROCESS_ITEM ProcessItem, _In_opt_ PPROCESS_EXTENSION Extension);
static VOID DaepUpdateDpiAwarenessDescription(_In_ PPROCESS_EXTENSION Extension);

static PPH_PLUGIN PluginInstance;
static PH_CALLBACK_REGISTRATION TreeNewMessageCallbackRegistration;
static PH_CALLBACK_REGISTRATION ProcessTreeNewInitializingCallbackRegistration;
static PH_CALLBACK_REGISTRATION ProcessAddedCallbackRegistration;
static PH_CALLBACK_REGISTRATION ProcessRemovedCallbackRegistration;
static PH_CALLBACK_REGISTRATION ProcessesUpdatedCallbackRegistration;

static LIST_ENTRY ProcessListHead = { &ProcessListHead, &ProcessListHead };
static PVOID ExtendedProcesses;

static BOOL (WINAPI *getProcessDpiAwarenessInternal)(
    _In_ HANDLE hprocess,
    _Out_ ULONG *value
    );
static ULONG_PTR gfDPIAwareOffset;
#ifdef _WIN64
static ULONG_PTR gfDPIAwareOffset32;
#endif

static VOID DaepTreeNewMessageCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_TREENEW_MESSAGE message = Parameter;

    if (!message)
        return;

    switch (message->Message)
    {
    case TreeNewGetCellText:
        {
            PPH_TREENEW_GET_CELL_TEXT getCellText = message->Parameter1;
            PPH_PROCESS_NODE node;

            node = (PPH_PROCESS_NODE)getCellText->Node;

            switch (message->SubId)
            {
            case DAE_DPIAWARENESSEXT_COLUMN_ID:
                {
                    PPROCESS_EXTENSION extension;

                    extension = PhPluginGetObjectExtension(PluginInstance, node->ProcessItem, EmProcessItemType);
                    DaepUpdateDpiAwareness(node->ProcessItem, extension);
                    DaepUpdateDpiAwarenessDescription(extension);
                    PhInitializeStringRef(&getCellText->Text, extension->DpiAwarenessExtDescription);
                }
                break;
            }
        }
        break;
    }
}

static VOID DaepUpdateDpiAwarenessDescription(_In_ PPROCESS_EXTENSION Extension)
{
    PWSTR awarenessDesc, forcedState;
    if (Extension->ValidDescription)
        return;

    switch (Extension->DpiAwareness)
    {
    case DAE_DPI_AWARENESS_UNKNOWN:
        Extension->DpiAwarenessExtDescription[0] = L'\0';
        Extension->ValidDescription = TRUE;
        return;
    case DAE_DPI_AWARENESS_UNAWARE:
        awarenessDesc = L"Unaware";
        break;
    case DAE_DPI_AWARENESS_SYSTEM:
        awarenessDesc = L"System aware";
        break;
    case DAE_DPI_AWARENESS_PER_MONITOR:
        awarenessDesc = L"Per-monitor aware";
        break;
    }
    forcedState = DAE_TRIS_STR(Extension->DpiAwarenessForced);

    swprintf(Extension->DpiAwarenessExtDescription, sizeof(Extension->DpiAwarenessExtDescription) / sizeof(WCHAR),
        L"%s (F%s)", awarenessDesc, forcedState);
    Extension->ValidDescription = TRUE;
}

static LONG NTAPI DaepDpiAwarenessExtSortFunction(
    _In_ PVOID Node1,
    _In_ PVOID Node2,
    _In_ ULONG SubId,
    _In_ PH_SORT_ORDER SortOrder,
    _In_ PVOID Context
    )
{
    LONG cmp1;
    PPH_PROCESS_NODE node1 = Node1;
    PPH_PROCESS_NODE node2 = Node2;
    PPROCESS_EXTENSION extension1 = PhPluginGetObjectExtension(PluginInstance, node1->ProcessItem, EmProcessItemType);
    PPROCESS_EXTENSION extension2 = PhPluginGetObjectExtension(PluginInstance, node2->ProcessItem, EmProcessItemType);

    DaepUpdateDpiAwareness(node1->ProcessItem, extension1);
    DaepUpdateDpiAwareness(node2->ProcessItem, extension2);

    cmp1 = uintcmp(
        extension1->DpiAwareness,
        extension2->DpiAwareness);
    return cmp1 != 0 ? cmp1 : uintcmp(extension1->DpiAwarenessForced, extension2->DpiAwarenessForced);
}

static VOID DaepProcessTreeNewInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_TREENEW_INFORMATION info = Parameter;
    PH_TREENEW_COLUMN column;

    if (!info)
        return;

    memset(&column, 0, sizeof(PH_TREENEW_COLUMN));
    column.Text = L"DPI awareness extended";
    column.Width = 50;
    column.Alignment = PH_ALIGN_RIGHT;
    column.TextFlags = DT_RIGHT;

    PhPluginAddTreeNewColumn(PluginInstance, info->CmData, &column,
        DAE_DPIAWARENESSEXT_COLUMN_ID, NULL, DaepDpiAwarenessExtSortFunction);
}


VOID DaepProcessItemCreateCallback(
    _In_ PVOID Object,
    _In_ PH_EM_OBJECT_TYPE ObjectType,
    _In_ PVOID Extension)
{
    PPH_PROCESS_ITEM processItem = Object;
    PPROCESS_EXTENSION extension = Extension;

    memset(extension, 0, sizeof(PROCESS_EXTENSION));
    extension->ProcessItem = processItem;
}


VOID DaepProcessAddedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PROCESS_ITEM processItem = Parameter;

    if (!processItem)
        return;

    PPROCESS_EXTENSION extension = PhPluginGetObjectExtension(PluginInstance, processItem, EmProcessItemType);

    InsertTailList(&ProcessListHead, &extension->ListEntry);
}

VOID DaepProcessRemovedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PROCESS_ITEM processItem = Parameter;

    if (!processItem)
        return;

    PPROCESS_EXTENSION extension = PhPluginGetObjectExtension(PluginInstance, processItem, EmProcessItemType);

    RemoveEntryList(&extension->ListEntry);
}

static int __cdecl DaepProcessInfoPidCompare(
    _In_ const void *elem1,
    _In_ const void *elem2
)
{
    const SYSTEM_PROCESS_INFORMATION **pi1 = elem1;
    const SYSTEM_PROCESS_INFORMATION **pi2 = elem2;

    return uintptrcmp((ULONG_PTR)(*pi1)->UniqueProcessId, (ULONG_PTR)(*pi2)->UniqueProcessId);
}

static PSYSTEM_PROCESS_INFORMATION DaepLookupProcessInfo(
    _In_ PSYSTEM_PROCESS_INFORMATION *OrderedProcesses,
    _In_ HANDLE ProcessId,
    _In_ ULONG ProcessCount)
{
    PSYSTEM_PROCESS_INFORMATION proc;
    ULONG startIdx = 0, endIdx = ProcessCount - 1;
    ULONG mid;

    while (endIdx >= startIdx)
    {
        mid = (startIdx + endIdx) / 2;
        proc = OrderedProcesses[mid];
        if (proc->UniqueProcessId == ProcessId)
            return proc;
        else if (startIdx == endIdx)
            break;
        else if (proc->UniqueProcessId > ProcessId)
            endIdx = mid;
        else
            startIdx = mid + 1;
    }
    return NULL;
}

static VOID DaepProcessesUpdatedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
)
{
    PVOID processes;
    PSYSTEM_PROCESS_INFORMATION *orderedProcesses;
    ULONG processCount = 0;
    ULONG processIdx;
    if (!NT_SUCCESS(PhEnumProcessesEx(&processes, SystemExtendedProcessInformation)))
        return;

    for (PSYSTEM_PROCESS_INFORMATION process = PH_FIRST_PROCESS(processes); process; process = PH_NEXT_PROCESS(process))
    {
        processCount++;
    }
    orderedProcesses = calloc(processCount, sizeof(PSYSTEM_PROCESS_INFORMATION));
    if (!orderedProcesses)
    {
        PhFree(processes);
        return;
    }
    processIdx = 0;
    for (PSYSTEM_PROCESS_INFORMATION process = PH_FIRST_PROCESS(processes); process; (process = PH_NEXT_PROCESS(process)), processIdx++)
    {
        orderedProcesses[processIdx] = process;
    }

    qsort(orderedProcesses, processCount, sizeof(PSYSTEM_PROCESS_INFORMATION), DaepProcessInfoPidCompare);

    if (ExtendedProcesses)
        PhFree(ExtendedProcesses);
    ExtendedProcesses = processes;

    for (PLIST_ENTRY listEntry = ProcessListHead.Flink; listEntry != &ProcessListHead; listEntry = listEntry->Flink)
    {
        PPROCESS_EXTENSION extension = CONTAINING_RECORD(listEntry, PROCESS_EXTENSION, ListEntry);
        PSYSTEM_PROCESS_INFORMATION procInfo = DaepLookupProcessInfo(orderedProcesses, extension->ProcessItem->ProcessId, processCount);
        extension->ExtProcessInfo = procInfo;

        // The DPI awareness defaults to unaware if not set or declared in the manifest in which case
        // it can be changed once, so we can only be sure that it won't be changed again if it is different
        // from Unaware.
        if (extension->DpiAwareness == DAE_DPI_AWARENESS_UNKNOWN ||
            (extension->DpiAwareness == DAE_DPI_AWARENESS_UNAWARE && extension->DpiAwarenessForced != DAE_TRIS_TRUE))
            extension->ValidAwareness = FALSE;
    }

    free(orderedProcesses);
}

static ULONG DaepGetGfDPIAwareOffset32(
    /* user32VBase is the base the offset in the movzx is relative to,
     * either the current load address if relocations are performed, or
     * the default base if not. */
    _In_ ULONG User32VBase,
    _In_ PBYTE IsProcessDPIAware32)
{
    // 0FB605xxxxxxxx    movzx eax, gfDPIAware
    // C3                ret
    __try
    {
        if (IsProcessDPIAware32[0] == 0x0F && IsProcessDPIAware32[1] == 0xB6 &&
            IsProcessDPIAware32[2] == 0x05 && IsProcessDPIAware32[7] == 0xC3)
        {
            return *(PULONG)(IsProcessDPIAware32 + 3) - User32VBase;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
    return 0;
}

static ULONG_PTR DaepGetGfDPIAwareOffset64(
    _In_ PBYTE User32Base,
    _In_ PBYTE IsProcessDPIAware64)
{
    // 0FB605xxxxxxxx    movzx eax,byte [rel gfDPIAware]
    // C3                ret
    __try
    {
        if (IsProcessDPIAware64[0] == 0x0F && IsProcessDPIAware64[1] == 0xB6 &&
            IsProcessDPIAware64[2] == 0x05 && IsProcessDPIAware64[7] == 0xC3)
        {
            /* NB: x86_64 relative addressing is relative to start
                   of the following instruction, hence the + 7 */
            PBYTE gfDPIAwareOffsetAddr =
                PTR_ADD_OFFSET(
                    PTR_ADD_OFFSET(IsProcessDPIAware64, *(PULONG)(IsProcessDPIAware64 + 3)),
                    7);
            return gfDPIAwareOffsetAddr - User32Base;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
    return 0;
}

static void DaepInitGfDPIAwareOffset32WoW(_Out_ PULONG_PTR GfDPIAwareOffset32)
{
    PBYTE isProcessDPIAware32;
    PH_STRINGREF systemRoot;
    PPH_STRING user32FileName;
    PH_MAPPED_IMAGE mappedImage;
    PH_MAPPED_IMAGE_EXPORTS exports;
    PH_MAPPED_IMAGE_EXPORT_FUNCTION isProcessDPIAwareFun;
    NTSTATUS status;

    *GfDPIAwareOffset32 = 0;

    PhGetSystemRoot(&systemRoot);
    user32FileName = PhConcatStringRefZ(&systemRoot, L"\\SysWow64\\user32.dll");
    status = PhLoadMappedImage(user32FileName->Buffer, NULL, TRUE, &mappedImage);
    PhDereferenceObject(user32FileName);
    if (!NT_SUCCESS(status))
        return;

    if (!NT_SUCCESS(PhGetMappedImageExports(&exports, &mappedImage)))
        goto CleanupExit;

    if (!NT_SUCCESS(PhGetMappedImageExportFunction(
        &exports,
        "IsProcessDPIAware",
        0,
        &isProcessDPIAwareFun
        )) || !isProcessDPIAwareFun.Function)
        goto CleanupExit;
    isProcessDPIAware32 = PhMappedImageRvaToVa(
        &mappedImage,
        PtrToUlong(isProcessDPIAwareFun.Function),
        NULL);
    if (isProcessDPIAware32)
        *GfDPIAwareOffset32 = DaepGetGfDPIAwareOffset32(
            mappedImage.NtHeaders32->OptionalHeader.ImageBase,
            isProcessDPIAware32);

CleanupExit:
    PhUnloadMappedImage(&mappedImage);
}

#ifdef _WIN64
#define DaepGetGfDPIAwareOffset DaepGetGfDPIAwareOffset64
#else
#define DaepGetGfDPIAwareOffset(user32Base, isProcessDPIAware) DaepGetGfDPIAwareOffset32((ULONG)(user32Base), isProcessDPIAware)
#endif

static void DaepInitDpiAwarenessValues(
    _Out_ PVOID* GetProcessDpiAwarenessInternal,
#ifdef _WIN64
    _Out_ PULONG_PTR GfDPIAwareOffset32,
#endif
    _Out_ PULONG_PTR GfDPIAwareOffset
    )
{
    PBYTE isProcessDPIAware = 0;
    PVOID user32Base;

    *GetProcessDpiAwarenessInternal = NULL;
    *GfDPIAwareOffset = 0;
#ifdef _WIN64
    *GfDPIAwareOffset32 = 0;
#endif
    user32Base = PhGetDllHandle(L"user32.dll");
    if (!user32Base)
        return;
    if (*GetProcessDpiAwarenessInternal =
        PhGetProcedureAddress(user32Base, "GetProcessDpiAwarenessInternal", 0))
        return;
    isProcessDPIAware = PhGetProcedureAddress(user32Base, "IsProcessDPIAware", 0);

    if (isProcessDPIAware)
        *GfDPIAwareOffset = DaepGetGfDPIAwareOffset(user32Base, isProcessDPIAware);

#ifdef _WIN64
    DaepInitGfDPIAwareOffset32WoW(GfDPIAwareOffset32);
#endif
}

static ULONG DaepGetDpiAwareness(_In_ PPH_PROCESS_ITEM ProcessItem, _Inout_ PHANDLE VmReadHandle)
{
    ULONG dpiAwareness = DAE_DPI_AWARENESS_UNKNOWN;
    if (getProcessDpiAwarenessInternal)
    {
        ULONG winDpiAwareness;
        if (getProcessDpiAwarenessInternal(ProcessItem->QueryHandle, &winDpiAwareness))
            dpiAwareness = winDpiAwareness + 1;
    }
    else
    {
        ULONG_PTR curOffset;
#ifdef _WIN64
        if (ProcessItem->IsWow64)
            curOffset = gfDPIAwareOffset32;
        else
#endif
            curOffset = gfDPIAwareOffset;
        PH_STRINGREF user32sr = PH_STRINGREF_INIT(L"user32.dll");
        PVOID user32Base;
        BOOLEAN gfDPIAware;

        if (!*VmReadHandle)
        {
            if (!NT_SUCCESS(PhOpenProcess(VmReadHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, ProcessItem->ProcessId)))
                return DAE_DPI_AWARENESS_UNKNOWN;
        }
        if (!NT_SUCCESS(
            DaeGetDllBaseRemote(
                *VmReadHandle,
                &user32sr,
                &user32Base)) || !user32Base)
            return DAE_DPI_AWARENESS_UNKNOWN;
        if (!NT_SUCCESS(
            NtReadVirtualMemory(
                *VmReadHandle,
                PTR_ADD_OFFSET(user32Base, curOffset),
                &gfDPIAware,
                sizeof(BOOLEAN),
                NULL)))
            return DAE_DPI_AWARENESS_UNKNOWN;
        dpiAwareness = !!gfDPIAware + 1;
    }
    return dpiAwareness;
}

static ULONG DaepGetDpiAwarenessForced(_In_ PPROCESS_EXTENSION Extension, _Inout_ PHANDLE VmReadHandle)
{
    PSYSTEM_PROCESS_INFORMATION procInfo = Extension->ExtProcessInfo;
    if (!procInfo || procInfo->NumberOfThreads == 0)
        return DAE_TRIS_UNKNOWN;

    if (!*VmReadHandle)
    {
        if (!NT_SUCCESS(PhOpenProcess(VmReadHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, Extension->ProcessItem->ProcessId)))
            return DAE_TRIS_UNKNOWN;
    }

    for (ULONG i = 0; i < procInfo->NumberOfThreads; i++)
    {
        ULONG ciFlags = 0;
        PSYSTEM_EXTENDED_THREAD_INFORMATION threadInfo = (PSYSTEM_EXTENDED_THREAD_INFORMATION)procInfo->Threads + i;
        if (!threadInfo->TebBase)
            continue;
        if (!NT_SUCCESS(NtReadVirtualMemory(
                *VmReadHandle,
                PTR_ADD_OFFSET(threadInfo->TebBase, CI_FLAGS_TEB_OFFSET),
                &ciFlags,
                sizeof(ciFlags),
                NULL)))
            continue;
        if (ciFlags & CI_INITTHREAD)
            return (ciFlags & CI_FORCEDPIAWARE) ? DAE_TRIS_TRUE : DAE_TRIS_FALSE;
    }
    return DAE_TRIS_UNKNOWN;
}

static VOID DaepUpdateDpiAwareness(_In_ PPH_PROCESS_ITEM ProcessItem, _In_opt_ PPROCESS_EXTENSION Extension)
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    BOOL query_gfDPIAware32 = FALSE;
    BOOL query_gfDPIAware64 = FALSE;
    HANDLE vmReadHandle = NULL;

    if (!Extension)
        Extension = PhPluginGetObjectExtension(PluginInstance, ProcessItem, EmProcessItemType);

    if (PhBeginInitOnce(&initOnce))
    {
        DaepInitDpiAwarenessValues(
            (PVOID*)&getProcessDpiAwarenessInternal,
#ifdef _WIN64
            &gfDPIAwareOffset32,
#endif
            &gfDPIAwareOffset
        );
        PhEndInitOnce(&initOnce);
    }

    if (!getProcessDpiAwarenessInternal)
    {
#ifdef _WIN64
        query_gfDPIAware32 = ProcessItem->IsWow64 &&
            gfDPIAwareOffset32;
        query_gfDPIAware64 = !ProcessItem->IsWow64 &&
            gfDPIAwareOffset;
#else
        query_gfDPIAware32 = gfDPIAwareOffset;
#endif
        if (!query_gfDPIAware32 && !query_gfDPIAware64)
            return;
    }

    if (!Extension->ValidAwareness)
    {
        ULONG oldDpiAwareness = Extension->DpiAwareness;
        if (ProcessItem->QueryHandle)
        {
            Extension->DpiAwareness = DaepGetDpiAwareness(ProcessItem, &vmReadHandle);
        }

        Extension->ValidAwareness = TRUE;
        Extension->ValidDescription = Extension->ValidDescription && oldDpiAwareness == Extension->DpiAwareness;
    }

    if (!Extension->ValidForced)
    {
        ULONG oldDpiAwarenessForced = Extension->DpiAwarenessForced;
        if (ProcessItem->QueryHandle)
        {
            Extension->DpiAwarenessForced = DaepGetDpiAwarenessForced(Extension, &vmReadHandle);
        }

        Extension->ValidForced = TRUE;
        Extension->ValidDescription = Extension->ValidDescription && oldDpiAwarenessForced == Extension->DpiAwarenessForced;
    }

    if (vmReadHandle)
        NtClose(vmReadHandle);
}

LOGICAL DllMain(
    _In_ HINSTANCE Instance,
    _In_ ULONG Reason,
    _Reserved_ PVOID Reserved
    )
{
    if (Reason == DLL_PROCESS_ATTACH)
    {
        PPH_PLUGIN_INFORMATION info;

        PluginInstance = PhRegisterPlugin(L"poizan42.DpiAwarenessExtPlugin", Instance, &info);

        if (!PluginInstance)
            return FALSE;

        info->DisplayName = L"DPI Awareness Extras";
        info->Description = L"Adds a column to display extended DPI awareness information as well as information on Windows versions prior to Windows 8.1.";
        info->Author = L"poizan42";

        PhRegisterCallback(PhGetPluginCallback(PluginInstance, PluginCallbackTreeNewMessage),
            DaepTreeNewMessageCallback, NULL, &TreeNewMessageCallbackRegistration);
        PhRegisterCallback(PhGetGeneralCallback(GeneralCallbackProcessTreeNewInitializing),
            DaepProcessTreeNewInitializingCallback, NULL, &ProcessTreeNewInitializingCallbackRegistration);
        PhRegisterCallback(PhGetGeneralCallback(GeneralCallbackProcessProviderAddedEvent),
            DaepProcessAddedHandler, NULL, &ProcessAddedCallbackRegistration);
        PhRegisterCallback(PhGetGeneralCallback(GeneralCallbackProcessProviderRemovedEvent),
            DaepProcessRemovedHandler, NULL, &ProcessRemovedCallbackRegistration);
        PhRegisterCallback(PhGetGeneralCallback(GeneralCallbackProcessProviderUpdatedEvent),
            DaepProcessesUpdatedHandler, NULL, &ProcessesUpdatedCallbackRegistration);

        PhPluginSetObjectExtension(PluginInstance, EmProcessItemType, sizeof(PROCESS_EXTENSION),
            DaepProcessItemCreateCallback, NULL);
    }

    return TRUE;
}
