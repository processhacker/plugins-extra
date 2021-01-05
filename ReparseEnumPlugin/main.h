/*
 * Process Hacker Extra Plugins -
 *   Reparse Plugin
 *
 * Copyright (C) 2020 dmex
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

#ifndef _REPARSE_H_
#define _REPARSE_H_

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phappresource.h>
#include <settings.h>
#include <shlobj.h>
#include <symprv.h>
#include <windowsx.h>
#include <workqueue.h>

#include "resource.h"

#define PLUGIN_NAME L"dmex.ReparseEnumPlugin"
#define SETTING_NAME_WINDOW_POSITION (PLUGIN_NAME L".WindowPosition")
#define SETTING_NAME_WINDOW_SIZE (PLUGIN_NAME L".WindowSize")
#define SETTING_NAME_REPARSE_LISTVIEW_COLUMNS (PLUGIN_NAME L".ReparseListViewColumns")
#define SETTING_NAME_OBJECTID_LISTVIEW_COLUMNS (PLUGIN_NAME L".ObjidListViewColumns")
#define SETTING_NAME_SD_LISTVIEW_COLUMNS (PLUGIN_NAME L".SdListViewColumns")

extern PPH_PLUGIN PluginInstance;

#endif _REPARSE_H_
