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

static HWND GroupsLv = NULL;
static PH_LAYOUT_MANAGER LayoutManager;

INT_PTR CALLBACK SxGroupsDlgProc(
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
            GroupsLv = GetDlgItem(hwndDlg, IDC_SESSIONS);

            PhSetListViewStyle(GroupsLv, FALSE, TRUE);
            PhSetControlTheme(GroupsLv, L"explorer");
            PhAddListViewColumn(GroupsLv, 0, 0, 0, LVCFMT_LEFT, 200, L"Name");
            PhAddListViewColumn(GroupsLv, 1, 1, 1, LVCFMT_LEFT, 300, L"SID");
            PhSetExtendedListView(GroupsLv);

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, GroupsLv, NULL, PH_ANCHOR_ALL);

            SxpRefreshGroups(GroupsLv);
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