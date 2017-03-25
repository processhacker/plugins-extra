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
    ULONG cbSize; 	// Size of everything.
    ULONG data_sz; 	// Always sz-0x18.
    ULONG endpad; 	// End padding. Usually 4.
    ULONG tainted; 	// 1 if tainted.
    ULONG pad1; 	// Always 1
} wind_pol_hdr;

// Policy entry
// dmex: modified based on https://github.com/vyvojar/poldump
typedef struct _wind_pol_ent 
{
    USHORT cbSize; 	    // Size of whole entry.
    USHORT NameLength;  // Size of the following field, in bytes.
    USHORT type; 	    // Field type
    USHORT DataLength;  // Field size
    ULONG flags; 	    // Field flags
    ULONG pad0; 	    // Always 0
    WCHAR name[1]; 	    // WCHAR name, NOT zero terminated!
} wind_pol_ent;

// dmex: modified based on https://github.com/vyvojar/poldump
static PPH_LIST wind_pol_unpack(_In_ PBYTE blob)
{
    PPH_LIST policyEntryList;
    wind_pol_hdr* h = (PVOID)blob;
    wind_pol_ent* e = PTR_ADD_OFFSET(blob, sizeof(wind_pol_hdr));
    PVOID endptr = PTR_ADD_OFFSET(e, h->data_sz);

    if (h->cbSize >= USHRT_MAX)
        return NULL;
    if (h->endpad != 4)
        return NULL;
    if (h->data_sz + 0x18 != h->cbSize)
        return NULL;
    if (blob[h->cbSize - 4] != 0x45)
        return NULL;

    policyEntryList = PhCreateList(0x200);

    while ((PVOID)e < endptr)
    {
        PhAddItemList(policyEntryList, e);

        e = PTR_ADD_OFFSET(e, e->cbSize);
    }

    return policyEntryList;
}

PPH_LIST QueryProductPolicies(VOID)
{
    static PH_STRINGREF policyKeyName = PH_STRINGREF_INIT(L"System\\CurrentControlSet\\Control\\ProductOptions");
    static PH_STRINGREF policyValueName = PH_STRINGREF_INIT(L"ProductPolicy");
    HANDLE keyHandle;
    PVOID valueBuffer = NULL;
    PKEY_VALUE_PARTIAL_INFORMATION buffer;
    PPH_LIST policyList;
    PPH_LIST policyEntries;

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
        
        entry = PhAllocate(sizeof(NT_POLICY_ENTRY));
        memset(entry, 0, sizeof(NT_POLICY_ENTRY));

        entry->Name = PhCreateStringEx(e->name, e->NameLength);

        switch (e->type)
        {
        case REG_DWORD: 
            entry->Value = PhFormatUInt64(*(ULONG*)PTR_ADD_OFFSET(e->name, e->NameLength), TRUE);
            break;
        case REG_SZ:
            entry->Value = PhCreateStringEx((PWSTR)PTR_ADD_OFFSET(e->name, e->NameLength), e->DataLength);
            break;
        case REG_BINARY:
            {
                PBYTE value = (PBYTE)PTR_ADD_OFFSET(e->name, e->NameLength);

                entry->Value = PhBufferToHexString(value, e->DataLength);
            }
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