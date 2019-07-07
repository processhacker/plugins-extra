#ifndef FWMON_H
#define FWMON_H

#include <phdk.h>
#include <settings.h>

#include <fwpmu.h>
#include <fwpsu.h>

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
#define SETTING_NAME_LISTVIEW_COLUMNS (PLUGIN_NAME L".ListColumns")
#define SETTING_NAME_FW_TREE_LIST_COLUMNS (PLUGIN_NAME L".TreeColumns")
#define SETTING_NAME_FW_TREE_LIST_SORT (PLUGIN_NAME L".TreeSort")

extern PPH_PLUGIN PluginInstance;
extern BOOLEAN FwEnabled;
extern PPH_LIST FwNodeList;

typedef enum _FW_COLUMN_NAME
{
    //FW_COLUMN_TIME,
    FW_COLUMN_PROCESSBASENAME,
    FW_COLUMN_ACTION,
    FW_COLUMN_DIRECTION,
    FW_COLUMN_RULENAME,
    FW_COLUMN_RULEDESCRIPTION,
    FW_COLUMN_PROCESSFILENAME,
    FW_COLUMN_LOCALADDRESS,
    FW_COLUMN_LOCALPORT,
    FW_COLUMN_REMOTEADDRESS,
    FW_COLUMN_REMOTEPORT,
    FW_COLUMN_PROTOCOL,
    FW_COLUMN_USER,
    FW_COLUMN_MAXIMUM
} FW_COLUMN_NAME;

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
    PH_TREENEW_NODE Node;

    PPH_PROCESS_ITEM ProcessItem;
    HICON ProcessIcon;
    BOOLEAN ProcessIconValid;

    BOOLEAN Loopback;
    UINT16 LocalPort;
    UINT16 RemotePort;
    UINT32 FwRuleEventDirection;
    FWPM_NET_EVENT_TYPE FwRuleEventType;
    LARGE_INTEGER AddedTime;
    PH_STRINGREF ProtocalString;
    PPH_STRING TimeString;
    PPH_STRING UserNameString;
    PPH_STRING ProcessFileNameString;
    PPH_STRING ProcessNameString;
    PPH_STRING ProcessBaseString;
    PPH_STRING LocalPortString;
    PPH_STRING LocalAddressString;
    PPH_STRING RemotePortString;
    PPH_STRING RemoteAddressString;
    PPH_STRING FwRuleNameString;
    PPH_STRING FwRuleDescriptionString;
    PPH_STRING FwRuleLayerNameString;
    PPH_STRING FwRuleLayerDescriptionString;

    PPH_STRING TooltipText;
    PH_STRINGREF TextCache[FW_COLUMN_MAXIMUM];
} FW_EVENT_ITEM, *PFW_EVENT_ITEM;

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

VOID ShowFwDialog(VOID);

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

typedef ULONG (WINAPI* _FwpmNetEventSubscribe3)(
    _In_ HANDLE engineHandle,
    _In_ const FWPM_NET_EVENT_SUBSCRIPTION0* subscription,
    _In_ FWPM_NET_EVENT_CALLBACK3 callback,
    _In_opt_ void* context,
    _Out_ HANDLE* eventsHandle
    );

typedef ULONG(WINAPI* _FwpmNetEventSubscribe4)(
    _In_ HANDLE engineHandle,
    _In_ const FWPM_NET_EVENT_SUBSCRIPTION0* subscription,
    _In_ FWPM_NET_EVENT_CALLBACK4 callback,
    _In_opt_ void* context,
    _Out_ HANDLE* eventsHandle
    );

#endif
