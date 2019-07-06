/*
 * Process Hacker Extra Plugins -
 *   Trusted Installer Plugin
 *
 * Copyright (C) 2016-2019 dmex
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

#ifndef _RUNAS_H_
#define _RUNAS_H_

#define PLUGIN_NAME L"dmex.TrustedInstallerPlugin"
#define RUNAS_MENU_ITEM 1

#include <phdk.h>
#include <phappresource.h>

#include <windowsx.h>

#include "resource.h"

extern PPH_PLUGIN PluginInstance;

VOID ShowRunAsDialog(
    _In_opt_ HWND Parent
    );

NTSTATUS RunAsCreateProcessThread(
    _In_ PVOID Parameter
    );

#endif _RUNAS_H_
