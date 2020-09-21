/*
 * Process Hacker Extra Plugins -
 *   Memory Extras Plugin
 *
 * Copyright (C) 2017 dmex
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
#include "..\..\plugins\include\commonutil.h"

PH_CALLBACK_DECLARE(DbgLoggedCallback);
static HANDLE PageTableThreadHandle = NULL;
static HWND PageTableDialogHandle = NULL;
static PH_EVENT InitializedEvent = PH_EVENT_INIT;

#define PAGE_SHIFT 12

typedef struct _PF_MEMORY_RANGE_INFO_V1
{
    ULONG Version;
    ULONG RangeCount;
    PF_PHYSICAL_MEMORY_RANGE Ranges[ANYSIZE_ARRAY];
} PF_MEMORY_RANGE_INFO_V1, *PPF_MEMORY_RANGE_INFO_V1;

typedef struct _PF_MEMORY_RANGE_INFO_V2
{
    ULONG Version;
    ULONG Flags;
    ULONG RangeCount;
    PF_PHYSICAL_MEMORY_RANGE Ranges[ANYSIZE_ARRAY];
} PF_MEMORY_RANGE_INFO_V2, *PPF_MEMORY_RANGE_INFO_V2;

typedef PF_MEMORY_RANGE_INFO_V2 PF_MEMORY_RANGE_INFO, *PPF_MEMORY_RANGE_INFO;

typedef struct _PF_FILE 
{
    ULONG FileKey;
    PPH_STRING FileName;
} PF_FILE, *PPF_FILE;

typedef struct _PF_PROCESS 
{
    ULONG_PTR ProcessKey;
    PPH_STRING ProcessName;
    ULONG PrivatePages;
    HANDLE ProcessId;
    ULONG SessionId;
    HANDLE ProcessHandle;
    PFS_PRIVATE_PAGE_SOURCE dads;
    PPH_LIST ProcessPfnList; 
} PF_PROCESS, *PPF_PROCESS;

// NL Log Entry Types
typedef enum _PFNL_ENTRY_TYPE 
{
    PfNLInfoTypeFile,
    PfNLInfoTypePfBacked,
    PfNLInfoTypeVolume,
    PfNLInfoTypeDelete,
    PfNLInfoTypeMax
} PFNL_ENTRY_TYPE;

// Header for NL Log Entry
typedef struct _PFNL_ENTRY_HEADER 
{
    PFNL_ENTRY_TYPE Type : 3;
    ULONG Size : 28;
    ULONG Timestamp;
    ULONG SequenceNumber;
} PFNL_ENTRY_HEADER, *PPFNL_ENTRY_HEADER;

// File Information NL Log Entry
typedef struct _PFNL_FILE_INFO 
{
    ULONG_PTR Key;
    ULONG VolumeKey; 
    ULONG unknown;
    ULONG VolumeSequenceNumber;
    ULONG Metafile : 1;
    ULONG FileRenamed : 1;
    ULONG PagingFile : 1;
#ifdef _WIN64
    ULONG Spare : 29;
    USHORT Something;
#else
    ULONG Spare : 13;
#endif
    USHORT NameLength;
    WCHAR Filename[ANYSIZE_ARRAY];
} PFNL_FILE_INFO, *PPFNL_FILE_INFO;

// Pagefile Backed Information NL Log Entry
typedef struct _PFNL_PFBACKED_INFO 
{
    ULONG Key;
    PVOID ProtoPteStart;
    PVOID ProtoPteEnd;
    USHORT NameLength;
    WCHAR SectionName[ANYSIZE_ARRAY];
} PFNL_PFBACKED_INFO, *PPFNL_PFBACKED_INFO;

// Volume Information NL Log Entry
typedef struct _PFNL_VOLUME_INFO 
{
    LARGE_INTEGER CreationTime;
    ULONG Key;
    ULONG SerialNumber;
    ULONG DeviceType : 4;
    ULONG DeviceFlags : 4;
    ULONG Reserved : 24;
    OBJECT_NAME_INFORMATION Path;
    WCHAR VolumePath[ANYSIZE_ARRAY];
} PFNL_VOLUME_INFO, *PPFNL_VOLUME_INFO;

// Delete Information NL Log Entry
typedef struct _PFNL_DELETE_ENTRY_INFO 
{
    union 
    {
        struct 
        {
            ULONG Type : 2;
            ULONG FileDeleted : 1;
            ULONG Spare : 28;
        };
        struct 
        {
            ULONG Reserved : 2;
            ULONG DeleteFlags : 29;
        };
    };
    ULONG Key;
} PFNL_DELETE_ENTRY_INFO, *PPFNL_DELETE_ENTRY_INFO;

//
// NL Log Entry
//
typedef struct _PFNL_LOG_ENTRY
{
    PFNL_ENTRY_HEADER Header;
    union
    {
        PFNL_FILE_INFO FileInfo;
        PFNL_PFBACKED_INFO PfBackedInfo;
        PFNL_VOLUME_INFO VolumeInfo;
        PFNL_DELETE_ENTRY_INFO DeleteEntryInfo;
    };
} PFNL_LOG_ENTRY, *PPFNL_LOG_ENTRY;

// Input Structure for IOCTL_PFFI_ENUMERATE
typedef struct _PFFI_ENUMERATE_INFO 
{
    ULONG Version;
    ULONG LogForETW : 1;
    ULONG LogForSuperfetch : 1;
    ULONG Reserved : 30;
    ULONG ETWLoggerId;
} PFFI_ENUMERATE_INFO, *PPFFI_ENUMERATE_INFO;

// Output Structure for IOCTL_PFFI_ENUMERATE
typedef struct _PFFI_UNKNOWN 
{
    USHORT SuperfetchVersion;
    USHORT FileinfoVersion;
    ULONG Tag;
    ULONG BufferSize;
    ULONG Always1;
    ULONG Always3;
    ULONG Reserved;
    ULONG Reserved2;
    ULONG AlignedSize;
    ULONG EntryCount;
    ULONG Reserved3;
} PFFI_UNKNOWN, *PPFFI_UNKNOWN;

// Mapping of page priority strings
PWCHAR Priorities[] =
{
    L"Idle",
    L"Very Low",
    L"Low",
    L"Background",
    L"Background",
    L"Normal",
    L"SuperFetch",
    L"SuperFetch"
};

typedef enum _MMLISTS
{
    ZeroedPageList = 0,
    FreePageList = 1,
    StandbyPageList = 2,
    ModifiedPageList = 3,
    ModifiedNoWritePageList = 4,
    BadPageList = 5,
    ActiveAndValid = 6,
    TransitionPage = 7
} MMLISTS;

// Mapping of page list short-strings
PWCHAR ShortPfnList[] =
{
    L"Zero",
    L"Free",
    L"Standby",
    L"Modified",
    L"Mod No-Write",
    L"Bad",
    L"Active",
    L"Transition"
};

typedef enum _MMPFNUSE
{
    ProcessPrivatePage,
    MemoryMappedFilePage,
    PageFileMappedPage,
    PageTablePage,
    PagedPoolPage,
    NonPagedPoolPage,
    SystemPTEPage,
    SessionPrivatePage,
    MetafilePage,
    AWEPage,
    DriverLockedPage,
    KernelStackPage
} MMPFNUSE;

// Mapping of page usage strings
PWSTR UseList[] =
{
    L"Process Private",
    L"Memory Mapped File",
    L"Page File Mapped",
    L"Page Table",
    L"Paged Pool",
    L"Non Paged Pool",
    L"System PTE",
    L"Session Private",
    L"Metafile",
    L"AWE Pages",
    L"Driver Lock Pages",
    L"Kernel Stack",
    L"Unknown"
};

typedef struct _PHYSICAL_MEMORY_RUN 
{
    SIZE_T BasePage;
    SIZE_T PageCount;
} PHYSICAL_MEMORY_RUN, *PPHYSICAL_MEMORY_RUN;

SIZE_T MmPageCounts[TransitionPage + 1];
SIZE_T MmUseCounts[MMPFNUSE_KERNELSTACK + 1];
SIZE_T MmPageUseCounts[MMPFNUSE_KERNELSTACK + 1][TransitionPage + 1];
PPF_PFN_PRIO_REQUEST MmPfnDatabase = NULL;
RTL_BITMAP MmVaBitmap, MmPfnBitMap;
ULONG MmPfnDatabaseSize;
HANDLE PfiFileInfoHandle = NULL;
PPF_MEMORY_RANGE_INFO MemoryRanges = NULL;
BOOLEAN IsLocalMemoryRange = FALSE;
PVOID BitMapBuffer = NULL;
PPH_LIST ProcessKeyList;
PPH_LIST FileKeyList;
PPH_LIST VolumeKeyList;
PPH_HASHTABLE ProcessKeyHashtable;
PPH_HASHTABLE FileKeyHashtable;
PPH_HASHTABLE VolumeKeyHashtable;

#define MI_GET_PFN(x) (PMMPFN_IDENTITY)(&MmPfnDatabase->PageData[(x)])
#define MI_PFN_IS_MISMATCHED(Pfn) (Pfn->u2.e1.Mismatch)
// Page-Rounding Macros
#define PAGE_ROUND_DOWN(x) (((ULONG_PTR)(x))&(~(PAGE_SIZE-1)))

NTSTATUS PfiQueryMemoryRanges(VOID)
{
    NTSTATUS status;
    SUPERFETCH_INFORMATION info;
    PF_MEMORY_RANGE_INFO rangeInfo;
    ULONG resultLength = 0;

    // Memory Ranges API was added in RTM, this is Version 1
    rangeInfo.Version = 2;

    info.Version = SUPERFETCH_INFORMATION_VERSION;
    info.Magic = SUPERFETCH_INFORMATION_MAGIC;
    info.Data = &rangeInfo;
    info.Length = sizeof(PF_MEMORY_RANGE_INFO);
    info.InfoClass = SuperfetchMemoryRangesQuery;

    status = NtQuerySystemInformation(
        SystemSuperfetchInformation,
        &info,
        sizeof(SUPERFETCH_INFORMATION),
        &resultLength
        );

    if (status == STATUS_BUFFER_TOO_SMALL)
    {
        MemoryRanges = PhAllocate(resultLength);
        memset(MemoryRanges, 0, resultLength);

        MemoryRanges->Version = 2;

        info.Version = SUPERFETCH_INFORMATION_VERSION;
        info.Magic = SUPERFETCH_INFORMATION_MAGIC;
        info.Data = MemoryRanges;
        info.Length = resultLength;
        info.InfoClass = SuperfetchMemoryRangesQuery;

        status = NtQuerySystemInformation(
            SystemSuperfetchInformation,
            &info,
            sizeof(SUPERFETCH_INFORMATION),
            &resultLength
            );
    }
    else
    {
        // Use local buffer
        MemoryRanges = &rangeInfo;
        IsLocalMemoryRange = TRUE;
    }

    return status;
}

NTSTATUS PfiInitializePfnDatabase(VOID)
{
    NTSTATUS status;
    SUPERFETCH_INFORMATION info;
    ULONG ResultLength = 0;
    PMMPFN_IDENTITY Pfn1;
    ULONG PfnCount, i, k;
    ULONG PfnOffset = 0;
    PPF_PFN_PRIO_REQUEST PfnDbStart;
    PPF_PHYSICAL_MEMORY_RANGE Node;
    SYSTEM_BASIC_INFORMATION basicInfo;

    NtQuerySystemInformation(
        SystemBasicInformation,
        &basicInfo,
        sizeof(SYSTEM_BASIC_INFORMATION),
        NULL
        );

    // Calculate maximum amount of memory required
    PfnCount = basicInfo.HighestPhysicalPageNumber + 1;
    
    // Build the PFN List Information Request
    MmPfnDatabaseSize = FIELD_OFFSET(PF_PFN_PRIO_REQUEST, PageData) + PfnCount * sizeof(MMPFN_IDENTITY);
    PfnDbStart = MmPfnDatabase = PhAllocate(MmPfnDatabaseSize);
    memset(MmPfnDatabase, 0, MmPfnDatabaseSize);

    MmPfnDatabase->Version = 1;
    MmPfnDatabase->RequestFlags = 1;

    info.Version = SUPERFETCH_INFORMATION_VERSION;
    info.Magic = SUPERFETCH_INFORMATION_MAGIC;
    info.Data = MmPfnDatabase;
    info.Length = MmPfnDatabaseSize;
    info.InfoClass = SuperfetchPfnQuery;

#if 1
    // Initial request, assume all bits valid
    for (ULONG i = 0; i < PfnCount; i++) 
    {
        // Get the PFN and write the physical page number
        Pfn1 = MI_GET_PFN(i);
        Pfn1->PageFrameIndex = i;
    }

    // Build a bitmap of pages
    BitMapBuffer = PhAllocate(PfnCount / 8); // leak
    RtlInitializeBitMap(&MmPfnBitMap, (PULONG)BitMapBuffer, PfnCount);
    RtlSetAllBits(&MmPfnBitMap);
    MmVaBitmap = MmPfnBitMap;
#endif

    // Loop all the ranges
    for (k = 0, i = 0; i < MemoryRanges->RangeCount; i++)
    {
        // Print information on the range
        Node = &MemoryRanges->Ranges[i];
        
        for (ULONG_PTR j = Node->BasePfn; j < (Node->BasePfn + Node->PageCount); j++)
        {
            // Get the PFN and write the physical page number
            Pfn1 = MI_GET_PFN(k++);
            Pfn1->PageFrameIndex = j;
        }
    }

    // Query all valid PFNs
    MmPfnDatabase->PfnCount = k;

    // Query the PFN Database
    status = NtQuerySystemInformation(
        SystemSuperfetchInformation,
        &info,
        sizeof(SUPERFETCH_INFORMATION),
        &ResultLength
        );

    return status;
}

PPF_PROCESS PfiFindProcess(
    _In_ ULONG_PTR UniqueProcessKey
    )
{
    // Sign-extend the key
#ifdef _WIN64
    UniqueProcessKey |= 0xFFFF000000000000;
#else
    UniqueProcessKey |= 0xFFFFFFFF00000000;
#endif

    PPF_PROCESS* foundProcessPtr = (PPF_PROCESS*)PhFindItemSimpleHashtable(
        ProcessKeyHashtable,
        (PVOID)UniqueProcessKey
        );

    if (foundProcessPtr)
    {
        return *foundProcessPtr;
    }

    return NULL;
}

NTSTATUS PfiQueryPrivateSources()
{
    NTSTATUS status;
    SUPERFETCH_INFORMATION info;
    PF_PRIVSOURCE_QUERY_REQUEST PrivateSourcesQuery = { 0 };
    PPF_PRIVSOURCE_QUERY_REQUEST request = NULL;
    ULONG resultLength = 0;

    PrivateSourcesQuery.Version = 8; // 3

    info.Version = SUPERFETCH_INFORMATION_VERSION;
    info.Magic = SUPERFETCH_INFORMATION_MAGIC;
    info.Data = &PrivateSourcesQuery;
    info.Length = sizeof(PF_PRIVSOURCE_QUERY_REQUEST);
    info.InfoClass = SuperfetchPrivSourceQuery;

    status = NtQuerySystemInformation(
        SystemSuperfetchInformation,
        &info,
        sizeof(SUPERFETCH_INFORMATION),
        &resultLength
        );

    if (status == STATUS_BUFFER_TOO_SMALL)
    {
        request = PhAllocate(resultLength);
        memset(request, 0, resultLength);

        request->Version = 8;

        info.Version = SUPERFETCH_INFORMATION_VERSION;
        info.Magic = SUPERFETCH_INFORMATION_MAGIC;
        info.Data = request;
        info.Length = resultLength;
        info.InfoClass = SuperfetchPrivSourceQuery;

        status = NtQuerySystemInformation(
            SystemSuperfetchInformation,
            &info,
            sizeof(SUPERFETCH_INFORMATION),
            &resultLength
            );
    }

    if (!NT_SUCCESS(status))
        return status;

    for (ULONG i = 0; i < request->InfoCount; i++)
    {
        PPF_PROCESS process;

        if (request->InfoArray[i].DbInfo.Type != PfsPrivateSourceProcess)
            continue;

        if (!(process = PfiFindProcess((ULONG_PTR)request->InfoArray[i].EProcess)))
        {
            process = PhAllocate(sizeof(PF_PROCESS) + request->InfoArray[i].WsPrivatePages * sizeof(ULONG));
            memset(process, 0, sizeof(PF_PROCESS) + request->InfoArray[i].WsPrivatePages * sizeof(ULONG));

            process->ProcessKey = (ULONG_PTR)request->InfoArray[i].EProcess;
            process->PrivatePages = (ULONG)request->InfoArray[i].WsPrivatePages;
            process->ProcessId = UlongToHandle(request->InfoArray[i].DbInfo.ProcessId);
            process->SessionId = request->InfoArray[i].SessionID;
            //process->ProcessPfnList = PhCreateList(1);

            PhAddItemList(ProcessKeyList, process);
            PhAddItemSimpleHashtable(ProcessKeyHashtable, (PVOID)request->InfoArray[i].EProcess, process);

            PPH_STRING processFileName = NULL;

            status = PhOpenProcess(
                &process->ProcessHandle,
                PROCESS_ALL_ACCESS,
                process->ProcessId
                );

            PhGetProcessImageFileNameByProcessId(process->ProcessId, &processFileName);

            // check ImageName
            //strncpy(process->ProcessName, request->InfoArray[i].ImageName, 16);

            if (processFileName)
            {
                PhMoveReference(&process->ProcessName, PhGetFileName(processFileName));
            }
            else
            {
                PhMoveReference(&process->ProcessName, PhConvertUtf8ToUtf16(request->InfoArray[i].ImageName));
            }
        }
    }

    PhFree(request);

    return status;
}

NTSTATUS PfSvFICommand(
    HANDLE DeviceHandle, 
    ULONG IoCtl, 
    PVOID a3, 
    int a4, 
    PVOID a5, 
    PULONG ResultLength)
{
    NTSTATUS status;
    IO_STATUS_BLOCK isb;

    status = NtDeviceIoControlFile(
        DeviceHandle,
        0,
        0,
        0,
        &isb,
        IoCtl,
        a3,
        a4,
        a5,
        *ResultLength
        );
    
    *ResultLength = (ULONG)isb.Information;

    return status;
}

PPF_FILE PfiFindFile(
    _In_ ULONGLONG UniqueFileKey
    )
{
    PPF_FILE* foundFilePtr;

    foundFilePtr = (PPF_FILE*)PhFindItemSimpleHashtable(
        FileKeyHashtable,
        (PVOID)UniqueFileKey
        );

    if (foundFilePtr)
    {
        return *foundFilePtr;
    }

    return NULL;
}

NTSTATUS PfiQueryFileInfo(VOID)
{
    PFFI_ENUMERATE_INFO request;
    PPFNL_LOG_ENTRY LogEntry;
    PPFFI_UNKNOWN LogHeader;
    PVOID OutputBuffer;
    ULONG OutputLength;
    NTSTATUS status;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK isb;
    UNICODE_STRING fileInfoUs;

    OutputLength = 16 * 1024 * 1024;
    OutputBuffer = PhAllocate(OutputLength);

    // Build the request
    RtlZeroMemory(&request, sizeof(PFFI_ENUMERATE_INFO));
    request.ETWLoggerId = 1;
    request.LogForETW = TRUE;
    request.LogForSuperfetch = TRUE;
    request.Version = 13;// 12 == win10_14393?

    RtlInitUnicodeString(&fileInfoUs, L"\\Device\\FileInfo");
    InitializeObjectAttributes(&oa, &fileInfoUs, OBJ_CASE_INSENSITIVE, NULL, NULL);

    if (!NT_SUCCESS(status = NtOpenFile(
        &PfiFileInfoHandle,
        FILE_GENERIC_READ,
        &oa,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_NON_DIRECTORY_FILE
        )))
    {
        return status;
    }

    if (!NT_SUCCESS(status = PfSvFICommand(
        PfiFileInfoHandle,
        0x22000F,
        &request,
        sizeof(PFFI_ENUMERATE_INFO),
        OutputBuffer,
        &OutputLength
        )))
    {
        NtClose(PfiFileInfoHandle);
        return status;
    }

    // Parse the log
    LogHeader = (PPFFI_UNKNOWN)OutputBuffer;
    LogEntry = (PPFNL_LOG_ENTRY)(LogHeader + 1);

    while (TRUE)
    {
        PPF_FILE file;

        if (!(file = PfiFindFile(LogEntry->FileInfo.Key)))
        {
            file = PhAllocate(sizeof(PF_FILE) + LogEntry->Header.Size - FIELD_OFFSET(PFNL_LOG_ENTRY, FileInfo));
            memset(file, 0, sizeof(PF_FILE) + LogEntry->Header.Size - FIELD_OFFSET(PFNL_LOG_ENTRY, FileInfo));

            if (LogEntry->Header.Type == PfNLInfoTypeVolume)
            {
                file->FileName = PhCreateString(LogEntry->VolumeInfo.VolumePath);
                file->FileKey = LogEntry->VolumeInfo.Key;
                
                PhAddItemSimpleHashtable(VolumeKeyHashtable, UlongToPtr(file->FileKey), file);
                //PhAddItemList(VolumeKeyList, file);
            }
            else if (LogEntry->Header.Type == PfNLInfoTypeFile)
            {
                file->FileName = PhCreateString(LogEntry->FileInfo.Filename);
                file->FileKey = (ULONG)LogEntry->FileInfo.Key;

                _wcslwr(file->FileName->Buffer);

                PPF_FILE* foundFilePtr = (PPF_FILE*)PhFindItemSimpleHashtable(
                    VolumeKeyHashtable,
                    UlongToPtr(LogEntry->FileInfo.VolumeKey)
                    );

                if (foundFilePtr)
                {
                    PhMoveReference(&file->FileName, PhConcatStringRef2(&(*foundFilePtr)->FileName->sr, &file->FileName->sr));
                }

                //NTSTATUS status;
                //HANDLE fileHandle;
                //PFILE_NAME_INFORMATION nameInfo;
                //IO_STATUS_BLOCK isb;
                //nameInfo = PhCreateAlloc(sizeof(FILE_NAME_INFORMATION));
                //memset(nameInfo, 0, sizeof(FILE_NAME_INFORMATION));
                //if (NT_SUCCESS(status = PhCreateFileWin32(
                //    &fileHandle,
                //    PhGetString(file->FileName),
                //    FILE_GENERIC_READ,
                //    0,
                //    FILE_SHARE_READ | FILE_SHARE_DELETE,
                //    FILE_OPEN,
                //    FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
                //    )))
                //{
                //    status = NtQueryInformationFile(
                //        fileHandle,
                //        &isb,
                //        nameInfo,
                //        sizeof(FILE_NAME_INFORMATION),
                //        FileNameInformation
                //        );
                //
                //    nameInfo = PhReAllocate(nameInfo, sizeof(FILE_NAME_INFORMATION) + nameInfo->FileNameLength);
                //    memset(nameInfo, 0, sizeof(FILE_NAME_INFORMATION) + nameInfo->FileNameLength);
                //
                //    if (NT_SUCCESS(status = NtQueryInformationFile(
                //        fileHandle,
                //        &isb,
                //        nameInfo,
                //        sizeof(FILE_NAME_INFORMATION) + nameInfo->FileNameLength,
                //        FileNameInformation
                //        )))
                //    {
                        PhMoveReference(&file->FileName, PhGetFileName(file->FileName));
                //    }
                //}
                //PhFree(nameInfo);

                PhAddItemList(FileKeyList, file);
                PhAddItemSimpleHashtable(FileKeyHashtable, (PVOID)LogEntry->FileInfo.Key, file);
            }
            else if (LogEntry->Header.Type == PfNLInfoTypePfBacked)
            {
                file->FileKey = LogEntry->PfBackedInfo.Key;
                file->FileName = PhCreateString(LogEntry->PfBackedInfo.SectionName);

                PhAddItemList(FileKeyList, file);
                PhAddItemSimpleHashtable(FileKeyHashtable, (PVOID)LogEntry->FileInfo.Key, file);
            }
            else
            {

            }
        }

        LogEntry = PTR_ADD_OFFSET(LogEntry, LogEntry->Header.Size);
        
        if (LogEntry >= (PPFNL_LOG_ENTRY)PTR_ADD_OFFSET(OutputBuffer, LogHeader->BufferSize))
            break;
    }

    PhFree(OutputBuffer);
    NtClose(PfiFileInfoHandle);

    return status;
}

NTSTATUS PfiQueryPfnDatabase(VOID)
{
    NTSTATUS status;
    PMMPFN_IDENTITY Pfn1;
    SUPERFETCH_INFORMATION superfetchInfo;
    ULONG resultLength = 0;

    // Build the Superfetch Query
    superfetchInfo.Version = SUPERFETCH_INFORMATION_VERSION;
    superfetchInfo.Magic = SUPERFETCH_INFORMATION_MAGIC;
    superfetchInfo.Data = MmPfnDatabase;
    superfetchInfo.Length = MmPfnDatabaseSize;
    superfetchInfo.InfoClass = SuperfetchPfnQuery;

    // Query the PFN Database
    status = NtQuerySystemInformation(
        SystemSuperfetchInformation,
        &superfetchInfo,
        sizeof(SUPERFETCH_INFORMATION),
        &resultLength
        );

    // Initialize page counts
    RtlZeroMemory(MmPageCounts, sizeof(MmPageCounts));
    RtlZeroMemory(MmUseCounts, sizeof(MmUseCounts));
    RtlZeroMemory(MmPageUseCounts, sizeof(MmPageUseCounts));

    // Loop the database
    for (ULONG i = 0; i < MmPfnDatabase->PfnCount; i++)
    {
        // Get the PFN
        Pfn1 = MI_GET_PFN(i);

        // Save the count
        MmPageCounts[Pfn1->u1.e1.ListDescription]++;

        // Save the usage
        MmUseCounts[Pfn1->u1.e1.UseDescription]++;

        // Save both
        MmPageUseCounts[Pfn1->u1.e1.UseDescription][Pfn1->u1.e1.ListDescription]++;

        // Is this a process page?
        if ((Pfn1->u1.e1.UseDescription == MMPFNUSE_PROCESSPRIVATE) && (Pfn1->u1.e4.UniqueProcessKey != 0))
        {
            // Get the process structure
            PPF_PROCESS Process;

TryAgain:
            Process = PfiFindProcess((ULONG_PTR)Pfn1->u1.e4.UniqueProcessKey);

            if (Process)
            {
                // Add this to the process' PFN array
                //PhAddItemList(Process->ProcessPfnList, Pfn1);

                //if (Process->ProcessPfnList->Count == Process->PrivatePages)
                {
                    // Our original estimate might be off, let's allocate some more PFNs
                    // PLIST_ENTRY PreviousEntry, NextEntry;
                    //PreviousEntry = Process->ProcessLinks.Blink;
                    //NextEntry = Process->ProcessLinks.Flink;
                    //Process = PhReAllocate(Process, sizeof(PF_PROCESS) + Process->PrivatePages * 2 * sizeof(ULONG));
                    //Process = HeapReAlloc(GetProcessHeap(), 0, Process, sizeof(PF_PROCESS) + Process->PrivatePages * 2 * sizeof(ULONG));
                    //Process->PrivatePages *= 2;
                    //PreviousEntry->Flink = NextEntry->Blink = &Process->ProcessLinks;
                }

                // One more PFN
                //Process->ProcessPfnCount++;
            }
            else
            {
                // The private sources changed during a query -- reload private sources
               PfiQueryPrivateSources();
               goto TryAgain;
            }
        }
    }

    return STATUS_SUCCESS;
}

VOID PfiDumpPfnProcUsages(PVOID Context)
{
    //PLIST_ENTRY NextEntry;
    //PPF_PROCESS Process;
    //
    //printf("Process         Session   PID       WorkingSet\n");
    //
    ////NextEntry = MmProcessListHead.Flink;
    //
    ////while (NextEntry != &MmProcessListHead) 
    //{
    //    Process = CONTAINING_RECORD(NextEntry, PF_PROCESS, ProcessLinks);
    //
    //    //printf("%-15s %7d %5d %6d (%7d KB)\n",
    //    //    Process->ProcessName,
    //    //    Process->SessionId,
    //    //    Process->ProcessId,
    //    //    Process->ProcessPfnCount,
    //    //    (Process->ProcessPfnCount << PAGE_SHIFT) >> 10);
    //    PPH_STRING name = PhConvertUtf8ToUtf16(Process->ProcessName);
    //
    //    INT index = PhAddListViewItem((HWND)Context, MAXINT, name->Buffer, NULL);
    //    //PhSetListViewSubItem((HWND)Context, index, 1, virtualAddressString);
    //    //PhSetListViewSubItem((HWND)Context, index, 2, UseList[pfnident->u1.e1.UseDescription]);
    //    //PhSetListViewSubItem((HWND)Context, index, 3, ShortPfnList[pfnident->u1.e1.ListDescription]);
    //    PhDereferenceObject(name);
    //
    //    //PhSetListViewSubItem((HWND)Context, index, 5, Priorities[pfnident->u1.e1.Priority]);
    //
    //    NextEntry = NextEntry->Flink;
    //}
}

VOID PfiDumpProcess(IN PPF_PROCESS Process)
{
    for (ULONG i = 0; i < Process->ProcessPfnList->Count; i++)
    {
        PMMPFN_IDENTITY Pfn1;
        PCHAR Type = "UNKNOWN";
        PCHAR Protect = "PAGE_NOACCESS";
        PCHAR Usage = "Private";
        SIZE_T ResultLength = 0;
        MEMORY_BASIC_INFORMATION MemoryBasicInfo;
        PPF_PROCESS Process;

        Pfn1 = MI_GET_PFN(i);
        //sprintf_s(VirtualAddress, "0x%08p", Pfn1->u2.VirtualAddress);

        Process = PfiFindProcess((ULONG_PTR)Pfn1->u1.e4.UniqueProcessKey);

        SIZE_T size = VirtualQueryEx(
            Process->ProcessHandle,
            (PVOID)Pfn1->u2.VirtualAddress,
            &MemoryBasicInfo,
            sizeof(MemoryBasicInfo)
            );

        if (size)
        {
            switch (MemoryBasicInfo.Type)
            {
            case MEM_IMAGE:
                Type = "MEM_IMAGE";
                break;

            case MEM_MAPPED:
                Type = "MEM_MAPPED";
                break;

            case MEM_PRIVATE:
                Type = "MEM_PRIVATE";
                break;
            }

            switch (MemoryBasicInfo.Protect)
            {
            case PAGE_EXECUTE:
                Protect = "PAGE_EXECUTE";
                break;

            case PAGE_READWRITE:
                Protect = "PAGE_READWRITE";
                break;

            case PAGE_READONLY:
                Protect = "PAGE_READONLY";
                break;

            case PAGE_EXECUTE_READWRITE:
                Protect = "PAGE_EXECUTE_READWRITE";
                break;
            }
        }

        //printf("0x%08p %-11s %d %-10s %-11s %-23s %-7s\n",
        //    Pfn1->PageFrameIndex << PAGE_SHIFT,
        //    ShortPfnList[Pfn1->u1.e1.ListDescription],
        //    (UCHAR)Pfn1->u1.e1.Priority,
        //    VirtualAddress,
        //    Type,
        //    Protect,
        //    Usage);
    }
}

PPF_PROCESS PfiLookupProcessByPid(IN HANDLE Pid)
{
    //PLIST_ENTRY NextEntry;
    //PPF_PROCESS FoundProcess;

    //NextEntry = MmProcessListHead.Flink;

    //while (NextEntry != &MmProcessListHead)
    //{
    //    FoundProcess = CONTAINING_RECORD(NextEntry, PF_PROCESS, ProcessLinks);
    //
    //    if (FoundProcess->ProcessId == Pid)
    //        return FoundProcess;
    //
    //    NextEntry = NextEntry->Flink;
    //}

    return NULL;
}

ULONG PfiConvertVaToPa(IN ULONG_PTR Va)
{
    ULONG i;
    PMMPFN_IDENTITY Pfn1 = NULL;
    ULONG_PTR AlignedVa = PAGE_ROUND_DOWN(Va);

    // Is the address in the VA bitmap?
    if (RtlTestBit(&MmVaBitmap, (ULONG)(AlignedVa >> PAGE_SHIFT)))
    {
        for (i = 0; i < MmPfnDatabase->PfnCount; i++)
        {
            Pfn1 = MI_GET_PFN(i);

            if (Pfn1->u2.VirtualAddress == AlignedVa)
                return i;
        }
    }

    return 0;
}

ULONG PfiGetIndexForPfn(IN ULONG Pfn)
{
    PMMPFN_IDENTITY Pfn1 = NULL;

    for (ULONG i = 0; i < MmPfnDatabase->PfnCount; i++)
    {
        Pfn1 = MI_GET_PFN(i);

        if (Pfn1->PageFrameIndex == Pfn)
            return i;
    }

    return 0;
}


VOID NTAPI DbgLoggedEventCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PROT_WINDOW_CONTEXT context = (PROT_WINDOW_CONTEXT)Context;

    if (context && context->WindowHandle)
    {
        PostMessage(context->WindowHandle, MEM_LOG_UPDATED, 0, 0);
    }
}

VOID DbgUpdateLogList(
    _Inout_ PROT_WINDOW_CONTEXT Context
    )
{
    Context->ListViewCount = Context->LogMessageList->Count;
    ListView_SetItemCountEx(Context->ListViewHandle, Context->ListViewCount, LVSICF_NOSCROLL);

    //if (Context->ListViewCount >= 2 && Button_GetCheck(Context->AutoScrollHandle) == BST_CHECKED)
    {
        if (ListView_IsItemVisible(Context->ListViewHandle, Context->ListViewCount - 2))
        {
            ListView_EnsureVisible(Context->ListViewHandle, Context->ListViewCount - 1, FALSE);
        }
    }
}

NTSTATUS EnumeratePageTable(
    _In_ PROT_WINDOW_CONTEXT Context
    )
{
    NTSTATUS status;
    BOOLEAN old;

    ProcessKeyList = PhCreateList(0x100);   
    FileKeyList = PhCreateList(0x1000);
    VolumeKeyList = PhCreateList(0x20);
    ProcessKeyHashtable = PhCreateSimpleHashtable(0x100);
    FileKeyHashtable = PhCreateSimpleHashtable(0x1000);
    VolumeKeyHashtable = PhCreateSimpleHashtable(0x20);

    if (!NT_SUCCESS(status = RtlAdjustPrivilege(SE_PROF_SINGLE_PROCESS_PRIVILEGE, TRUE, FALSE, &old)))
        goto CleanupExit;
    if (!NT_SUCCESS(status = RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, TRUE, FALSE, &old)))
        goto CleanupExit;
    if (!NT_SUCCESS(status = PfiQueryMemoryRanges()))
        goto CleanupExit;
    if (!NT_SUCCESS(status = PfiInitializePfnDatabase()))
        goto CleanupExit;
    if (!NT_SUCCESS(status = PfiQueryPrivateSources()))
        goto CleanupExit;

    PfiQueryFileInfo();

    if (NT_SUCCESS(status = PfiQueryPfnDatabase()))
    {
        for (ULONG i = 0; i < MmPfnDatabase->PfnCount; i++)
        {
            PPAGE_ENTRY entry;
            PMMPFN_IDENTITY pfnident = MI_GET_PFN(i);
            MMLISTS list = (MMLISTS)pfnident->u1.e1.ListDescription;
            MMPFNUSE page = (MMPFNUSE)pfnident->u1.e1.UseDescription;

            //if (list != BadPageList)
            //    continue;
            //if (list == ZeroedPageList)
            //    continue;
            //if (list == FreePageList)
            //    continue;
            //if (page == DriverLockedPage)
            //    continue;
            //if (page == NonPagedPoolPage)
            //    continue;
            
            entry = PhAllocate(sizeof(PAGE_ENTRY));
            memset(entry, 0, sizeof(PAGE_ENTRY));

            entry->Type = (ULONG)pfnident->u1.e1.UseDescription;
            entry->List = (ULONG)pfnident->u1.e1.ListDescription;
            entry->Priority = (ULONG)pfnident->u1.e1.Priority;
            entry->UniqueProcessKey = (ULONG_PTR)pfnident->u1.e4.UniqueProcessKey;
            entry->Address = (PVOID)(pfnident->PageFrameIndex << PAGE_SHIFT);

            if (!(entry->VirtualAddress = (PVOID)pfnident->u2.VirtualAddress))
                entry->VirtualAddress = entry->Address;

            if (pfnident->u1.e1.UseDescription == MMPFNUSE_PROCESSPRIVATE && pfnident->u1.e4.UniqueProcessKey)
            {
                PPF_PROCESS process = PfiFindProcess((ULONG_PTR)pfnident->u1.e4.UniqueProcessKey);

                if (process && process->ProcessName)
                {
                    entry->ProcessName = PhReferenceObject(process->ProcessName);
                }
            }

            if (pfnident->u1.e1.UseDescription == MMPFNUSE_FILE)
            {
                if ((pfnident->u2.FileObject & ~0x1))
                {
                    PPF_FILE file = PfiFindFile((pfnident->u2.FileObject & ~0x1));

                    if (file && file->FileName)
                    {
                        entry->FileName = PhReferenceObject(file->FileName);
                    }

                    //sprintf_s(Extra, "0x%07X", static_cast<ULONG>(Pfn1->u1.e2.Offset));
                }
            }

            PhAcquireQueuedLockExclusive(&Context->LogMessageListLock);
            PhAddItemList(Context->LogMessageList, entry);
            PhReleaseQueuedLockExclusive(&Context->LogMessageListLock);

            PhInvokeCallback(&DbgLoggedCallback, entry);  
   
            if (Context->LogMessageList->Count % 5000 == 0)
            {
                DbgUpdateLogList(Context);
            }
        }
    }

    DbgUpdateLogList(Context);

CleanupExit:

    if (!NT_SUCCESS(status))
    {
        PhShowStatus(Context->WindowHandle, L"Unable to query the PFN database", status, 0);
    }

    PhDereferenceObject(Context);

    return status;
}

INT_PTR CALLBACK RotViewDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PROT_WINDOW_CONTEXT context;

    if (uMsg == WM_INITDIALOG)
    {
        context = (PROT_WINDOW_CONTEXT)PhCreateAlloc(sizeof(ROT_WINDOW_CONTEXT));
        memset(context, 0, sizeof(ROT_WINDOW_CONTEXT));

        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);

        if (uMsg == WM_DESTROY)
        {
            PhSaveWindowPlacementToSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            PhDeleteLayoutManager(&context->LayoutManager);
            PhRemoveWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
            
            //PhDereferenceObject(MmPfnDatabase);      

            PhAcquireQueuedLockExclusive(&context->LogMessageListLock);
            for (ULONG i = 0; i < context->LogMessageList->Count; i++)
            {
                PPAGE_ENTRY entry = context->LogMessageList->Items[i];

                if (entry->ProcessName)
                    PhDereferenceObject(entry->ProcessName);
                if (entry->FileName)
                    PhDereferenceObject(entry->FileName);

                PhFree(entry);
            }
            for (ULONG i = 0; i < ProcessKeyList->Count; i++)
            {
                PPF_PROCESS entry = ProcessKeyList->Items[i];

                if (entry->ProcessName)
                    PhDereferenceObject(entry->ProcessName);
                if (entry->ProcessHandle)
                    NtClose(entry->ProcessHandle);

                PhFree(entry);
            }
            for (ULONG i = 0; i < FileKeyList->Count; i++)
            {
                PPF_FILE entry = FileKeyList->Items[i];

                if (entry->FileName)
                    PhDereferenceObject(entry->FileName);

                PhFree(entry);
            }

            if (BitMapBuffer)
                PhFree(BitMapBuffer);
            if (MmPfnDatabase)
                PhFree(MmPfnDatabase);
            if (MemoryRanges && !IsLocalMemoryRange)
                PhFree(MemoryRanges);
            PhReleaseQueuedLockExclusive(&context->LogMessageListLock);

            PhDereferenceObject(context);

            PostQuitMessage(0);
        }
    }

    if (!context)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            context->WindowHandle = hwndDlg;
            context->ListViewHandle = GetDlgItem(hwndDlg, IDC_LIST1);
            context->SearchboxHandle = GetDlgItem(hwndDlg, IDC_SEARCH);
            context->LogMessageList = PhCreateList(0x1000 * 0x1000);

            PhCreateSearchControl(hwndDlg, context->SearchboxHandle, L"Search (Ctrl+K)");

            PhSetListViewStyle(context->ListViewHandle, FALSE, TRUE);
            PhSetControlTheme(context->ListViewHandle, L"explorer");
            PhAddListViewColumn(context->ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 120, L"Address");
            PhAddListViewColumn(context->ListViewHandle, 1, 1, 1, LVCFMT_LEFT, 120, L"PFN List");
            PhAddListViewColumn(context->ListViewHandle, 2, 2, 2, LVCFMT_LEFT, 120, L"PFN List");
            PhAddListViewColumn(context->ListViewHandle, 3, 3, 3, LVCFMT_LEFT, 120, L"List");
            PhAddListViewColumn(context->ListViewHandle, 4, 4, 4, LVCFMT_LEFT, 400, L"List");
            PhSetExtendedListView(context->ListViewHandle);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, context->ListViewHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&context->LayoutManager, context->SearchboxHandle, NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_OPTIONS), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDC_REFRESH), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDCANCEL), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
            PhLoadWindowPlacementFromSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            
            DbgUpdateLogList(context);

            PhReferenceObject(context);

            PhCreateThread2(EnumeratePageTable, context);
        }
        break;    
    case MEM_LOG_UPDATED:
        DbgUpdateLogList(context);
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&context->LayoutManager);
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDCANCEL:
                DestroyWindow(hwndDlg);
                break;
            case IDC_REFRESH:
                {
                    DbgUpdateLogList(context);
                }
                break;
            }
        }
        break;
    case WM_SHOWDIALOG:
        {
            if (IsMinimized(hwndDlg))
                ShowWindow(hwndDlg, SW_RESTORE);
            else
                ShowWindow(hwndDlg, SW_SHOW);

            SetForegroundWindow(hwndDlg);
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR hdr = (LPNMHDR)lParam;

            switch (hdr->code)
            {
            case NM_RCLICK:
                {
                    //if (hdr->hwndFrom == context->ListViewHandle)
                    //ShowListViewMenu(context);
                }
                break;
            case NM_DBLCLK:
                {
                    //PDEBUG_LOG_ENTRY entry;
                    //INT index;
                    //index = PhFindListViewItemByFlags(context->ListViewHandle, -1, LVNI_SELECTED);
                    //if (index == -1)
                    //    break;
                    //entry = context->LogMessageList->Items[index];
                    //DialogBoxParam(
                    //    PluginInstance->DllBase,
                    //    MAKEINTRESOURCE(IDD_MESSAGE_DIALOG),
                    //    hwndDlg,
                    //    DbgPropDlgProc,
                    //    (LPARAM)entry
                    //);
                }
                break;
            case LVN_GETDISPINFO:
                {
                    NMLVDISPINFO* dispInfo = (NMLVDISPINFO*)hdr;
                    PPAGE_ENTRY entry;

                    PhAcquireQueuedLockShared(&context->LogMessageListLock);

                    entry = context->LogMessageList->Items[dispInfo->item.iItem];

                    //if (dispInfo->item.mask & LVIF_IMAGE)
                    //{
                    //    dispInfo->item.iImage = entry->ImageIndex;
                    //}

                    if (dispInfo->item.iSubItem == 0)
                    {
                        if (dispInfo->item.mask & LVIF_TEXT)
                        {
                            WCHAR virtualAddressString[PH_PTR_STR_LEN_1] = L"";

                            PhPrintPointer(virtualAddressString, entry->VirtualAddress);

                            wcsncpy_s(
                                dispInfo->item.pszText,
                                dispInfo->item.cchTextMax,
                                virtualAddressString,
                                _TRUNCATE
                                );
                        }
                    }
                    else if (dispInfo->item.iSubItem == 1)
                    {
                        if (dispInfo->item.mask & LVIF_TEXT)
                        {
                            if (dispInfo->item.mask & LVIF_TEXT)
                            {
                                wcsncpy_s(
                                    dispInfo->item.pszText,
                                    dispInfo->item.cchTextMax,
                                    UseList[entry->Type],
                                    _TRUNCATE
                                    );
                            }
                        }
                    }
                    else if (dispInfo->item.iSubItem == 2)
                    {
                        if (dispInfo->item.mask & LVIF_TEXT)
                        {
                            wcsncpy_s(
                                dispInfo->item.pszText,
                                dispInfo->item.cchTextMax,
                                ShortPfnList[entry->List],
                                _TRUNCATE
                                );
                        }
                    }
                    else if (dispInfo->item.iSubItem == 3)
                    {    
                        if (dispInfo->item.mask & LVIF_TEXT)
                        {
                            wcsncpy_s(
                                dispInfo->item.pszText,
                                dispInfo->item.cchTextMax,
                                Priorities[entry->Priority],
                                _TRUNCATE
                                );
                        }
                    }
                    else if (dispInfo->item.iSubItem == 4)
                    {
                        if (dispInfo->item.mask & LVIF_TEXT)
                        {
                            if (entry->FileName)
                            {
                                wcsncpy_s(
                                    dispInfo->item.pszText,
                                    dispInfo->item.cchTextMax,
                                    PhGetStringOrEmpty(entry->FileName),
                                    _TRUNCATE
                                    );
                            }
                            else if (entry->ProcessName)
                            {
                                wcsncpy_s(
                                    dispInfo->item.pszText,
                                    dispInfo->item.cchTextMax,
                                    PhGetStringOrEmpty(entry->ProcessName),
                                    _TRUNCATE
                                    );
                            }
                        }
                    }
                    else if (dispInfo->item.iSubItem == 5)
                    {

                        WCHAR addressString[PH_PTR_STR_LEN_1] = L"";

                        PhPrintPointer(addressString, entry->Address);

                        wcsncpy_s(
                            dispInfo->item.pszText,
                            dispInfo->item.cchTextMax,
                            addressString,
                            _TRUNCATE
                            );
                    }

                    PhReleaseQueuedLockShared(&context->LogMessageListLock);
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}

NTSTATUS PageTableDialogThread(
    _In_ PVOID Parameter
    )
{
    BOOL result;
    MSG message;
    PH_AUTO_POOL autoPool;

    PhInitializeAutoPool(&autoPool);

    PageTableDialogHandle = CreateDialog(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_PAGES),
        NULL,
        RotViewDlgProc
        );

    PhSetEvent(&InitializedEvent);

    while (result = GetMessage(&message, NULL, 0, 0))
    {
        if (result == -1)
            break;

        if (!IsDialogMessage(PageTableDialogHandle, &message))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        PhDrainAutoPool(&autoPool);
    }

    PhDeleteAutoPool(&autoPool);
    PhResetEvent(&InitializedEvent);

    if (PageTableThreadHandle)
    {
        NtClose(PageTableThreadHandle);
        PageTableThreadHandle = NULL;
    }

    return STATUS_SUCCESS;
}

VOID ShowPageTableWindow(VOID)
{
    if (!PageTableThreadHandle)
    {
        if (!NT_SUCCESS(PhCreateThreadEx(&PageTableThreadHandle, PageTableDialogThread, NULL)))
        {
            PhShowStatus(PhMainWndHandle, L"Unable to create the window.", 0, GetLastError());
            return;
        }

        PhWaitForEvent(&InitializedEvent, NULL);
    }

    PostMessage(PageTableDialogHandle, WM_SHOWDIALOG, 0, 0);
}
