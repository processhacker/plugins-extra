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

#ifndef _EXPLORER_H_
#define _EXPLORER_H_

#pragma comment(lib, "Samlib.lib")
#pragma comment(lib, "Secur32.lib")

#include <phdk.h>
#include <secedit.h>
#include <ntsam.h>
#include <Sddl.h>

#include "resource.h"

extern PPH_PLUGIN PluginInstance;
extern PSID SelectedAccount;

_Callback_ NTSTATUS SxpOpenLsaPolicy(
    _Out_ PHANDLE Handle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ PVOID Context
    );

_Callback_ NTSTATUS SxpOpenSelectedLsaAccount(
    _Out_ PHANDLE Handle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ PVOID Context
    );

_Callback_ NTSTATUS SxpOpenSelectedSamAccount(
    _Out_ PHANDLE Handle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ PVOID Context
    );

VOID SxShowExplorer();

VOID SxpRefreshSessions(
    _In_ HWND ListViewHandle
    );

VOID SxpRefreshUsers(
    _In_ HWND ListViewHandle
    );

VOID SxpRefreshGroups(
    _In_ HWND ListViewHandle
    );

INT_PTR CALLBACK SxLsaDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

INT_PTR CALLBACK SxSessionsDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

INT_PTR CALLBACK SxUsersDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

INT_PTR CALLBACK SxGroupsDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

INT_PTR CALLBACK SxCredentialsDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

#endif _EXPLORER_H_
