/*
 * Process Hacker Extra Plugins -
 *   Service Backup and Restore Plugin
 *
 * Copyright (C) 2015 dmex
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

#ifndef _SR_EXT_H_
#define _SR_EXT_H_

#define PLUGIN_NAME L"dmex.ServiceBackupRestorePlugin"
#define SETTING_NAME_REG_FORMAT (PLUGIN_NAME L".RegFileFormat")

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phappresource.h>
#include <settings.h>
#include "resource.h"

extern PPH_PLUGIN PluginInstance;

VOID PhBackupService(
    _In_ HWND OwnerWindow,
    _In_ PPH_SERVICE_ITEM ServiceItem
    );

VOID PhRestoreService(
    _In_ HWND OwnerWindow,
    _In_ PPH_SERVICE_ITEM ServiceItem
    );

#endif _SR_EXT_H_