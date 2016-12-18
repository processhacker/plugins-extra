#include "fwmon.h"
#include "wf.h"

HMODULE FwApiLibraryHandle = NULL;
_FWStatusMessageFromStatusCode FWStatusMessageFromStatusCode_I;
_FWOpenPolicyStore FWOpenPolicyStore_I;
_FWClosePolicyStore FWClosePolicyStore_I;
_FWEnumFirewallRules FWEnumFirewallRules_I;
_FWFreeFirewallRules FWFreeFirewallRules_I;

FW_POLICY_STORE_HANDLE PolicyStoreHandles[4];
#define MAIN_POLICY_STORE PolicyStoreHandles[0]
#define GPRSOP_POLICY_STORE PolicyStoreHandles[1]
#define DYNAMIC_POLICY_STORE PolicyStoreHandles[2]
#define DEFAULT_POLICY_STORE PolicyStoreHandles[3]

BOOLEAN InitializeFirewallApi(
    VOID
    )
{
    FwApiLibraryHandle = LoadLibrary(L"FirewallAPI.dll");
    FWStatusMessageFromStatusCode_I = PhGetProcedureAddress(FwApiLibraryHandle, "FWStatusMessageFromStatusCode", 0);
    FWOpenPolicyStore_I = PhGetProcedureAddress(FwApiLibraryHandle, "FWOpenPolicyStore", 0);
    FWClosePolicyStore_I = PhGetProcedureAddress(FwApiLibraryHandle, "FWClosePolicyStore", 0);
    FWEnumFirewallRules_I = PhGetProcedureAddress(FwApiLibraryHandle, "FWEnumFirewallRules", 0);
    FWFreeFirewallRules_I = PhGetProcedureAddress(FwApiLibraryHandle, "FWFreeFirewallRules", 0);

    FWOpenPolicyStore_I(
        FW_REDSTONE_BINARY_VERSION,
        NULL,
        FW_STORE_TYPE_LOCAL,
        FW_POLICY_ACCESS_RIGHT_READ_WRITE,
        FW_POLICY_STORE_FLAGS_NONE,
        &PolicyStoreHandles[0]
        );
    FWOpenPolicyStore_I(
        FW_REDSTONE_BINARY_VERSION,
        NULL,
        FW_STORE_TYPE_GP_RSOP,
        FW_POLICY_ACCESS_RIGHT_READ,
        FW_POLICY_STORE_FLAGS_NONE,
        &PolicyStoreHandles[1]
        );
    FWOpenPolicyStore_I(
        FW_REDSTONE_BINARY_VERSION,
        NULL,
        FW_STORE_TYPE_DYNAMIC,
        FW_POLICY_ACCESS_RIGHT_READ,
        FW_POLICY_STORE_FLAGS_NONE,
        &PolicyStoreHandles[2]
        );
    FWOpenPolicyStore_I(
        FW_REDSTONE_BINARY_VERSION,
        NULL,
        FW_STORE_TYPE_DEFAULTS,
        FW_POLICY_ACCESS_RIGHT_READ,
        FW_POLICY_STORE_FLAGS_NONE,
        &PolicyStoreHandles[3]
        );

    return TRUE;
}

VOID FreeFirewallApi(
    VOID
    )
{
    if (FWClosePolicyStore_I && PolicyStoreHandles[0])
        FWClosePolicyStore_I(PolicyStoreHandles[0]);

    if (FWClosePolicyStore_I && PolicyStoreHandles[1])
        FWClosePolicyStore_I(PolicyStoreHandles[1]);

    if (FWClosePolicyStore_I && PolicyStoreHandles[2])
        FWClosePolicyStore_I(PolicyStoreHandles[2]);

    if (FWClosePolicyStore_I && PolicyStoreHandles[3])
        FWClosePolicyStore_I(PolicyStoreHandles[3]);
}

BOOLEAN ValidateFirewallConnectivity(
    _In_ ULONG returnValue
    )
{
    if (returnValue == ERROR_INVALID_HANDLE || returnValue == RPC_S_UNKNOWN_IF)
        return FALSE;

    return TRUE;
}

VOID FwStatusMessageFromStatusCode(
    _In_ FW_RULE_STATUS code
    )
{
    ULONG statusLength = 0;

    FWStatusMessageFromStatusCode_I(code, NULL, &statusLength);
}

VOID EnumerateFirewallRules(
    _In_ FW_POLICY_STORE Store,
    _In_ FW_PROFILE_TYPE Type,
    _In_ FW_DIRECTION Direction,
    _In_ PFW_WALK_RULES Callback,
    _In_ PVOID Context
    )
{
    UINT32 uRuleCount = 0;
    ULONG result = 0;
    FW_POLICY_STORE_HANDLE storeHandle = NULL;
    PFW_RULE pRules = NULL;

    switch (Store)
    {
    case FW_POLICY_STORE_MAIN:
        storeHandle = MAIN_POLICY_STORE;
        break;
    case FW_POLICY_STORE_GPRSOP:
        storeHandle = GPRSOP_POLICY_STORE;
        break;
    case FW_POLICY_STORE_DYNAMIC:
        storeHandle = DYNAMIC_POLICY_STORE;
        break;
    default:
        storeHandle = DEFAULT_POLICY_STORE;
        break;
    }

    //FW_PROFILE_TYPE_CURRENT
    result = FWEnumFirewallRules_I(
        storeHandle,
        FW_RULE_STATUS_CLASS_OK,
        Type, 
        FW_ENUM_RULES_FLAG_INCLUDE_METADATA | FW_ENUM_RULES_FLAG_RESOLVE_NAME | FW_ENUM_RULES_FLAG_RESOLVE_DESCRIPTION | FW_ENUM_RULES_FLAG_RESOLVE_APPLICATION, 
        &uRuleCount, 
        &pRules
        );

    if (ValidateFirewallConnectivity(result))
    {
        for (PFW_RULE i = pRules; i; i = i->pNext)
        {
            if (Direction == FW_DIR_BOTH)
            {
                if (!Callback || !Callback(i, Context))
                    break;
            }
            else
            {
                if (i->Direction == Direction)
                {
                    if (!Callback || !Callback(i, Context))
                        break;
                }
            }
        }

        FWFreeFirewallRules_I(pRules);
    }
}