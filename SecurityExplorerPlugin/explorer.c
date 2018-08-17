/*
 * Process Hacker Extra Plugins -
 *   LSA Security Explorer Plugin
 *
 * Copyright (C) 2013 wj32
 * Copyright (C) 2015-2016 dmex
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

#include "explorer.h"

HWND AccountsLv = NULL;
PPH_LIST AccountsList = NULL;
HWND PrivilegesLv = NULL;
PSID SelectedAccount = NULL;

VOID SxShowExplorer()
{
    PROPSHEETHEADER propSheetHeader = { sizeof(propSheetHeader) };
    PROPSHEETPAGE propSheetPage;
    HPROPSHEETPAGE pages[5];

    propSheetHeader.dwFlags =
        PSH_NOAPPLYNOW |
        PSH_NOCONTEXTHELP |
        PSH_PROPTITLE;
    propSheetHeader.hwndParent = PhMainWndHandle;
    propSheetHeader.pszCaption = L"Security";
    propSheetHeader.nPages = 0;
    propSheetHeader.nStartPage = 0;
    propSheetHeader.phpage = pages;

    // LSA page
    memset(&propSheetPage, 0, sizeof(PROPSHEETPAGE));
    propSheetPage.dwSize = sizeof(PROPSHEETPAGE);
    propSheetPage.hInstance = PluginInstance->DllBase;
    propSheetPage.pszTemplate = MAKEINTRESOURCE(IDD_LSA);
    propSheetPage.pfnDlgProc = SxLsaDlgProc;
    pages[propSheetHeader.nPages++] = CreatePropertySheetPage(&propSheetPage);

    // Sessions page
    memset(&propSheetPage, 0, sizeof(PROPSHEETPAGE));
    propSheetPage.dwSize = sizeof(PROPSHEETPAGE);
    propSheetPage.hInstance = PluginInstance->DllBase;
    propSheetPage.pszTemplate = MAKEINTRESOURCE(IDD_SESSIONS);
    propSheetPage.pfnDlgProc = SxSessionsDlgProc;
    pages[propSheetHeader.nPages++] = CreatePropertySheetPage(&propSheetPage);

    // Users page
    memset(&propSheetPage, 0, sizeof(PROPSHEETPAGE));
    propSheetPage.dwSize = sizeof(PROPSHEETPAGE);
    propSheetPage.hInstance = PluginInstance->DllBase;
    propSheetPage.pszTemplate = MAKEINTRESOURCE(IDD_USERS);
    propSheetPage.pfnDlgProc = SxUsersDlgProc;
    pages[propSheetHeader.nPages++] = CreatePropertySheetPage(&propSheetPage);

    // Groups page
    memset(&propSheetPage, 0, sizeof(PROPSHEETPAGE));
    propSheetPage.dwSize = sizeof(PROPSHEETPAGE);
    propSheetPage.hInstance = PluginInstance->DllBase;
    propSheetPage.pszTemplate = MAKEINTRESOURCE(IDD_GROUPS);
    propSheetPage.pfnDlgProc = SxGroupsDlgProc;
    pages[propSheetHeader.nPages++] = CreatePropertySheetPage(&propSheetPage);

    // Credentials page
    memset(&propSheetPage, 0, sizeof(PROPSHEETPAGE));
    propSheetPage.dwSize = sizeof(PROPSHEETPAGE);
    propSheetPage.hInstance = PluginInstance->DllBase;
    propSheetPage.pszTemplate = MAKEINTRESOURCE(IDD_CREDENTIALS);
    propSheetPage.pfnDlgProc = SxCredentialsDlgProc;
    pages[propSheetHeader.nPages++] = CreatePropertySheetPage(&propSheetPage);

    PropertySheet(&propSheetHeader);
}

VOID SxpFreeAccounts()
{
    if (AccountsList)
    {
        for (ULONG i = 0; i < AccountsList->Count; i++)
            PhFree(AccountsList->Items[i]);

        PhClearList(AccountsList);
    }
}

VOID SxpRefreshAccounts()
{
    LSA_HANDLE policyHandle;
    LSA_ENUMERATION_HANDLE enumerationHandle = 0;
    PLSA_ENUMERATION_INFORMATION accounts;
    ULONG numberOfAccounts;

    if (AccountsList)
    {
        SxpFreeAccounts();
    }
    else
    {
        AccountsList = PhCreateList(40);
    }

    ListView_DeleteAllItems(AccountsLv);

    if (NT_SUCCESS(PhOpenLsaPolicy(&policyHandle, POLICY_VIEW_LOCAL_INFORMATION, NULL)))
    {
        while (NT_SUCCESS(LsaEnumerateAccounts(
            policyHandle,
            &enumerationHandle,
            &accounts,
            0x100,
            &numberOfAccounts
            )))
        {
            for (ULONG i = 0; i < numberOfAccounts; i++)
            {
                INT lvItemIndex;
                PSID sid;
                PPH_STRING name;
                PPH_STRING sidString;

                sid = PhAllocateCopy(accounts[i].Sid, RtlLengthSid(accounts[i].Sid));
                PhAddItemList(AccountsList, sid);

                name = PH_AUTO(PhGetSidFullName(sid, TRUE, NULL));
                lvItemIndex = PhAddListViewItem(AccountsLv, MAXINT, PhGetStringOrDefault(name, L"(unknown)"), sid);

                sidString = PH_AUTO(PhSidToStringSid(sid));
                PhSetListViewSubItem(AccountsLv, lvItemIndex, 1, PhGetStringOrDefault(sidString, L"(unknown)"));
            }

            LsaFreeMemory(accounts);
        }

        LsaClose(policyHandle);
    }

    ExtendedListView_SortItems(AccountsLv);
}

VOID SxpRefreshPrivileges()
{
    LSA_HANDLE policyHandle;
    LSA_ENUMERATION_HANDLE enumerationHandle = 0;
    PPOLICY_PRIVILEGE_DEFINITION privileges;
    ULONG numberOfPrivileges;

    ListView_DeleteAllItems(PrivilegesLv);

    if (NT_SUCCESS(PhOpenLsaPolicy(&policyHandle, POLICY_VIEW_LOCAL_INFORMATION, NULL)))
    {
        while (NT_SUCCESS(LsaEnumeratePrivileges(
            policyHandle,
            &enumerationHandle,
            &privileges,
            0x100,
            &numberOfPrivileges
            )))
        {
            for (ULONG i = 0; i < numberOfPrivileges; i++)
            {
                INT lvItemIndex;
                PPH_STRING name;
                PPH_STRING displayName;

                name = PhCreateStringEx(privileges[i].Name.Buffer, privileges[i].Name.Length);
                lvItemIndex = PhAddListViewItem(PrivilegesLv, MAXINT, name->Buffer, NULL);

                if (PhLookupPrivilegeDisplayName(&name->sr, &displayName))
                {
                    PhSetListViewSubItem(PrivilegesLv, lvItemIndex, 1, displayName->Buffer);
                    PhDereferenceObject(displayName);
                }

                PhDereferenceObject(name);
            }

            LsaFreeMemory(privileges);
        }

        LsaClose(policyHandle);
    }

    ExtendedListView_SortItems(PrivilegesLv);
}

VOID SxpRefreshSessions(
    _In_ HWND ListViewHandle
    )
{
    ULONG logonSessionCount = 0;
    PLUID logonSessionList = NULL;

    if (AccountsList)
    {
        SxpFreeAccounts();
    }
    else
    {
        AccountsList = PhCreateList(40);
    }

    ListView_DeleteAllItems(ListViewHandle);

    if (NT_SUCCESS(LsaEnumerateLogonSessions(
        &logonSessionCount,
        &logonSessionList
        )))
    {
        for (ULONG i = 0; i < logonSessionCount; i++)
        {
            PSECURITY_LOGON_SESSION_DATA logonSessionData;

            if (NT_SUCCESS(LsaGetLogonSessionData(&logonSessionList[i], &logonSessionData)))
            {
                WCHAR logonSessionLuid[PH_INT64_STR_LEN_1];

                if (RtlValidSid(logonSessionData->Sid))
                {
                    INT lvItemIndex;
                    PSID sid = NULL;
                    PPH_STRING name;
                    PPH_STRING sidString;

                    sid = PhAllocateCopy(logonSessionData->Sid, RtlLengthSid(logonSessionData->Sid));
                    PhAddItemList(AccountsList, sid);

                    PhPrintPointer(logonSessionLuid, UlongToPtr(logonSessionData->LogonId.LowPart));
                    lvItemIndex = PhAddListViewItem(ListViewHandle, MAXINT, logonSessionLuid, sid);

                    name = PH_AUTO(PhGetSidFullName(sid, TRUE, NULL));
                    PhSetListViewSubItem(ListViewHandle, lvItemIndex, 1, PhGetStringOrDefault(name, L"(unknown)"));

                    sidString = PH_AUTO(PhSidToStringSid(sid));
                    PhSetListViewSubItem(ListViewHandle, lvItemIndex, 2, PhGetStringOrDefault(sidString, L"(unknown)"));
                }
                else
                {
                    PhPrintPointer(logonSessionLuid, UlongToPtr(logonSessionData->LogonId.LowPart));
                    PhAddListViewItem(ListViewHandle, MAXINT, logonSessionLuid, NULL);
                }

                LsaFreeReturnBuffer(logonSessionData);
            }
        }

        LsaFreeReturnBuffer(logonSessionList);
    }

    ExtendedListView_SortItems(ListViewHandle);
}

VOID SxpRefreshUsers(
    _In_ HWND ListViewHandle
    )
{
    NTSTATUS status;
    LSA_HANDLE policyHandle = NULL;
    SAM_HANDLE serverHandle = NULL;
    SAM_HANDLE domainHandle = NULL;
    SAM_HANDLE userHandle = NULL;
    SAM_ENUMERATE_HANDLE enumContext = 0;
    ULONG enumBufferLength = 0;
    PSAM_RID_ENUMERATION enumBuffer = NULL;
    PPOLICY_ACCOUNT_DOMAIN_INFO policyDomainInfo = NULL;

    if (!NT_SUCCESS(status = PhOpenLsaPolicy(
        &policyHandle,
        POLICY_VIEW_LOCAL_INFORMATION,
        NULL
        )))
    {
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = LsaQueryInformationPolicy(
        policyHandle,
        PolicyAccountDomainInformation,
        &policyDomainInfo
        )))
    {
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = SamConnect(
        NULL,
        &serverHandle,
        SAM_SERVER_CONNECT | SAM_SERVER_LOOKUP_DOMAIN,
        NULL
        )))
    {
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = SamOpenDomain(
        serverHandle,
        DOMAIN_LIST_ACCOUNTS | DOMAIN_LOOKUP,
        policyDomainInfo->DomainSid,
        &domainHandle
        )))
    {
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = SamEnumerateUsersInDomain(
        domainHandle,
        &enumContext,
        0, // USER_ACCOUNT_TYPE_MASK
        &enumBuffer,
        -1,
        &enumBufferLength
        )))
    {
        goto CleanupExit;
    }

    for (ULONG i = 0; i < enumBufferLength; i++)
    {
        PSID userSid = NULL;
        PUSER_ALL_INFORMATION userInfo = NULL;

        if (!NT_SUCCESS(status = SamOpenUser(
            domainHandle,
            USER_ALL_ACCESS,
            enumBuffer[i].RelativeId,
            &userHandle
            )))
        {
            continue;
        }

        if (!NT_SUCCESS(status = SamQueryInformationUser(
            userHandle,
            UserAllInformation,
            &userInfo
            )))
        {
            SamCloseHandle(userHandle);
            continue;
        }

        if (NT_SUCCESS(status = SamRidToSid(
            userHandle,
            enumBuffer[i].RelativeId,
            &userSid
            )))
        {
            INT lvItemIndex;
            PSID sid;
            PPH_STRING name;
            PPH_STRING sidString;

            sid = PhAllocateCopy(userSid, RtlLengthSid(userSid));
            PhAddItemList(AccountsList, sid);

            name = PH_AUTO(PhGetSidFullName(sid, TRUE, NULL));
            lvItemIndex = PhAddListViewItem(ListViewHandle, MAXINT, PhGetStringOrDefault(name, L"(unknown)"), UlongToPtr(enumBuffer[i].RelativeId));

            sidString = PH_AUTO(PhSidToStringSid(sid));
            PhSetListViewSubItem(ListViewHandle, lvItemIndex, 1, PhGetStringOrDefault(sidString, L"(unknown)"));
        }

        SamCloseHandle(userHandle);
        SamFreeMemory(userInfo);
    }

CleanupExit:

    if (enumBuffer)
    {
        SamFreeMemory(enumBuffer);
    }

    if (domainHandle)
    {
        SamCloseHandle(domainHandle);
    }

    if (serverHandle)
    {
        SamCloseHandle(serverHandle);
    }

    if (policyDomainInfo)
    {
        LsaFreeMemory(policyDomainInfo);
    }

    if (policyHandle)
    {
        LsaClose(policyHandle);
    }
}

VOID SxpRefreshGroups(
    _In_ HWND ListViewHandle
)
{
    NTSTATUS status;
    LSA_HANDLE policyHandle = NULL;
    SAM_HANDLE serverHandle = NULL;
    SAM_HANDLE domainHandle = NULL;
    SAM_HANDLE groupHandle = NULL;
    SAM_ENUMERATE_HANDLE enumContext = 0;
    ULONG enumBufferLength = 0;
    PSAM_RID_ENUMERATION enumBuffer = NULL;
    PPOLICY_ACCOUNT_DOMAIN_INFO policyDomainInfo = NULL;

    if (!NT_SUCCESS(status = PhOpenLsaPolicy(
        &policyHandle,
        POLICY_VIEW_LOCAL_INFORMATION,
        NULL
        )))
    {
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = LsaQueryInformationPolicy(
        policyHandle,
        PolicyAccountDomainInformation,
        &policyDomainInfo
        )))
    {
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = SamConnect(
        NULL,
        &serverHandle,
        SAM_SERVER_CONNECT | SAM_SERVER_LOOKUP_DOMAIN,
        NULL
        )))
    {
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = SamOpenDomain(
        serverHandle,
        DOMAIN_LIST_ACCOUNTS | DOMAIN_LOOKUP,
        policyDomainInfo->DomainSid,
        &domainHandle
        )))
    {
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = SamEnumerateGroupsInDomain(
        domainHandle,
        &enumContext,
        &enumBuffer,
        -1,
        &enumBufferLength
        )))
    {
        goto CleanupExit;
    }

    for (ULONG i = 0; i < enumBufferLength; i++)
    {
        PGROUP_GENERAL_INFORMATION groupInfo = NULL;

        if (!NT_SUCCESS(status = SamOpenGroup(
            domainHandle,
            GROUP_ALL_ACCESS,
            enumBuffer[i].RelativeId,
            &groupHandle
            )))
        {
            continue;
        }

        if (NT_SUCCESS(status = SamQueryInformationGroup(
            groupHandle,
            GroupGeneralInformation,
            &groupInfo
            )))
        {
            INT lvItemIndex;
            PPH_STRING groupName;
            PPH_STRING groupComment;

            groupName = PH_AUTO(PhCreateStringFromUnicodeString(&groupInfo->Name));
            groupComment = PH_AUTO(PhCreateStringFromUnicodeString(&groupInfo->AdminComment));

            lvItemIndex = PhAddListViewItem(ListViewHandle, MAXINT, PhGetStringOrDefault(groupName, L"(unknown)"), NULL);
            PhSetListViewSubItem(ListViewHandle, lvItemIndex, 1, PhGetStringOrDefault(groupComment, L"(unknown)"));

            SamFreeMemory(groupInfo);
        }

        SamCloseHandle(groupHandle);
    }

CleanupExit:

    if (enumBuffer)
    {
        SamFreeMemory(enumBuffer);
    }

    if (domainHandle)
    {
        SamCloseHandle(domainHandle);
    }

    if (serverHandle)
    {
        SamCloseHandle(serverHandle);
    }

    if (policyDomainInfo)
    {
        LsaFreeMemory(policyDomainInfo);
    }

    if (policyHandle)
    {
        LsaClose(policyHandle);
    }
}

INT_PTR CALLBACK SxLsaDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PhCenterWindow(GetParent(hwndDlg), GetParent(GetParent(hwndDlg)));

            AccountsLv = GetDlgItem(hwndDlg, IDC_ACCOUNTS);
            PrivilegesLv = GetDlgItem(hwndDlg, IDC_PRIVILEGES);

            PhSetListViewStyle(AccountsLv, FALSE, TRUE);
            PhSetControlTheme(AccountsLv, L"explorer");
            PhAddListViewColumn(AccountsLv, 0, 0, 0, LVCFMT_LEFT, 220, L"Name");
            PhAddListViewColumn(AccountsLv, 1, 1, 1, LVCFMT_LEFT, 300, L"SID");
            PhSetExtendedListView(AccountsLv);

            PhSetListViewStyle(PrivilegesLv, FALSE, TRUE);
            PhSetControlTheme(PrivilegesLv, L"explorer");
            PhAddListViewColumn(PrivilegesLv, 0, 0, 0, LVCFMT_LEFT, 200, L"Name");
            PhAddListViewColumn(PrivilegesLv, 1, 1, 1, LVCFMT_LEFT, 360, L"Description");
            PhSetExtendedListView(PrivilegesLv);

            SxpRefreshAccounts();
            SxpRefreshPrivileges();
        }
        break;
    case WM_DESTROY:
        {
            SxpFreeAccounts();
        }
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDC_EDITPOLICYSECURITY:
                {
                    PhEditSecurity(
                        hwndDlg,
                        L"Local LSA Policy",
                        L"LsaPolicy",
                        SxpOpenLsaPolicy,
                        NULL,
                        NULL
                        );
                }
                break;
            case IDC_ACCOUNT_DELETE:
                {
                    if (!SelectedAccount)
                        return FALSE;

                    if (PhShowConfirmMessage(
                        hwndDlg,
                        L"delete",
                        L"the selected account",
                        NULL,
                        TRUE
                        ))
                    {
                        NTSTATUS status;
                        LSA_HANDLE policyHandle;
                        LSA_HANDLE accountHandle;

                        if (NT_SUCCESS(status = PhOpenLsaPolicy(&policyHandle, POLICY_LOOKUP_NAMES, NULL)))
                        {
                            if (NT_SUCCESS(status = LsaOpenAccount(
                                policyHandle,
                                SelectedAccount,
                                ACCOUNT_VIEW | DELETE, // ACCOUNT_VIEW is needed as well for some reason
                                &accountHandle
                                )))
                            {
                                status = LsaDelete(accountHandle);
                                LsaClose(accountHandle);
                            }

                            LsaClose(policyHandle);
                        }

                        if (NT_SUCCESS(status))
                            SxpRefreshAccounts();
                        else
                            PhShowStatus(hwndDlg, L"Unable to delete the account", status, 0);
                    }
                }
                break;
            case IDC_ACCOUNT_SECURITY:
                {
                    PPH_STRING name;

                    if (!SelectedAccount)
                        return FALSE;

                    name = PhGetSidFullName(SelectedAccount, TRUE, NULL);

                    PhEditSecurity(
                        hwndDlg,
                        PhGetStringOrDefault(name, L"(unknown)"),
                        L"LsaAccount",
                        SxpOpenSelectedLsaAccount,
                        NULL,
                        SelectedAccount
                        );

                    PhClearReference(&name);
                }
                break;
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR header = (LPNMHDR)lParam;

            switch (header->code)
            {
            case LVN_ITEMCHANGED:
                {
                    if (header->hwndFrom == AccountsLv)
                    {
                        if (ListView_GetSelectedCount(AccountsLv) == 1)
                        {
                            SelectedAccount = PhGetSelectedListViewItemParam(AccountsLv);
                        }
                        else
                        {
                            SelectedAccount = NULL;
                        }
                    }
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}
