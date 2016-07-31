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
                    MAKEINTRESOURCE(IDD_FWTABERROR),
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
                &FwItemAddedRegistration);

            PhRegisterCallback(
                &FwItemModifiedEvent,
                FwItemModifiedHandler,
                NULL,
                &FwItemModifiedRegistration);

            PhRegisterCallback(
                &FwItemRemovedEvent,
                FwItemRemovedHandler,
                NULL,
                &FwItemRemovedRegistration);

            PhRegisterCallback(
                &FwItemsUpdatedEvent,
                FwItemsUpdatedHandler,
                NULL,
                &FwItemsUpdatedRegistration);

            *(HWND*)Parameter1 = hwnd;
        }
        return TRUE;
    case MainTabPageLoadSettings:
        {
            // Nothing
        }
        return TRUE;
    case MainTabPageSaveSettings:
        {
            // Nothing
        }
        return TRUE;
    case MainTabPageExportContent:
        {

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

    if (toolStatusPlugin = PhFindPlugin(TOOLSTATUS_PLUGIN_NAME))
    {
        ToolStatusInterface = PhGetPluginInformation(toolStatusPlugin)->Interface;

        if (ToolStatusInterface->Version < TOOLSTATUS_INTERFACE_VERSION)
            ToolStatusInterface = NULL;
    }

    memset(&page, 0, sizeof(PH_MAIN_TAB_PAGE));
    PhInitializeStringRef(&page.Name, L"Firewall");
    page.Callback = FwTabPageCallback;
    addedTabPage = ProcessHacker_CreateTabPage(PhMainWndHandle, &page);

    if (ToolStatusInterface)
    {
        PTOOLSTATUS_TAB_INFO tabInfo;

        tabInfo = ToolStatusInterface->RegisterTabInfo(addedTabPage->Index);
        tabInfo->BannerText = L"Search Firewall";
        tabInfo->ActivateContent = FwToolStatusActivateContent;
        tabInfo->GetTreeNewHandle = FwToolStatusGetTreeNewHandle;
    }
}

VOID InitializeFwTreeList(
    _In_ HWND hwnd
    )
{
    FwTreeNewHandle = hwnd;
    PhSetControlTheme(FwTreeNewHandle, L"explorer");
    SendMessage(TreeNew_GetTooltips(FwTreeNewHandle), TTM_SETDELAYTIME, TTDT_AUTOPOP, 0x7fff);

    TreeNew_SetCallback(hwnd, FwTreeNewCallback, NULL);
    TreeNew_SetRedraw(hwnd, FALSE);

    PhAddTreeNewColumn(hwnd, FWTNC_PROCESSBASENAME, TRUE, L"Name", 100, PH_ALIGN_LEFT, MAXINT, DT_PATH_ELLIPSIS);
    PhAddTreeNewColumn(hwnd, FWTNC_PROCESSFILENAME, FALSE, L"File Path", 80, PH_ALIGN_LEFT, MAXINT, DT_PATH_ELLIPSIS);    
    PhAddTreeNewColumn(hwnd, FWTNC_DIRECTION, TRUE, L"Direction", 40, PH_ALIGN_LEFT, MAXINT, 0);
    PhAddTreeNewColumn(hwnd, FWTNC_ACTION, TRUE, L"Action", 70, PH_ALIGN_LEFT, MAXINT, 0);
    PhAddTreeNewColumn(hwnd, FWTNC_PROTOCOL, TRUE, L"Protocol", 60, PH_ALIGN_LEFT, MAXINT, 0);
    PhAddTreeNewColumnEx(hwnd, FWTNC_LOCALADDRESS, TRUE, L"Local Address", 220, PH_ALIGN_RIGHT, MAXINT, DT_RIGHT, TRUE);
    PhAddTreeNewColumnEx(hwnd, FWTNC_LOCALPORT, TRUE, L"Local Port", 50, PH_ALIGN_LEFT, MAXINT, DT_LEFT, TRUE);
    PhAddTreeNewColumnEx(hwnd, FWTNC_REMOTEADDRESS, TRUE, L"Remote Address", 220, PH_ALIGN_RIGHT, MAXINT, DT_RIGHT, TRUE);
    PhAddTreeNewColumnEx(hwnd, FWTNC_REMOTEPORT, TRUE, L"Remote Port", 50, PH_ALIGN_LEFT, MAXINT, DT_LEFT, TRUE);
    PhAddTreeNewColumn(hwnd, FWTNC_USER, FALSE, L"User", 120, PH_ALIGN_LEFT, MAXINT, DT_PATH_ELLIPSIS);
    PhAddTreeNewColumnEx(hwnd, FWTNC_TIME, TRUE, L"Time", 30, PH_ALIGN_LEFT, MAXINT, 0, TRUE);
    PhAddTreeNewColumn(hwnd, FWTNC_RULENAME, TRUE, L"Rule", 200, PH_ALIGN_LEFT, MAXINT, 0);
    PhAddTreeNewColumn(hwnd, FWTNC_RULEDESCRIPTION, TRUE, L"Description", 180, PH_ALIGN_LEFT, MAXINT, 0);
    PhAddTreeNewColumn(hwnd, FWTNC_INDEX, TRUE, L"Index", 10, PH_ALIGN_LEFT, MAXINT, 0);

    TreeNew_SetRedraw(hwnd, TRUE);
    TreeNew_SetSort(hwnd, FWTNC_INDEX, DescendingSortOrder);

    LoadSettingsFwTreeList();
 
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
    PH_INTEGER_PAIR sortSettings;

    settings = PhGetStringSetting(SETTING_NAME_FW_TREE_LIST_COLUMNS);
    PhCmLoadSettings(FwTreeNewHandle, &settings->sr);
    PhDereferenceObject(settings);

    sortSettings = PhGetIntegerPairSetting(SETTING_NAME_FW_TREE_LIST_SORT);
    TreeNew_SetSort(FwTreeNewHandle, (ULONG)sortSettings.X, (PH_SORT_ORDER)sortSettings.Y);
}

VOID SaveSettingsFwTreeList(
    VOID
    )
{
    PPH_STRING settings;
    PH_INTEGER_PAIR sortSettings;
    ULONG sortColumn;
    PH_SORT_ORDER sortOrder;
        
    if (!FwTreeNewCreated)  
        return;

    settings = PhCmSaveSettings(FwTreeNewHandle);
    PhSetStringSetting2(SETTING_NAME_FW_TREE_LIST_COLUMNS, &settings->sr);
    PhDereferenceObject(settings);

    TreeNew_GetSort(FwTreeNewHandle, &sortColumn, &sortOrder);
    sortSettings.X = sortColumn;
    sortSettings.Y = sortOrder;
    PhSetIntegerPairSetting(SETTING_NAME_FW_TREE_LIST_SORT, sortSettings);
}

PFW_EVENT_NODE AddFwNode(
    _In_ PFW_EVENT_ITEM FwItem
    )
{
    PFW_EVENT_NODE FwNode;

    FwNode = PhAllocate(sizeof(FW_EVENT_NODE));
    memset(FwNode, 0, sizeof(FW_EVENT_NODE));
    PhInitializeTreeNewNode(&FwNode->Node);

    FwNode->EventItem = FwItem;
    PhReferenceObject(FwItem);

    memset(FwNode->TextCache, 0, sizeof(PH_STRINGREF) * FWTNC_MAXIMUM);
    FwNode->Node.TextCache = FwNode->TextCache;
    FwNode->Node.TextCacheSize = FWTNC_MAXIMUM;

    PhAcquireQueuedLockExclusive(&FwLock);
    PhAddItemList(FwNodeList, FwNode);
    PhReleaseQueuedLockExclusive(&FwLock);
        
    if (FilterSupport.NodeList)
        FwNode->Node.Visible = PhApplyTreeNewFiltersToNode(&FilterSupport, &FwNode->Node);

    TreeNew_NodesStructured(FwTreeNewHandle);

    return FwNode;
}

VOID RemoveFwNode(
    _In_ PFW_EVENT_NODE FwNode
    )
{
    ULONG index;

    PhAcquireQueuedLockExclusive(&FwLock);

    // Remove from the hashtable/list and cleanup.
    if ((index = PhFindItemList(FwNodeList, FwNode)) != -1)
        PhRemoveItemList(FwNodeList, index);
    
    PhReleaseQueuedLockExclusive(&FwLock);
        
    if (FwNode->TooltipText)
        PhDereferenceObject(FwNode->TooltipText);

    if (FwNode->EventItem->TimeString)
        PhDereferenceObject(FwNode->EventItem->TimeString);
    if (FwNode->EventItem->UserNameString)
        PhDereferenceObject(FwNode->EventItem->UserNameString);
    if (FwNode->EventItem->ProcessNameString)
        PhDereferenceObject(FwNode->EventItem->ProcessNameString);
    if (FwNode->EventItem->ProcessBaseString)
        PhDereferenceObject(FwNode->EventItem->ProcessBaseString);
    if (FwNode->EventItem->LocalPortString)
        PhDereferenceObject(FwNode->EventItem->LocalPortString);
    if (FwNode->EventItem->LocalAddressString)
        PhDereferenceObject(FwNode->EventItem->LocalAddressString);
    if (FwNode->EventItem->RemotePortString)
        PhDereferenceObject(FwNode->EventItem->RemotePortString);
    if (FwNode->EventItem->RemoteAddressString)
        PhDereferenceObject(FwNode->EventItem->RemoteAddressString);
    if (FwNode->EventItem->FwRuleNameString)
        PhDereferenceObject(FwNode->EventItem->FwRuleNameString);
    if (FwNode->EventItem->FwRuleDescriptionString)
        PhDereferenceObject(FwNode->EventItem->FwRuleDescriptionString);
    if (FwNode->EventItem->FwRuleLayerNameString)
        PhDereferenceObject(FwNode->EventItem->FwRuleLayerNameString);
    if (FwNode->EventItem->FwRuleLayerDescriptionString)
        PhDereferenceObject(FwNode->EventItem->FwRuleLayerDescriptionString);

    if (FwNode->EventItem->IndexString)
        PhDereferenceObject(FwNode->EventItem->IndexString);

    //if (FwNode->EventItem->Icon)
    //    DestroyIcon(FwNode->EventItem->Icon);

    PhDereferenceObject(FwNode->EventItem);
    PhFree(FwNode);

    TreeNew_NodesStructured(FwTreeNewHandle);
}

VOID UpdateFwNode(
    _In_ PFW_EVENT_NODE FwNode
    )
{
    memset(FwNode->TextCache, 0, sizeof(PH_STRINGREF) * FWTNC_MAXIMUM);

    PhInvalidateTreeNewNode(&FwNode->Node, TN_CACHE_ICON);
    TreeNew_NodesStructured(FwTreeNewHandle);
}



#define SORT_FUNCTION(Column) FwTreeNewCompare##Column
#define BEGIN_SORT_FUNCTION(Column) static int __cdecl FwTreeNewCompare##Column( \
    _In_ const void *_elem1, \
    _In_ const void *_elem2 \
    ) \
{ \
    PFW_EVENT_NODE node1 = *(PFW_EVENT_NODE*)_elem1; \
    PFW_EVENT_NODE node2 = *(PFW_EVENT_NODE*)_elem2; \
    PFW_EVENT_ITEM fwItem1 = node1->EventItem; \
    PFW_EVENT_ITEM fwItem2 = node2->EventItem; \
    int sortResult = 0;

#define END_SORT_FUNCTION \
    if (sortResult == 0) \
        sortResult = uintcmp(fwItem1->Index, fwItem2->Index); \
    \
    return PhModifySort(sortResult, FwTreeNewSortOrder); \
}

BEGIN_SORT_FUNCTION(Name)
{
    sortResult = PhCompareStringWithNull(fwItem1->ProcessBaseString, fwItem2->ProcessBaseString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(FilePath)
{
    sortResult = PhCompareStringWithNull(fwItem1->ProcessNameString, fwItem2->ProcessNameString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Direction)
{
    sortResult = PhCompareStringRef(&fwItem1->DirectionString, &fwItem2->DirectionString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Action)
{
    sortResult = PhCompareStringRef(&fwItem1->FwRuleActionString, &fwItem2->FwRuleActionString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Protocol)
{
    sortResult = PhCompareStringRef(&fwItem1->ProtocalString, &fwItem2->ProtocalString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(LocalAddress)
{
    sortResult = PhCompareStringWithNull(fwItem1->LocalAddressString, fwItem2->LocalAddressString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(LocalPort)
{
    sortResult = PhCompareStringWithNull(fwItem1->LocalPortString, fwItem2->LocalPortString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(RemoteAddress)
{
    sortResult = PhCompareStringWithNull(fwItem1->RemoteAddressString, fwItem2->RemoteAddressString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(RemotePort)
{
    sortResult = PhCompareStringWithNull(fwItem1->RemotePortString, fwItem2->RemotePortString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(User)
{
    sortResult = PhCompareStringWithNull(fwItem1->UserNameString, fwItem2->UserNameString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Time)
{
    sortResult = uint64cmp(fwItem1->AddedTime.QuadPart, fwItem2->AddedTime.QuadPart);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Rule)
{
    sortResult = PhCompareStringWithNull(fwItem1->FwRuleNameString, fwItem2->FwRuleNameString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Description)
{
    sortResult = PhCompareStringWithNull(fwItem1->FwRuleDescriptionString, fwItem2->FwRuleDescriptionString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Index)
{
    sortResult = uintcmp(fwItem1->Index, fwItem2->Index);
}
END_SORT_FUNCTION

BOOLEAN NTAPI FwTreeNewCallback(
    _In_ HWND hwnd,
    _In_ PH_TREENEW_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2,
    _In_opt_ PVOID Context
    )
{
    PFW_EVENT_NODE node;

    switch (Message)
    {
    case TreeNewGetChildren:
        {
            PPH_TREENEW_GET_CHILDREN getChildren = Parameter1;

            if (!getChildren->Node)
            {
                static PVOID sortFunctions[] =
                {
                    SORT_FUNCTION(Name),
                    SORT_FUNCTION(FilePath),
                    SORT_FUNCTION(Direction),
                    SORT_FUNCTION(Action),
                    SORT_FUNCTION(Protocol),
                    SORT_FUNCTION(LocalAddress),
                    SORT_FUNCTION(LocalPort),
                    SORT_FUNCTION(RemoteAddress),
                    SORT_FUNCTION(RemotePort),
                    SORT_FUNCTION(User),
                    SORT_FUNCTION(Time),
                    SORT_FUNCTION(Rule),
                    SORT_FUNCTION(Description),
                    SORT_FUNCTION(Index)
                };
                int (__cdecl *sortFunction)(const void *, const void *);

                if (FwTreeNewSortColumn < FWTNC_MAXIMUM)
                    sortFunction = sortFunctions[FwTreeNewSortColumn];
                else
                    sortFunction = NULL;

                if (sortFunction)
                {
                    qsort(FwNodeList->Items, FwNodeList->Count, sizeof(PVOID), sortFunction);
                }

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
            PFW_EVENT_ITEM fwItem;

            node = (PFW_EVENT_NODE)getCellText->Node;
            fwItem = node->EventItem;

            switch (getCellText->Id)
            {            
            case FWTNC_TIME:
                getCellText->Text = PhGetStringRef(node->EventItem->TimeString);
                break;
            case FWTNC_ACTION:
                getCellText->Text = node->EventItem->FwRuleActionString;
                break;
            case FWTNC_RULENAME:
                getCellText->Text = PhGetStringRef(node->EventItem->FwRuleNameString);
                break; 
            case FWTNC_RULEDESCRIPTION:
                getCellText->Text = PhGetStringRef(node->EventItem->FwRuleDescriptionString);
                break;
            case FWTNC_PROCESSBASENAME:
                getCellText->Text = PhGetStringRef(node->EventItem->ProcessBaseString);
                break;
            case FWTNC_PROCESSFILENAME:
                getCellText->Text = PhGetStringRef(node->EventItem->ProcessNameString);
                break;
            case FWTNC_USER:
                getCellText->Text = PhGetStringRef(node->EventItem->UserNameString);
                break;
            case FWTNC_LOCALADDRESS:
                getCellText->Text = PhGetStringRef(node->EventItem->LocalAddressString);
                break;
            case FWTNC_LOCALPORT:
                getCellText->Text = PhGetStringRef(node->EventItem->LocalPortString);
                break;
            case FWTNC_REMOTEADDRESS:
                getCellText->Text = PhGetStringRef(node->EventItem->RemoteAddressString);
                break;
            case FWTNC_REMOTEPORT:
                getCellText->Text = PhGetStringRef(node->EventItem->RemotePortString);
                break;
            case FWTNC_PROTOCOL:
                getCellText->Text = node->EventItem->ProtocalString;
                break;
            case FWTNC_DIRECTION:
                getCellText->Text = node->EventItem->DirectionString;
                break;
            case FWTNC_INDEX:
                getCellText->Text = PhGetStringRef(node->EventItem->IndexString);
                break;
            default:
                return FALSE;
            }

            getCellText->Flags = TN_CACHE;
        }
        return TRUE;
    //case TreeNewGetNodeIcon:
    //    {
    //        PPH_TREENEW_GET_NODE_ICON getNodeIcon = (PPH_TREENEW_GET_NODE_ICON)Parameter1;
    //        node = (PFW_EVENT_NODE)getNodeIcon->Node;
    //
    //        if (node->EventItem->Icon)
    //        {
    //            getNodeIcon->Icon = node->EventItem->Icon;
    //        }
    //        else
    //        {
    //            PhGetStockApplicationIcon(&getNodeIcon->Icon, NULL);
    //        }
    //
    //        getNodeIcon->Flags = TN_CACHE;
    //    }
    //    return TRUE;
    case TreeNewGetCellTooltip:
        {
            PPH_TREENEW_GET_CELL_TOOLTIP getCellTooltip = (PPH_TREENEW_GET_CELL_TOOLTIP)Parameter1;
            node = (PFW_EVENT_NODE)getCellTooltip->Node;

            if (getCellTooltip->Column->Id != 0)
                return FALSE;

            //if (!node->TooltipText)
            //{
                // TODO
            //}
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
            case VK_RETURN:
                //EtHandleDiskCommand(ID_EVENT_?);
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
    case TreeNewLeftDoubleClick:
        {
            PPH_TREENEW_MOUSE_EVENT mouseEvent = (PPH_TREENEW_MOUSE_EVENT)Parameter1;
            node = (PFW_EVENT_NODE)mouseEvent->Node;

            //PhCreateThread(0, ShowFwRuleProperties, node);
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

PFW_EVENT_NODE GetSelectedFwItem(
    VOID
    )
{
    PFW_EVENT_NODE FwItem = NULL;

    for (ULONG i = 0; i < FwNodeList->Count; i++)
    {
        PFW_EVENT_NODE node = (PFW_EVENT_NODE)FwNodeList->Items[i];

        if (node->Node.Selected)
        {
            FwItem = node;
            break;
        }
    }

    return FwItem;
}

VOID GetSelectedFwItems(
    _Out_ PFW_EVENT_NODE **FwItems,
    _Out_ PULONG NumberOfFwItems
    )
{
    PPH_LIST list = PhCreateList(2);

    for (ULONG i = 0; i < FwNodeList->Count; i++)
    {
        PFW_EVENT_NODE node = (PFW_EVENT_NODE)FwNodeList->Items[i];

        if (node->Node.Selected)
        {
            PhAddItemList(list, node->EventItem);
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
    _In_ PFW_EVENT_NODE FwNode
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
            PFW_EVENT_NODE fwItem = GetSelectedFwItem();
            //PhCreateThread(0, ShowFwRuleProperties, fwItem);
        }
        break;
    }
}

VOID InitializeFwMenu(
    _In_ PPH_EMENU Menu,
    _In_ PFW_EVENT_NODE* FwItems,
    _In_ ULONG NumberOfFwItems
    )
{
    //PPH_EMENU_ITEM item;

    if (NumberOfFwItems == 0)
    {
        PhSetFlagsAllEMenuItems(Menu, PH_EMENU_DISABLED, PH_EMENU_DISABLED);
    }
    else if (NumberOfFwItems == 1)
    {
        // Stuff
        //item = PhFindEMenuItem(Menu, 0, L"?", 0);

        // Stuff
        //item->Flags |= 0;
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
    PFW_EVENT_NODE* fwItems;
    ULONG numberOfFwItems;

    GetSelectedFwItems(&fwItems, &numberOfFwItems);

    if (numberOfFwItems != 0)
    {
        PPH_EMENU menu;
        PPH_EMENU_ITEM item;

        menu = PhCreateEMenu();

        PhLoadResourceEMenuItem(
            menu, 
            PluginInstance->DllBase,
            MAKEINTRESOURCE(IDR_FW), 
            0
            );

        //PhSetFlagsEMenuItem(menu, ID_EVENT_?, PH_EMENU_DEFAULT, PH_EMENU_DEFAULT);
        InitializeFwMenu(menu, fwItems, numberOfFwItems);

        item = PhShowEMenu(
            menu,
            PhMainWndHandle,
            PH_EMENU_SHOW_LEFTRIGHT,
            PH_ALIGN_LEFT | PH_ALIGN_TOP,
            Location.x,
            Location.y
            );

        if (item)
        {
            HandleFwCommand(item->Id);
        }

        PhDestroyEMenu(menu);
    }

    PhFree(fwItems);
}

static VOID NTAPI FwItemAddedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PFW_EVENT_ITEM fwItem = (PFW_EVENT_ITEM)Parameter;

    PhReferenceObject(fwItem);
    ProcessHacker_Invoke(PhMainWndHandle, OnFwItemAdded, fwItem);
}

static VOID NTAPI FwItemModifiedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    ProcessHacker_Invoke(PhMainWndHandle, OnFwItemModified, (PFW_EVENT_ITEM)Parameter);
}

static VOID NTAPI FwItemRemovedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    ProcessHacker_Invoke(PhMainWndHandle, OnFwItemRemoved, (PFW_EVENT_ITEM)Parameter);
}

static VOID NTAPI FwItemsUpdatedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    ProcessHacker_Invoke(PhMainWndHandle, OnFwItemsUpdated, NULL);
}

static VOID NTAPI OnFwItemAdded(
    _In_ PVOID Parameter
    )
{
    PFW_EVENT_ITEM fwItem = (PFW_EVENT_ITEM)Parameter;
    PFW_EVENT_NODE fwNode;

    if (!FwNeedsRedraw)
    {
        //TreeNew_SetRedraw(FwTreeNewHandle, FALSE);
        //FwNeedsRedraw = TRUE;
    }

    fwNode = AddFwNode(fwItem);
    PhDereferenceObject(fwItem);
}

static VOID NTAPI OnFwItemModified(
    _In_ PVOID Parameter
    )
{
    PFW_EVENT_NODE fwNode = (PFW_EVENT_NODE)Parameter;

    UpdateFwNode(fwNode);
}

static VOID NTAPI OnFwItemRemoved(
    _In_ PVOID Parameter
    )
{
    PFW_EVENT_NODE fwNode = (PFW_EVENT_NODE)Parameter;

    if (!FwNeedsRedraw)
    {
        //TreeNew_SetRedraw(FwTreeNewHandle, FALSE);
        //FwNeedsRedraw = TRUE;
    }

    RemoveFwNode(fwNode);
}

static VOID NTAPI OnFwItemsUpdated(
    _In_ PVOID Parameter
    )
{
    if (FwNeedsRedraw)
    {
        //TreeNew_SetRedraw(FwTreeNewHandle, TRUE);
        //FwNeedsRedraw = FALSE;
    }

    // Text invalidation
    for (ULONG i = 0; i < FwNodeList->Count; i++)
    {
        PFW_EVENT_NODE node = (PFW_EVENT_NODE)FwNodeList->Items[i];

        // The name and file name never change, so we don't invalidate that.
        memset(&node->TextCache[1], 0, sizeof(PH_STRINGREF) * (FWTNC_MAXIMUM - 2));
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
    PFW_EVENT_NODE fwNode = (PFW_EVENT_NODE)Node;
    PTOOLSTATUS_WORD_MATCH wordMatch = ToolStatusInterface->WordMatch;

    if (PhIsNullOrEmptyString(ToolStatusInterface->GetSearchboxText()))
        return TRUE;

    if (wordMatch(&fwNode->EventItem->ProcessNameString->sr))
        return TRUE;

    if (wordMatch(&fwNode->EventItem->LocalAddressString->sr))
        return TRUE;

    if (wordMatch(&fwNode->EventItem->RemoteAddressString->sr))
        return TRUE;

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
            SelectAndEnsureVisibleFwNode((PFW_EVENT_NODE)TreeNew_GetFlatNode(FwTreeNewHandle, 0));
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
