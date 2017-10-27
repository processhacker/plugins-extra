/*
 * Process Hacker Extra Plugins -
 *   Boot Entries Plugin
 *
 * Copyright (C) 2016-2017 dmex
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

#ifndef _BOOT_H_
#define _BOOT_H_

#define BOOT_ENTRIES_MENUITEM 1000
#define PLUGIN_NAME L"dmex.UefiFirmwareEntriesPlugin"
#define SETTING_NAME_WINDOW_POSITION (PLUGIN_NAME L".WindowPosition")
#define SETTING_NAME_WINDOW_SIZE (PLUGIN_NAME L".WindowSize")
#define SETTING_NAME_LISTVIEW_COLUMNS (PLUGIN_NAME L".ListViewColumns")

#define CINTERFACE
#define COBJMACROS
#define INITGUID
#include <phdk.h>
#include <phappresource.h>
#include <settings.h>
#include <hexedit.h>

#include <windowsx.h>
#include <stdint.h>
#include <cguid.h>

#include "resource.h"

extern PPH_PLUGIN PluginInstance;

typedef struct _UEFI_WINDOW_CONTEXT
{
    HWND ListViewHandle;
    PH_LAYOUT_MANAGER LayoutManager;
} UEFI_WINDOW_CONTEXT, *PUEFI_WINDOW_CONTEXT;

typedef struct _EFI_ENTRY
{
    ULONG Length;
    PPH_STRING Name;
    PPH_STRING GuidString;
} EFI_ENTRY, *PEFI_ENTRY;

INT_PTR CALLBACK UefiEntriesDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

NTSTATUS EnumerateFirmwareValues(
    _Out_ PVOID *Values
    );

BOOLEAN EfiSupported(
    VOID
    );

VOID ShowUefiEditorDialog(
    _In_ PEFI_ENTRY Entry
    );

// Windows types

typedef enum _SYSTEM_ENVIRONMENT_INFORMATION_CLASS
{
    SystemEnvironmentUnknownInformation,
    SystemEnvironmentNameInformation, // q: VARIABLE_NAME
    SystemEnvironmentValueInformation, // q: VARIABLE_NAME_AND_VALUE
    MaxSystemEnvironmentInfoClass
} SYSTEM_ENVIRONMENT_INFORMATION_CLASS;


typedef struct _VARIABLE_NAME 
{
    ULONG NextEntryOffset;
    GUID VendorGuid;
    WCHAR Name[ANYSIZE_ARRAY];
} VARIABLE_NAME, *PVARIABLE_NAME;

#define EFI_VARIABLE_NON_VOLATILE                             0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS                       0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS                           0x00000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD                    0x00000008
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS               0x00000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS    0x00000020
#define EFI_VARIABLE_APPEND_WRITE                             0x00000040

typedef struct _VARIABLE_NAME_AND_VALUE 
{
    ULONG NextEntryOffset;
    ULONG ValueOffset;
    ULONG ValueLength;
    ULONG Attributes;
    GUID VendorGuid;
    WCHAR Name[ANYSIZE_ARRAY];
} VARIABLE_NAME_AND_VALUE, *PVARIABLE_NAME_AND_VALUE;

#define PH_FIRST_EFI_VARIABLE(Variables) ((PVARIABLE_NAME_AND_VALUE)(Variables))
#define PH_NEXT_EFI_VARIABLE(Variable) ( \
    ((PVARIABLE_NAME_AND_VALUE)(Variable))->NextEntryOffset ? \
    (PVARIABLE_NAME_AND_VALUE)((PCHAR)(Variable) + \
    ((PVARIABLE_NAME_AND_VALUE)(Variable))->NextEntryOffset) : \
    NULL \
    )

#define PH_FIRST_BOOT_ENTRY(Variables) ((PBOOT_ENTRY_LIST)(Variables))
#define PH_NEXT_BOOT_ENTRY(Variable) ( \
    ((PBOOT_ENTRY_LIST)(Variable))->NextEntryOffset ? \
    (PBOOT_ENTRY_LIST)((PCHAR)(Variable) + \
    ((PBOOT_ENTRY_LIST)(Variable))->NextEntryOffset) : \
    NULL \
    )

typedef BOOLEAN (NTAPI *PPH_BOOT_ENTRY_CALLBACK)(
    _In_ PBOOT_ENTRY BootEntry,
    _In_ PVOID Context
    );

#define FILE_PATH_TYPE_MIN FILE_PATH_TYPE_ARC
#define FILE_PATH_TYPE_MAX FILE_PATH_TYPE_EFI

#define WINDOWS_OS_OPTIONS_VERSION 1
#define WINDOWS_OS_OPTIONS_SIGNATURE "WINDOWS"

typedef struct _WINDOWS_OS_OPTIONS
{
    UCHAR Signature[8];
    ULONG Version;
    ULONG Length;
    ULONG OsLoadPathOffset; //FILE_PATH OsLoadPath;
    WCHAR OsLoadOptions[1];
} WINDOWS_OS_OPTIONS, *PWINDOWS_OS_OPTIONS;

#endif _BOOT_H_