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

    LARGE_INTEGER AddedTime;

    PPH_STRING TimeString;
    PPH_STRING UserNameString;
    PH_STRINGREF ProtocalString;
    PH_STRINGREF ProcessNameString;
    PPH_STRING ProcessBaseString;
    PH_STRINGREF DirectionString;

    PPH_STRING LocalPortString;
    PPH_STRING LocalAddressString;
    PPH_STRING RemotePortString;
    PPH_STRING RemoteAddressString;

    HICON Icon;
    PH_STRINGREF FwRuleActionString;
    PPH_STRING FwRuleNameString;
    PPH_STRING FwRuleDescriptionString;
    PPH_STRING FwRuleLayerNameString;
    PPH_STRING FwRuleLayerDescriptionString;
} FW_EVENT_ITEM, *PFW_EVENT_ITEM;

typedef enum _FWTC_COLUMN
{
    FWTNC_TIME,
    FWTNC_ACTION,
    FWTNC_RULENAME,
    FWTNC_RULEDESCRIPTION,
    FWTNC_PROCESSBASENAME,
    FWTNC_PROCESSFILENAME,
    FWTNC_USER,
    FWTNC_LOCALADDRESS,
    FWTNC_LOCALPORT,
    FWTNC_REMOTEADDRESS,
    FWTNC_REMOTEPORT,
    FWTNC_PROTOCOL,
    FWTNC_DIRECTION,
    FWTNC_MAXIMUM
} FWTC_COLUMN;

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

#endif
