#ifndef FWMON_H
#define FWMON_H

#include <phdk.h>

#include "resource.h"

#include <Winsock2.h>
#include <fwpmu.h>
#include <fwpsu.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")

#define PLUGIN_NAME L"dmex.FirewallMonitor"
#define SETTING_NAME_FW_TREE_LIST_COLUMNS (PLUGIN_NAME L".TreeListColumns")
#define SETTING_NAME_FW_TREE_LIST_SORT (PLUGIN_NAME L".TreeListSort")

extern PPH_PLUGIN PluginInstance;
extern BOOLEAN FwEnabled;
extern PPH_LIST FwNodeList;

typedef struct _FW_EVENT_ITEM
{
    UINT16 LocalPort;
    UINT16 RemotePort;
    ULONG Index;
    PPH_STRING IndexString;

    LARGE_INTEGER AddedTime;

    PPH_STRING TimeString;
    PPH_STRING UserNameString;
    PH_STRINGREF ProtocalString;
    PPH_STRING ProcessNameString;
    PPH_STRING ProcessBaseString;
    PH_STRINGREF DirectionString;

    PPH_STRING LocalPortString;
    PPH_STRING LocalAddressString;
    PPH_STRING RemotePortString;
    PPH_STRING RemoteAddressString;

    //HICON Icon;
    PH_STRINGREF FwRuleActionString;
    PPH_STRING FwRuleNameString;
    PPH_STRING FwRuleDescriptionString;
    PPH_STRING FwRuleLayerNameString;
    PPH_STRING FwRuleLayerDescriptionString;
} FW_EVENT_ITEM, *PFW_EVENT_ITEM;

#define FWTNC_TIME 0
#define FWTNC_ACTION 1
#define FWTNC_RULENAME 2
#define FWTNC_RULEDESCRIPTION 3
#define FWTNC_PROCESSBASENAME 4
#define FWTNC_PROCESSFILENAME 5
#define FWTNC_USER 6
#define FWTNC_LOCALADDRESS 7
#define FWTNC_LOCALPORT 8
#define FWTNC_REMOTEADDRESS 9
#define FWTNC_REMOTEPORT 10
#define FWTNC_PROTOCOL 11
#define FWTNC_DIRECTION 12
#define FWTNC_INDEX 13
#define FWTNC_MAXIMUM 14

typedef struct _FW_EVENT_NODE
{
    PH_TREENEW_NODE Node;
    PH_STRINGREF TextCache[FWTNC_MAXIMUM];
    PPH_STRING TooltipText;

    PFW_EVENT_ITEM EventItem;
} FW_EVENT_NODE, *PFW_EVENT_NODE;


// monitor
extern PH_CALLBACK FwItemAddedEvent;
extern PH_CALLBACK FwItemModifiedEvent;
extern PH_CALLBACK FwItemRemovedEvent;
extern PH_CALLBACK FwItemsUpdatedEvent;

BOOLEAN StartFwMonitor(VOID);
VOID StopFwMonitor(VOID);
VOID InitializeFwTab(VOID);
VOID LoadSettingsFwTreeList(VOID);
VOID SaveSettingsFwTreeList(VOID);

NTSTATUS NTAPI ShowFwRuleProperties(
    _In_ PVOID ThreadParameter
    );


typedef ULONG (WINAPI* _FwpmNetEventSubscribe1)(
   _In_ HANDLE engineHandle,
   _In_ const FWPM_NET_EVENT_SUBSCRIPTION0* subscription,
   _In_ FWPM_NET_EVENT_CALLBACK1 callback,
   _In_opt_ void* context,
   _Out_ HANDLE* eventsHandle
   );

typedef ULONG (WINAPI* _FwpmNetEventSubscribe2)(
   _In_ HANDLE engineHandle,
   _In_ const FWPM_NET_EVENT_SUBSCRIPTION0* subscription,
   _In_ FWPM_NET_EVENT_CALLBACK2 callback,
   _In_opt_ void* context,
   _Out_ HANDLE* eventsHandle
   );

#endif
