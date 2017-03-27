/*
 * Process Hacker Extra Plugins -
 *   NT Product Policy Plugin
 *
 * Copyright (C) 2017 dmex
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

#include "main.h"

// Policy header
// dmex: copied from https://github.com/vyvojar/poldump
typedef struct _wind_pol_hdr 
{
    ULONG Size; 	// Size of everything.
    ULONG DataLength; 	// Always sz-0x18.
    ULONG Endpad; 	// End padding. Usually 4.
    ULONG Tainted; 	// 1 if tainted.
    ULONG Padding; 	// Always 1
} wind_pol_hdr;

// Policy entry
// dmex: modified based on https://github.com/vyvojar/poldump
typedef struct _wind_pol_ent 
{
    USHORT Size; 	    // Size of whole entry.
    USHORT NameLength;  // Size of the following field, in bytes.
    USHORT DataType; 	// Field type
    USHORT DataLength;  // Field size
    ULONG DataFlags; 	// Field flags
    ULONG Padding; 	    // Always 0
    WCHAR Name[1]; 	    // WCHAR name, NOT zero terminated!
} wind_pol_ent;

// dmex: modified based on https://github.com/vyvojar/poldump
static PPH_LIST wind_pol_unpack(_In_ PBYTE blob)
{
    PPH_LIST policyEntryList;
    wind_pol_hdr* h = (PVOID)blob;
    wind_pol_ent* e = PTR_ADD_OFFSET(h, sizeof(wind_pol_hdr));
    PVOID endptr = PTR_ADD_OFFSET(e, h->DataLength);

    if (h->Size >= USHRT_MAX)
        return NULL;
    if (h->Endpad != 0x4)
        return NULL;
    if (h->DataLength + 0x18 != h->Size)
        return NULL;
    if (blob[h->Size - 0x4] != 0x45)
        return NULL;

    policyEntryList = PhCreateList(0x200);

    while ((PVOID)e < endptr)
    {
        PhAddItemList(policyEntryList, e);

        e = PTR_ADD_OFFSET(e, e->Size);
    }

    return policyEntryList;
}

VOID QueryLicenseValue(_In_ PWSTR Name, _In_ ULONG Type)
{
    UNICODE_STRING us;
    ULONG type = Type;
    ULONG bufferLength = 0;
    PVOID buffer;

    RtlInitUnicodeString(&us, Name);

    if (NT_SUCCESS(NtQueryLicenseValue(&us, &type, NULL, 0, &bufferLength)))
        return;

    buffer = PhAllocate(bufferLength);
    memset(buffer, 0, bufferLength);

    if (!NT_SUCCESS(NtQueryLicenseValue(&us, &type, buffer, bufferLength, &bufferLength)))
    {
        PhFree(buffer);
        return;
    }

    PhFree(buffer);
}

PPH_LIST QueryProductPolicies(VOID)
{
    static PH_STRINGREF policyKeyName = PH_STRINGREF_INIT(L"System\\CurrentControlSet\\Control\\ProductOptions");
    static PH_STRINGREF policyValueName = PH_STRINGREF_INIT(L"ProductPolicy");
    HANDLE keyHandle = NULL;
    PVOID valueBuffer = NULL;
    PKEY_VALUE_PARTIAL_INFORMATION buffer = NULL;
    PPH_LIST policyList = NULL;
    PPH_LIST policyEntries = NULL;

    if (!NT_SUCCESS(PhOpenKey(
        &keyHandle,
        KEY_READ,
        PH_KEY_LOCAL_MACHINE,
        &policyKeyName,
        0
        )))
    {
        goto CleanupExit;
    }

    if (NT_SUCCESS(PhQueryValueKey(
        keyHandle,
        &policyValueName,
        KeyValuePartialInformation,
        &buffer
        )))
    {
        if (buffer->Type == REG_BINARY && buffer->DataLength)
        {
            valueBuffer = PhAllocate(buffer->DataLength);
            memset(valueBuffer, 0, buffer->DataLength);
            memcpy(valueBuffer, buffer->Data, buffer->DataLength);
        }
    }

    if (!valueBuffer)
        goto CleanupExit;

    if (!(policyEntries = wind_pol_unpack(valueBuffer)))
        goto CleanupExit;

    policyList = PhCreateList(policyEntries->Count);

    for (ULONG i = 0; i < policyEntries->Count; i++)
    {
        PNT_POLICY_ENTRY entry;
        wind_pol_ent* e = policyEntries->Items[i];

        //QueryLicenseValue(e->Name, e->DataType);

        entry = PhAllocate(sizeof(NT_POLICY_ENTRY));
        memset(entry, 0, sizeof(NT_POLICY_ENTRY));

        entry->Name = PhCreateStringEx(e->Name, e->NameLength);

        switch (e->DataType)
        {
        case REG_DWORD:
            entry->Value = PhFormatUInt64(*(ULONG*)PTR_ADD_OFFSET(e->Name, e->NameLength), TRUE);
            break;
        case REG_SZ:
            entry->Value = PhCreateStringEx((PWSTR)PTR_ADD_OFFSET(e->Name, e->NameLength), e->DataLength);
            break;
        case REG_BINARY:
            entry->Value = PhBufferToHexString((PBYTE)PTR_ADD_OFFSET(e->Name, e->NameLength), e->DataLength);
            break;
        }

        PhAddItemList(policyList, entry);
    }

CleanupExit:

    if (keyHandle)
        NtClose(keyHandle);

    if (valueBuffer)
        PhFree(valueBuffer);

    if (buffer)
        PhFree(buffer);

    if (policyEntries)
        PhDereferenceObject(policyEntries);

    return policyList;
}