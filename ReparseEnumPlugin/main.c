/*
 * Process Hacker Extra Plugins -
 *   Reparse Plugin
 *
 * Copyright (C) 2020 dmex
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
#include <hndlinfo.h>

PPH_PLUGIN PluginInstance;
PH_CALLBACK_REGISTRATION PluginLoadCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginUnloadCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
PH_CALLBACK_REGISTRATION MainMenuInitializingCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginShowOptionsCallbackRegistration;

typedef BOOLEAN (NTAPI *PPH_ENUM_REPARSE_INDEX_FILE)(
    _In_ PFILE_REPARSE_POINT_INFORMATION Information,
    _In_ HANDLE RootDirectory,
    _In_opt_ PVOID Context
    );

typedef BOOLEAN (NTAPI *PPH_ENUM_OBJECTID_INDEX_FILE)(
    _In_ PFILE_OBJECTID_INFORMATION Information,
    _In_ HANDLE RootDirectory,
    _In_opt_ PVOID Context
    );

typedef struct _REPARSE_WINDOW_CONTEXT
{
    HWND ListViewHandle;
    BOOLEAN EnumReparsePoints;
    PH_LAYOUT_MANAGER LayoutManager;
    ULONG Count;
} REPARSE_WINDOW_CONTEXT, *PREPARSE_WINDOW_CONTEXT;

typedef struct _REPARSE_LISTVIEW_ENTRY
{
    LONGLONG FileReference;
    PPH_STRING RootDirectory;
} REPARSE_LISTVIEW_ENTRY, *PREPARSE_LISTVIEW_ENTRY;

NTSTATUS GetFileHandleName(
    _In_ HANDLE FileHandle,
    _Out_ PPH_STRING *FileName
    )
{
    NTSTATUS status;
    ULONG bufferSize;
    PFILE_NAME_INFORMATION buffer;
    IO_STATUS_BLOCK isb;

    bufferSize = sizeof(FILE_NAME_INFORMATION) + MAX_PATH;
    buffer = PhAllocateZero(bufferSize);

    status = NtQueryInformationFile(
        FileHandle,
        &isb,
        buffer,
        bufferSize,
        FileNameInformation
        );

    if (status == STATUS_BUFFER_OVERFLOW)
    {
        bufferSize = sizeof(FILE_NAME_INFORMATION) + buffer->FileNameLength + sizeof(UNICODE_NULL);
        PhFree(buffer);
        buffer = PhAllocateZero(bufferSize);

        status = NtQueryInformationFile(
            FileHandle,
            &isb,
            buffer,
            bufferSize,
            FileNameInformation
            );
    }

    if (!NT_SUCCESS(status))
    {
        PhFree(buffer);
        return status;
    }

    *FileName = PhCreateStringEx(buffer->FileName, buffer->FileNameLength);
    PhFree(buffer);

    return status;
}

NTSTATUS EnumerateVolumeReparsePoints(
    _In_ ULONG64 VolumeIndex,
    _In_ PPH_ENUM_REPARSE_INDEX_FILE Callback,
    _In_opt_ PVOID Context
    )
{
    NTSTATUS status;
    HANDLE fileHandle;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK isb;
    UNICODE_STRING fileInfoUs;
    PPH_STRING volumeName;

    volumeName = PhFormatString(L"\\Device\\HarddiskVolume%I64u\\$Extend\\$Reparse:$R:$INDEX_ALLOCATION", VolumeIndex);
    PhStringRefToUnicodeString(&volumeName->sr, &fileInfoUs);
    InitializeObjectAttributes(&oa, &fileInfoUs, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = NtOpenFile(
        &fileHandle,
        FILE_GENERIC_READ,
        &oa,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_SYNCHRONOUS_IO_NONALERT
        );

    if (NT_SUCCESS(status))
    {
        BOOLEAN firstTime = TRUE;
        PVOID buffer;
        ULONG bufferSize = 0x400;

        buffer = PhAllocate(bufferSize);

        while (TRUE)
        {
            while (TRUE)
            {
                status = NtQueryDirectoryFile(
                    fileHandle,
                    NULL,
                    NULL,
                    NULL,
                    &isb,
                    buffer,
                    bufferSize,
                    FileReparsePointInformation,
                    TRUE,
                    NULL,
                    firstTime
                    );

                if (status == STATUS_PENDING)
                {
                    status = NtWaitForSingleObject(fileHandle, FALSE, NULL);

                    if (NT_SUCCESS(status))
                        status = isb.Status;
                }

                if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_INFO_LENGTH_MISMATCH)
                {
                    PhFree(buffer);
                    bufferSize *= 2;
                    buffer = PhAllocate(bufferSize);
                }
                else
                {
                    break;
                }
            }

            if (status == STATUS_NO_MORE_FILES)
            {
                status = STATUS_SUCCESS;
                break;
            }

            if (!NT_SUCCESS(status))
                break;

            if (Callback)
            {
                if (!Callback(buffer, fileHandle, Context))
                    break;
            }

            firstTime = FALSE;
        }

        PhFree(buffer);
        NtClose(fileHandle);
    }

    return status;
}

NTSTATUS EnumerateVolumeObjectIds(
    _In_ ULONG64 VolumeIndex,
    _In_ PPH_ENUM_OBJECTID_INDEX_FILE Callback,
    _In_opt_ PVOID Context
    )
{
    NTSTATUS status;
    HANDLE fileHandle;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK isb;
    UNICODE_STRING fileInfoUs;
    PPH_STRING volumeName;

    volumeName = PhFormatString(L"\\Device\\HarddiskVolume%I64u\\$Extend\\$ObjId:$O:$INDEX_ALLOCATION", VolumeIndex);
    PhStringRefToUnicodeString(&volumeName->sr, &fileInfoUs);
    InitializeObjectAttributes(&oa, &fileInfoUs, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = NtOpenFile(
        &fileHandle,
        FILE_GENERIC_READ,
        &oa,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_SYNCHRONOUS_IO_NONALERT
        );

    if (NT_SUCCESS(status))
    {
        BOOLEAN firstTime = TRUE;
        PVOID buffer;
        ULONG bufferSize = 0x400;

        buffer = PhAllocate(bufferSize);

        while (TRUE)
        {
            while (TRUE)
            {
                status = NtQueryDirectoryFile(
                    fileHandle,
                    NULL,
                    NULL,
                    NULL,
                    &isb,
                    buffer,
                    bufferSize,
                    FileObjectIdInformation,
                    TRUE,
                    NULL,
                    firstTime
                    );

                if (status == STATUS_PENDING)
                {
                    status = NtWaitForSingleObject(fileHandle, FALSE, NULL);

                    if (NT_SUCCESS(status))
                        status = isb.Status;
                }

                if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_INFO_LENGTH_MISMATCH)
                {
                    PhFree(buffer);
                    bufferSize *= 2;
                    buffer = PhAllocate(bufferSize);
                }
                else
                {
                    break;
                }
            }

            if (status == STATUS_NO_MORE_FILES)
            {
                status = STATUS_SUCCESS;
                break;
            }

            if (!NT_SUCCESS(status))
                break;

            if (Callback)
            {
                if (!Callback(buffer, fileHandle, Context))
                    break;
            }

            firstTime = FALSE;
        }

        PhFree(buffer);
        NtClose(fileHandle);
    }

    return status;
}

NTSTATUS PhDeleteFileReparsePoint(
    _In_ PREPARSE_LISTVIEW_ENTRY Entry
    )
{
#define REPARSE_DATA_BUFFER_HEADER_SIZE UFIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)
    NTSTATUS status;
    HANDLE directoryHandle;
    HANDLE fileHandle;
    UNICODE_STRING fileNameUs;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK isb;
    PREPARSE_DATA_BUFFER reparseBuffer;
    ULONG reparseLength;

    PhStringRefToUnicodeString(&Entry->RootDirectory->sr, &fileNameUs);
    InitializeObjectAttributes(&oa, &fileNameUs, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = NtOpenFile(
        &directoryHandle,
        FILE_GENERIC_READ,
        &oa,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_SYNCHRONOUS_IO_NONALERT
        );

    if (!NT_SUCCESS(status))
        return status;

    fileNameUs.Length = sizeof(LONGLONG);
    fileNameUs.MaximumLength = sizeof(LONGLONG);
    fileNameUs.Buffer = (PWSTR)&Entry->FileReference;

    InitializeObjectAttributes(
        &oa,
        &fileNameUs,
        OBJ_CASE_INSENSITIVE,
        directoryHandle,
        NULL
        );

    status = NtOpenFile(
        &fileHandle,
        FILE_WRITE_ATTRIBUTES | SYNCHRONIZE,
        &oa,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_BY_FILE_ID | FILE_OPEN_REPARSE_POINT
        );

    if (!NT_SUCCESS(status))
    {
        NtClose(directoryHandle);
        return status;
    }

    reparseLength = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
    reparseBuffer = PhAllocate(reparseLength);

    status = NtFsControlFile(
        fileHandle,
        NULL,
        NULL,
        NULL,
        &isb,
        FSCTL_GET_REPARSE_POINT,
        NULL,
        0,
        reparseBuffer,
        reparseLength
        );

    if (!NT_SUCCESS(status))
    {
        NtClose(fileHandle);
        NtClose(directoryHandle);
        return status;
    }

    reparseBuffer->ReparseDataLength = 0;

    status = NtFsControlFile(
        fileHandle,
        NULL,
        NULL,
        NULL,
        &isb,
        FSCTL_DELETE_REPARSE_POINT,
        reparseBuffer,
        REPARSE_DATA_BUFFER_HEADER_SIZE,
        NULL,
        0
        );

    NtClose(fileHandle);
    NtClose(directoryHandle);

    return status;
}

NTSTATUS PhDeleteFileObjectId(
    _In_ PREPARSE_LISTVIEW_ENTRY Entry
    )
{
    NTSTATUS status;
    HANDLE directoryHandle;
    HANDLE fileHandle;
    UNICODE_STRING fileNameUs;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK isb;

    PhStringRefToUnicodeString(&Entry->RootDirectory->sr, &fileNameUs);
    InitializeObjectAttributes(&oa, &fileNameUs, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = NtOpenFile(
        &directoryHandle,
        FILE_GENERIC_READ,
        &oa,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_SYNCHRONOUS_IO_NONALERT
        );

    if (!NT_SUCCESS(status))
        return status;

    fileNameUs.Length = sizeof(LONGLONG);
    fileNameUs.MaximumLength = sizeof(LONGLONG);
    fileNameUs.Buffer = (PWSTR)&Entry->FileReference;

    InitializeObjectAttributes(
        &oa,
        &fileNameUs,
        OBJ_CASE_INSENSITIVE,
        directoryHandle,
        NULL
        );

    status = NtOpenFile(
        &fileHandle,
        FILE_WRITE_ATTRIBUTES | SYNCHRONIZE,
        &oa,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_BY_FILE_ID
        );

    if (!NT_SUCCESS(status))
    {
        NtClose(directoryHandle);
        return status;
    }

    status = NtFsControlFile(
        fileHandle,
        NULL,
        NULL,
        NULL,
        &isb,
        FSCTL_DELETE_OBJECT_ID,
        NULL,
        0,
        NULL,
        0
        );

    NtClose(fileHandle);
    NtClose(directoryHandle);

    return status;
}

PWSTR ReparseTagToString(
    _In_ ULONG Tag)
{
#ifndef IO_REPARSE_TAG_LX_SYMLINK
#define IO_REPARSE_TAG_LX_SYMLINK (0xA000001DL)
#endif
#ifndef IO_REPARSE_TAG_LX_FIFO
#define IO_REPARSE_TAG_LX_FIFO (0x80000024L)
#endif
#ifndef IO_REPARSE_TAG_LX_CHR
#define IO_REPARSE_TAG_LX_CHR (0x80000025L)
#endif
#ifndef IO_REPARSE_TAG_LX_BLK
#define IO_REPARSE_TAG_LX_BLK (0x80000026L)
#endif
#ifndef IO_REPARSE_TAG_DATALESS_CIM
#define IO_REPARSE_TAG_DATALESS_CIM (0xA0000028L)
#endif

    switch (Tag)
    {
    case IO_REPARSE_TAG_MOUNT_POINT:
        return L"MOUNT_POINT";
    case IO_REPARSE_TAG_HSM:
        return L"HSM";
    case IO_REPARSE_TAG_HSM2:
        return L"HSM2";
    case IO_REPARSE_TAG_SIS:
        return L"SIS";
    case IO_REPARSE_TAG_WIM:
        return L"WIM";
    case IO_REPARSE_TAG_CSV:
        return L"CSV";
    case IO_REPARSE_TAG_DFS:
        return L"DFS";
    case IO_REPARSE_TAG_SYMLINK:
        return L"SYMLINK";
    case IO_REPARSE_TAG_DFSR:
        return L"DFSR";
    case IO_REPARSE_TAG_DEDUP:
        return L"DEDUP";
    case IO_REPARSE_TAG_NFS:
        return L"NFS";
    case IO_REPARSE_TAG_FILE_PLACEHOLDER:
        return L"FILE_PLACEHOLDER";
    case IO_REPARSE_TAG_WOF:
        return L"WOF";
    case IO_REPARSE_TAG_WCI:
    case IO_REPARSE_TAG_WCI_1:
        return L"WCI";
    case IO_REPARSE_TAG_GLOBAL_REPARSE:
        return L"GLOBAL_REPARSE";
    case IO_REPARSE_TAG_CLOUD:
    case IO_REPARSE_TAG_CLOUD_1:
    case IO_REPARSE_TAG_CLOUD_2:
    case IO_REPARSE_TAG_CLOUD_3:
    case IO_REPARSE_TAG_CLOUD_4:
    case IO_REPARSE_TAG_CLOUD_5:
    case IO_REPARSE_TAG_CLOUD_6:
    case IO_REPARSE_TAG_CLOUD_7:
    case IO_REPARSE_TAG_CLOUD_8:
    case IO_REPARSE_TAG_CLOUD_9:
    case IO_REPARSE_TAG_CLOUD_A:
    case IO_REPARSE_TAG_CLOUD_B:
    case IO_REPARSE_TAG_CLOUD_C:
    case IO_REPARSE_TAG_CLOUD_D:
    case IO_REPARSE_TAG_CLOUD_E:
    case IO_REPARSE_TAG_CLOUD_F:
    case IO_REPARSE_TAG_CLOUD_MASK:
        return L"CLOUD";
    case IO_REPARSE_TAG_APPEXECLINK:
        return L"APPEXECLINK";
    case IO_REPARSE_TAG_PROJFS:
        return L"PROJFS";
    case IO_REPARSE_TAG_STORAGE_SYNC:
        return L"STORAGE_SYNC";
    case IO_REPARSE_TAG_WCI_TOMBSTONE:
        return L"WCI_TOMBSTONE";
    case IO_REPARSE_TAG_UNHANDLED:
        return L"UNHANDLED";
    case IO_REPARSE_TAG_ONEDRIVE:
        return L"ONEDRIVE";
    case IO_REPARSE_TAG_PROJFS_TOMBSTONE:
        return L"PROJFS_TOMBSTONE";
    case IO_REPARSE_TAG_AF_UNIX:
        return L"AF_UNIX";
    case IO_REPARSE_TAG_WCI_LINK:
    case IO_REPARSE_TAG_WCI_LINK_1:
        return L"WCI_LINK";
    case IO_REPARSE_TAG_DATALESS_CIM:
        return L"DATALESS_CIM";
    case IO_REPARSE_TAG_LX_SYMLINK:
        return L"LX_SYMLINK";
    case IO_REPARSE_TAG_LX_FIFO:
        return L"LX_FIFO";
    case IO_REPARSE_TAG_LX_CHR:
        return L"LX_CHR";
    case IO_REPARSE_TAG_LX_BLK:
        return L"LX_BLK";
    }

    return PhaFormatString(L"UNKNOWN: %lu", Tag)->Buffer;
}

BOOLEAN NTAPI EnumVolumeReparseCallback(
    _In_ PFILE_REPARSE_POINT_INFORMATION Information,
    _In_ HANDLE RootDirectory,
    _In_opt_ PVOID Context
    )
{
    PREPARSE_WINDOW_CONTEXT context = Context;
    NTSTATUS status;
    HANDLE reparseHandle;
    PPH_STRING objectName = NULL;
    PPH_STRING fileName = NULL;
    UNICODE_STRING fileNameUs;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK isb;

    fileNameUs.Length = sizeof(LONGLONG);
    fileNameUs.MaximumLength = sizeof(LONGLONG);
    fileNameUs.Buffer = (PWSTR)&Information->FileReference;

    InitializeObjectAttributes(
        &oa,
        &fileNameUs,
        OBJ_CASE_INSENSITIVE,
        RootDirectory,
        NULL
        );

    status = NtOpenFile(
        &reparseHandle,
        FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        &oa,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_BY_FILE_ID | FILE_OPEN_REPARSE_POINT
        );

    if (NT_SUCCESS(status))
    {
        if (NT_SUCCESS(PhGetHandleInformation(
            NtCurrentProcess(),
            reparseHandle,
            ULONG_MAX,
            NULL,
            NULL,
            NULL,
            &objectName
            )))
        {
            fileName = objectName;
        }

        //PREPARSE_DATA_BUFFER reparseBuffer;
        //ULONG reparseLength;
        //reparseLength = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
        //reparseBuffer = PhAllocate(reparseLength);
        //
        //if (NT_SUCCESS(NtFsControlFile(
        //    reparseHandle,
        //    NULL,
        //    NULL,
        //    NULL,
        //    &isb,
        //    FSCTL_GET_REPARSE_POINT,
        //    NULL,
        //    0,
        //    reparseBuffer,
        //    reparseLength
        //    )))
        //{
        //}
        //
        //PhFree(reparseBuffer);
        NtClose(reparseHandle);
    }

    if (context)
    {
        INT index;
        PREPARSE_LISTVIEW_ENTRY entry;
        PPH_STRING rootFileName = NULL;
        WCHAR number[PH_PTR_STR_LEN_1];

        objectName = NULL;

        if (NT_SUCCESS(PhGetHandleInformation(
            NtCurrentProcess(),
            RootDirectory,
            ULONG_MAX,
            NULL,
            NULL,
            &objectName,
            NULL
            )))
        {
            rootFileName = objectName;
        }

        entry = PhAllocate(sizeof(REPARSE_LISTVIEW_ENTRY));
        entry->FileReference = Information->FileReference;
        entry->RootDirectory = rootFileName;

        PhPrintUInt32(number, ++context->Count);
        index = PhAddListViewItem(context->ListViewHandle, MAXINT, number, entry);
        PhSetListViewSubItem(context->ListViewHandle, index, 1, PhaFormatUInt64(Information->FileReference, FALSE)->Buffer);
        PhSetListViewSubItem(context->ListViewHandle, index, 2, ReparseTagToString(Information->Tag));
        PhSetListViewSubItem(context->ListViewHandle, index, 3, PhGetStringOrEmpty(fileName));
    }

    PhClearReference(&fileName);

    return TRUE;
}

BOOLEAN NTAPI EnumVolumeObjectIdCallback(
    _In_ PFILE_OBJECTID_INFORMATION Information,
    _In_ HANDLE RootDirectory,
    _In_opt_ PVOID Context
    )
{
    PREPARSE_WINDOW_CONTEXT context = Context;
    NTSTATUS status;
    HANDLE reparseHandle;
    PPH_STRING objectName = NULL;
    PPH_STRING fileName = NULL;
    UNICODE_STRING fileNameUs;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK isb;

    fileNameUs.Length = sizeof(LONGLONG);
    fileNameUs.MaximumLength = sizeof(LONGLONG);
    fileNameUs.Buffer = (PWSTR)&Information->FileReference;

    InitializeObjectAttributes(
        &oa,
        &fileNameUs,
        OBJ_CASE_INSENSITIVE,
        RootDirectory,
        NULL
        );

    status = NtOpenFile(
        &reparseHandle,
        FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        &oa,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_BY_FILE_ID
        );

    if (NT_SUCCESS(status))
    {
        if (NT_SUCCESS(PhGetHandleInformation(
            NtCurrentProcess(),
            reparseHandle,
            ULONG_MAX,
            NULL,
            NULL,
            NULL,
            &objectName
            )))
        {
            fileName = objectName;
        }

        NtClose(reparseHandle);
    }

    if (context)
    {
        INT index;
        PREPARSE_LISTVIEW_ENTRY entry;
        PPH_STRING rootFileName = NULL;
        WCHAR number[PH_PTR_STR_LEN_1];

        if (NT_SUCCESS(PhGetHandleInformation(
            NtCurrentProcess(),
            RootDirectory,
            ULONG_MAX,
            NULL,
            NULL,
            &objectName,
            NULL
            )))
        {
            rootFileName = objectName;
        }

        entry = PhAllocate(sizeof(REPARSE_LISTVIEW_ENTRY));
        entry->FileReference = Information->FileReference;
        entry->RootDirectory = rootFileName;

        PhPrintUInt32(number, ++context->Count);
        index = PhAddListViewItem(context->ListViewHandle, MAXINT, number, entry);
        PhSetListViewSubItem(context->ListViewHandle, index, 1, PhaFormatUInt64(Information->FileReference, FALSE)->Buffer);

        {
            PPH_STRING string;
            PGUID guid = (PGUID)Information->ObjectId;

            // The swap returns the same value as 'fsutil objectid query filepath' (dmex)
            guid->Data1 = _byteswap_ulong(guid->Data1);
            guid->Data2 = _byteswap_ushort(guid->Data2);
            guid->Data3 = _byteswap_ushort(guid->Data3);

            string = PhFormatGuid(guid);
            PhSetListViewSubItem(context->ListViewHandle, index, 2, PhGetStringOrEmpty(string));
            PhDereferenceObject(string);
        }

        PhSetListViewSubItem(context->ListViewHandle, index, 3, PhGetStringOrEmpty(fileName));
    }

    PhClearReference(&fileName);

    return TRUE;
}

BOOLEAN NTAPI EnumDirectoryObjectsCallback(
    _In_ PPH_STRINGREF Name,
    _In_ PPH_STRINGREF TypeName,
    _In_opt_ PVOID Context
    )
{
    static PH_STRINGREF volumePath = PH_STRINGREF_INIT(L"HarddiskVolume");
    PREPARSE_WINDOW_CONTEXT context = Context;
    PH_STRINGREF stringBefore;
    PH_STRINGREF stringAfter;
    ULONG64 volumeIndex = ULLONG_MAX;

    if (context && PhStartsWithStringRef(Name, &volumePath, TRUE))
    {
        if (PhSplitStringRefAtString(Name, &volumePath, FALSE, &stringBefore, &stringAfter))
        {
            if (PhStringToInteger64(&stringAfter, 0, &volumeIndex))
            {
                if (context->EnumReparsePoints)
                {
                    EnumerateVolumeReparsePoints(volumeIndex, EnumVolumeReparseCallback, Context);
                }
                else
                {
                    EnumerateVolumeObjectIds(volumeIndex, EnumVolumeObjectIdCallback, Context);
                }
            }
        }
    }

    return TRUE;
}

NTSTATUS EnumerateVolumeDirectoryObjects(
    _In_opt_ PVOID Context
    )
{
    PREPARSE_WINDOW_CONTEXT context = Context;
    NTSTATUS status;
    HANDLE directoryHandle;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING name;

    RtlInitUnicodeString(&name, L"\\Device");
    InitializeObjectAttributes(
        &oa, 
        &name, 
        0, 
        NULL, 
        NULL
        );

    status = NtOpenDirectoryObject(
        &directoryHandle,
        DIRECTORY_QUERY,
        &oa
        );

    if (NT_SUCCESS(status))
    {
        if (context)
        {
            context->Count = 0;
        }

        PhEnumDirectoryObjects(
            directoryHandle, 
            EnumDirectoryObjectsCallback, 
            Context
            );

        NtClose(directoryHandle);
    }

    return status;
}

INT_PTR CALLBACK MainWindowDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PREPARSE_WINDOW_CONTEXT context;

    if (uMsg == WM_INITDIALOG)
    {
        context = PhAllocateZero(sizeof(REPARSE_WINDOW_CONTEXT));
        context->EnumReparsePoints = (BOOLEAN)lParam;

        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
    }

    if (!context)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            NTSTATUS status;

            context->ListViewHandle = GetDlgItem(hwndDlg, IDC_LIST);

            PhSetWindowText(hwndDlg, context->EnumReparsePoints ? L"NTFS Reparse Points" : L"NTFS Object Identifiers");
            PhSetApplicationWindowIcon(hwndDlg);

            PhCenterWindow(hwndDlg, PhMainWndHandle);
            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, context->ListViewHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDRETRY), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&context->LayoutManager, GetDlgItem(hwndDlg, IDCANCEL), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
            PhLoadWindowPlacementFromSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);

            PhSetListViewStyle(context->ListViewHandle, FALSE, TRUE);
            PhSetControlTheme(context->ListViewHandle, L"explorer");
            PhAddListViewColumn(context->ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 40, L"#");
            PhAddListViewColumn(context->ListViewHandle, 1, 1, 1, LVCFMT_LEFT, 100, L"File identifier");
            PhAddListViewColumn(context->ListViewHandle, 2, 2, 2, LVCFMT_LEFT, 100, context->EnumReparsePoints ? L"Reparse tag" : L"Object identifier");
            PhAddListViewColumn(context->ListViewHandle, 3, 3, 3, LVCFMT_LEFT, 250, L"Filename");
            PhSetExtendedListView(context->ListViewHandle);
            PhLoadListViewColumnsFromSetting(SETTING_NAME_LISTVIEW_COLUMNS, context->ListViewHandle);

            status = EnumerateVolumeDirectoryObjects(context);

            if (!NT_SUCCESS(status))
            {
                PhShowStatus(NULL, L"Unable to enumerate the objects.", status, 0);
            }
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&context->LayoutManager);
        break;
    case WM_DESTROY:
        {
            PhSaveWindowPlacementToSetting(SETTING_NAME_WINDOW_POSITION, SETTING_NAME_WINDOW_SIZE, hwndDlg);
            PhSaveListViewColumnsToSetting(SETTING_NAME_LISTVIEW_COLUMNS, context->ListViewHandle);
            PhDeleteLayoutManager(&context->LayoutManager);

            PhFree(context);
        }
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDCANCEL:
                EndDialog(hwndDlg, IDOK);
                break;
            case IDRETRY:
                {
                    NTSTATUS status;

                    ExtendedListView_SetRedraw(context->ListViewHandle, FALSE);
                    ListView_DeleteAllItems(context->ListViewHandle);
                    status = EnumerateVolumeDirectoryObjects(context);
                    ExtendedListView_SetRedraw(context->ListViewHandle, TRUE);

                    if (!NT_SUCCESS(status))
                    {
                        PhShowStatus(hwndDlg, L"Unable to enumerate the objects.", status, 0);
                    }
                }
                break;
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR hdr = (LPNMHDR)lParam;

            switch (hdr->code)
            {
            case NM_RCLICK:
                {
                    if (hdr->hwndFrom == context->ListViewHandle)
                    {
                        POINT point;
                        PPH_EMENU menu;
                        ULONG numberOfItems;
                        PVOID* listviewItems;
                        PPH_EMENU_ITEM selectedItem;

                        PhGetSelectedListViewItemParams(context->ListViewHandle, &listviewItems, &numberOfItems);

                        if (numberOfItems == 0)
                            break;

                        menu = PhCreateEMenu();
                        PhInsertEMenuItem(menu, PhCreateEMenuItem(0, 1, L"Remove", NULL, NULL), ULONG_MAX);
                        PhInsertEMenuItem(menu, PhCreateEMenuSeparator(), ULONG_MAX);
                        PhInsertEMenuItem(menu, PhCreateEMenuItem(0, USHRT_MAX, L"&Copy", NULL, NULL), ULONG_MAX);
                        PhInsertCopyListViewEMenuItem(menu, USHRT_MAX, context->ListViewHandle);

                        GetCursorPos(&point);
                        selectedItem = PhShowEMenu(
                            menu,
                            PhMainWndHandle,
                            PH_EMENU_SHOW_LEFTRIGHT,
                            PH_ALIGN_LEFT | PH_ALIGN_TOP,
                            point.x,
                            point.y
                            );

                        if (selectedItem && selectedItem->Id != -1)
                        {
                            switch (selectedItem->Id)
                            {
                            case 1:
                                {
                                    ExtendedListView_SetRedraw(context->ListViewHandle, FALSE);

                                    for (ULONG i = 0; i < numberOfItems; i++)
                                    {
                                        PREPARSE_LISTVIEW_ENTRY entry = listviewItems[i];
                                        INT index = PhFindListViewItemByParam(context->ListViewHandle, -1, entry);

                                        if (index != -1)
                                        {
                                            if (context->EnumReparsePoints)
                                            {
                                                NTSTATUS status;

                                                status = PhDeleteFileReparsePoint(entry);

                                                if (NT_SUCCESS(status))
                                                {
                                                    PhRemoveListViewItem(context->ListViewHandle, index);
                                                }
                                                else
                                                {
                                                    PhShowStatus(hwndDlg, L"Unable to remove the reparse point.", status, 0);
                                                }
                                            }
                                            else
                                            {
                                                NTSTATUS status;

                                                status = PhDeleteFileObjectId(entry);

                                                if (NT_SUCCESS(status))
                                                {
                                                    PhRemoveListViewItem(context->ListViewHandle, index);
                                                }
                                                else
                                                {
                                                    PhShowStatus(hwndDlg, L"Unable to remove the object identifier.", status, 0);
                                                }
                                            }
                                        }
                                    }

                                    ExtendedListView_SetRedraw(context->ListViewHandle, TRUE);
                                }
                                break;
                            case USHRT_MAX:
                                {
                                    PhCopyListView(context->ListViewHandle);
                                }
                                break;
                            }
                        }

                        PhDestroyEMenu(menu);

                    }
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}


VOID NTAPI LoadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    NOTHING;
}

VOID NTAPI UnloadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    NOTHING;
}

VOID NTAPI MainMenuInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_INFORMATION menuInfo = Parameter;
    PPH_EMENU_ITEM systemMenu;

    if (!menuInfo || menuInfo->u.MainMenu.SubMenuIndex != PH_MENU_ITEM_LOCATION_TOOLS)
        return;
    
    if (!(systemMenu = PhFindEMenuItem(menuInfo->Menu, 0, L"System", 0)))
    {
        PhInsertEMenuItem(menuInfo->Menu, PhCreateEMenuSeparator(), -1);
        PhInsertEMenuItem(menuInfo->Menu, systemMenu = PhPluginCreateEMenuItem(PluginInstance, 0, 0, L"&System", NULL), ULONG_MAX);
    }

    PhInsertEMenuItem(systemMenu, PhPluginCreateEMenuItem(PluginInstance, 0, 1, L"NTFS Reparse Points", NULL), ULONG_MAX);
    PhInsertEMenuItem(systemMenu, PhPluginCreateEMenuItem(PluginInstance, 0, 2, L"NTFS Object Identifiers", NULL), ULONG_MAX);
}

VOID NTAPI MenuItemCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_MENU_ITEM menuItem = (PPH_PLUGIN_MENU_ITEM)Parameter;

    if (!menuItem)
        return;

    switch (menuItem->Id)
    {
    case 1:
        {
            DialogBoxParam(
                PluginInstance->DllBase,
                MAKEINTRESOURCE(IDD_REPARSEDIALOG),
                NULL,
                MainWindowDlgProc,
                (LPARAM)TRUE
                );
        }
        break;
    case 2:
        {
            DialogBoxParam(
                PluginInstance->DllBase,
                MAKEINTRESOURCE(IDD_REPARSEDIALOG),
                NULL,
                MainWindowDlgProc,
                (LPARAM)FALSE
                );
        }
        break;
    }
}

LOGICAL DllMain(
    _In_ HINSTANCE Instance,
    _In_ ULONG Reason,
    _Reserved_ PVOID Reserved
    )
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        {
            PPH_PLUGIN_INFORMATION info;
            PH_SETTING_CREATE settings[] =
            {
                { IntegerPairSettingType, SETTING_NAME_WINDOW_POSITION, L"350,350" },
                { ScalableIntegerPairSettingType, SETTING_NAME_WINDOW_SIZE, L"@96|510,380" },
                { StringSettingType, SETTING_NAME_LISTVIEW_COLUMNS, L"" },
            };

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->Author = L"dmex";
            info->DisplayName = L"ReparseEnumPlugin";
            info->Description = L"ReparseEnumPlugin";
            info->HasOptions = FALSE;

            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackLoad),
                LoadCallback,
                NULL,
                &PluginLoadCallbackRegistration
                );

            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackUnload),
                UnloadCallback,
                NULL,
                &PluginUnloadCallbackRegistration
                );

            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackMainMenuInitializing),
                MainMenuInitializingCallback,
                NULL,
                &MainMenuInitializingCallbackRegistration
                );
            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackMenuItem),
                MenuItemCallback,
                NULL,
                &PluginMenuItemCallbackRegistration
                );

            PhAddSettings(settings, RTL_NUMBER_OF(settings));
        }
        break;
    }

    return TRUE;
}
