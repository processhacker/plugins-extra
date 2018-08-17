/*
 * Process Hacker Extra Plugins -
 *   LSA Security Explorer Plugin
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

#include "explorer.h"

static HWND SessionsLv = NULL;
static PH_LAYOUT_MANAGER LayoutManager;

INT_PTR CALLBACK SxSessionsDlgProc(
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
            SessionsLv = GetDlgItem(hwndDlg, IDC_SESSIONS);

            PhSetListViewStyle(SessionsLv, FALSE, TRUE);
            PhSetControlTheme(SessionsLv, L"explorer");
            PhAddListViewColumn(SessionsLv, 0, 0, 0, LVCFMT_LEFT, 80, L"LogonId");
            PhAddListViewColumn(SessionsLv, 1, 1, 1, LVCFMT_LEFT, 200, L"Name");
            PhAddListViewColumn(SessionsLv, 2, 2, 2, LVCFMT_LEFT, 300, L"SID");
            PhSetExtendedListView(SessionsLv);

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, SessionsLv, NULL, PH_ANCHOR_ALL);

            SxpRefreshSessions(SessionsLv);
        }
        break;
    case WM_DESTROY:
        {
            PhDeleteLayoutManager(&LayoutManager);
        }
        break;
    case WM_SIZE:
        PhLayoutManagerLayout(&LayoutManager);
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDC_ACCOUNT_DELETE:
                {
                    if (!SelectedAccount)
                        return FALSE;

                    if (PhShowConfirmMessage(
                        hwndDlg,
                        L"delete",
                        L"the selected session",
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
                            SxpRefreshSessions(SessionsLv);
                        else
                            PhShowStatus(hwndDlg, L"Unable to delete the session", status, 0);
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
                    if (header->hwndFrom == SessionsLv)
                    {
                        if (ListView_GetSelectedCount(SessionsLv) == 1)
                        {
                            SelectedAccount = PhGetSelectedListViewItemParam(SessionsLv);
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
