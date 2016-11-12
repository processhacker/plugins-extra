/*
 * Process Hacker Extra Plugins -
 *   Plugin Manager
 *
 * Copyright (C) 2013 dmex
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

#ifndef _WCT_H_
#define _WCT_H_

#define CINTERFACE
#define COBJMACROS
#include <phdk.h>
#include <phappresource.h>
#include <mxml.h>
#include <verify.h>
#include <workqueue.h>

#include <shlobj.h>
#include <Shellapi.h>
#include <psapi.h>
#include <winhttp.h>
#include <windowsx.h>
#include <uxtheme.h>

#include "resource.h"
#include "json-c/json.h"

#define IDD_WCT_MENUITEM 1000
#define PH_UPDATEISERRORED (WM_APP + 501)
#define PH_UPDATEAVAILABLE (WM_APP + 502)
#define PH_UPDATEISCURRENT (WM_APP + 503)
#define PH_UPDATENEWER     (WM_APP + 504)
#define PH_UPDATESUCCESS   (WM_APP + 505)
#define PH_UPDATEFAILURE   (WM_APP + 506)
#define WM_SHOWDIALOG      (WM_APP + 550)

#define ID_WCTSHOWCONTEXTMENU 19584
#define SETTING_PREFIX L"dmex.ExtraPlugins"
#define SETTING_NAME_TREE_LIST_COLUMNS (SETTING_PREFIX L".TreeListColumns")
#define SETTING_NAME_WINDOW_POSITION (SETTING_PREFIX L".WindowPosition")
#define SETTING_NAME_WINDOW_SIZE (SETTING_PREFIX L".WindowSize")
#define SETTING_NAME_LAST_CHECK (SETTING_PREFIX L".LastUpdateCheckTime")

#define ID_SEARCH_CLEAR (WM_APP + 8001)
#define ID_UPDATE_ADD (WM_APP + 8002)
#define ID_UPDATE_COUNT (WM_APP + 8003)

#define MAKE_VERSION_ULONGLONG(major, minor, build, revision) \
    (((ULONGLONG)(major) << 48) | \
    ((ULONGLONG)(minor) << 32) | \
    ((ULONGLONG)(build) << 16) | \
    ((ULONGLONG)(revision) <<  0))

extern PPH_PLUGIN PluginInstance;
extern PPH_STRING SearchboxText;

typedef enum _TREE_PLUGIN_STATE
{
    PLUGIN_STATE_RESTART,
    PLUGIN_STATE_LOCAL,
    PLUGIN_STATE_REMOTE,
    PLUGIN_STATE_UPDATE,
} TREE_PLUGIN_STATE;

typedef enum _WCT_TREE_COLUMN_ITEM_NAME
{
    TREE_COLUMN_ITEM_NAME,
    TREE_COLUMN_ITEM_AUTHOR,
    TREE_COLUMN_ITEM_VERSION,
    TREE_COLUMN_ITEM_MAXIMUM
} WCT_TREE_COLUMN_ITEM_NAME;

typedef struct _PHAPP_PLUGIN
{
    // Public
    PH_AVL_LINKS Links;
    PVOID Reserved;
    PVOID DllBase;
    // end

    // Private
    PPH_STRING FileName;
    ULONG Flags;
    PH_STRINGREF Name;
    PH_PLUGIN_INFORMATION Information;

    PH_CALLBACK Callbacks[PluginCallbackMaximum];
} PHAPP_PLUGIN, *PPHAPP_PLUGIN;


typedef struct _PLUGIN_NODE
{
    PH_TREENEW_NODE Node;

    HICON Icon;
    TREE_PLUGIN_STATE State;

    BOOLEAN PluginOptions;
    PPHAPP_PLUGIN PluginInstance;

    // Local
    PPH_STRING FilePath;
    PPH_STRING InternalName;

    // Remote
    PPH_STRING Id;
    PPH_STRING Visible;
    
    PPH_STRING Name;
    PPH_STRING Version;
    PPH_STRING Author;
    PPH_STRING Description;
    PPH_STRING IconUrl;
    PPH_STRING Requirements;
    PPH_STRING FeedbackUrl;
    PPH_STRING Screenshots;
    PPH_STRING AddedTime;
    PPH_STRING UpdatedTime;
    PPH_STRING Download_count;
    PPH_STRING Download_link_32;
    PPH_STRING Download_link_64;
    PPH_STRING SHA2_32;
    PPH_STRING SHA2_64;
    PPH_STRING HASH_32;
    PPH_STRING HASH_64;

    PH_STRINGREF TextCache[TREE_COLUMN_ITEM_MAXIMUM];
} PLUGIN_NODE, *PPLUGIN_NODE;



typedef struct _WCT_TREE_CONTEXT
{
    HWND ParentWindowHandle;
    HWND TreeNewHandle;

    ULONG TreeNewSortColumn;
    PH_SORT_ORDER TreeNewSortOrder;

    PPH_HASHTABLE NodeHashtable;
    PPH_LIST NodeList;

    HFONT TitleFontHandle;
    HFONT NormalFontHandle;
    HFONT BoldFontHandle;
} WCT_TREE_CONTEXT, *PWCT_TREE_CONTEXT;


// dialog.c 
VOID ShowPluginManagerDialog(VOID);



VOID WtcInitializeWindowTree(
    _In_ HWND ParentWindowHandle,
    _In_ HWND TreeNewHandle,
    _Out_ PWCT_TREE_CONTEXT Context
    );

VOID WtcDeleteWindowTree(
    _In_ PWCT_TREE_CONTEXT Context
    );

struct _PH_TN_FILTER_SUPPORT* WtcGetTreeListFilterSupport(
    VOID
    );

VOID CloudAddChildWindowNode(
    _In_ PWCT_TREE_CONTEXT Context,
    _In_ PPLUGIN_NODE Entry
    );

PPLUGIN_NODE WeAddWindowNode(
    _Inout_ PWCT_TREE_CONTEXT Context
    );

PPLUGIN_NODE FindTreeNode(
    _In_ PWCT_TREE_CONTEXT Context,
    _In_ TREE_PLUGIN_STATE Type,
    _In_ PPH_STRING InternalName
    );

VOID WeRemoveWindowNode(
    _In_ PWCT_TREE_CONTEXT Context,
    _In_ PPLUGIN_NODE WindowNode
    );

VOID WeClearWindowTree(
    _In_ PWCT_TREE_CONTEXT Context
    );

PPLUGIN_NODE WeGetSelectedWindowNode(
    _In_ PWCT_TREE_CONTEXT Context
    );

VOID WeGetSelectedWindowNodes(
    _In_ PWCT_TREE_CONTEXT Context,
    _Out_ PPLUGIN_NODE **Windows,
    _Out_ PULONG NumberOfWindows
    );


typedef struct _WCT_CONTEXT
{
    HWND DialogHandle;
    HWND SearchHandle;
    HWND TreeNewHandle;

    HFONT NormalFontHandle;
    HFONT BoldFontHandle;

    TREE_PLUGIN_STATE Type;
    HWND PluginMenuActive;
    UINT PluginMenuActiveId;

    WCT_TREE_CONTEXT TreeContext;
    PPH_TN_FILTER_SUPPORT TreeFilter;

    PH_LAYOUT_MANAGER LayoutManager;
} WCT_CONTEXT, *PWCT_CONTEXT;    

NTSTATUS QueryPluginsCallbackThread(_In_ PVOID Parameter);

VOID EnumerateLoadedPlugins(_In_ PWCT_CONTEXT Context);

typedef enum _PLUGIN_ACTION
{
    PLUGIN_ACTION_INSTALL,
    PLUGIN_ACTION_UNINSTALL,
    PLUGIN_ACTION_RESTORE
} PLUGIN_ACTION;

typedef struct _PH_UPDATER_CONTEXT
{
    BOOLEAN FixedWindowStyles;
    HWND DialogHandle;
    HICON IconSmallHandle;
    HICON IconLargeHandle;

    PLUGIN_ACTION Action;
    PPLUGIN_NODE Node;

    PPH_STRING FileDownloadUrl;
    PPH_STRING RevVersion;
    PPH_STRING Size;
    PPH_STRING SetupFilePath;
} PH_UPDATER_CONTEXT, *PPH_UPDATER_CONTEXT;


BOOLEAN StartInitialCheck(
    _In_ HWND Parent,
    _In_ PPLUGIN_NODE Node,
    _In_ PLUGIN_ACTION Action
    );

VOID ShowAvailableDialog(_In_ PPH_UPDATER_CONTEXT Context);
VOID ShowCheckForUpdatesDialog(_In_ PPH_UPDATER_CONTEXT Context);
VOID ShowCheckingForUpdatesDialog(_In_ PPH_UPDATER_CONTEXT Context);
VOID TaskDialogLinkClicked(_In_ PPH_UPDATER_CONTEXT Context);

NTSTATUS UpdateDownloadThread(_In_ PVOID Parameter);

// page1.c
VOID ShowCheckForUpdatesDialog(_In_ PPH_UPDATER_CONTEXT Context);
// page2.c
VOID ShowCheckingForUpdatesDialog(_In_ PPH_UPDATER_CONTEXT Context);
// page3.c
VOID ShowAvailableDialog(_In_ PPH_UPDATER_CONTEXT Context);
// page4.c
VOID ShowProgressDialog(_In_ PPH_UPDATER_CONTEXT Context);
VOID ShowLatestVersionDialog(_In_ PPH_UPDATER_CONTEXT Context);
VOID ShowPluginUninstallDialog(_In_ PPH_UPDATER_CONTEXT Context);

VOID ShowUpdateFailedDialog(
    _In_ PPH_UPDATER_CONTEXT Context,
    _In_ BOOLEAN HashFailed,
    _In_ BOOLEAN SignatureFailed
    );

// page6.c
VOID ShowInstallRestartDialog(_In_ PPH_UPDATER_CONTEXT Context);
VOID ShowUninstallRestartDialog(_In_ PPH_UPDATER_CONTEXT Context);
NTSTATUS SetupExtractBuild(_In_ PVOID Parameter);

// plugin.c
VOID ShowPluginProperties(
    _In_ HWND Parent
    );

// verify.c

typedef struct _UPDATER_HASH_CONTEXT
{
    BCRYPT_ALG_HANDLE SignAlgHandle;
    BCRYPT_ALG_HANDLE HashAlgHandle;
    BCRYPT_KEY_HANDLE KeyHandle;
    BCRYPT_HASH_HANDLE HashHandle;
    ULONG HashObjectSize;
    ULONG HashSize;
    PVOID HashObject;
    PVOID Hash;
} UPDATER_HASH_CONTEXT, *PUPDATER_HASH_CONTEXT;

BOOLEAN UpdaterInitializeHash(
    _Out_ PUPDATER_HASH_CONTEXT *Context
    );

BOOLEAN UpdaterUpdateHash(
    _Inout_ PUPDATER_HASH_CONTEXT Context,
    _In_reads_bytes_(Length) PVOID Buffer,
    _In_ ULONG Length
    );

BOOLEAN UpdaterVerifyHash(
    _Inout_ PUPDATER_HASH_CONTEXT Context,
    _In_ PPH_STRING Sha2Hash
    );

BOOLEAN UpdaterVerifySignature(
    _Inout_ PUPDATER_HASH_CONTEXT Context,
    _In_ PPH_STRING HexSignature
    );

VOID UpdaterDestroyHash(
    _Inout_ PUPDATER_HASH_CONTEXT Context
    );



VOID CreateSearchControl(
    _In_ HWND Parent,
    _In_ HWND WindowHandle,
    _In_ UINT CommandID
    );

typedef struct _EDIT_CONTEXT
{
    UINT CommandID;
    LONG CXWidth;
    INT CXBorder;
    INT ImageWidth;
    INT ImageHeight;

    HWND WindowHandle;
    HFONT WindowFont;
    HIMAGELIST ImageList;
    HBITMAP BitmapActive;
    HBITMAP BitmapInactive;

    HBRUSH BrushNormal;
    HBRUSH BrushPushed;
    HBRUSH BrushHot;
    //COLORREF BackgroundColorRef;

    union
    {
        ULONG Flags;
        struct
        {
            ULONG Hot : 1;
            ULONG Pushed : 1;
            ULONG Spare : 30;
        };
    };
} EDIT_CONTEXT, *PEDIT_CONTEXT;

HBITMAP LoadImageFromResources(
    _In_ UINT Width,
    _In_ UINT Height,
    _In_ PCWSTR Name,
    _In_ BOOLEAN RGBAImage
    );

LRESULT SysButtonCustomDraw(
    _In_ PWCT_CONTEXT Context,
    _In_ LPARAM lParam
    );

#endif