/*
 * Process Hacker Extra Plugins -
 *   LSA Security Explorer Plugin
 *
 * Copyright (C) 2013 wj32
 * Copyright (C) 2015-2016 dmex
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

#include "explorer.h"

_Callback_ NTSTATUS SxpOpenLsaPolicy(
    _Out_ PHANDLE Handle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ PVOID Context
    )
{
    return PhOpenLsaPolicy(Handle, DesiredAccess, NULL);
}

_Callback_ NTSTATUS SxpOpenSelectedLsaAccount(
    _Out_ PHANDLE Handle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ PVOID Context
    )
{
    NTSTATUS status;
    LSA_HANDLE policyHandle;

    if (NT_SUCCESS(status = PhOpenLsaPolicy(&policyHandle, POLICY_LOOKUP_NAMES, NULL)))
    {
        status = LsaOpenAccount(policyHandle, Context, DesiredAccess, Handle);
        LsaClose(policyHandle);
    }

    return status;
}

_Callback_ NTSTATUS SxpOpenSelectedSamAccount(
    _Out_ PHANDLE Handle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ PVOID Context
    )
{
    NTSTATUS status;
    LSA_HANDLE policyHandle = NULL;
    SAM_HANDLE serverHandle = NULL;
    SAM_HANDLE domainHandle = NULL;
    PPOLICY_ACCOUNT_DOMAIN_INFO policyDomainInfo = NULL;
    
    __try
    {
        if (!NT_SUCCESS(status = PhOpenLsaPolicy(
            &policyHandle,
            POLICY_VIEW_LOCAL_INFORMATION,
            NULL
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = LsaQueryInformationPolicy(
            policyHandle,
            PolicyAccountDomainInformation,
            &policyDomainInfo
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = SamConnect(
            NULL,
            &serverHandle,
            SAM_SERVER_CONNECT | SAM_SERVER_LOOKUP_DOMAIN,
            NULL
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = SamOpenDomain(
            serverHandle,
            DOMAIN_LIST_ACCOUNTS | DOMAIN_LOOKUP,
            policyDomainInfo->DomainSid,
            &domainHandle
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = SamOpenUser(
            domainHandle,
            DesiredAccess,
            PtrToUlong(Context),
            Handle
            )))
        {
            __leave;
        }
    }
    __finally
    {
        if (domainHandle)
        {
            SamFreeMemory(domainHandle);
        }

        if (serverHandle)
        {
            SamFreeMemory(serverHandle);
        }

        if (policyDomainInfo)
        {
            LsaFreeMemory(policyDomainInfo);
        }

        if (policyHandle)
        {
            LsaClose(policyHandle);
        }
    }

    return status;
}
