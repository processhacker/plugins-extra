/*
* Process Hacker Extra Plugins -
*   Plugin Manager
*
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
#include "miniz\miniz.h"

static ULONGLONG ParseVersionString(
    _In_ PPH_STRING Version
    )
{
    PH_STRINGREF remainingPart, majorPart, minorPart, revisionPart, reservedPart;
    ULONG64 majorInteger = 0, minorInteger = 0, revisionInteger = 0, reservedInteger = 0;

    PhInitializeStringRef(&remainingPart, PhGetString(Version));
    PhSplitStringRefAtChar(&remainingPart, '.', &majorPart, &remainingPart);
    PhSplitStringRefAtChar(&remainingPart, '.', &minorPart, &remainingPart);
    PhSplitStringRefAtChar(&remainingPart, '.', &revisionPart, &remainingPart);
    PhSplitStringRefAtChar(&remainingPart, '.', &reservedPart, &remainingPart);

    PhStringToInteger64(&majorPart, 10, &majorInteger);
    PhStringToInteger64(&minorPart, 10, &minorInteger);
    PhStringToInteger64(&revisionPart, 10, &revisionInteger);
    PhStringToInteger64(&reservedPart, 10, &reservedInteger);

    return MAKE_VERSION_ULONGLONG(majorInteger, minorInteger, reservedInteger, revisionInteger);
}

static json_object_ptr json_get_object(
    _In_ json_object_ptr Object, 
    _In_ PCSTR Key
    )
{
    json_object_ptr returnObj;

    if (json_object_object_get_ex(Object, Key, &returnObj))
    {
        return returnObj;
    }

    return NULL;
}

static BOOLEAN ReadRequestString(
    _In_ HINTERNET Handle,
    _Out_ _Deref_post_z_cap_(*DataLength) PSTR *Data,
    _Out_ ULONG *DataLength
    )
{
    BYTE buffer[PAGE_SIZE];
    PSTR data;
    ULONG allocatedLength;
    ULONG dataLength;
    ULONG returnLength;

    allocatedLength = sizeof(buffer);
    data = (PSTR)PhAllocate(allocatedLength);
    dataLength = 0;

    // Zero the buffer
    memset(buffer, 0, PAGE_SIZE);
    memset(data, 0, allocatedLength);

    while (WinHttpReadData(Handle, buffer, PAGE_SIZE, &returnLength))
    {
        if (returnLength == 0)
            break;

        if (allocatedLength < dataLength + returnLength)
        {
            allocatedLength *= 2;
            data = (PSTR)PhReAllocate(data, allocatedLength);
        }

        // Copy the returned buffer into our pointer
        memcpy(data + dataLength, buffer, returnLength);
        // Zero the returned buffer for the next loop
        //memset(buffer, 0, returnLength);

        dataLength += returnLength;
    }

    if (allocatedLength < dataLength + 1)
    {
        allocatedLength++;
        data = (PSTR)PhReAllocate(data, allocatedLength);
    }

    // Ensure that the buffer is null-terminated.
    data[dataLength] = 0;

    *DataLength = dataLength;
    *Data = data;

    return TRUE;
}

VOID EnumerateLoadedPlugins(
    _In_ PWCT_CONTEXT Context
    )
{
    PPH_AVL_LINKS root;
    PPH_AVL_LINKS links;

    root = PluginInstance->Links.Parent;

    while (root)
    {
        if (!root->Parent)
            break;

        root = root->Parent;
    }

    for (
        links = PhMinimumElementAvlTree((PPH_AVL_TREE)root); 
        links; 
        links = PhSuccessorElementAvlTree(links)
        )
    {
        PPHAPP_PLUGIN pluginInstance = CONTAINING_RECORD(links, PHAPP_PLUGIN, Links);
        PH_IMAGE_VERSION_INFO versionInfo;

        if (PhInitializeImageVersionInfo(&versionInfo, PhGetString(pluginInstance->FileName)))
        {
            SYSTEMTIME stUTC, stLocal;

            IO_STATUS_BLOCK isb;
            FILE_BASIC_INFORMATION basic = { 0 };

            HANDLE handle;

            if (NT_SUCCESS(PhCreateFileWin32(
                &handle, 
                PhGetString(pluginInstance->FileName), 
                FILE_GENERIC_READ, 
                FILE_ATTRIBUTE_NORMAL, 
                FILE_SHARE_READ, 
                FILE_OPEN, 
                FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
                )))
            {
                NtQueryInformationFile(
                    handle,
                    &isb,
                    &basic,
                    sizeof(FILE_BASIC_INFORMATION),
                    FileBasicInformation
                    );

                NtClose(handle);
            }




            PPLUGIN_NODE entry = PhCreateAlloc(sizeof(PLUGIN_NODE));
            memset(entry, 0, sizeof(PLUGIN_NODE));

            entry->State = PLUGIN_STATE_LOCAL;
            entry->PluginInstance = pluginInstance;
            entry->FilePath = PhCreateString2(&pluginInstance->FileName->sr);
            entry->InternalName = PhCreateString2(&pluginInstance->Name);
            entry->Name = PhCreateString(pluginInstance->Information.DisplayName);
            entry->Version = PhSubstring(versionInfo.FileVersion, 0, versionInfo.FileVersion->Length / 2 - 4);
            entry->Author = PhCreateString(pluginInstance->Information.Author);
            entry->Description = PhCreateString(pluginInstance->Information.Description);

            //entry->Id = PhConvertUtf8ToUtf16(json_object_get_string(plugin_id_object));
            //entry->Visible = PhConvertUtf8ToUtf16(json_object_get_string(plugin_visible_object));
            //entry->IconUrl = PhConvertUtf8ToUtf16(json_object_get_string(plugin_icon_object));
            /*PPH_STRING plugin_requirements = PhConvertUtf8ToUtf16(json_object_get_string(plugin_requirements_object));
            PPH_STRING plugin_feedback = PhConvertUtf8ToUtf16(json_object_get_string(plugin_feedback_object));
            PPH_STRING plugin_screenshots = PhConvertUtf8ToUtf16(json_object_get_string(plugin_screenshots_object));
            PPH_STRING plugin_datetime_added = PhConvertUtf8ToUtf16(json_object_get_string(plugin_datetime_added_object));
            PPH_STRING plugin_datetime_updated = PhConvertUtf8ToUtf16(json_object_get_string(plugin_datetime_updated_object));*/     
            /*PPH_STRING plugin_download_count = PhConvertUtf8ToUtf16(json_object_get_string(plugin_download_count_object));
            PPH_STRING plugin_download_link_32 = PhConvertUtf8ToUtf16(json_object_get_string(plugin_download_link_32_object));
            PPH_STRING plugin_download_link_64 = PhConvertUtf8ToUtf16(json_object_get_string(plugin_download_link_64_object));
            PPH_STRING plugin_sha2_32 = PhConvertUtf8ToUtf16(json_object_get_string(plugin_sha2_32_object));
            PPH_STRING plugin_sha2_64 = PhConvertUtf8ToUtf16(json_object_get_string(plugin_sha2_64_object));
            PPH_STRING plugin_hash = PhConvertUtf8ToUtf16(json_object_get_string(plugin_hash_object));*/

            PhLargeIntegerToSystemTime(&stUTC, &basic.LastWriteTime);
            SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
            entry->UpdatedTime = PhFormatDateTime(&stLocal);

            CloudAddChildWindowNode(&Context->TreeContext, entry);

            PhDeleteImageVersionInfo(&versionInfo);
        }
    }

    TreeNew_NodesStructured(Context->TreeNewHandle);
    TreeNew_AutoSizeColumn(Context->TreeNewHandle, TREE_COLUMN_ITEM_NAME, TN_AUTOSIZE_REMAINING_SPACE);
}

NTSTATUS QueryPluginsCallbackThread(
    _In_ PVOID Parameter
    )
{
    HINTERNET httpSessionHandle = NULL;
    HINTERNET httpConnectionHandle = NULL;
    HINTERNET httpRequestHandle = NULL;
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig = { 0 };
    ULONG xmlStringBufferLength = 0;
    PSTR xmlStringBuffer = NULL;
    json_object_ptr rootJsonObject;
    PWCT_CONTEXT context = Parameter;

    WinHttpGetIEProxyConfigForCurrentUser(&proxyConfig);

    if (!(httpSessionHandle = WinHttpOpen(
        L"PH",
        proxyConfig.lpszProxy ? WINHTTP_ACCESS_TYPE_NAMED_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        proxyConfig.lpszProxy,
        proxyConfig.lpszProxyBypass,
        0
        )))
    {
        goto CleanupExit;
    }

    if (!(httpConnectionHandle = WinHttpConnect(
        httpSessionHandle,
        L"wj32.org",
        INTERNET_DEFAULT_HTTP_PORT,
        0
        )))
    {
        goto CleanupExit;
    }

    if (!(httpRequestHandle = WinHttpOpenRequest(
        httpConnectionHandle,
        NULL,
        L"/processhacker/plugins/list.php",
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_REFRESH
        )))
    {
        goto CleanupExit;
    }

    if (!WinHttpSendRequest(
        httpRequestHandle,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH,
        0
        ))
    {
        goto CleanupExit;
    }

    if (!WinHttpReceiveResponse(httpRequestHandle, NULL))
        goto CleanupExit;

    if (!ReadRequestString(httpRequestHandle, &xmlStringBuffer, &xmlStringBufferLength))
        goto CleanupExit;

    if (xmlStringBuffer == NULL || xmlStringBuffer[0] == '\0')
        goto CleanupExit;

    if (!(rootJsonObject = json_tokener_parse(xmlStringBuffer)))
        goto CleanupExit;

    //TreeNew_SetRedraw(context->TreeNewHandle, TRUE);

    for (int i = 0; i < json_object_array_length(rootJsonObject); i++)
    {
        json_object_ptr jvalue = json_object_array_get_idx(rootJsonObject, i);
        json_object_ptr plugin_id_object = json_get_object(jvalue, "plugin_id");
        json_object_ptr plugin_visible_object = json_get_object(jvalue, "plugin_visible");
        json_object_ptr plugin_internal_name_object = json_get_object(jvalue, "plugin_internal_name");
        json_object_ptr plugin_name_object = json_get_object(jvalue, "plugin_name");
        json_object_ptr plugin_version_object = json_get_object(jvalue, "plugin_version");
        json_object_ptr plugin_author_object = json_get_object(jvalue, "plugin_author");
        json_object_ptr plugin_description_object = json_get_object(jvalue, "plugin_description");
        json_object_ptr plugin_icon_object = json_get_object(jvalue, "plugin_icon");
        json_object_ptr plugin_requirements_object = json_get_object(jvalue, "plugin_requirements");
        json_object_ptr plugin_feedback_object = json_get_object(jvalue, "plugin_feedback");
        json_object_ptr plugin_screenshots_object = json_get_object(jvalue, "plugin_screenshots");
        json_object_ptr plugin_datetime_added_object = json_get_object(jvalue, "plugin_datetime_added");
        json_object_ptr plugin_datetime_updated_object = json_get_object(jvalue, "plugin_datetime_updated");
        json_object_ptr plugin_download_count_object = json_get_object(jvalue, "plugin_download_count");
        json_object_ptr plugin_download_link_32_object = json_get_object(jvalue, "plugin_download_link_32");
        json_object_ptr plugin_download_link_64_object = json_get_object(jvalue, "plugin_download_link_64");
        json_object_ptr plugin_hash32_object = json_get_object(jvalue, "plugin_hash_32");
        json_object_ptr plugin_hash64_object = json_get_object(jvalue, "plugin_hash_64");
        json_object_ptr plugin_signed32_object = json_get_object(jvalue, "plugin_signed_32");
        json_object_ptr plugin_signed64_object = json_get_object(jvalue, "plugin_signed_64");

        SYSTEMTIME time = { 0 };
        SYSTEMTIME localTime = { 0 };
        PPLUGIN_NODE entry;
        
        entry = PhCreateAlloc(sizeof(PLUGIN_NODE));
        memset(entry, 0, sizeof(PLUGIN_NODE));

        entry->Id = PhConvertUtf8ToUtf16(json_object_get_string(plugin_id_object));
        entry->Visible = PhConvertUtf8ToUtf16(json_object_get_string(plugin_visible_object));
        entry->InternalName = PhConvertUtf8ToUtf16(json_object_get_string(plugin_internal_name_object));
        entry->Name = PhConvertUtf8ToUtf16(json_object_get_string(plugin_name_object));
        entry->Version = PhConvertUtf8ToUtf16(json_object_get_string(plugin_version_object));
        entry->Author = PhConvertUtf8ToUtf16(json_object_get_string(plugin_author_object));
        entry->Description = PhConvertUtf8ToUtf16(json_object_get_string(plugin_description_object));
        entry->IconUrl = PhConvertUtf8ToUtf16(json_object_get_string(plugin_icon_object));
        entry->Requirements = PhConvertUtf8ToUtf16(json_object_get_string(plugin_requirements_object));
        entry->FeedbackUrl = PhConvertUtf8ToUtf16(json_object_get_string(plugin_feedback_object));
        entry->Screenshots = PhConvertUtf8ToUtf16(json_object_get_string(plugin_screenshots_object));
        entry->AddedTime = PhConvertUtf8ToUtf16(json_object_get_string(plugin_datetime_added_object));
        entry->UpdatedTime = PhConvertUtf8ToUtf16(json_object_get_string(plugin_datetime_updated_object));
        entry->Download_count = PhConvertUtf8ToUtf16(json_object_get_string(plugin_download_count_object));
        entry->Download_link_32 = PhConvertUtf8ToUtf16(json_object_get_string(plugin_download_link_32_object));
        entry->Download_link_64 = PhConvertUtf8ToUtf16(json_object_get_string(plugin_download_link_64_object));
        entry->SHA2_32 = PhConvertUtf8ToUtf16(json_object_get_string(plugin_hash32_object));
        entry->SHA2_64 = PhConvertUtf8ToUtf16(json_object_get_string(plugin_hash64_object));
        entry->HASH_32 = PhConvertUtf8ToUtf16(json_object_get_string(plugin_signed32_object));
        entry->HASH_64 = PhConvertUtf8ToUtf16(json_object_get_string(plugin_signed64_object));

        swscanf(
            PhGetString(entry->UpdatedTime),
            L"%hu-%hu-%hu %hu:%hu:%hu",
            &time.wYear,
            &time.wMonth,
            &time.wDay,
            &time.wHour,
            &time.wMinute,
            &time.wSecond
            );

        if (SystemTimeToTzSpecificLocalTime(NULL, &time, &localTime))
        {
            entry->UpdatedTime = PhFormatDateTime(&localTime);
        }

        if (entry->PluginInstance = (PPHAPP_PLUGIN)PhFindPlugin(PhGetString(entry->InternalName)))
        {
            PH_IMAGE_VERSION_INFO versionInfo;

            entry->FilePath = PhCreateString2(&entry->PluginInstance->FileName->sr);

            if (PhInitializeImageVersionInfo(&versionInfo, PhGetString(entry->PluginInstance->FileName)))
            {
                ULONGLONG currentVersion = ParseVersionString(versionInfo.FileVersion);
                ULONGLONG latestVersion = ParseVersionString(entry->Version);

                if (currentVersion == latestVersion)
                {
                    // User is running the latest version
                    //PostMessage(context->DialogHandle, PH_UPDATEISCURRENT, 0, 0);
                    //entry->State = PLUGIN_STATE_LOCAL;
                }
                else if (currentVersion > latestVersion)
                {
                    // User is running a newer version
                    //PostMessage(context->DialogHandle, PH_UPDATENEWER, 0, 0);
                }
                else
                {
                    // User is running an older version
                    //PostMessage(context->DialogHandle, PH_UPDATEAVAILABLE, 0, 0);

                    entry->State = PLUGIN_STATE_UPDATE;

                    CloudAddChildWindowNode(&context->TreeContext, entry);
                }
            }
            else
            {
                //entry->State = PLUGIN_STATE_LOCAL;               
            }
        }
        else
        {
            entry->State = PLUGIN_STATE_REMOTE;

            CloudAddChildWindowNode(&context->TreeContext, entry);
        }  
    }

CleanupExit:

    if (httpRequestHandle)
        WinHttpCloseHandle(httpRequestHandle);

    if (httpConnectionHandle)
        WinHttpCloseHandle(httpConnectionHandle);

    if (httpSessionHandle)
        WinHttpCloseHandle(httpSessionHandle);

    if (xmlStringBuffer)
        PhFree(xmlStringBuffer);

    TreeNew_NodesStructured(context->TreeNewHandle);
    TreeNew_AutoSizeColumn(context->TreeNewHandle, TREE_COLUMN_ITEM_NAME, TN_AUTOSIZE_REMAINING_SPACE);


    if (context->DialogHandle)
    {
        ULONG count = 0;

        for (ULONG i = 0; i < context->TreeContext.NodeList->Count; i++)
        {
            PPLUGIN_NODE windowNode = context->TreeContext.NodeList->Items[i];

            if (windowNode->State == PLUGIN_STATE_UPDATE)
            {
                count++;
            }
        }

        if (count != 0)
        {
            SetWindowText(GetDlgItem(context->DialogHandle, IDC_UPDATES), PhFormatString(L"Updates (%u)", count)->Buffer);
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS SetupExtractBuild(
    _In_ PVOID Parameter
    )
{
    static PH_STRINGREF pluginsDirectory = PH_STRINGREF_INIT(L"plugins\\");

    mz_uint64 totalLength = 0;
    mz_uint64 total_size = 0;
    mz_uint64 downloadedBytes = 0;
    PPH_STRING totalSizeStr = NULL;
    mz_bool status = MZ_FALSE;
    mz_zip_archive zip_archive;
    PPH_UPDATER_CONTEXT context = (PPH_UPDATER_CONTEXT)Parameter;

    memset(&zip_archive, 0, sizeof(zip_archive));

    // TODO: Move existing folder items.

    if (!(status = mz_zip_reader_init_file(&zip_archive, PhGetStringOrEmpty(context->SetupFilePath), 0)))
    {
        goto error;
    }

    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++)
    {
        mz_zip_archive_file_stat file_stat;

        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
        {
            //mz_zip_reader_end(&zip_archive);
            break;
        }

        total_size += file_stat.m_uncomp_size;
     }

    totalSizeStr = PhFormatSize(total_size, -1);

    for (ULONG i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++)
    {
        mz_zip_archive_file_stat stat;

        if (!mz_zip_reader_file_stat(&zip_archive, i, &stat))
        {
            continue;
        }

        if (mz_zip_reader_is_file_a_directory(&zip_archive, i))
        {
            PPH_STRING directory;
            PPH_STRING fileName;
            PPH_STRING fullSetupPath;
            PPH_STRING extractPath;
            ULONG indexOfFileName = -1;

            fileName = PhConvertUtf8ToUtf16(stat.m_filename);
            directory = PhGetApplicationDirectory();
            extractPath = PhConcatStringRef3(&directory->sr, &pluginsDirectory, &fileName->sr);
            fullSetupPath = PhGetFullPath(PhGetStringOrEmpty(extractPath), &indexOfFileName);

            SHCreateDirectoryEx(NULL, PhGetStringOrEmpty(fullSetupPath), NULL);
        }
        else
        {
            PPH_STRING directory;
            PPH_STRING fileName;
            PPH_STRING fullSetupPath;
            PPH_STRING extractPath;
            PPH_STRING directoryPath;
            //PPH_STRING baseNameString;
            PPH_STRING fileNameString;
            ULONG indexOfFileName = -1;

            fileName = PhConvertUtf8ToUtf16(stat.m_filename);
            directory = PhGetApplicationDirectory();
            extractPath = PhConcatStringRef3(&directory->sr, &pluginsDirectory, &fileName->sr);
            fullSetupPath = PhGetFullPath(PhGetStringOrEmpty(extractPath), &indexOfFileName);
            //baseNameString = PhGetBaseName(fullSetupPath);
            //fullSetupPath = PhGetFullPath(PhGetStringOrEmpty(fileName), &indexOfFileName);
            fileNameString = PhConcatStrings(2, fullSetupPath->Buffer, L".bak");

            if (indexOfFileName != -1)
            {
                if (directoryPath = PhSubstring(fullSetupPath, 0, indexOfFileName))
                {
                    SHCreateDirectoryEx(NULL, PhGetStringOrEmpty(directoryPath), NULL);
                    PhDereferenceObject(directoryPath);
                }
            }

            if (RtlDoesFileExists_U(PhGetStringOrEmpty(fullSetupPath)))
            {
                MoveFileEx(PhGetString(fullSetupPath), PhGetString(fileNameString), MOVEFILE_REPLACE_EXISTING);
            }

            if (!mz_zip_reader_extract_to_file(&zip_archive, i, PhGetString(fullSetupPath), 0))
            {
                goto error;
            }

            PhDereferenceObject(fullSetupPath);
            PhDereferenceObject(extractPath);
        }
    }

    mz_zip_reader_end(&zip_archive);
    return STATUS_SUCCESS;

error:
    mz_zip_reader_end(&zip_archive);
    return STATUS_FAIL_CHECK;
}