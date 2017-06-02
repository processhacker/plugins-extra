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
#include "fwtabp.h"
#include "..\..\plugins\include\toolstatusintf.h"

static BOOLEAN FwTreeNewCreated = FALSE;
static HWND FwTreeNewHandle = NULL;
static ULONG FwTreeNewSortColumn = 0;
static PH_SORT_ORDER FwTreeNewSortOrder;
static PPH_MAIN_TAB_PAGE addedTabPage;

BOOLEAN FwEnabled;
PPH_LIST FwNodeList;
static PH_QUEUED_LOCK FwLock = PH_QUEUED_LOCK_INIT;
static PH_CALLBACK_REGISTRATION FwItemAddedRegistration;
static PH_CALLBACK_REGISTRATION FwItemModifiedRegistration;
static PH_CALLBACK_REGISTRATION FwItemRemovedRegistration;
static PH_CALLBACK_REGISTRATION FwItemsUpdatedRegistration;
static BOOLEAN FwNeedsRedraw = FALSE;

static PH_TN_FILTER_SUPPORT FilterSupport;
static PTOOLSTATUS_INTERFACE ToolStatusInterface;
static PH_CALLBACK_REGISTRATION SearchChangedRegistration;

HWND NTAPI EtpToolStatusGetTreeNewHandle(
    VOID
    );
INT_PTR CALLBACK FwTabErrorDialogProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

BOOLEAN FwTabPageCallback(
    _In_ struct _PH_MAIN_TAB_PAGE *Page,
    _In_ PH_MAIN_TAB_PAGE_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2)
{
    switch (Message)
    {
    case MainTabPageCreateWindow:
        {
            HWND hwnd;

            if (FwEnabled)
            {
                ULONG thinRows;

                thinRows = PhGetIntegerSetting(L"ThinRows") ? TN_STYLE_THIN_ROWS : 0;
                hwnd = CreateWindow(
                    PH_TREENEW_CLASSNAME,
                    NULL,
                    WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_BORDER | TN_STYLE_ICONS | TN_STYLE_DOUBLE_BUFFERED | thinRows,
                    0,
                    0,
                    3,
                    3,
                    PhMainWndHandle,
                    NULL,
                    NULL,
                    NULL);

                if (!hwnd)
                    return FALSE;
            }
            else
            {
                *(HWND*)Parameter1 = CreateDialog(
                    PluginInstance->DllBase,
                    MAKEINTRESOURCE(IDD_FWERROR),
                    PhMainWndHandle,
                    FwTabErrorDialogProc);

                return TRUE;
            }

            FwTreeNewCreated = TRUE;

            InitializeFwTreeList(hwnd);

            PhRegisterCallback(
                &FwItemAddedEvent,
                FwItemAddedHandler,
                NULL,
                &FwItemAddedRegistration
                );
            PhRegisterCallback(
                &FwItemModifiedEvent,
                FwItemModifiedHandler,
                NULL,
                &FwItemModifiedRegistration
                );
            PhRegisterCallback(
                &FwItemRemovedEvent,
                FwItemRemovedHandler,
                NULL,
                &FwItemRemovedRegistration
                );
            PhRegisterCallback(
                &FwItemsUpdatedEvent,
                FwItemsUpdatedHandler,
                NULL,
                &FwItemsUpdatedRegistration
                );

            *(HWND*)Parameter1 = hwnd;
        }
        return TRUE;
    case MainTabPageLoadSettings:
        {
            NOTHING;
        }
        return TRUE;
    case MainTabPageSaveSettings:
        {
            SaveSettingsFwTreeList();
        }
        return TRUE;
    case MainTabPageExportContent:
        {
            NOTHING;
        }
        return TRUE;
    case MainTabPageFontChanged:
        {
            HFONT font = (HFONT)Parameter1;

            if (FwTreeNewHandle)
                SendMessage(FwTreeNewHandle, WM_SETFONT, (WPARAM)Parameter1, TRUE);
        }
        break;
    }

    return FALSE;
}

VOID InitializeFwTab(
    VOID
    )
{
    PH_MAIN_TAB_PAGE page;
    PPH_PLUGIN toolStatusPlugin;

    memset(&page, 0, sizeof(PH_MAIN_TAB_PAGE));
    PhInitializeStringRef(&page.Name, L"Firewall");
    page.Callback = FwTabPageCallback;
    addedTabPage = ProcessHacker_CreateTabPage(PhMainWndHandle, &page);

    if (toolStatusPlugin = PhFindPlugin(TOOLSTATUS_PLUGIN_NAME))
    {
        ToolStatusInterface = PhGetPluginInformation(toolStatusPlugin)->Interface;

        if (ToolStatusInterface->Version <= TOOLSTATUS_INTERFACE_VERSION)
        {
            PTOOLSTATUS_TAB_INFO tabInfo;

            tabInfo = ToolStatusInterface->RegisterTabInfo(addedTabPage->Index);
            tabInfo->BannerText = L"Search Firewall";
            tabInfo->ActivateContent = FwToolStatusActivateContent;
            tabInfo->GetTreeNewHandle = FwToolStatusGetTreeNewHandle;
        }
    }
}

VOID InitializeFwTreeList(
    _In_ HWND hwnd
    )
{
    FwTreeNewHandle = hwnd;
    PhSetControlTheme(FwTreeNewHandle, L"explorer");

    TreeNew_SetCallback(FwTreeNewHandle, FwTreeNewCallback, NULL);
    TreeNew_SetRedraw(FwTreeNewHandle, FALSE);

    PhAddTreeNewColumn(FwTreeNewHandle, FW_COLUMN_PROCESSBASENAME, TRUE, L"Name", 140, PH_ALIGN_LEFT, FW_COLUMN_PROCESSBASENAME, DT_PATH_ELLIPSIS);
    PhAddTreeNewColumn(FwTreeNewHandle, FW_COLUMN_ACTION, TRUE, L"Action", 70, PH_ALIGN_LEFT, FW_COLUMN_ACTION, 0);
    PhAddTreeNewColumn(FwTreeNewHandle, FW_COLUMN_DIRECTION, TRUE, L"Direction", 40, PH_ALIGN_LEFT, FW_COLUMN_DIRECTION, 0);
    PhAddTreeNewColumn(FwTreeNewHandle, FW_COLUMN_RULENAME, TRUE, L"Rule", 240, PH_ALIGN_LEFT, FW_COLUMN_RULENAME, 0);
    PhAddTreeNewColumn(FwTreeNewHandle, FW_COLUMN_RULEDESCRIPTION, FALSE, L"Description", 180, PH_ALIGN_LEFT, FW_COLUMN_RULEDESCRIPTION, 0);
    PhAddTreeNewColumn(FwTreeNewHandle, FW_COLUMN_PROCESSFILENAME, FALSE, L"File Path", 80, PH_ALIGN_LEFT, FW_COLUMN_PROCESSFILENAME, DT_PATH_ELLIPSIS);
    PhAddTreeNewColumnEx(FwTreeNewHandle, FW_COLUMN_LOCALADDRESS, TRUE, L"Local Address", 220, PH_ALIGN_RIGHT, FW_COLUMN_LOCALADDRESS, DT_RIGHT, TRUE);
    PhAddTreeNewColumnEx(FwTreeNewHandle, FW_COLUMN_LOCALPORT, TRUE, L"Local Port", 50, PH_ALIGN_LEFT, FW_COLUMN_LOCALPORT, DT_LEFT, TRUE);
    PhAddTreeNewColumnEx(FwTreeNewHandle, FW_COLUMN_REMOTEADDRESS, TRUE, L"Remote Address", 220, PH_ALIGN_RIGHT, FW_COLUMN_REMOTEADDRESS, DT_RIGHT, TRUE);
    PhAddTreeNewColumnEx(FwTreeNewHandle, FW_COLUMN_REMOTEPORT, TRUE, L"Remote Port", 50, PH_ALIGN_LEFT, FW_COLUMN_REMOTEPORT, DT_LEFT, TRUE);
    PhAddTreeNewColumn(FwTreeNewHandle, FW_COLUMN_PROTOCOL, TRUE, L"Protocol", 60, PH_ALIGN_LEFT, FW_COLUMN_PROTOCOL, 0);
    PhAddTreeNewColumn(FwTreeNewHandle, FW_COLUMN_USER, FALSE, L"User", 120, PH_ALIGN_LEFT, FW_COLUMN_USER, DT_PATH_ELLIPSIS);
   
    LoadSettingsFwTreeList();

    TreeNew_SetRedraw(FwTreeNewHandle, TRUE);
    //TreeNew_SetSort(FwTreeNewHandle, 0, DescendingSortOrder);

    PhInitializeTreeNewFilterSupport(&FilterSupport, hwnd, FwNodeList);

    if (ToolStatusInterface)
    {
        PhRegisterCallback(ToolStatusInterface->SearchChangedEvent, FwSearchChangedHandler, NULL, &SearchChangedRegistration);
        PhAddTreeNewFilter(&FilterSupport, FwSearchFilterCallback, NULL);
    }
}

VOID LoadSettingsFwTreeList(
    VOID
    )
{
    PPH_STRING settings;
    //PH_INTEGER_PAIR sortSettings;

    settings = PhGetStringSetting(SETTING_NAME_FW_TREE_LIST_COLUMNS);
    PhCmLoadSettings(FwTreeNewHandle, &settings->sr);
    PhDereferenceObject(settings);

    //sortSettings = PhGetIntegerPairSetting(SETTING_NAME_FW_TREE_LIST_SORT);
    //TreeNew_SetSort(FwTreeNewHandle, (ULONG)sortSettings.X, (PH_SORT_ORDER)sortSettings.Y);
}

VOID SaveSettingsFwTreeList(
    VOID
    )
{
    PPH_STRING settings;
    //PH_INTEGER_PAIR sortSettings;
    //ULONG sortColumn;
    //PH_SORT_ORDER sortOrder;
        
    if (!FwTreeNewCreated)  
        return;

    settings = PhCmSaveSettings(FwTreeNewHandle);
    PhSetStringSetting2(SETTING_NAME_FW_TREE_LIST_COLUMNS, &settings->sr);
    PhDereferenceObject(settings);

    //TreeNew_GetSort(FwTreeNewHandle, &sortColumn, &sortOrder);
    //sortSettings.X = sortColumn;
    //sortSettings.Y = sortOrder;
    //PhSetIntegerPairSetting(SETTING_NAME_FW_TREE_LIST_SORT, sortSettings);
}

PFW_EVENT_ITEM AddFwNode(
    _In_ PFW_EVENT_ITEM FwItem
    )
{
    PhReferenceObject(FwItem);
    PhInitializeTreeNewNode(&FwItem->Node);

    memset(FwItem->TextCache, 0, sizeof(PH_STRINGREF) * FW_COLUMN_MAXIMUM);
    FwItem->Node.TextCache = FwItem->TextCache;
    FwItem->Node.TextCacheSize = FW_COLUMN_MAXIMUM;

    PhAcquireQueuedLockExclusive(&FwLock);
    PhInsertItemList(FwNodeList, 0, FwItem);
    PhReleaseQueuedLockExclusive(&FwLock);
        
    if (FilterSupport.NodeList)
        FwItem->Node.Visible = PhApplyTreeNewFiltersToNode(&FilterSupport, &FwItem->Node);

    TreeNew_NodesStructured(FwTreeNewHandle);

    return FwItem;
}

VOID RemoveFwNode(
    _In_ PFW_EVENT_ITEM FwNode
    )
{
    ULONG index;

    PhAcquireQueuedLockExclusive(&FwLock);
    if ((index = PhFindItemList(FwNodeList, FwNode)) != -1)
        PhRemoveItemList(FwNodeList, index); 
    PhReleaseQueuedLockExclusive(&FwLock);
        
    PhClearReference(&FwNode->TooltipText);
    PhClearReference(&FwNode->ProcessNameString);
    PhClearReference(&FwNode->ProcessBaseString);
    PhClearReference(&FwNode->LocalPortString);
    PhClearReference(&FwNode->LocalAddressString);
    PhClearReference(&FwNode->RemotePortString);
    PhClearReference(&FwNode->RemoteAddressString);
    PhClearReference(&FwNode->FwRuleNameString);
    PhClearReference(&FwNode->FwRuleDescriptionString);
    PhClearReference(&FwNode->FwRuleLayerNameString);
    PhClearReference(&FwNode->FwRuleLayerDescriptionString);

    if (FwNode->Icon)
        DestroyIcon(FwNode->Icon);

    PhDereferenceObject(FwNode);

    TreeNew_NodesStructured(FwTreeNewHandle);
}

VOID UpdateFwNode(
    _In_ PFW_EVENT_ITEM FwNode
    )
{
    memset(FwNode->TextCache, 0, sizeof(PH_STRINGREF) * FW_COLUMN_MAXIMUM);

    PhInvalidateTreeNewNode(&FwNode->Node, TN_CACHE_ICON);
    TreeNew_NodesStructured(FwTreeNewHandle);
}

static int __cdecl FwTreeNewCompareIndex(
    _In_ const void *_elem1,
    _In_ const void *_elem2
    ) 
{
    PFW_EVENT_ITEM node1 = *(PFW_EVENT_ITEM*)_elem1;
    PFW_EVENT_ITEM node2 = *(PFW_EVENT_ITEM*)_elem2;

    int sortResult = !uint64cmp(node1->Node.Index, node2->Node.Index);

    return PhModifySort(sortResult, DescendingSortOrder);
}

BOOLEAN NTAPI FwTreeNewCallback(
    _In_ HWND hwnd,
    _In_ PH_TREENEW_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2,
    _In_opt_ PVOID Context
    )
{
    PFW_EVENT_ITEM node;

    switch (Message)
    {
    case TreeNewGetChildren:
        {
            PPH_TREENEW_GET_CHILDREN getChildren = Parameter1;

            if (!getChildren->Node)
            {
                qsort(FwNodeList->Items, FwNodeList->Count, sizeof(PVOID), FwTreeNewCompareIndex);

                getChildren->Children = (PPH_TREENEW_NODE *)FwNodeList->Items;
                getChildren->NumberOfChildren = FwNodeList->Count;
            }
        }
        return TRUE;
    case TreeNewIsLeaf:
        {
            PPH_TREENEW_IS_LEAF isLeaf = (PPH_TREENEW_IS_LEAF)Parameter1;

            isLeaf->IsLeaf = TRUE;
        }
        return TRUE;
    case TreeNewGetCellText:
        {
            PPH_TREENEW_GET_CELL_TEXT getCellText = (PPH_TREENEW_GET_CELL_TEXT)Parameter1;

            node = (PFW_EVENT_ITEM)getCellText->Node;

            switch (getCellText->Id)
            {
            case FW_COLUMN_PROCESSBASENAME:
                {
                    if (node->ProcessBaseString)
                    {
                        getCellText->Text = PhGetStringRef(node->ProcessBaseString);
                    }
                    else
                    {
                        PhInitializeStringRef(&getCellText->Text, L"System");
                    }
                }
                break;
            case FW_COLUMN_ACTION:
                {
                    switch (node->FwRuleEventType)
                    {
                    case FWPM_NET_EVENT_TYPE_CLASSIFY_DROP:
                    case FWPM_NET_EVENT_TYPE_CAPABILITY_DROP:
                        PhInitializeStringRef(&getCellText->Text, L"DROP");
                        break;
                    case FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW:    
                    case FWPM_NET_EVENT_TYPE_CAPABILITY_ALLOW:
                        PhInitializeStringRef(&getCellText->Text, L"ALLOW");
                        break;
                    case FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC:
                        PhInitializeStringRef(&getCellText->Text, L"DROP_MAC");
                        break;
                    case FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP:
                        PhInitializeStringRef(&getCellText->Text, L"IPSEC_KERNEL_DROP");
                        break;
                    case FWPM_NET_EVENT_TYPE_IPSEC_DOSP_DROP:
                        PhInitializeStringRef(&getCellText->Text, L"IPSEC_DOSP_DROP");
                        break;
                    case FWPM_NET_EVENT_TYPE_IKEEXT_MM_FAILURE:
                        PhInitializeStringRef(&getCellText->Text, L"IKEEXT_MM_FAILURE");
                        break;
                    case FWPM_NET_EVENT_TYPE_IKEEXT_QM_FAILURE:
                        PhInitializeStringRef(&getCellText->Text, L"QM_FAILURE");
                        break;
                    case FWPM_NET_EVENT_TYPE_IKEEXT_EM_FAILURE:
                        PhInitializeStringRef(&getCellText->Text, L"EM_FAILURE");
                        break;
                    default:
                        PhInitializeStringRef(&getCellText->Text, L"Unknown");
                        break;
                    }
                }
                break;
            case FW_COLUMN_DIRECTION:
                {
                    if (node->Loopback)
                        PhInitializeStringRef(&getCellText->Text, L"Loopback");
                    else
                    {
                        switch (node->FwRuleEventDirection)
                        {
                        case FWP_DIRECTION_INBOUND:
                            PhInitializeStringRef(&getCellText->Text, L"In");
                            break;
                        case FWP_DIRECTION_OUTBOUND:
                            PhInitializeStringRef(&getCellText->Text, L"Out");
                            break;
                        default:
                            PhInitializeStringRef(&getCellText->Text, L"Unknown");
                            break;
                        }
                    }
                }
                break;
            case FW_COLUMN_RULENAME:
                getCellText->Text = PhGetStringRef(node->FwRuleNameString);
                break; 
            case FW_COLUMN_RULEDESCRIPTION:
                getCellText->Text = PhGetStringRef(node->FwRuleDescriptionString);
                break;
            case FW_COLUMN_PROCESSFILENAME:
                getCellText->Text = PhGetStringRef(node->ProcessNameString);
                break;
            case FW_COLUMN_USER:
                getCellText->Text = PhGetStringRef(node->UserNameString);
                break;
            case FW_COLUMN_LOCALADDRESS:
                getCellText->Text = PhGetStringRef(node->LocalAddressString);
                break;
            case FW_COLUMN_LOCALPORT:
                getCellText->Text = PhGetStringRef(node->LocalPortString);
                break;
            case FW_COLUMN_REMOTEADDRESS:
                getCellText->Text = PhGetStringRef(node->RemoteAddressString);
                break;
            case FW_COLUMN_REMOTEPORT:
                getCellText->Text = PhGetStringRef(node->RemotePortString);
                break;
            case FW_COLUMN_PROTOCOL:
                getCellText->Text = node->ProtocalString;
                break;
            default:
                return FALSE;
            }

            getCellText->Flags = TN_CACHE;
        }
        return TRUE;
    case TreeNewGetNodeIcon:
        {
            PPH_TREENEW_GET_NODE_ICON getNodeIcon = (PPH_TREENEW_GET_NODE_ICON)Parameter1;
            node = (PFW_EVENT_ITEM)getNodeIcon->Node;
    
            if (node->Icon)
            {
                getNodeIcon->Icon = node->Icon;
            }
            else
            {
                node->Icon = PhGetFileShellIcon(PhGetString(node->ProcessFileNameString), L".exe", FALSE);
                getNodeIcon->Icon = node->Icon;
            }

            getNodeIcon->Flags = TN_CACHE;
        }
        return TRUE;
    case TreeNewGetNodeColor:
        {
            PPH_TREENEW_GET_NODE_COLOR getNodeColor = (PPH_TREENEW_GET_NODE_COLOR)Parameter1;
            node = (PFW_EVENT_ITEM)getNodeColor->Node;

            switch (node->FwRuleEventType)
            {
            case FWPM_NET_EVENT_TYPE_CLASSIFY_DROP:
            case FWPM_NET_EVENT_TYPE_CAPABILITY_DROP:
                getNodeColor->ForeColor = RGB(0xff, 0x0, 0x0);
                break;
            case FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW:
            case FWPM_NET_EVENT_TYPE_CAPABILITY_ALLOW:
                getNodeColor->ForeColor = RGB(0x0, 0x0, 0x0);
                break;
            }

            getNodeColor->Flags = TN_CACHE;
        }
        return TRUE;
    case TreeNewSortChanged:
        {
            TreeNew_GetSort(hwnd, &FwTreeNewSortColumn, &FwTreeNewSortOrder);
            // Force a rebuild to sort the items.
            TreeNew_NodesStructured(hwnd);
        }
        return TRUE;
    case TreeNewKeyDown:
        {
            PPH_TREENEW_KEY_EVENT keyEvent = Parameter1;

            switch (keyEvent->VirtualKey)
            {
            case 'C':
                if (GetKeyState(VK_CONTROL) < 0)
                    HandleFwCommand(ID_EVENT_COPY);
                break;
            case 'A':
                TreeNew_SelectRange(FwTreeNewHandle, 0, -1);
                break;
            }
        }
        return TRUE;
    case TreeNewHeaderRightClick:
        {
            PH_TN_COLUMN_MENU_DATA data;

            data.TreeNewHandle = hwnd;
            data.MouseEvent = Parameter1;
            data.DefaultSortColumn = 0;
            data.DefaultSortOrder = AscendingSortOrder;
            PhInitializeTreeNewColumnMenu(&data);

            data.Selection = PhShowEMenu(data.Menu, hwnd, PH_EMENU_SHOW_LEFTRIGHT,
                PH_ALIGN_LEFT | PH_ALIGN_TOP, data.MouseEvent->ScreenLocation.x, data.MouseEvent->ScreenLocation.y);
            PhHandleTreeNewColumnMenu(&data);
            PhDeleteTreeNewColumnMenu(&data);
        }
        return TRUE;
    case TreeNewContextMenu:
        {
            PPH_TREENEW_MOUSE_EVENT mouseEvent = (PPH_TREENEW_MOUSE_EVENT)Parameter1;

            ShowFwContextMenu(mouseEvent->Location);
        }
        return TRUE;
    case TreeNewDestroying:
        return TRUE;
    }

    return FALSE;
}

PFW_EVENT_ITEM GetSelectedFwItem(
    VOID
    )
{
    PFW_EVENT_ITEM FwItem = NULL;

    for (ULONG i = 0; i < FwNodeList->Count; i++)
    {
        PFW_EVENT_ITEM node = (PFW_EVENT_ITEM)FwNodeList->Items[i];

        if (node->Node.Selected)
        {
            FwItem = node;
            break;
        }
    }

    return FwItem;
}

VOID GetSelectedFwItems(
    _Out_ PFW_EVENT_ITEM **FwItems,
    _Out_ PULONG NumberOfFwItems
    )
{
    PPH_LIST list = PhCreateList(2);

    for (ULONG i = 0; i < FwNodeList->Count; i++)
    {
        PFW_EVENT_ITEM node = (PFW_EVENT_ITEM)FwNodeList->Items[i];

        if (node->Node.Selected)
        {
            PhAddItemList(list, node);
        }
    }

    *FwItems = PhAllocateCopy(list->Items, sizeof(PVOID) * list->Count);
    *NumberOfFwItems = list->Count;

    PhDereferenceObject(list);
}

VOID DeselectAllFwNodes(
    VOID
    )
{
    TreeNew_DeselectRange(FwTreeNewHandle, 0, -1);
}

VOID SelectAndEnsureVisibleFwNode(
    _In_ PFW_EVENT_ITEM FwNode
    )
{
    DeselectAllFwNodes();

    if (!FwNode->Node.Visible)
        return;

    TreeNew_SetFocusNode(FwTreeNewHandle, &FwNode->Node);
    TreeNew_SetMarkNode(FwTreeNewHandle, &FwNode->Node);
    TreeNew_SelectRange(FwTreeNewHandle, FwNode->Node.Index, FwNode->Node.Index);
    TreeNew_EnsureVisible(FwTreeNewHandle, &FwNode->Node);
}

VOID CopyFwList(
    VOID
    )
{
    PPH_STRING text;

    text = PhGetTreeNewText(FwTreeNewHandle, 0);
    PhSetClipboardString(FwTreeNewHandle, &text->sr);
    PhDereferenceObject(text);
}

VOID WriteFwList(
    __inout PPH_FILE_STREAM FileStream,
    _In_ ULONG Mode
    )
{
    PPH_LIST lines = PhGetGenericTreeNewLines(FwTreeNewHandle, Mode);

    for (ULONG i = 0; i < lines->Count; i++)
    {
        PPH_STRING line;

        line = lines->Items[i];
        
        PhWriteStringAsUtf8FileStream(FileStream, &line->sr);
        PhDereferenceObject(line);
        PhWriteStringAsUtf8FileStream2(FileStream, L"\r\n");
    }

    PhDereferenceObject(lines);
}

VOID HandleFwCommand(
    _In_ ULONG Id
    )
{
    switch (Id)
    {
    case ID_FW_PROPERTIES:
        {
            PFW_EVENT_ITEM fwItem = GetSelectedFwItem();
            //PhCreateThread(0, ShowFwRuleProperties, fwItem);
        }
        break;
    case ID_EVENT_COPY:
        {
            CopyFwList();
        }
        break;
    }
}

VOID InitializeFwMenu(
    _In_ PPH_EMENU Menu,
    _In_ PFW_EVENT_ITEM *FwItems,
    _In_ ULONG NumberOfFwItems
    )
{
    if (NumberOfFwItems == 0)
    {
        PhSetFlagsAllEMenuItems(Menu, PH_EMENU_DISABLED, PH_EMENU_DISABLED);
    }
    else
    {
        PhSetFlagsAllEMenuItems(Menu, PH_EMENU_DISABLED, PH_EMENU_DISABLED);
        PhEnableEMenuItem(Menu, ID_EVENT_COPY, TRUE);
    }
}

VOID ShowFwContextMenu(
    _In_ POINT Location
    )
{
    PFW_EVENT_ITEM *fwItems;
    ULONG numberOfFwItems;

    GetSelectedFwItems(&fwItems, &numberOfFwItems);

    if (numberOfFwItems != 0)
    {
        PPH_EMENU menu;
        PPH_EMENU_ITEM item;

        menu = PhCreateEMenu();
        PhLoadResourceEMenuItem(menu, PluginInstance->DllBase, MAKEINTRESOURCE(IDR_FW_MENU), 0);
        InitializeFwMenu(menu, fwItems, numberOfFwItems);

        if (item = PhShowEMenu(
            menu,
            PhMainWndHandle,
            PH_EMENU_SHOW_LEFTRIGHT,
            PH_ALIGN_LEFT | PH_ALIGN_TOP,
            Location.x,
            Location.y
            ))
        {
            HandleFwCommand(item->Id);
        }

        PhDestroyEMenu(menu);
    }

    PhFree(fwItems);
}

VOID NTAPI FwItemAddedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PFW_EVENT_ITEM fwItem = (PFW_EVENT_ITEM)Parameter;

    PhReferenceObject(fwItem);
    ProcessHacker_Invoke(PhMainWndHandle, OnFwItemAdded, fwItem);
}

VOID NTAPI FwItemModifiedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    ProcessHacker_Invoke(PhMainWndHandle, OnFwItemModified, (PFW_EVENT_ITEM)Parameter);
}

VOID NTAPI FwItemRemovedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    ProcessHacker_Invoke(PhMainWndHandle, OnFwItemRemoved, (PFW_EVENT_ITEM)Parameter);
}

VOID NTAPI FwItemsUpdatedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    ProcessHacker_Invoke(PhMainWndHandle, OnFwItemsUpdated, NULL);
}

VOID NTAPI OnFwItemAdded(
    _In_ PVOID Parameter
    )
{
    PFW_EVENT_ITEM fwItem = (PFW_EVENT_ITEM)Parameter;
    PFW_EVENT_ITEM fwNode;

    if (!FwNeedsRedraw)
    {
       // TreeNew_SetRedraw(FwTreeNewHandle, FALSE);
       // FwNeedsRedraw = TRUE;
    }

    fwNode = AddFwNode(fwItem);
    PhDereferenceObject(fwItem);
}

VOID NTAPI OnFwItemModified(
    _In_ PVOID Parameter
    )
{
    PFW_EVENT_ITEM fwNode = (PFW_EVENT_ITEM)Parameter;

    UpdateFwNode(fwNode);
}

VOID NTAPI OnFwItemRemoved(
    _In_ PVOID Parameter
    )
{
    PFW_EVENT_ITEM fwNode = (PFW_EVENT_ITEM)Parameter;

    if (!FwNeedsRedraw)
    {
        TreeNew_SetRedraw(FwTreeNewHandle, FALSE);
        FwNeedsRedraw = TRUE;
    }

    RemoveFwNode(fwNode);
}

VOID NTAPI OnFwItemsUpdated(
    _In_ PVOID Parameter
    )
{
    if (FwNeedsRedraw)
    {
        TreeNew_SetRedraw(FwTreeNewHandle, TRUE);
        FwNeedsRedraw = FALSE;
    }

    // Text invalidation
    for (ULONG i = 0; i < FwNodeList->Count; i++)
    {
        PFW_EVENT_ITEM node = (PFW_EVENT_ITEM)FwNodeList->Items[i];

        // The name and file name never change, so we don't invalidate that.
        memset(&node->TextCache[1], 0, sizeof(PH_STRINGREF) * (FW_COLUMN_MAXIMUM - 2));
        // Always get the newest tooltip text from the process tree.
        PhSwapReference(&node->TooltipText, NULL);
    }

    InvalidateRect(FwTreeNewHandle, NULL, FALSE);
}

VOID NTAPI FwSearchChangedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    if (!FwEnabled)
        return;

    PhApplyTreeNewFilters(&FilterSupport);
}

BOOLEAN NTAPI FwSearchFilterCallback(
    _In_ PPH_TREENEW_NODE Node,
    _In_opt_ PVOID Context
    )
{
    PFW_EVENT_ITEM fwNode = (PFW_EVENT_ITEM)Node;
    PTOOLSTATUS_WORD_MATCH wordMatch = ToolStatusInterface->WordMatch;

    if (PhIsNullOrEmptyString(ToolStatusInterface->GetSearchboxText()))
        return TRUE;
    
    if (fwNode->ProcessNameString)
    {
        if (wordMatch(&fwNode->ProcessNameString->sr))
            return TRUE;
    }

    if (fwNode->LocalAddressString)
    {
        if (wordMatch(&fwNode->LocalAddressString->sr))
            return TRUE;
    }

    if (fwNode->RemoteAddressString)
    {
        if (wordMatch(&fwNode->RemoteAddressString->sr))
            return TRUE;
    }

    return FALSE;
}

VOID NTAPI FwToolStatusActivateContent(
    _In_ BOOLEAN Select
    )
{
    SetFocus(FwTreeNewHandle);

    if (Select)
    {
        if (TreeNew_GetFlatNodeCount(FwTreeNewHandle) > 0)
            SelectAndEnsureVisibleFwNode((PFW_EVENT_ITEM)TreeNew_GetFlatNode(FwTreeNewHandle, 0));
    }
}

HWND NTAPI FwToolStatusGetTreeNewHandle(
    VOID
    )
{
    return FwTreeNewHandle;
}

INT_PTR CALLBACK FwTabErrorDialogProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            if (!PhGetOwnTokenAttributes().Elevated)
            {
                SendMessage(GetDlgItem(hwndDlg, IDC_RESTART), BCM_SETSHIELD, 0, TRUE);
            }
            else
            {
                //SetDlgItemText(hwndDlg, IDC_ERROR, L"Unable to start the kernel event tracing session.");
                ShowWindow(GetDlgItem(hwndDlg, IDC_RESTART), SW_HIDE);
            }
        }
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDC_RESTART:
                ProcessHacker_PrepareForEarlyShutdown(PhMainWndHandle);

                if (PhShellProcessHacker(
                    PhMainWndHandle,
                    L"-v -selecttab Firewall",
                    SW_SHOW,
                    PH_SHELL_EXECUTE_ADMIN,
                    PH_SHELL_APP_PROPAGATE_PARAMETERS | PH_SHELL_APP_PROPAGATE_PARAMETERS_IGNORE_VISIBILITY,
                    0,
                    NULL
                    ))
                {
                    ProcessHacker_Destroy(PhMainWndHandle);
                }
                else
                {
                    ProcessHacker_CancelEarlyShutdown(PhMainWndHandle);
                }

                break;
            }
        }
        break;
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        {
            SetBkMode((HDC)wParam, TRANSPARENT);
            return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
        }
        break;
    }

    return FALSE;
}
