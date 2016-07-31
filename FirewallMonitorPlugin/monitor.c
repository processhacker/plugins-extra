/*
 * Process Hacker Extra Plugins -
 *   Firewall Monitor
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

static VOID NTAPI FwObjectTypeDeleteProcedure(
    _In_ PVOID Object,
    _In_ ULONG Flags
    )
{
    PhClearReference(&Object);
}

PFW_EVENT_ITEM EtCreateFirewallEntryItem(
    VOID
    )
{
    static ULONG itemCount = 0;
    PFW_EVENT_ITEM diskItem;

    diskItem = PhCreateObject(sizeof(FW_EVENT_ITEM), FwObjectType);
    memset(diskItem, 0, sizeof(FW_EVENT_ITEM));

    diskItem->Index = itemCount;
    diskItem->IndexString = PhFormatString(L"%lu", itemCount);
    itemCount++;

    return diskItem;
}

static VOID CALLBACK DropEventCallback(
    _Inout_ PVOID FwContext,
    _In_ const FWPM_NET_EVENT* FwEvent
    )
{
    PFW_EVENT_ITEM fwEventItem = EtCreateFirewallEntryItem();  
    SYSTEMTIME systemTime;

    PhQuerySystemTime(&fwEventItem->AddedTime);
    PhLargeIntegerToLocalSystemTime(&systemTime, &fwEventItem->AddedTime);
    fwEventItem->TimeString = PhFormatDateTime(&systemTime);
    
    switch (FwEvent->type)
    {
    case FWPM_NET_EVENT_TYPE_CLASSIFY_DROP:
        {
            FWPM_FILTER* fwFilterItem = NULL;
            FWPM_LAYER* fwLayerItem = NULL;
            FWPM_NET_EVENT_CLASSIFY_DROP* fwDropEvent = FwEvent->classifyDrop;

            PhInitializeStringRef(&fwEventItem->FwRuleActionString, L"DROP");

            if (FwpmFilterGetById(FwEngineHandle, fwDropEvent->filterId, &fwFilterItem) == ERROR_SUCCESS)
            {
                if (fwFilterItem->displayData.name)
                {
                    fwEventItem->FwRuleNameString = PhCreateString(fwFilterItem->displayData.name);
                }

                if (fwFilterItem->displayData.description)
                {
                    fwEventItem->FwRuleDescriptionString = PhCreateString(fwFilterItem->displayData.description);
                }

                if ((fwFilterItem->action.type & FWP_ACTION_BLOCK) != 0)
                {

                }

                FwpmFreeMemory(&fwFilterItem);
            }

            if (FwpmLayerGetById(FwEngineHandle, fwDropEvent->layerId, &fwLayerItem) == ERROR_SUCCESS)
            {           
                if (fwLayerItem->displayData.name)
                {
                    fwEventItem->FwRuleLayerNameString = PhCreateString(fwLayerItem->displayData.name);
                }

                //fwEventItem->FwRuleLayerDescriptionString = PhCreateString(fwLayerRuleItem->displayData.description);

                for (UINT32 i = 0; i < fwLayerItem->numFields; i++)
                {
                    FWPM_FIELD fwRuleField = fwLayerItem->field[i];

                    // http://msdn.microsoft.com/en-us/library/windows/desktop/aa363996.aspx

                    if (memcmp(fwRuleField.fieldKey, &FWPM_CONDITION_ALE_APP_ID, sizeof(GUID)) == 0)
                    {
                        //The fully qualified device path of the application, as returned by the FwpmGetAppIdFromFileName0 function.
                        // (For example, "\device0\hardiskvolume1\Program Files\Application.exe".)
                        // Data type : FWP_BYTE_BLOB_TYPE

                        //fwDropEvent->
                    }
                    else if (IsEqualGUID(fwRuleField.fieldKey, &FWPM_CONDITION_ALE_ORIGINAL_APP_ID))
                    {
                        // The fully qualified device path of the application, such as "\device0\hardiskvolume1\Program Files\Application.exe".
                        // When a connection has been redirected, this will be the identifier of the originating app, 
                        //  otherwise this will be the same as FWPM_CONDITION_ALE_APP_ID.
                    }
                    else if (IsEqualGUID(fwRuleField.fieldKey, &FWPM_CONDITION_ALE_USER_ID))
                    {
                        //The identification of the local user.
                        //Data type : FWP_SECURITY_DESCRIPTOR_TYPE
                    }
                    else if (IsEqualGUID(fwRuleField.fieldKey, &FWPM_CONDITION_IP_LOCAL_ADDRESS))
                    {

                    }
                    else if (IsEqualGUID(fwRuleField.fieldKey, &FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE))
                    {
                        //The local IP address type.
                        //Possible values : Any of the following NL_ADDRESS_TYPE enumeration values.
                        //NlatUnspecified
                        //NlatUnicast
                        //NlatAnycast
                        //NlatMulticast
                        //NlatBroadcast
                        //Data type : FWP_UINT8

                    }
                    else if (IsEqualGUID(fwRuleField.fieldKey, &FWPM_CONDITION_IP_LOCAL_PORT))
                    {
                        //The local transport protocol port number.
                        //Data type : FWP_UINT16
                        
                    }
                    //else
                    //{
                    //    PhInitializeStringRef(&fwEventItem->FwRuleModeString, L"UNKNOWN");
                    //}
                }

                FwpmFreeMemory(&fwLayerItem);
            }       
    
            if (fwDropEvent->isLoopback)
            {
                PhInitializeStringRef(&fwEventItem->DirectionString, L"Loopback");
            }
            else
            {
                switch (fwDropEvent->msFwpDirection)
                {
                case FWP_DIRECTION_IN:
                case FWP_DIRECTION_INBOUND:
                    PhInitializeStringRef(&fwEventItem->DirectionString, L"In");
                    break;
                case FWP_DIRECTION_OUT:
                case FWP_DIRECTION_OUTBOUND:
                    PhInitializeStringRef(&fwEventItem->DirectionString, L"Out");
                    break;
                case FWP_DIRECTION_FORWARD:
                    PhInitializeStringRef(&fwEventItem->DirectionString, L"Forward");
                    break;
                default:
                    PhInitializeStringRef(&fwEventItem->DirectionString, L"");
                    break;
                }
            }
        }
        break;
    case FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW:
        {
            FWPM_FILTER* fwFilterItem = NULL;
            FWPM_LAYER* fwLayerItem = NULL;
            FWPM_NET_EVENT_CLASSIFY_ALLOW* fwAllowEvent = FwEvent->classifyAllow;

            PhInitializeStringRef(&fwEventItem->FwRuleActionString, L"ALLOW");

            if (FwpmFilterGetById(FwEngineHandle, fwAllowEvent->filterId, &fwFilterItem) == ERROR_SUCCESS)
            {
                if (fwFilterItem->displayData.name)
                {
                    fwEventItem->FwRuleNameString = PhCreateString(fwFilterItem->displayData.name);
                }

                if (fwFilterItem->displayData.description)
                {
                    fwEventItem->FwRuleDescriptionString = PhCreateString(fwFilterItem->displayData.description);
                }

                if ((fwFilterItem->action.type & FWP_ACTION_BLOCK) != 0)
                {

                }

                FwpmFreeMemory(&fwFilterItem);
            }

            if (FwpmLayerGetById(FwEngineHandle, fwAllowEvent->layerId, &fwLayerItem) == ERROR_SUCCESS)
            {
                for (UINT32 i = 0; i < fwLayerItem->numFields; i++)
                {
                    FWPM_FIELD fwRuleField = fwLayerItem->field[i];

                    // http://msdn.microsoft.com/en-us/library/windows/desktop/aa363996.aspx

                    if (memcmp(fwRuleField.fieldKey, &FWPM_CONDITION_ALE_APP_ID, sizeof(GUID)) == 0)
                    {
                        //The fully qualified device path of the application, as returned by the FwpmGetAppIdFromFileName0 function.
                        // (For example, "\device0\hardiskvolume1\Program Files\Application.exe".)
                        // Data type : FWP_BYTE_BLOB_TYPE

                        //fwAllowEvent->
                    }
                    else if (IsEqualGUID(fwRuleField.fieldKey, &FWPM_CONDITION_ALE_ORIGINAL_APP_ID))
                    {
                        // The fully qualified device path of the application, such as "\device0\hardiskvolume1\Program Files\Application.exe".
                        // When a connection has been redirected, this will be the identifier of the originating app, 
                        //  otherwise this will be the same as FWPM_CONDITION_ALE_APP_ID.
                    }
                    else if (IsEqualGUID(fwRuleField.fieldKey, &FWPM_CONDITION_ALE_USER_ID))
                    {
                        //The identification of the local user.
                        //Data type : FWP_SECURITY_DESCRIPTOR_TYPE
                    }
                    else if (IsEqualGUID(fwRuleField.fieldKey, &FWPM_CONDITION_IP_LOCAL_ADDRESS))
                    {

                    }
                    else if (IsEqualGUID(fwRuleField.fieldKey, &FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE))
                    {
                        //The local IP address type.
                        //Possible values : Any of the following NL_ADDRESS_TYPE enumeration values.
                        //NlatUnspecified
                        //NlatUnicast
                        //NlatAnycast
                        //NlatMulticast
                        //NlatBroadcast
                        //Data type : FWP_UINT8

                    }
                    else if (IsEqualGUID(fwRuleField.fieldKey, &FWPM_CONDITION_IP_LOCAL_PORT))
                    {
                        //The local transport protocol port number.
                        //Data type : FWP_UINT16
                        
                    }
                    //else
                    //{
                    //    PhInitializeStringRef(&fwEventItem->FwRuleModeString, L"UNKNOWN");
                    //}
                }

                fwEventItem->FwRuleLayerNameString = PhCreateString(fwLayerItem->displayData.name);
                //fwEventItem->FwRuleLayerDescriptionString = PhCreateString(fwLayerRuleItem->displayData.description);

                FwpmFreeMemory(&fwLayerItem);
            }
            
            if (fwAllowEvent->isLoopback)
            {
                PhInitializeStringRef(&fwEventItem->DirectionString, L"Loopback");
            }
            else
            {
                switch (fwAllowEvent->msFwpDirection)
                {
                case FWP_DIRECTION_IN:
                case FWP_DIRECTION_INBOUND:
                    PhInitializeStringRef(&fwEventItem->DirectionString, L"In");
                    break;
                case FWP_DIRECTION_OUT:
                case FWP_DIRECTION_OUTBOUND:
                    PhInitializeStringRef(&fwEventItem->DirectionString, L"Out");
                    break;
                case FWP_DIRECTION_FORWARD:
                    PhInitializeStringRef(&fwEventItem->DirectionString, L"Forward");
                    break;
                default:
                    PhInitializeStringRef(&fwEventItem->DirectionString, L"");
                    break;
                }
            }
        }
        break;
    case FWPM_NET_EVENT_TYPE_CAPABILITY_DROP:
        //FWPM_NET_EVENT_CAPABILITY_DROP* fwCapDropEvent = FwEvent->capabilityDrop;
        PhInitializeStringRef(&fwEventItem->FwRuleActionString, L"CAPABILITY_DROP");
        return;
    case FWPM_NET_EVENT_TYPE_CAPABILITY_ALLOW: 
        //FWPM_NET_EVENT_CAPABILITY_ALLOW* fwCapAllowEvent = FwEvent->capabilityAllow; 
        PhInitializeStringRef(&fwEventItem->FwRuleActionString, L"CAPABILITY_ALLOW");
        return;
    case FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC:
        //FWPM_NET_EVENT_CLASSIFY_DROP_MAC* fwMacDropEvent = FwEvent->classifyDropMac;
        PhInitializeStringRef(&fwEventItem->FwRuleActionString, L"CLASSIFY_DROP_MAC");
        break;
    case FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP:
        //FWPM_NET_EVENT_IPSEC_KERNEL_DROP* fwIpSecDropEvent = FwEvent->ipsecDrop;
        PhInitializeStringRef(&fwEventItem->FwRuleActionString, L"IPSEC_KERNEL_DROP");
        break;
    case FWPM_NET_EVENT_TYPE_IPSEC_DOSP_DROP:
        //FWPM_NET_EVENT_IPSEC_DOSP_DROP* fwIpSecDoSDropEvent = FwEvent->idpDrop; 
        PhInitializeStringRef(&fwEventItem->FwRuleActionString, L"IPSEC_DOSP_DROP");
        break;
    case FWPM_NET_EVENT_TYPE_IKEEXT_MM_FAILURE:
        //FWPM_NET_EVENT_IKEEXT_MM_FAILURE* fwIkeextMMFailureEvent = FwEvent->ikeMmFailure;
        PhInitializeStringRef(&fwEventItem->FwRuleActionString, L"IKEEXT_MM_FAILURE");
        break;
    case FWPM_NET_EVENT_TYPE_IKEEXT_QM_FAILURE:
        //FWPM_NET_EVENT_IKEEXT_QM_FAILURE* fwIkeextQMFailureEvent = FwEvent->ikeQmFailure;
        PhInitializeStringRef(&fwEventItem->FwRuleActionString, L"QM_FAILURE");
        break;
    case FWPM_NET_EVENT_TYPE_IKEEXT_EM_FAILURE:
        //FWPM_NET_EVENT_IKEEXT_EM_FAILURE* fwIkeextEMFailureEvent = FwEvent->ikeEmFailure;
        PhInitializeStringRef(&fwEventItem->FwRuleActionString, L"EM_FAILURE");
        break;
    default:
        PhInitializeStringRef(&fwEventItem->FwRuleActionString, L"unknown");
        break;
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
 
    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_IP_VERSION_SET) != 0)
    {
        if (FwEvent->header.ipVersion == FWP_IP_VERSION_V4)
        {
            if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0)
            {
                //IN_ADDR ipv4Address = { 0 };
                //PWSTR ipv4StringTerminator = 0;
                //WCHAR ipv4AddressString[INET_ADDRSTRLEN] = L"";
                //
                //ULONG localAddrV4 = _byteswap_ulong(FwEvent->header.localAddrV4);
                //
                //RtlIpv4AddressToString((PIN_ADDR)&localAddrV4, ipv4AddressString);
                //RtlIpv4StringToAddress(ipv4AddressString, TRUE, &ipv4StringTerminator, &ipv4Address);
                //
                //fwEventItem->LocalAddressString = PhFormatString(L"%s", ipv4AddressString);

                fwEventItem->LocalAddressString = PhFormatString(
                    L"%lu.%lu.%lu.%lu",
                    ((PBYTE)&FwEvent->header.localAddrV4)[3],
                    ((PBYTE)&FwEvent->header.localAddrV4)[2],
                    ((PBYTE)&FwEvent->header.localAddrV4)[1],
                    ((PBYTE)&FwEvent->header.localAddrV4)[0]
                    );
            }

            if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)
            {           
                //IN_ADDR ipv4Address = { 0 };
                //PWSTR ipv4StringTerminator = 0;
                //WCHAR ipv4AddressString[INET_ADDRSTRLEN] = L"";
                //
                //ULONG remoteAddrV4 = _byteswap_ulong(FwEvent->header.remoteAddrV4);
                //
                //RtlIpv4AddressToString((PIN_ADDR)&remoteAddrV4, ipv4AddressString);
                //RtlIpv4StringToAddress(ipv4AddressString, TRUE, &ipv4StringTerminator, &ipv4Address);
                //
                //fwEventItem->RemoteAddressString = PhFormatString(L"%s", ipv4AddressString);

                fwEventItem->RemoteAddressString = PhFormatString(
                    L"%lu.%lu.%lu.%lu",
                    ((PBYTE)&FwEvent->header.remoteAddrV4)[3],
                    ((PBYTE)&FwEvent->header.remoteAddrV4)[2],
                    ((PBYTE)&FwEvent->header.remoteAddrV4)[1],
                    ((PBYTE)&FwEvent->header.remoteAddrV4)[0]
                    );
            }
        }
        else if (FwEvent->header.ipVersion == FWP_IP_VERSION_V6)
        {
            if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0)
            {
                IN6_ADDR ipv6Address = { 0 };
                PWSTR ipv6StringTerminator = 0;
                WCHAR ipv6AddressString[INET6_ADDRSTRLEN] = L"";

                RtlIpv6AddressToString((struct in6_addr*)&FwEvent->header.localAddrV6, ipv6AddressString);
                RtlIpv6StringToAddress(ipv6AddressString, &ipv6StringTerminator, &ipv6Address);

                fwEventItem->LocalAddressString = PhFormatString(L"%s", ipv6AddressString);

                //fwEventItem->LocalAddressString = PhFormatString(
                //    L"%x:%x:%x:%x%x:%x:%x:%x",
                //    ((WORD*)&FwEvent->header.localAddrV6)[7],
                //    ((WORD*)&FwEvent->header.localAddrV6)[6],
                //    ((WORD*)&FwEvent->header.localAddrV6)[5],
                //    ((WORD*)&FwEvent->header.localAddrV6)[4],
                //    ((WORD*)&FwEvent->header.localAddrV6)[3],
                //    ((WORD*)&FwEvent->header.localAddrV6)[2],
                //    ((WORD*)&FwEvent->header.localAddrV6)[1],
                //    ((WORD*)&FwEvent->header.localAddrV6)[0]
                //    );
            }

            if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)
            {
                WCHAR ipv6AddressString[INET6_ADDRSTRLEN] = L"";
                PWSTR ipv6StringTerminator = 0;
                IN6_ADDR ipv6Address = { 0 };

                RtlIpv6AddressToString((struct in6_addr*)&FwEvent->header.remoteAddrV6, ipv6AddressString);
                RtlIpv6StringToAddress(ipv6AddressString, &ipv6StringTerminator, &ipv6Address);

                fwEventItem->RemoteAddressString = PhFormatString(L"%s", ipv6AddressString);

                //fwEventItem->RemoteAddressString = PhFormatString(
                //    L"%x:%x:%x:%x%x:%x:%x:%x",
                //    ((WORD*)&FwEvent->header.remoteAddrV6)[7],
                //    ((WORD*)&FwEvent->header.remoteAddrV6)[6],
                //    ((WORD*)&FwEvent->header.remoteAddrV6)[5],
                //    ((WORD*)&FwEvent->header.remoteAddrV6)[4],
                //    ((WORD*)&FwEvent->header.remoteAddrV6)[3],
                //    ((WORD*)&FwEvent->header.remoteAddrV6)[2],
                //    ((WORD*)&FwEvent->header.remoteAddrV6)[1],
                //    ((WORD*)&FwEvent->header.remoteAddrV6)[0]
                //    );
            }
        }
        else
        {
            //FwEvent->header.addressFamily            Available when ipVersion is FWP_IP_VERSION_NONE.
        }
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_APP_ID_SET) != 0)
    {
        PPH_STRING fileName = NULL;
        PPH_STRING resolvedName = NULL;

        if (FwEvent->header.appId.data && FwEvent->header.appId.size > 0)
        {
            fileName = PhCreateStringEx((PWSTR)FwEvent->header.appId.data, (SIZE_T)FwEvent->header.appId.size);
            resolvedName = PhResolveDevicePrefix(fileName);
            PhDereferenceObject(fileName);
        }

        if (resolvedName)
        {
            fwEventItem->ProcessNameString = PhGetFileName(resolvedName);
            fwEventItem->ProcessBaseString = PhGetBaseName(resolvedName);

            //FWP_BYTE_BLOB* fwpApplicationByteBlob = NULL;
            //if (FwpmGetAppIdFromFileName(fileNameString->Buffer, &fwpApplicationByteBlob) == ERROR_SUCCESS)
            //fwEventItem->ProcessBaseString = PhCreateStringEx(fwpApplicationByteBlob->data, fwpApplicationByteBlob->size);
           
            //fwEventItem->Icon = PhGetFileShellIcon(PhGetString(resolvedName), L".exe", FALSE);

            PhDereferenceObject(resolvedName);
        }
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_USER_ID_SET) != 0)
    {
        if (RtlValidSid(FwEvent->header.userId))
        {
            fwEventItem->UserNameString = PhGetSidFullName(FwEvent->header.userId, TRUE, NULL);
        }
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET))
    {
        // The ipProtocol member is set.
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_SCOPE_ID_SET))
    {
        // The scopeId  member is set.
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_REAUTH_REASON_SET) != 0)
    {
        // Indicates an existing connection was reauthorized.
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET) != 0)
    {
        // The packageSid member is set.
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
    PhInvokeCallback(&FwItemsUpdatedEvent, NULL);
}

static VOID NTAPI ProcessesUpdatedCallback(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{   
    static LARGE_INTEGER systemTime;

    PhQuerySystemTime(&systemTime);

    for (ULONG i = 0; i < FwNodeList->Count; i++)
    {
        PFW_EVENT_NODE node = (PFW_EVENT_NODE)FwNodeList->Items[i];

        if (systemTime.QuadPart > (node->EventItem->AddedTime.QuadPart + (60 * PH_TIMEOUT_SEC)))
        {
            PhInvokeCallback(&FwItemRemovedEvent, node);
        }
    }
 
    //PhInvokeCallback(&FwItemsUpdatedEvent, NULL);
}


BOOLEAN StartFwMonitor(
    VOID
    )
{
    FWP_VALUE value = { FWP_EMPTY };
    FWPM_SESSION session = { 0 };
    FWPM_NET_EVENT_SUBSCRIPTION subscription = { 0 };
    FWPM_NET_EVENT_ENUM_TEMPLATE eventTemplate = { 0 };

    
    FwpmNetEventSubscribe1_I = PhGetModuleProcAddress(L"fwpuclnt.dll", "FwpmNetEventSubscribe1");
   
    FwNodeList = PhCreateList(100);
    FwObjectType = PhCreateObjectType(L"FwObject", 0, FwObjectTypeDeleteProcedure);


    session.flags = 0;
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
       
    if (WindowsVersion > WINDOWS_7)
    {
        value.type = FWP_UINT32;
        value.uint32 = FWPM_NET_EVENT_KEYWORD_CAPABILITY_DROP | FWPM_NET_EVENT_KEYWORD_CAPABILITY_ALLOW | FWPM_NET_EVENT_KEYWORD_CLASSIFY_ALLOW; // FWPM_NET_EVENT_KEYWORD_INBOUND_MCAST | FWPM_NET_EVENT_KEYWORD_INBOUND_BCAST

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
    if (FwpmNetEventSubscribe1_I)
    {
        if (FwpmNetEventSubscribe1_I(
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
    else
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

    PhRegisterCallback(
        &PhProcessesUpdatedEvent,
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
