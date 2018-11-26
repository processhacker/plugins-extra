/*
 * Process Hacker Extra Plugins -
 *   Firewall Monitor
 *
 * Copyright (C) 2015-2017 dmex
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

#include "fwmon.h"

#define FWP_DIRECTION_IN 0x00003900L
#define FWP_DIRECTION_OUT 0x00003901L
#define FWP_DIRECTION_FORWARD 0x00003902L

PH_CALLBACK_DECLARE(FwItemAddedEvent);
PH_CALLBACK_DECLARE(FwItemModifiedEvent);
PH_CALLBACK_DECLARE(FwItemRemovedEvent);
PH_CALLBACK_DECLARE(FwItemsUpdatedEvent);

static PH_CALLBACK_REGISTRATION ProcessesUpdatedCallbackRegistration;
static PPH_OBJECT_TYPE FwObjectType = NULL;
static HANDLE FwEngineHandle = NULL;
static HANDLE FwEventHandle = NULL;
static HANDLE FwEnumHandle = NULL;
static _FwpmNetEventSubscribe1 FwpmNetEventSubscribe1_I = NULL;
static _FwpmNetEventSubscribe2 FwpmNetEventSubscribe2_I = NULL;
static _FwpmNetEventSubscribe3 FwpmNetEventSubscribe3_I = NULL;
static _FwpmNetEventSubscribe4 FwpmNetEventSubscribe4_I = NULL;

VOID NTAPI FwObjectTypeDeleteProcedure(
    _In_ PVOID Object,
    _In_ ULONG Flags
    )
{
    PFW_EVENT_ITEM event = Object;

    PhClearReference(&event->TimeString);
    PhClearReference(&event->UserNameString);
    PhClearReference(&event->ProcessFileNameString);
    PhClearReference(&event->ProcessNameString);
    PhClearReference(&event->ProcessBaseString);
    PhClearReference(&event->LocalPortString);
    PhClearReference(&event->LocalAddressString);
    PhClearReference(&event->RemotePortString);
    PhClearReference(&event->RemoteAddressString);
    PhClearReference(&event->FwRuleNameString);
    PhClearReference(&event->FwRuleDescriptionString);
    PhClearReference(&event->FwRuleLayerNameString);
    PhClearReference(&event->FwRuleLayerDescriptionString);
    PhClearReference(&event->TooltipText);
}

PFW_EVENT_ITEM EtCreateFirewallEntryItem(
    VOID
    )
{
    PFW_EVENT_ITEM diskItem;

    diskItem = PhCreateObject(sizeof(FW_EVENT_ITEM), FwObjectType);
    memset(diskItem, 0, sizeof(FW_EVENT_ITEM));

    return diskItem;
}

VOID CALLBACK DropEventCallback(
    _Inout_ PVOID FwContext,
    _In_ const FWPM_NET_EVENT* FwEvent
    )
{
    PFW_EVENT_ITEM fwEventItem;
    SYSTEMTIME systemTime;

    fwEventItem = EtCreateFirewallEntryItem();
    PhQuerySystemTime(&fwEventItem->AddedTime);
    PhLargeIntegerToLocalSystemTime(&systemTime, &fwEventItem->AddedTime);

    fwEventItem->FwRuleEventType = FwEvent->type;
    fwEventItem->TimeString = PhFormatDateTime(&systemTime);

    if (FwEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP)
    {
        FWPM_FILTER* fwFilterItem = NULL;
        FWPM_LAYER* fwLayerItem = NULL;
        FWPM_NET_EVENT_CLASSIFY_DROP* fwDropEvent = FwEvent->classifyDrop;

        switch (fwDropEvent->msFwpDirection)
        {
        case FWP_DIRECTION_IN:
        case FWP_DIRECTION_INBOUND:
        case FWP_DIRECTION_OUT:
        case FWP_DIRECTION_OUTBOUND:
            break;
        default:
            PhDereferenceObject(fwEventItem);
            return;
        }

        if (fwEventItem->Loopback = fwDropEvent->isLoopback)
        {
            PhDereferenceObject(fwEventItem);
            return;
        }

        switch (fwDropEvent->msFwpDirection)
        {
        case FWP_DIRECTION_IN:
        case FWP_DIRECTION_INBOUND:
            fwEventItem->FwRuleEventDirection = FWP_DIRECTION_INBOUND;
            break;
        case FWP_DIRECTION_OUT:
        case FWP_DIRECTION_OUTBOUND:
            fwEventItem->FwRuleEventDirection = FWP_DIRECTION_OUTBOUND;
            break;
        }

        if (fwDropEvent->layerId && (FwpmLayerGetById(FwEngineHandle, fwDropEvent->layerId, &fwLayerItem) == ERROR_SUCCESS))
        {
            if (
                IsEqualGUID(&fwLayerItem->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4) ||
                IsEqualGUID(&fwLayerItem->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6)
                )
            {
                FwpmFreeMemory(&fwLayerItem);
                PhDereferenceObject(fwEventItem);
                return;
            }

            if (fwLayerItem->displayData.name && PhCountStringZ(fwLayerItem->displayData.name) > 0)
            {
                fwEventItem->FwRuleLayerNameString = PhCreateString(fwLayerItem->displayData.name);
            }

            //fwEventItem->FwRuleLayerDescriptionString = PhCreateString(fwLayerRuleItem->displayData.description);

            FwpmFreeMemory(&fwLayerItem);
        }

        if (fwDropEvent->filterId && (FwpmFilterGetById(FwEngineHandle, fwDropEvent->filterId, &fwFilterItem) == ERROR_SUCCESS))
        {
            if (fwFilterItem->displayData.name && PhCountStringZ(fwFilterItem->displayData.name) > 0)
                fwEventItem->FwRuleNameString = PhCreateString(fwFilterItem->displayData.name);

            if (fwFilterItem->displayData.description && PhCountStringZ(fwFilterItem->displayData.description) > 0)
                fwEventItem->FwRuleDescriptionString = PhCreateString(fwFilterItem->displayData.description);

            FwpmFreeMemory(&fwFilterItem);
        }
    }
    else if (FwEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW)
    {
        FWPM_FILTER* fwFilterItem = NULL;
        FWPM_LAYER* fwLayerItem = NULL;
        FWPM_NET_EVENT_CLASSIFY_ALLOW* fwAllowEvent = FwEvent->classifyAllow;

        if (fwEventItem->Loopback = fwAllowEvent->isLoopback)
        {
            PhDereferenceObject(fwEventItem);
            return;
        }

        switch (fwAllowEvent->msFwpDirection)
        {
        case FWP_DIRECTION_IN:
        case FWP_DIRECTION_INBOUND:
            fwEventItem->FwRuleEventDirection = FWP_DIRECTION_INBOUND;
            break;
        case FWP_DIRECTION_OUT:
        case FWP_DIRECTION_OUTBOUND:
            fwEventItem->FwRuleEventDirection = FWP_DIRECTION_OUTBOUND;
            break;
        }

        if (fwAllowEvent->layerId && (FwpmLayerGetById(FwEngineHandle, fwAllowEvent->layerId, &fwLayerItem) == ERROR_SUCCESS))
        {
            if (
                IsEqualGUID(&fwLayerItem->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4) ||
                IsEqualGUID(&fwLayerItem->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6)
                )
            {
                FwpmFreeMemory(&fwLayerItem);
                PhDereferenceObject(fwEventItem);
                return;
            }

            if (fwLayerItem->displayData.name && PhCountStringZ(fwLayerItem->displayData.name) > 0)
            {
                fwEventItem->FwRuleLayerNameString = PhCreateString(fwLayerItem->displayData.name);
            }

            //fwEventItem->FwRuleLayerDescriptionString = PhCreateString(fwLayerRuleItem->displayData.description);

            FwpmFreeMemory(&fwLayerItem);
        }

        if (fwAllowEvent->filterId && (FwpmFilterGetById(FwEngineHandle, fwAllowEvent->filterId, &fwFilterItem) == ERROR_SUCCESS))
        {
            if (fwFilterItem->displayData.name && PhCountStringZ(fwFilterItem->displayData.name) > 0)
            {
                fwEventItem->FwRuleNameString = PhCreateString(fwFilterItem->displayData.name);
            }

            if (fwFilterItem->displayData.description && PhCountStringZ(fwFilterItem->displayData.description) > 0)
            {
                fwEventItem->FwRuleDescriptionString = PhCreateString(fwFilterItem->displayData.description);
            }

            FwpmFreeMemory(&fwFilterItem);
        }
    }
    else
    {
        PhDereferenceObject(fwEventItem);
        return;
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_IP_VERSION_SET) != 0)
    {
        if (FwEvent->header.ipVersion == FWP_IP_VERSION_V4)
        {
            if (IN4_IS_ADDR_UNSPECIFIED((PIN_ADDR)&FwEvent->header.localAddrV4))
            {
                PhDereferenceObject(fwEventItem);
                return;
            }

            if (IN4_IS_ADDR_LOOPBACK((PIN_ADDR)&FwEvent->header.localAddrV4))
            {
                PhDereferenceObject(fwEventItem);
                return;
            }

            //if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0)
            fwEventItem->LocalAddressString = PhFormatString(
                L"%lu.%lu.%lu.%lu",
                ((PBYTE)&FwEvent->header.localAddrV4)[3],
                ((PBYTE)&FwEvent->header.localAddrV4)[2],
                ((PBYTE)&FwEvent->header.localAddrV4)[1],
                ((PBYTE)&FwEvent->header.localAddrV4)[0]
                );

            //if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)
            fwEventItem->RemoteAddressString = PhFormatString(
                L"%lu.%lu.%lu.%lu",
                ((PBYTE)&FwEvent->header.remoteAddrV4)[3],
                ((PBYTE)&FwEvent->header.remoteAddrV4)[2],
                ((PBYTE)&FwEvent->header.remoteAddrV4)[1],
                ((PBYTE)&FwEvent->header.remoteAddrV4)[0]
                );

            //IN_ADDR ipv4Address = { 0 };
            //PWSTR ipv4StringTerminator = 0;
            //WCHAR ipv4AddressString[INET_ADDRSTRLEN] = L"";
            //ULONG localAddrV4 = _byteswap_ulong(FwEvent->header.localAddrV4);
            //RtlIpv4AddressToString((PIN_ADDR)&localAddrV4, ipv4AddressString);
            //RtlIpv4StringToAddress(ipv4AddressString, TRUE, &ipv4StringTerminator, &ipv4Address);
            //fwEventItem->LocalAddressString = PhFormatString(L"%s", ipv4AddressString);
            //if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)           
            //IN_ADDR ipv4Address = { 0 };
            //PWSTR ipv4StringTerminator = 0;
            //WCHAR ipv4AddressString[INET_ADDRSTRLEN] = L"";
            //ULONG remoteAddrV4 = _byteswap_ulong(FwEvent->header.remoteAddrV4);
            //RtlIpv4AddressToString((PIN_ADDR)&remoteAddrV4, ipv4AddressString);
            //RtlIpv4StringToAddress(ipv4AddressString, TRUE, &ipv4StringTerminator, &ipv4Address);
            //fwEventItem->RemoteAddressString = PhCreateString(ipv4AddressString);
        }
        else if (FwEvent->header.ipVersion == FWP_IP_VERSION_V6)
        {
            if (IN6_IS_ADDR_UNSPECIFIED((PIN6_ADDR)&FwEvent->header.localAddrV6))
            {
                PhDereferenceObject(fwEventItem);
                return;
            }

            if (IN6_IS_ADDR_LOOPBACK((PIN6_ADDR)&FwEvent->header.localAddrV6))
            {
                PhDereferenceObject(fwEventItem);
                return;
            }
           
            IN6_ADDR ipv6Address = { 0 };
            PWSTR ipv6StringTerminator = 0;
            WCHAR ipv6AddressString[INET6_ADDRSTRLEN] = L"";

            //if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0)
            RtlIpv6AddressToString((struct in6_addr*)&FwEvent->header.localAddrV6, ipv6AddressString);
            RtlIpv6StringToAddress(ipv6AddressString, &ipv6StringTerminator, &ipv6Address);
            fwEventItem->LocalAddressString = PhFormatString(L"%s", ipv6AddressString);

            //if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)
            RtlIpv6AddressToString((struct in6_addr*)&FwEvent->header.remoteAddrV6, ipv6AddressString);
            RtlIpv6StringToAddress(ipv6AddressString, &ipv6StringTerminator, &ipv6Address);
            fwEventItem->RemoteAddressString = PhFormatString(L"%s", ipv6AddressString);
        }
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET) != 0)
    {
        fwEventItem->LocalPort = FwEvent->header.localPort;
        fwEventItem->LocalPortString = PhFormatString(L"%u", FwEvent->header.localPort);
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET) != 0)
    {
        fwEventItem->RemotePort = FwEvent->header.remotePort;
        fwEventItem->RemotePortString = PhFormatString(L"%u", FwEvent->header.remotePort);
    }
 
    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_APP_ID_SET) != 0)
    {
        PPH_STRING fileName;

        if (FwEvent->header.appId.data && FwEvent->header.appId.size > 0)
        {
            fileName = PhCreateStringEx((PWSTR)FwEvent->header.appId.data, (SIZE_T)FwEvent->header.appId.size);
            fwEventItem->ProcessFileNameString = PhResolveDevicePrefix(fileName);
            PhDereferenceObject(fileName);
        }

        if (fwEventItem->ProcessFileNameString)
        {
            fwEventItem->ProcessNameString = PhGetFileName(fwEventItem->ProcessFileNameString);
            fwEventItem->ProcessBaseString = PhGetBaseName(fwEventItem->ProcessFileNameString);

            //FWP_BYTE_BLOB* fwpApplicationByteBlob = NULL;
            //if (FwpmGetAppIdFromFileName(fileNameString->Buffer, &fwpApplicationByteBlob) == ERROR_SUCCESS)
            //fwEventItem->ProcessBaseString = PhCreateStringEx(fwpApplicationByteBlob->data, fwpApplicationByteBlob->size);        
            //fwEventItem->Icon = PhGetFileShellIcon(PhGetString(fwEventItem->ProcessFileNameString), L".exe", FALSE);
        }
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_USER_ID_SET) != 0)
    {
        fwEventItem->UserNameString = PhGetSidFullName(FwEvent->header.userId, TRUE, NULL);
    }

    switch (FwEvent->header.ipProtocol)
    {
    case IPPROTO_HOPOPTS:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"HOPOPTS");
        break;
    case IPPROTO_ICMP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"ICMP");
        break;
    case IPPROTO_IGMP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"IGMP");
        break;
    case IPPROTO_GGP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"GGP");
        break;
    case IPPROTO_IPV4:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"IPv4");
        break;
    case IPPROTO_ST:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"ST");
        break;
    case IPPROTO_TCP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"TCP");
        break;
    case IPPROTO_CBT:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"CBT");
        break;
    case IPPROTO_EGP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"EGP");
        break;
    case IPPROTO_IGP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"IGP");
        break;
    case IPPROTO_PUP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"PUP");
        break;
    case IPPROTO_UDP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"UDP");
        break;
    case IPPROTO_IDP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"IDP");
        break;
    case IPPROTO_RDP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"RDP");
        break;
    case IPPROTO_IPV6:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"IPv6");
        break;
    case IPPROTO_ROUTING:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"ROUTING");
        break;
    case IPPROTO_FRAGMENT:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"FRAGMENT");
        break;
    case IPPROTO_ESP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"ESP");
        break;
    case IPPROTO_AH:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"AH");
        break;
    case IPPROTO_ICMPV6:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"ICMPv6");
        break;
    case IPPROTO_DSTOPTS:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"DSTOPTS");
        break;
    case IPPROTO_ND:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"ND");
        break;
    case IPPROTO_ICLFXBM:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"ICLFXBM");
        break;
    case IPPROTO_PIM:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"PIM");
        break;
    case IPPROTO_PGM:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"PGM");
        break;
    case IPPROTO_L2TP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"L2TP");
        break;
    case IPPROTO_SCTP:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"SCTP");
        break;
    case IPPROTO_RESERVED_IPSEC: 
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"IPSEC");
        break;
    case IPPROTO_RESERVED_IPSECOFFLOAD:  
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"IPSECOFFLOAD");
        break;
    case IPPROTO_RESERVED_WNV:   
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"WNV");
        break; 
    case IPPROTO_RAW:
    case IPPROTO_RESERVED_RAW:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"RAW");
        break;
    case IPPROTO_NONE:
    default:
        PhInitializeStringRef(&fwEventItem->ProtocalString, L"Unknown");
        break;
    }

    PhInvokeCallback(&FwItemAddedEvent, fwEventItem);
}

VOID NTAPI ProcessesUpdatedCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{   
    LARGE_INTEGER systemTime;

    PhQuerySystemTime(&systemTime);

    for (ULONG i = 0; i < FwNodeList->Count; i++)
    {
        PFW_EVENT_ITEM node = (PFW_EVENT_ITEM)FwNodeList->Items[i];

        if (systemTime.QuadPart > (node->AddedTime.QuadPart + (60 * PH_TIMEOUT_SEC)))
        {
            PhInvokeCallback(&FwItemRemovedEvent, node);
        }
    }
 
    PhInvokeCallback(&FwItemsUpdatedEvent, NULL);
}


BOOLEAN StartFwMonitor(
    VOID
    )
{
    FWP_VALUE value = { FWP_EMPTY };
    FWPM_SESSION session = { 0 };
    FWPM_NET_EVENT_SUBSCRIPTION subscription = { 0 };
    FWPM_NET_EVENT_ENUM_TEMPLATE eventTemplate = { 0 };

    if (WindowsVersion >= WINDOWS_10_RS5)
    {
        FwpmNetEventSubscribe4_I = PhGetModuleProcAddress(L"fwpuclnt.dll", "FwpmNetEventSubscribe4");
    }
    else if (WindowsVersion >= WINDOWS_10_RS4)
    {
        FwpmNetEventSubscribe3_I = PhGetModuleProcAddress(L"fwpuclnt.dll", "FwpmNetEventSubscribe3");
    }
    else
    {
        if (!(FwpmNetEventSubscribe2_I = PhGetModuleProcAddress(L"fwpuclnt.dll", "FwpmNetEventSubscribe2")))
        {
            FwpmNetEventSubscribe1_I = PhGetModuleProcAddress(L"fwpuclnt.dll", "FwpmNetEventSubscribe1");
        }
    }
   
    FwNodeList = PhCreateList(100);
    FwObjectType = PhCreateObjectType(L"FwObject", 0, FwObjectTypeDeleteProcedure);

    session.flags = 0;// FWPM_SESSION_FLAG_DYNAMIC;
    session.displayData.name  = L"PhFirewallMonitoringSession";
    session.displayData.description = L"Non-Dynamic session for Process Hacker";

    // Create a non-dynamic BFE session
    if (FwpmEngineOpen(
        NULL, 
        RPC_C_AUTHN_WINNT, 
        NULL, 
        &session, 
        &FwEngineHandle
        ) != ERROR_SUCCESS)
    {
        return FALSE;
    }

    value.type = FWP_UINT32;
    value.uint32 = 1;

    // Enable collection of NetEvents
    if (FwpmEngineSetOption(
        FwEngineHandle, 
        FWPM_ENGINE_COLLECT_NET_EVENTS, 
        &value
        ) != ERROR_SUCCESS)
    {
        return FALSE;
    }

    if (WindowsVersion >= WINDOWS_8)
    {
        value.type = FWP_UINT32;
        value.uint32 =
            FWPM_NET_EVENT_KEYWORD_CAPABILITY_DROP |
            FWPM_NET_EVENT_KEYWORD_CAPABILITY_ALLOW |
            FWPM_NET_EVENT_KEYWORD_CLASSIFY_ALLOW |
            FWPM_NET_EVENT_KEYWORD_INBOUND_MCAST |
            FWPM_NET_EVENT_KEYWORD_INBOUND_BCAST;

        if (FwpmEngineSetOption(
            FwEngineHandle,
            FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS,
            &value
            ) != ERROR_SUCCESS)
        {
            return FALSE;
        }

        value.type = FWP_UINT32;
        value.uint32 = 1;

        if (FwpmEngineSetOption(
            FwEngineHandle,
            FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS,
            &value
            ) != ERROR_SUCCESS)
        {
            return FALSE;
        }
    }
  
    eventTemplate.numFilterConditions = 0; // get events for all conditions

    subscription.sessionKey = session.sessionKey;
    subscription.enumTemplate = &eventTemplate;

    // Subscribe to the events
    if (WindowsVersion >= WINDOWS_10_RS5)
    {
        if (!FwpmNetEventSubscribe4_I)
            return FALSE;

        if (FwpmNetEventSubscribe4_I(
            FwEngineHandle,
            &subscription,
            DropEventCallback,
            NULL,
            &FwEventHandle
            ) != ERROR_SUCCESS)
        {
            return FALSE;
        }
    }
    else if (WindowsVersion >= WINDOWS_10_RS4)
    {
        if (!FwpmNetEventSubscribe3_I)
            return FALSE;

        if (FwpmNetEventSubscribe3_I(
            FwEngineHandle,
            &subscription,
            (FWPM_NET_EVENT_CALLBACK3)DropEventCallback,
            NULL,
            &FwEventHandle
            ) != ERROR_SUCCESS)
        {
            return FALSE;
        }
    }
    else if (FwpmNetEventSubscribe2_I)
    {
        if (FwpmNetEventSubscribe2_I(
            FwEngineHandle,
            &subscription,
            (FWPM_NET_EVENT_CALLBACK2)DropEventCallback,
            NULL,
            &FwEventHandle
            ) != ERROR_SUCCESS)
        {
            return FALSE;
        }
    }
    else if (FwpmNetEventSubscribe1_I)
    {
        if (FwpmNetEventSubscribe1_I(
            FwEngineHandle,
            &subscription,
            (FWPM_NET_EVENT_CALLBACK1)DropEventCallback, // TODO: Use correct function.
            NULL,
            &FwEventHandle
            ) != ERROR_SUCCESS)
        {
            return FALSE;
        }
    }
    else if (FwpmNetEventSubscribe0)
    {
        if (FwpmNetEventSubscribe0(
            FwEngineHandle,
            &subscription,
            (FWPM_NET_EVENT_CALLBACK0)DropEventCallback, // TODO: Use correct function.
            NULL,
            &FwEventHandle
            ) != ERROR_SUCCESS)
        {
            return FALSE;
        }
    }
    else
    {
        return FALSE;
    }

    PhRegisterCallback(
        PhGetGeneralCallback(GeneralCallbackProcessProviderUpdatedEvent),
        ProcessesUpdatedCallback,
        NULL,
        &ProcessesUpdatedCallbackRegistration
        );

    return TRUE;
}

VOID StopFwMonitor(
    VOID
    )
{
    if (FwEventHandle)
    {
        FwpmNetEventUnsubscribe(FwEngineHandle, FwEventHandle);
        FwEventHandle = NULL;
    }

    if (FwEngineHandle)
    {
        //FWP_VALUE value = { FWP_EMPTY };
        //value.type = FWP_UINT32;
        //value.uint32 = 0;

        // TODO: return to previous state if other applications require event collection enabled??
        // Disable collection of NetEvents
        //FwpmEngineSetOption(FwEngineHandle, FWPM_ENGINE_COLLECT_NET_EVENTS, &value);

        FwpmEngineClose(FwEngineHandle);
        FwEngineHandle = NULL;
    }
}
