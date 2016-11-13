/*
 * Process Hacker Extra Plugins -
 *   Network Extras Plugin
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

#ifndef _EXTRA_H_
#define _EXTRA_H_

#define PLUGIN_NAME L"dmex.NetworkExtrasPlugin"
#define DATABASE_PATH L"plugins\\plugindata\\GeoLite2-Country.mmdb"

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phappresource.h>
#include <workqueue.h>
#include <wincodec.h>
#include <time.h>
#include "resource.h"

typedef struct _NETWORK_EXTRA_CONTEXT
{
    PH_LAYOUT_MANAGER LayoutManager;
} NETWORK_EXTRA_CONTEXT, *PNETWORK_EXTRA_CONTEXT;

typedef struct _NETWORK_EXTENSION
{
    BOOLEAN LocalValid;
    BOOLEAN RemoteValid;
    BOOLEAN CountryValid;
    PPH_STRING LocalServiceName;
    PPH_STRING RemoteServiceName;

    HICON CountryIcon;
    PPH_STRING RemoteCountryCode;
    PPH_STRING RemoteCountryName;
} NETWORK_EXTENSION, *PNETWORK_EXTENSION;

typedef enum _NETWORK_COLUMN_ID
{
    NETWORK_COLUMN_ID_LOCAL_SERVICE = 1,
    NETWORK_COLUMN_ID_REMOTE_SERVICE = 2,
    NETWORK_COLUMN_ID_REMOTE_COUNTRY = 3,
} NETWORK_COLUMN_ID;

typedef struct _RESOLVED_PORT
{
    PWSTR Name;
    USHORT Port;
} RESOLVED_PORT;

extern PPH_PLUGIN PluginInstance;
extern RESOLVED_PORT ResolvedPortsTable[6265];
extern BOOLEAN GeoDbLoaded;

HBITMAP LoadImageFromResources(
    _In_ UINT Width,
    _In_ UINT Height,
    _In_ PPH_STRING Name
    );

VOID UpdateNetworkNode(
    _In_ NETWORK_COLUMN_ID ColumnID,
    _In_ PPH_NETWORK_NODE Node,
    _In_ PNETWORK_EXTENSION Extension
    );

VOID LoadGeoLiteDb(VOID);
VOID FreeGeoLiteDb(VOID);

BOOLEAN LookupCountryCode(
    _In_ PH_IP_ADDRESS RemoteAddress,
    _Out_ PPH_STRING* CountryCode,
    _Out_ PPH_STRING* CountryName
    );

#endif