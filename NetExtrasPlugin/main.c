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

#include "main.h"

PPH_PLUGIN PluginInstance = NULL;
PH_CALLBACK_REGISTRATION PluginLoadCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginUnloadCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginShowOptionsCallbackRegistration;
PH_CALLBACK_REGISTRATION NetworkTreeNewInitializingCallbackRegistration;
PH_CALLBACK_REGISTRATION TreeNewMessageCallbackRegistration;
HWND NetworkTreeNewHandle = NULL;

LONG NTAPI NetworkServiceSortFunction(
    _In_ PVOID Node1,
    _In_ PVOID Node2,
    _In_ ULONG SubId,
    _In_ PVOID Context
    )
{
    PPH_NETWORK_NODE node1 = Node1;
    PPH_NETWORK_NODE node2 = Node2;
    PNETWORK_EXTENSION extension1 = PhPluginGetObjectExtension(PluginInstance, node1->NetworkItem, EmNetworkItemType);
    PNETWORK_EXTENSION extension2 = PhPluginGetObjectExtension(PluginInstance, node2->NetworkItem, EmNetworkItemType);

    UpdateNetworkNode(SubId, node1, extension1);
    UpdateNetworkNode(SubId, node2, extension2);

    switch (SubId)
    {
    case NETWORK_COLUMN_ID_LOCAL_SERVICE:
         return PhCompareStringWithNull(extension1->LocalServiceName, extension2->LocalServiceName, TRUE);
    case NETWORK_COLUMN_ID_REMOTE_SERVICE:
         return PhCompareStringWithNull(extension1->RemoteServiceName, extension2->RemoteServiceName, TRUE);
    case NETWORK_COLUMN_ID_REMOTE_COUNTRY:
         return PhCompareStringWithNull(extension1->RemoteCountryCode, extension2->RemoteCountryCode, TRUE);
    }

    return 0;
}

VOID NTAPI LoadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    LoadGeoLiteDb();
}

VOID NTAPI UnloadCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    NOTHING;
}

VOID NTAPI ShowOptionsCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    //ShowOptionsDialog((HWND)Parameter);
}

VOID NTAPI NetworkTreeNewInitializingCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_TREENEW_INFORMATION info = Parameter;
    PH_TREENEW_COLUMN column;

    *(HWND*)Context = info->TreeNewHandle;

    memset(&column, 0, sizeof(PH_TREENEW_COLUMN));
    column.Text = L"Local Service";
    column.Width = 140;
    column.Alignment = PH_ALIGN_LEFT;
    PhPluginAddTreeNewColumn(PluginInstance, info->CmData, &column, NETWORK_COLUMN_ID_LOCAL_SERVICE, NULL, NetworkServiceSortFunction);
 
    memset(&column, 0, sizeof(PH_TREENEW_COLUMN));
    column.Text = L"Remote Service";
    column.Width = 140;
    column.Alignment = PH_ALIGN_LEFT;
    PhPluginAddTreeNewColumn(PluginInstance, info->CmData, &column, NETWORK_COLUMN_ID_REMOTE_SERVICE, NULL, NetworkServiceSortFunction);

    memset(&column, 0, sizeof(PH_TREENEW_COLUMN));
    column.Text = L"Country";
    column.Width = 140;
    column.Alignment = PH_ALIGN_LEFT;
    column.CustomDraw = TRUE; // Owner-draw this column to show country flags
    PhPluginAddTreeNewColumn(PluginInstance, info->CmData, &column, NETWORK_COLUMN_ID_REMOTE_COUNTRY, NULL, NetworkServiceSortFunction);
}

VOID NTAPI NetworkItemCreateCallback(
    _In_ PVOID Object,
    _In_ PH_EM_OBJECT_TYPE ObjectType,
    _In_ PVOID Extension
    )
{
    PPH_NETWORK_ITEM networkItem = Object;
    PNETWORK_EXTENSION extension = Extension;

    memset(extension, 0, sizeof(NETWORK_EXTENSION));

    //PPH_NETWORK_NODE networkNode = Object;
    extension = PhPluginGetObjectExtension(PluginInstance, networkItem, EmNetworkItemType);

    // Update the country data for this connection
    if (!extension->CountryValid)
    {
        PPH_STRING remoteCountryCode;
        PPH_STRING remoteCountryName;

        if (LookupCountryCode(networkItem->RemoteEndpoint.Address, &remoteCountryCode, &remoteCountryName))
        {
            PhSwapReference(&extension->RemoteCountryCode, remoteCountryCode);
            PhSwapReference(&extension->RemoteCountryName, remoteCountryName);
        }

        extension->CountryValid = TRUE;
    }
}

VOID NTAPI NetworkItemDeleteCallback(
    _In_ PVOID Object,
    _In_ PH_EM_OBJECT_TYPE ObjectType,
    _In_ PVOID Extension
    )
{
    //PPH_NETWORK_ITEM networkItem = Object;
    PNETWORK_EXTENSION extension = Extension;

    PhClearReference(&extension->LocalServiceName);
    PhClearReference(&extension->RemoteServiceName);
    PhClearReference(&extension->RemoteCountryCode);
    PhClearReference(&extension->RemoteCountryName);

    if (extension->CountryIcon)
        DestroyIcon(extension->CountryIcon);
}

VOID NTAPI NetworkNodeCreateCallback(
    _In_ PVOID Object,
    _In_ PH_EM_OBJECT_TYPE ObjectType,
    _In_ PVOID Extension
    )
{

}

VOID NTAPI TreeNewMessageCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_PLUGIN_TREENEW_MESSAGE message = Parameter;

    switch (message->Message)
    {
    case TreeNewGetCellText:
        {
            if (message->TreeNewHandle == NetworkTreeNewHandle)
            {
                PPH_TREENEW_GET_CELL_TEXT getCellText = message->Parameter1;
                PPH_NETWORK_NODE networkNode = (PPH_NETWORK_NODE)getCellText->Node;
                PNETWORK_EXTENSION extension = PhPluginGetObjectExtension(PluginInstance, networkNode->NetworkItem, EmNetworkItemType);

                UpdateNetworkNode(message->SubId, networkNode, extension);

                switch (message->SubId)
                {
                case NETWORK_COLUMN_ID_LOCAL_SERVICE:
                    getCellText->Text = PhGetStringRef(extension->LocalServiceName);
                    break;
                case NETWORK_COLUMN_ID_REMOTE_SERVICE: 
                    getCellText->Text = PhGetStringRef(extension->RemoteServiceName);
                    break;
                }
            }
        }
        break;
    case TreeNewCustomDraw:
        {
            PPH_TREENEW_CUSTOM_DRAW customDraw = message->Parameter1;
            PPH_NETWORK_NODE networkNode = (PPH_NETWORK_NODE)customDraw->Node;
            PNETWORK_EXTENSION extension = PhPluginGetObjectExtension(PluginInstance, networkNode->NetworkItem, EmNetworkItemType);
            HDC hdc = customDraw->Dc;
            RECT rect = customDraw->CellRect;

            // Check if this is the country column
            if (message->SubId != NETWORK_COLUMN_ID_REMOTE_COUNTRY)
                break;

            // Check if there's something to draw
            if (rect.right - rect.left <= 1)
            {
                // nothing to draw
                break;
            }
            
            // Padding
            rect.left += 5;

            // Draw the column data
            if (GeoDbLoaded && extension->RemoteCountryCode && extension->RemoteCountryName)
            {
                if (!extension->CountryIcon)
                {                
                    HBITMAP countryBitmap;

                    if (countryBitmap = LoadImageFromResources(16, 11, extension->RemoteCountryCode))
                    {
                        HDC screenDc = CreateIC(L"DISPLAY", NULL, NULL, NULL);
                        HBITMAP screenBitmap = CreateCompatibleBitmap(screenDc, 16, 11);

                        ICONINFO iconInfo = { 0 };
                        iconInfo.fIcon = TRUE;
                        iconInfo.hbmColor = countryBitmap;
                        iconInfo.hbmMask = screenBitmap;

                        extension->CountryIcon = CreateIconIndirect(&iconInfo);

                        DeleteObject(screenBitmap);
                        DeleteDC(screenDc);
                        DeleteObject(countryBitmap);
                    }
                }
 
                if (extension->CountryIcon)
                {
                    DrawIconEx(
                        hdc,
                        rect.left,
                        rect.top + ((rect.bottom - rect.top) - 11) / 2,
                        extension->CountryIcon,
                        16,
                        11,
                        0,
                        NULL,
                        DI_NORMAL
                        );

                    rect.left += 16 + 2;
                }

                DrawText(
                    hdc, 
                    extension->RemoteCountryName->Buffer,
                    (INT)extension->RemoteCountryName->Length / 2,
                    &rect,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE
                    );
            }

            if (!GeoDbLoaded)
            {
                DrawText(
                    hdc,
                    L"Geoip database error.",
                    -1,
                    &rect,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE
                    );
            }
        }
        break;
    }
}

LOGICAL DllMain(
    _In_ HINSTANCE Instance,
    _In_ ULONG Reason,
    _Reserved_ PVOID Reserved  
    )
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        {
            PPH_PLUGIN_INFORMATION info;

            PluginInstance = PhRegisterPlugin(PLUGIN_NAME, Instance, &info);

            if (!PluginInstance)
                return FALSE;

            info->DisplayName = L"Network Extras";
            info->Author = L"dmex";
            info->Description = L"Plugin for extra network information.";
            info->HasOptions = TRUE;

            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackLoad),
                LoadCallback,
                NULL,
                &PluginLoadCallbackRegistration
                );
            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackUnload),
                UnloadCallback,
                NULL,
                &PluginUnloadCallbackRegistration
                );
            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackShowOptions),
                ShowOptionsCallback,
                NULL,
                &PluginShowOptionsCallbackRegistration
                );

            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackNetworkTreeNewInitializing),
                NetworkTreeNewInitializingCallback,
                &NetworkTreeNewHandle,
                &NetworkTreeNewInitializingCallbackRegistration
                );
            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackTreeNewMessage),
                TreeNewMessageCallback,
                NULL,
                &TreeNewMessageCallbackRegistration
                );     

            PhPluginSetObjectExtension(
                PluginInstance, 
                EmNetworkItemType, 
                sizeof(NETWORK_EXTENSION),
                NetworkItemCreateCallback,
                NetworkItemDeleteCallback
                );
            PhPluginSetObjectExtension(
                PluginInstance,
                EmNetworkNodeType,
                sizeof(NETWORK_EXTENSION),
                NetworkNodeCreateCallback,
                NULL
                );
        }
        break;
    }

    return TRUE;
}