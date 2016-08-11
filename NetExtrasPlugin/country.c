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

BOOLEAN GeoDbLoaded = FALSE;
static MMDB_s GeoDb = { 0 };


// Copied from mstcpip.h (due to PH-SDK conflicts).
// Note: Ipv6 versions are already available from ws2ipdef.h and did not need copying.

#define INADDR_ANY (ULONG)0x00000000
#define INADDR_LOOPBACK 0x7f000001

FORCEINLINE 
BOOLEAN 
IN4_IS_ADDR_UNSPECIFIED(_In_ CONST IN_ADDR *a)
{
    return (BOOLEAN)(a->s_addr == INADDR_ANY);
}

FORCEINLINE 
BOOLEAN 
IN4_IS_ADDR_LOOPBACK(_In_ CONST IN_ADDR *a)
{
    return (BOOLEAN)(*((PUCHAR)a) == 0x7f); // 127/8
}
// end copy from mstcpip.h


VOID LoadGeoLiteDb(VOID)
{
    PPH_STRING directory;
    PPH_STRING path;

    directory = PH_AUTO(PhGetApplicationDirectory());
    path = PhaCreateString(DATABASE_PATH);
    path = PH_AUTO(PhConcatStringRef2(&directory->sr, &path->sr));

    if (MMDB_open(path->Buffer, MMDB_MODE_MMAP, &GeoDb) == MMDB_SUCCESS)
    {
        time_t systemTime;

        // Query the current time
        time(&systemTime);

        // Check if the Geoip database is older than 6 months (182 days = approx. 6 months).
        if ((systemTime - GeoDb.metadata.build_epoch) > (182 * 24 * 60 * 60))
        {
            // TODO: Warn about old database...
        }

        if (GeoDb.metadata.ip_version == 6)
        {
            // Database includes ipv6 entires.
        }

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
    _Out_ PPH_STRING* CountryName
    )
{
    PPH_STRING countryCode = NULL;
    PPH_STRING countryName = NULL;
    MMDB_entry_data_s mmdb_entry;
    MMDB_lookup_result_s mmdb_lookup;
    INT mmdb_error = 0;

    if (!GeoDbLoaded)
        return FALSE;

    if (RemoteAddress.Type == PH_IPV4_NETWORK_TYPE)
    {
        SOCKADDR_IN ipv4SockAddr;

        if (IN4_IS_ADDR_UNSPECIFIED(&RemoteAddress.InAddr))
            return FALSE;

        if (IN4_IS_ADDR_LOOPBACK(&RemoteAddress.InAddr))
            return FALSE;
        
        memset(&ipv4SockAddr, 0, sizeof(SOCKADDR_IN));
        memset(&mmdb_lookup, 0, sizeof(MMDB_lookup_result_s));

        ipv4SockAddr.sin_family = AF_INET;
        ipv4SockAddr.sin_addr = RemoteAddress.InAddr;
      
        mmdb_lookup = MMDB_lookup_sockaddr(
            &GeoDb, 
            (struct sockaddr*)&ipv4SockAddr, 
            &mmdb_error
            );
    }
    else
    {
        SOCKADDR_IN6 ipv6SockAddr;

        if (IN6_IS_ADDR_UNSPECIFIED(&RemoteAddress.In6Addr))
            return FALSE;

        if (IN6_IS_ADDR_LOOPBACK(&RemoteAddress.In6Addr))
            return FALSE;

        memset(&ipv6SockAddr, 0, sizeof(SOCKADDR_IN6));
        memset(&mmdb_lookup, 0, sizeof(MMDB_lookup_result_s));

        ipv6SockAddr.sin6_family = AF_INET6;
        ipv6SockAddr.sin6_addr = RemoteAddress.In6Addr;
     
        mmdb_lookup = MMDB_lookup_sockaddr(
            &GeoDb, 
            (struct sockaddr*)&ipv6SockAddr, 
            &mmdb_error
            );
    }

    if (mmdb_error == 0 && mmdb_lookup.found_entry)
    {
        memset(&mmdb_entry, 0, sizeof(MMDB_entry_data_s));

        if (MMDB_get_value(&mmdb_lookup.entry, &mmdb_entry, "country", "iso_code", NULL) == MMDB_SUCCESS)
        {
            if (mmdb_entry.has_data && mmdb_entry.type == MMDB_DATA_TYPE_UTF8_STRING)
            {
                countryCode = PhConvertUtf8ToUtf16Ex((PCHAR)mmdb_entry.utf8_string, mmdb_entry.data_size);
            }
        }

        if (MMDB_get_value(&mmdb_lookup.entry, &mmdb_entry, "country", "names", "en", NULL) == MMDB_SUCCESS)
        {
            if (mmdb_entry.has_data && mmdb_entry.type == MMDB_DATA_TYPE_UTF8_STRING)
            {
                countryName = PhConvertUtf8ToUtf16Ex((PCHAR)mmdb_entry.utf8_string, mmdb_entry.data_size);
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