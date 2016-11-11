#ifndef FWMON_H
#define FWMON_H

#include <phdk.h>
#include <winsock2.h>
#include <fwpmu.h>
#include <fwpsu.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <windowsx.h>

#include "resource.h"

#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

#define PLUGIN_NAME L"dmex.FirewallMonitor"
#define SETTING_NAME_WINDOW_POSITION (PLUGIN_NAME L".WindowPosition")
#define SETTING_NAME_WINDOW_SIZE (PLUGIN_NAME L".WindowSize")
#define SETTING_NAME_LISTVIEW_COLUMNS (PLUGIN_NAME L".ListViewColumns")
#define SETTING_NAME_FW_TREE_LIST_COLUMNS (PLUGIN_NAME L".TreeListColumns")
#define SETTING_NAME_FW_TREE_LIST_SORT (PLUGIN_NAME L".TreeListSort")

extern PPH_PLUGIN PluginInstance;
extern BOOLEAN FwEnabled;
extern PPH_LIST FwNodeList;

typedef struct _BOOT_WINDOW_CONTEXT
{
    HWND ListViewHandle;
    HWND SearchHandle;

    PH_LAYOUT_MANAGER LayoutManager;

    HFONT NormalFontHandle;
    HFONT BoldFontHandle;

    HWND PluginMenuActive;
    UINT PluginMenuActiveId;
} BOOT_WINDOW_CONTEXT, *PBOOT_WINDOW_CONTEXT;

typedef struct _FW_EVENT_ITEM
{
    UINT16 LocalPort;
    UINT16 RemotePort;
    ULONG Index;

    LARGE_INTEGER AddedTime;

    PPH_STRING TimeString;
    PPH_STRING UserNameString;
    PH_STRINGREF ProtocalString;
    PPH_STRING ProcessFileNameString;
    PPH_STRING ProcessNameString;
    PPH_STRING ProcessBaseString;

    PPH_STRING LocalPortString;
    PPH_STRING LocalAddressString;
    PPH_STRING RemotePortString;
    PPH_STRING RemoteAddressString;

    HICON Icon;
    BOOLEAN Loopback;
    UINT32 FwRuleEventDirection;
    FWPM_NET_EVENT_TYPE FwRuleEventType;
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

VOID ShowFwDialog(
    VOID
);

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
