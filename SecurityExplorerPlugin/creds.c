/*
 * Process Hacker Extra Plugins -
 *   LSA Security Explorer Plugin
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

#include "explorer.h"
#include <wincred.h>
#include <wincrypt.h>
#pragma comment(lib, "Crypt32.lib")

static HWND CredentialsLv = NULL;
static PH_LAYOUT_MANAGER LayoutManager;

// TODO: Add CredDelete support.

VOID SxpEnumerateCredentials(VOID)
{
    ULONG count = 0;
    PCREDENTIAL* credential = NULL;

    CredEnumerate(NULL, CRED_ENUMERATE_ALL_CREDENTIALS, &count, &credential);

    for (ULONG i = 0; i < count; i++)
    {
        INT lvItemIndex;
        PPH_STRING name;
        PPH_STRING user;
        PPH_STRING comment;
        SYSTEMTIME time;

        FileTimeToSystemTime(&credential[i]->LastWritten, &time);

        name = credential[i]->TargetName ? PH_AUTO(PhCreateString(credential[i]->TargetName)) : NULL;
        user = credential[i]->UserName ? PH_AUTO(PhCreateString(credential[i]->UserName)) : NULL;
        comment = credential[i]->Comment ? PH_AUTO(PhCreateString(credential[i]->Comment)) : NULL;

        lvItemIndex = PhAddListViewItem(CredentialsLv, MAXINT, PhGetStringOrEmpty(name), NULL);
        PhSetListViewSubItem(CredentialsLv, lvItemIndex, 1, PhGetStringOrEmpty(user));
        PhSetListViewSubItem(CredentialsLv, lvItemIndex, 2, PhGetStringOrEmpty(comment));
        PhSetListViewSubItem(CredentialsLv, lvItemIndex, 3, PhaFormatDateTime(&time)->Buffer);
    }
}

INT_PTR CALLBACK SxCredentialsDlgProc(
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
            CredentialsLv = GetDlgItem(hwndDlg, IDC_SESSIONS);

            PhSetListViewStyle(CredentialsLv, FALSE, TRUE);
            PhSetControlTheme(CredentialsLv, L"explorer");
            PhAddListViewColumn(CredentialsLv, 0, 0, 0, LVCFMT_LEFT, 200, L"Name");
            PhAddListViewColumn(CredentialsLv, 1, 1, 1, LVCFMT_LEFT, 120, L"User");
            PhAddListViewColumn(CredentialsLv, 2, 2, 2, LVCFMT_LEFT, 200, L"Comment");
            PhAddListViewColumn(CredentialsLv, 3, 3, 3, LVCFMT_LEFT, 150, L"Last Written");
            PhSetExtendedListView(CredentialsLv);

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, CredentialsLv, NULL, PH_ANCHOR_ALL);

            SxpEnumerateCredentials();
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
    }

    return FALSE;
}