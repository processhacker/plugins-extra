#ifndef FWTABP_H
#define FWTABP_H

HWND NTAPI FwTabCreateFunction(
    _In_ PVOID Context
    );

VOID NTAPI FwTabSelectionChangedCallback(
    _In_ PVOID Parameter1,
    _In_ PVOID Parameter2,
    _In_ PVOID Parameter3,
    _In_ PVOID Context
    );

VOID NTAPI FwTabSaveContentCallback(
    _In_ PVOID Parameter1,
    _In_ PVOID Parameter2,
    _In_ PVOID Parameter3,
    _In_ PVOID Context
    );

VOID NTAPI FwTabFontChangedCallback(
    _In_ PVOID Parameter1,
    _In_ PVOID Parameter2,
    _In_ PVOID Parameter3,
    _In_ PVOID Context
    );

BOOLEAN FwNodeHashtableCompareFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
    );

ULONG FwNodeHashtableHashFunction(
    _In_ PVOID Entry
    );

VOID InitializeFwTreeList(
    _In_ HWND hwnd
    );

PFW_EVENT_NODE AddFwNode(
    _In_ PFW_EVENT_ITEM FwItem
    );

VOID RemoveFwNode(
    _In_ PFW_EVENT_NODE FwNode
    );

VOID UpdateFwNode(
    _In_ PFW_EVENT_NODE FwNode
    );

BOOLEAN NTAPI FwTreeNewCallback(
    _In_ HWND hwnd,
    _In_ PH_TREENEW_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2,
    _In_opt_ PVOID Context
    );

PFW_EVENT_NODE GetSelectedFwItem(
    VOID
    );

VOID GetSelectedFwItems(
    _Out_ PFW_EVENT_NODE **FwItems,
    _Out_ PULONG NumberOfFwItems
    );

VOID DeselectAllFwNodes(
    VOID
    );

VOID SelectAndEnsureVisibleFwNode(
    _In_ PFW_EVENT_NODE FwNode
    );

VOID CopyFwList(
    VOID
    );

VOID WriteFwList(
    __inout PPH_FILE_STREAM FileStream,
    _In_ ULONG Mode
    );

VOID HandleFwCommand(
    _In_ ULONG Id
    );

VOID InitializeFwMenu(
    _In_ PPH_EMENU Menu,
    _In_ PFW_EVENT_NODE *FwItems,
    _In_ ULONG NumberOfFwItems
    );

VOID ShowFwContextMenu(
    _In_ POINT Location
    );

VOID NTAPI FwItemAddedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI FwItemModifiedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI FwItemRemovedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI FwItemsUpdatedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI OnFwItemAdded(
    _In_ PVOID Parameter
    );

VOID NTAPI OnFwItemModified(
    _In_ PVOID Parameter
    );

VOID NTAPI OnFwItemRemoved(
    _In_ PVOID Parameter
    );

VOID NTAPI OnFwItemsUpdated(
    _In_ PVOID Parameter
    );

BOOLEAN NTAPI FwSearchFilterCallback(
    _In_ PPH_TREENEW_NODE Node,
    _In_opt_ PVOID Context
    );

VOID NTAPI FwSearchChangedHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    );

VOID NTAPI FwToolStatusActivateContent(
    _In_ BOOLEAN Select
    );

HWND NTAPI FwToolStatusGetTreeNewHandle(
    VOID
    );

#endif
