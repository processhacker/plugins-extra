/*
 * Process Hacker Extra Plugins -
 *   NT Product Policy Plugin
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

#pragma once

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phappresource.h>
#include <settings.h>
#include "resource.h"

#define POLICY_TABLE_MENUITEM 1000
#define PLUGIN_NAME L"dmex.ProductPolicyPlugin"
#define SETTING_NAME_WINDOW_POSITION (PLUGIN_NAME L".WindowPosition")
#define SETTING_NAME_WINDOW_SIZE (PLUGIN_NAME L".WindowSize")
#define SETTING_NAME_LISTVIEW_COLUMNS (PLUGIN_NAME L".ListViewColumns")

typedef struct _NT_POLICY_ENTRY
{
    PPH_STRING Name;
    PPH_STRING Value;
} NT_POLICY_ENTRY, *PNT_POLICY_ENTRY;

PPH_LIST QueryProductPolicies(
    VOID
    );