/*
 * Process Hacker Extra Plugins -
 *   Environment Edit Plugin
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

#ifndef _ENV_H_
#define _ENV_H_

#define PLUGIN_NAME L"dmex.EnvironmentEditPlugin"
#define SETTING_NAME_FIRST_RUN (PLUGIN_NAME L".FirstRun")
#define SETTING_NAME_WINDOW_POSITION (PLUGIN_NAME L".WindowPosition")
#define SETTING_NAME_WINDOW_SIZE (PLUGIN_NAME L".WindowSize")
#define SETTING_NAME_COLUMNS (PLUGIN_NAME L".WindowColumns")

#define ENV_MENUITEM     1000
#define WM_SHOWDIALOG    (WM_APP + 100)

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phapppub.h>
#include <phappresource.h>
#include <windowsx.h>

#include "resource.h"

extern PPH_PLUGIN PluginInstance;

// remote.c

NTSTATUS SetRemoteEnvironmentVariable(
    _In_ HANDLE ProcessHandle,
    _In_ PWSTR EnvironmentName,
    _In_opt_ PWSTR EnvironmentValue
    );

// dialog.c

typedef struct _ENVIRONMENT_ENTRY
{
    PPH_STRING Name;
    PPH_STRING Value;
} ENVIRONMENT_ENTRY, *PENVIRONMENT_ENTRY;

typedef struct _ENVIRONMENT_DIALOG_CONTEXT
{
    HANDLE ProcessHandle;
    PPH_PROCESS_ITEM ProcessItem;

    HWND DialogHandle;
    HWND ListViewHandle;

    PH_LAYOUT_MANAGER LayoutManager;
} ENVIRONMENT_DIALOG_CONTEXT, *PENVIRONMENT_DIALOG_CONTEXT;

VOID ShowEnvironmentDialog(
    _In_ PENVIRONMENT_DIALOG_CONTEXT Context
    );

// editor.c

typedef struct _ENVIRONMENT_EDIT_CONTEXT
{
    HANDLE ProcessHandle;
    PPH_STRING VariableName;
    PPH_STRING VariableValue;

    HWND DialogHandle;

    PH_LAYOUT_MANAGER LayoutManager;
} ENVIRONMENT_EDIT_CONTEXT, *PENVIRONMENT_EDIT_CONTEXT;

VOID ShowAddRemoveDialog(
    _In_ HWND Parent,
    _In_ HANDLE ProcessHandle,
    _In_opt_ PPH_STRING VariableName,
    _In_opt_ PPH_STRING VariableValue
    );

#endif