/*
 * Process Hacker Extra Plugins -
 *   Network Extras Plugin
 *
 * Copyright (C) 2016 dmex
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
#include "maxminddb.h"

static MMDB_s GeoDb = { 0 };
static BOOLEAN GeoDbLoaded = FALSE;

NTSYSAPI PSTR NTAPI RtlIpv4AddressToStringA(
    _In_ const IN_ADDR *Addr,
    _Out_writes_(16) PSTR S);

NTSYSAPI PSTR NTAPI RtlIpv6AddressToStringA(
    _In_ const IN6_ADDR *Addr,
    _Out_ PSTR S);

VOID LoadGeoLiteDb(VOID)
{
    if (MMDB_open("plugins\\maxminddb\\GeoLite2-Country.mmdb", MMDB_MODE_MMAP, &GeoDb) == MMDB_SUCCESS)
    {
        GeoDbLoaded = TRUE;
    }
}

VOID FreeGeoLiteDb(VOID)
{
    if (GeoDbLoaded)
    {
        MMDB_close(&GeoDb);
    }
}

BOOLEAN LookupCountryCode(
    _In_ PH_IP_ADDRESS RemoteAddress,
    _Out_ PPH_STRING* CountryCode,
    _Out_ PPH_STRING* CountryName)
{
    PPH_STRING countryCode = NULL;
    PPH_STRING countryName = NULL;
    time_t systemTime;
    MMDB_entry_data_s entry_data;
	MMDB_lookup_result_s lookup_result;
    int gai_error, mmdb_error;
    CHAR addressString[INET_ADDRSTRLEN] = "";

    if (!GeoDbLoaded)
        return FALSE;

    time(&systemTime);

    // 182 days = approx. 6 months
    if ((systemTime - GeoDb.metadata.build_epoch) < (182 * 24 * 60 * 60))
    {
        // valid
    }
    else
    {
        // expired
    }

    if (GeoDb.metadata.ip_version == 6)
    {
        // Database includes ipv6 entires.
    }

    if (RemoteAddress.Type == PH_IPV4_NETWORK_TYPE)
    {
        RtlIpv4AddressToStringA(&RemoteAddress.InAddr, addressString);
    }
    else
    {
        RtlIpv6AddressToStringA(&RemoteAddress.In6Addr, addressString);
    }
  
    //MMDB_lookup_result_s result = MMDB_lookup_sockaddr(&mmdb, &remoteAddr, &mmdb_error);
    lookup_result = MMDB_lookup_string(&GeoDb, addressString, &gai_error, &mmdb_error);

    if (lookup_result.found_entry)
    {
        //if (MMDB_get_value(&res.entry, &entry_data, "continent", "code", NULL) == MMDB_SUCCESS)

		if (MMDB_get_value(&lookup_result.entry, &entry_data, "country", "iso_code", NULL) == MMDB_SUCCESS)
		{
            if (entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING)
            {
                countryCode = PhConvertUtf8ToUtf16Ex((PCHAR)entry_data.utf8_string, entry_data.data_size);
            }
		}

        if (MMDB_get_value(&lookup_result.entry, &entry_data, "country", "names", "en", NULL) == MMDB_SUCCESS)
        {
            if (entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING)
            {
                countryName = PhConvertUtf8ToUtf16Ex((PCHAR)entry_data.utf8_string, entry_data.data_size);
            }
        }
    }

    if (countryCode && countryName)
    {
        *CountryCode = countryCode;
        *CountryName = countryName;
        return TRUE;
    }

    if (countryCode)
    {
        PhDereferenceObject(countryCode);
    }

    if (countryName)
    {
        PhDereferenceObject(countryName);
    }

    return FALSE;
}