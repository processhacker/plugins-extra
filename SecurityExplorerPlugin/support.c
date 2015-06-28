#include "explorer.h"

_Callback_ NTSTATUS SxStdGetObjectSecurity(
    _Out_ PSECURITY_DESCRIPTOR *SecurityDescriptor,
    _In_ SECURITY_INFORMATION SecurityInformation,
    _In_opt_ PVOID Context
    )
{
    NTSTATUS status;
    PPH_STD_OBJECT_SECURITY stdObjectSecurity;
    HANDLE handle;

    stdObjectSecurity = (PPH_STD_OBJECT_SECURITY)Context;

    if (
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"LsaAccount", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"LsaPolicy", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"LsaSecret", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"LsaTrusted", TRUE)
        )
    {
        PSECURITY_DESCRIPTOR securityDescriptor;

        status = stdObjectSecurity->OpenObject(
            &handle,
            PhGetAccessForGetSecurity(SecurityInformation),
            stdObjectSecurity->Context
            );

        if (!NT_SUCCESS(status))
            return status;

        status = LsaQuerySecurityObject(
            handle,
            SecurityInformation,
            &securityDescriptor
            );

        if (NT_SUCCESS(status))
        {
            *SecurityDescriptor = PhAllocateCopy(
                securityDescriptor,
                RtlLengthSecurityDescriptor(securityDescriptor)
                );
            LsaFreeMemory(securityDescriptor);
        }

        LsaClose(handle);
    }
    else if (
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"SamAlias", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"SamDomain", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"SamGroup", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"SamServer", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"SamUser", TRUE)
        )
    {
        PSECURITY_DESCRIPTOR securityDescriptor;

        status = stdObjectSecurity->OpenObject(
            &handle,
            PhGetAccessForGetSecurity(SecurityInformation),
            stdObjectSecurity->Context
            );

        if (!NT_SUCCESS(status))
            return status;

        status = SamQuerySecurityObject(
            handle,
            SecurityInformation,
            &securityDescriptor
            );

        if (NT_SUCCESS(status))
        {
            *SecurityDescriptor = PhAllocateCopy(
                securityDescriptor,
                RtlLengthSecurityDescriptor(securityDescriptor)
                );
            SamFreeMemory(securityDescriptor);
        }

        SamCloseHandle(handle);
    }
    else
    {
        status = PhStdGetObjectSecurity(SecurityDescriptor, SecurityInformation, Context);
    }

    return status;
}

_Callback_ NTSTATUS SxStdSetObjectSecurity(
    _In_ PSECURITY_DESCRIPTOR SecurityDescriptor,
    _In_ SECURITY_INFORMATION SecurityInformation,
    _In_opt_ PVOID Context
    )
{
    NTSTATUS status;
    PPH_STD_OBJECT_SECURITY stdObjectSecurity;
    HANDLE handle;

    stdObjectSecurity = (PPH_STD_OBJECT_SECURITY)Context;

    if (
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"LsaAccount", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"LsaPolicy", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"LsaSecret", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"LsaTrusted", TRUE)
        )
    {
        status = stdObjectSecurity->OpenObject(
            &handle,
            PhGetAccessForSetSecurity(SecurityInformation),
            stdObjectSecurity->Context
            );

        if (!NT_SUCCESS(status))
            return status;

        status = LsaSetSecurityObject(
            handle,
            SecurityInformation,
            SecurityDescriptor
            );

        LsaClose(handle);
    }
    else if (
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"SamAlias", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"SamDomain", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"SamGroup", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"SamServer", TRUE) ||
        PhEqualStringZ(stdObjectSecurity->ObjectType, L"SamUser", TRUE)
        )
    {
        status = stdObjectSecurity->OpenObject(
            &handle,
            PhGetAccessForSetSecurity(SecurityInformation),
            stdObjectSecurity->Context
            );

        if (!NT_SUCCESS(status))
            return status;

        status = SamSetSecurityObject(
            handle,
            SecurityInformation,
            SecurityDescriptor
            );

        SamCloseHandle(handle);
    }
    else
    {
        status = PhStdSetObjectSecurity(SecurityDescriptor, SecurityInformation, Context);
    }

    return status;
}
