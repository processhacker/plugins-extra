/*
* Process Hacker Extra Plugins -
*   Service Backup and Restore Plugin
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

static PH_STRINGREF servicesKeyName = PH_STRINGREF_INIT(L"System\\CurrentControlSet\\Services\\");
static PH_STRINGREF fileExtName = PH_STRINGREF_INIT(L".phservicebackup");

static PH_FILETYPE_FILTER filters[] =
{
    { L"Service Backup File (*.phservicebackup)", L"*.phservicebackup" },
};

VOID PhBackupService(
    _In_ HWND OwnerWindow,
    _In_ PPH_SERVICE_ITEM ServiceItem
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    HANDLE keyHandle = NULL;
    HANDLE fileHandle = NULL;
    PVOID fileDialog = NULL;
    PPH_STRING ofdFileName = NULL;
    PPH_STRING backupFileName = NULL;
    PPH_STRING serviceKeyName = NULL;

    backupFileName = PhConcatStringRef2(&ServiceItem->Name->sr, &fileExtName);
    serviceKeyName = PhConcatStringRef2(&servicesKeyName, &ServiceItem->Name->sr);

    __try
    {
        if (!NT_SUCCESS(status = PhOpenKey(
            &keyHandle,
            KEY_ALL_ACCESS, // KEY_READ,
            PH_KEY_LOCAL_MACHINE,
            &serviceKeyName->sr,
            0
            )))
        {
            __leave;
        }

        fileDialog = PhCreateSaveFileDialog();
        PhSetFileDialogFilter(fileDialog, filters, sizeof(filters) / sizeof(PH_FILETYPE_FILTER));
        PhSetFileDialogFileName(fileDialog, backupFileName->Buffer);

        if (!PhShowFileDialog(OwnerWindow, fileDialog))
            __leave;

        ofdFileName = PhGetFileDialogFileName(fileDialog);

        if (!NT_SUCCESS(status = PhCreateFileWin32(
            &fileHandle,
            ofdFileName->Buffer,
            FILE_GENERIC_WRITE | DELETE,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ,
            FILE_OVERWRITE_IF,
            FILE_NON_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT | FILE_SYNCHRONOUS_IO_NONALERT
            )))
        {
            __leave;
        }

        //RegCopyTreeW();

        if (!NT_SUCCESS(status = NtSaveKeyEx(
            keyHandle, 
            fileHandle, 
            PhGetIntegerSetting(SETTING_NAME_REG_FORMAT)
            )))
        {
            __leave;
        }
    }
    __finally
    {
        if (fileHandle)
        {
            NtClose(fileHandle);
        }

        if (keyHandle)
        {
            NtClose(keyHandle);
        }

        if (ofdFileName)
        {
            if (!NT_SUCCESS(status))
            {
                PhDeleteFileWin32(ofdFileName->Buffer);
            }

            PhDereferenceObject(ofdFileName);
        }

        if (fileDialog)
        {
            PhFreeFileDialog(fileDialog);
        }

        PhDereferenceObject(backupFileName);
        PhDereferenceObject(serviceKeyName);
    }

    if (!NT_SUCCESS(status))
    {
        PhShowStatus(OwnerWindow, L"Unable to backup the service", status, 0);
    }
}

VOID PhRestoreService(
    _In_ HWND OwnerWindow,
    _In_ PPH_SERVICE_ITEM ServiceItem
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    HANDLE keyHandle = NULL;
    HANDLE fileHandle = NULL;
    PVOID fileDialog = NULL;
    PPH_STRING ofdFileName = NULL;
    PPH_STRING backupFileName = NULL;
    PPH_STRING serviceKeyName = NULL;

    backupFileName = PhConcatStringRef2(&ServiceItem->Name->sr, &fileExtName);
    serviceKeyName = PhConcatStringRef2(&servicesKeyName, &ServiceItem->Name->sr);

    __try
    {
        if (!NT_SUCCESS(status = PhOpenKey(
            &keyHandle,
            KEY_ALL_ACCESS,// KEY_WRITE
            PH_KEY_LOCAL_MACHINE,
            &serviceKeyName->sr,
            0
            )))
        {
            __leave;
        }

        fileDialog = PhCreateOpenFileDialog();
        PhSetFileDialogFilter(fileDialog, filters, sizeof(filters) / sizeof(PH_FILETYPE_FILTER));

        if (!PhShowFileDialog(OwnerWindow, fileDialog))
            __leave;

        ofdFileName = PhGetFileDialogFileName(fileDialog);

        HANDLE appKeyHandle;

        if (NT_SUCCESS(PhLoadAppKey(
            &appKeyHandle,
            ofdFileName->Buffer,
            KEY_ALL_ACCESS,
            REG_APP_HIVE // REG_PROCESS_APPKEY
            )))
        {
            PPH_STRING displayName = PhQueryRegistryString(appKeyHandle, L"DisplayName");
            PPH_STRING currentDisplayName = PhQueryRegistryString(keyHandle, L"DisplayName");

            if (!PhEqualString(displayName, currentDisplayName, TRUE))
            {
                PhDereferenceObject(currentDisplayName);
                PhDereferenceObject(displayName);
                NtClose(appKeyHandle);
                PhShowError(OwnerWindow, L"%s", L"The display name does not match the backup of this service.");
                __leave;
            }

            PhDereferenceObject(currentDisplayName);
            PhDereferenceObject(displayName);
            NtClose(appKeyHandle);
        }

        if (PhFindStringInString(ofdFileName, 0, ServiceItem->Name->Buffer) == -1)
        {
           PhShowError(OwnerWindow, L"%s", L"The file name does not match the name of this service.");
            __leave;
        }

        if (!NT_SUCCESS(status = PhCreateFileWin32(
            &fileHandle,
            ofdFileName->Buffer,
            FILE_GENERIC_READ,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ,
            FILE_OPEN,
            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = NtRestoreKey(
            keyHandle,
            fileHandle,
            0 // REG_FORCE_RESTORE
            )))
        {
            __leave;
        }
    }
    __finally
    {
        if (fileHandle)
        {
            NtClose(fileHandle);
        }

        if (keyHandle)
        {
            NtClose(keyHandle);
        }

        if (ofdFileName)
        {
            PhDereferenceObject(ofdFileName);
        }

        if (fileDialog)
        {
            PhFreeFileDialog(fileDialog);
        }

        PhDereferenceObject(backupFileName);
        PhDereferenceObject(serviceKeyName);
    }

    if (!NT_SUCCESS(status))
    {
        PhShowStatus(OwnerWindow, L"Unable to restore the service", status, 0);
    }
}
