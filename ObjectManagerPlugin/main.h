#ifndef _WINOBJ_H
#define _WINOBJ_H

#include <phdk.h>
#include <settings.h>
#include <workqueue.h>
#include "resource.h"

#define WINOBJ_MENU_ITEM 1000
#define PLUGIN_NAME L"dmex.ObjectManagerPlugin"
#define SETTING_NAME_WINDOW_POSITION (PLUGIN_NAME L".WindowPosition")
#define SETTING_NAME_WINDOW_SIZE (PLUGIN_NAME L".WindowSize")
#define SETTING_NAME_COLUMNS (PLUGIN_NAME L".WindowColumns")

extern PPH_PLUGIN PluginInstance;

typedef struct _OBJECT_ENTRY
{
    PPH_STRING Name;
    PPH_STRING TypeName;
} OBJECT_ENTRY, *POBJECT_ENTRY;

typedef struct _OBJ_CONTEXT
{
    HWND ListViewHandle;
    HWND TreeViewHandle;
    PH_LAYOUT_MANAGER LayoutManager;

    HTREEITEM RootTreeObject;
    HTREEITEM SelectedTreeItem;

    HIMAGELIST TreeImageList;
    HIMAGELIST ListImageList;
} OBJ_CONTEXT, *POBJ_CONTEXT;

typedef struct _DIRECTORY_ENUM_CONTEXT
{
    HWND TreeViewHandle;
    HTREEITEM RootTreeItem;
    PH_STRINGREF DirectoryPath;
} DIRECTORY_ENUM_CONTEXT, *PDIRECTORY_ENUM_CONTEXT;

NTSTATUS EnumDirectoryObjects(
    _In_ HWND TreeViewHandle,
    _In_ HTREEITEM RootTreeItem,
    _In_ PH_STRINGREF DirectoryPath
    );

INT_PTR CALLBACK WinObjDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

#endif