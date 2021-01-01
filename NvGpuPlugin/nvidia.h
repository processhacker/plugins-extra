/*
 * Process Hacker Extra Plugins -
 *   Nvidia GPU Plugin
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

#include "main.h"
#include <pshpack8.h>

// rev
#define NVAPI_MAX_USAGES_PER_GPU    33
#define NVAPI_MAX_CLOCKS_PER_GPU    0x120
#define NVAPI_MAX_COOLERS_PER_GPU   3
#define NVAPI_MIN_COOLER_LEVEL      0
#define NVAPI_MAX_COOLER_LEVEL      100
#define NVAPI_MAX_COOLER_LEVELS     24
#define NVAPI_MAX_PROCESSES         128

// rev
typedef PVOID (WINAPIV *_NvAPI_QueryInterface)(NvU32 FunctionOffset);
_NvAPI_QueryInterface NvAPI_QueryInterface;

// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetShaderPipeCount)(_In_ NvPhysicalGpuHandle hPhysicalGPU, NvU32* pShaderPipeCount);
_NvAPI_GPU_GetShaderPipeCount NvAPI_GPU_GetShaderPipeCount;

// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetShaderSubPipeCount)(_In_ NvPhysicalGpuHandle hPhysicalGPU, NvU32* pShaderSubPipeCount);
_NvAPI_GPU_GetShaderSubPipeCount NvAPI_GPU_GetShaderSubPipeCount;


// rev
typedef enum _NV_RAM_TYPE
{
    NV_RAM_TYPE_NONE,
    NV_RAM_TYPE_SDRAM,
    NV_RAM_TYPE_DDR1,
    NV_RAM_TYPE_DDR2,
    NV_RAM_TYPE_GDDR2,
    NV_RAM_TYPE_GDDR3,
    NV_RAM_TYPE_GDDR4,
    NV_RAM_TYPE_DDR3,
    NV_RAM_TYPE_GDDR5,
    NV_RAM_TYPE_LPDDR2
} NV_RAM_TYPE;

// rev 
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetRamType)(_In_ NvPhysicalGpuHandle hPhysicalGPU, NV_RAM_TYPE* pRamType);
_NvAPI_GPU_GetRamType NvAPI_GPU_GetRamType;


// rev 
typedef enum _NV_RAM_MAKER
{
    NV_RAM_MAKER_NONE,
    NV_RAM_MAKER_SAMSUNG,
    NV_RAM_MAKER_QIMONDA,
    NV_RAM_MAKER_ELPIDA,
    NV_RAM_MAKER_ETRON,
    NV_RAM_MAKER_NANYA,
    NV_RAM_MAKER_HYNIX,
    NV_RAM_MAKER_MOSEL,
    NV_RAM_MAKER_WINBOND,
    NV_RAM_MAKER_ELITE,
    NV_RAM_MAKER_MICRON
} NV_RAM_MAKER;

// rev 
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetRamMaker)(_In_ NvPhysicalGpuHandle hPhysicalGPU, NV_RAM_MAKER* pRamMaker);
_NvAPI_GPU_GetRamMaker NvAPI_GPU_GetRamMaker;

// rev 
typedef NvAPI_Status(WINAPIV *_NvAPI_GPU_GetRamBusWidth)(_In_ NvPhysicalGpuHandle hPhysicalGPU, NvU32* pRamBusWidth);
_NvAPI_GPU_GetRamBusWidth NvAPI_GPU_GetRamBusWidth;

// rev 
typedef NvAPI_Status(WINAPIV *_NvAPI_GPU_GetRamBankCount)(_In_ NvPhysicalGpuHandle hPhysicalGPU, NvU32* pRamBankCount);
_NvAPI_GPU_GetRamBankCount NvAPI_GPU_GetRamBankCount;



typedef enum _NV_FOUNDRY
{
    NV_FOUNDRY_NONE,
    NV_FOUNDRY_TSMC,
    NV_FOUNDRY_UMC,
    NV_FOUNDRY_IBM,
    NV_FOUNDRY_SMIC,
    NV_FOUNDRY_CSM,
    NV_FOUNDRY_TOSHIBA
} NV_FOUNDRY;

// rev 
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetFoundry)(_In_ NvPhysicalGpuHandle hPhysicalGPU, NV_FOUNDRY* pFoundry);
_NvAPI_GPU_GetFoundry NvAPI_GPU_GetFoundry;



// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetFBWidthAndLocation)(_In_ NvPhysicalGpuHandle hPhysicalGPU, NvU32* pWidth, NvU32* pLocation);
_NvAPI_GPU_GetFBWidthAndLocation NvAPI_GPU_GetFBWidthAndLocation;


// rev  (This has a different offset than the NvAPI_GPU_GetMemoryInfo function despite both returning the same struct).
typedef NvAPI_Status (WINAPIV *_NvAPI_GetDisplayDriverMemoryInfo)(_In_ NvDisplayHandle NvDispHandle, _Inout_ NV_DISPLAY_DRIVER_MEMORY_INFO* pMemoryInfo);
_NvAPI_GetDisplayDriverMemoryInfo NvAPI_GetDisplayDriverMemoryInfo;



// rev
typedef enum _NV_COOLER_TYPE
{
    NVAPI_COOLER_TYPE_NONE = 0,
    NVAPI_COOLER_TYPE_FAN,
    NVAPI_COOLER_TYPE_WATER,
    NVAPI_COOLER_TYPE_LIQUID_NO2,
} NV_COOLER_TYPE;

// rev
typedef enum _NV_COOLER_CONTROLLER
{
    NVAPI_COOLER_CONTROLLER_NONE = 0,
    NVAPI_COOLER_CONTROLLER_ADI,
    NVAPI_COOLER_CONTROLLER_INTERNAL,
} NV_COOLER_CONTROLLER;

// rev
typedef enum _NV_COOLER_POLICY
{
    NVAPI_COOLER_POLICY_NONE = 0,
    NVAPI_COOLER_POLICY_MANUAL,                     // Manual adjustment of cooler level. Gets applied right away independent of temperature or performance level.
    NVAPI_COOLER_POLICY_PERF,                       // GPU performance controls the cooler level.
    NVAPI_COOLER_POLICY_TEMPERATURE_DISCRETE = 4,   // Discrete thermal levels control the cooler level.
    NVAPI_COOLER_POLICY_TEMPERATURE_CONTINUOUS = 8, // Cooler level adjusted at continuous thermal levels.
    NVAPI_COOLER_POLICY_HYBRID,                     // Hybrid of performance and temperature levels.
} NV_COOLER_POLICY;

// rev
typedef enum _NV_COOLER_TARGET
{
    NVAPI_COOLER_TARGET_NONE = 0,
    NVAPI_COOLER_TARGET_GPU,
    NVAPI_COOLER_TARGET_MEMORY,
    NVAPI_COOLER_TARGET_POWER_SUPPLY = 4,
    NVAPI_COOLER_TARGET_ALL = 7                    // This cooler cools all of the components related to its target gpu.
} NV_COOLER_TARGET;

// rev
typedef enum _NV_COOLER_CONTROL
{
    NVAPI_COOLER_CONTROL_NONE = 0,
    NVAPI_COOLER_CONTROL_TOGGLE,                   // ON/OFF
    NVAPI_COOLER_CONTROL_VARIABLE,                 // Suppports variable control.
} NV_COOLER_CONTROL;

// rev
typedef enum _NV_COOLER_ACTIVITY_LEVEL
{
    NVAPI_INACTIVE = 0,                             // inactive or unsupported
    NVAPI_ACTIVE = 1,                               // active and spinning in case of fan
} NV_COOLER_ACTIVITY_LEVEL;

// rev
typedef struct _NV_GPU_COOLER_SETTINGS
{
    NvU32 version;                           // structure version
    NvU32 count;                             // number of associated coolers with the selected GPU
    struct
    {
        NV_COOLER_TYPE type;                 // type of cooler - FAN, WATER, LIQUID_NO2...
        NV_COOLER_CONTROLLER controller;     // internal, ADI...
        NvU32 defaultMinLevel;               // the min default value % of the cooler
        NvU32 defaultMaxLevel;               // the max default value % of the cooler
        NvU32 currentMinLevel;               // the current allowed min value % of the cooler
        NvU32 currentMaxLevel;               // the current allowed max value % of the cooler
        NvU32 currentLevel;                  // the current value % of the cooler
        NV_COOLER_POLICY defaultPolicy;      // cooler control policy - auto-perf, auto-thermal, manual, hybrid...
        NV_COOLER_POLICY currentPolicy;      // cooler control policy - auto-perf, auto-thermal, manual, hybrid...
        NV_COOLER_TARGET target;             // cooling target - GPU, memory, chipset, powersupply, canoas...
        NV_COOLER_CONTROL controlType;       // toggle or variable
        NV_COOLER_ACTIVITY_LEVEL active;     // is the cooler active - fan spinning...
    } cooler[NVAPI_MAX_COOLERS_PER_GPU];
} NV_GPU_COOLER_SETTINGS, *PNV_GPU_COOLER_SETTINGS;

#define NV_GPU_COOLER_SETTINGS_VER  MAKE_NVAPI_VERSION(NV_GPU_COOLER_SETTINGS, 1)

// rev
typedef struct _NV_GPU_SETCOOLER_LEVEL
{
    NvU32 version;                       //structure version
    struct
    {
        NvU32 currentLevel;              // the new value % of the cooler
        NV_COOLER_POLICY currentPolicy;  // the new cooler control policy - auto-perf, auto-thermal, manual, hybrid...
    } cooler[NVAPI_MAX_COOLERS_PER_GPU];
} NV_GPU_SETCOOLER_LEVEL;

#define NV_GPU_SETCOOLER_LEVEL_VER  MAKE_NVAPI_VERSION(NV_GPU_SETCOOLER_LEVEL, 1)

// rev
typedef struct _NV_GPU_COOLER_POLICY_TABLE
{
    NvU32 version;                   //structure version
    NV_COOLER_POLICY policy;         //selected policy to update the cooler levels for, example NVAPI_COOLER_POLICY_PERF
    struct
    {
        NvU32 levelId;      // level indicator for a policy
        NvU32 currentLevel; // new cooler level for the selected policy level indicator.
        NvU32 defaultLevel; // default cooler level for the selected policy level indicator.
    } policyCoolerLevel[NVAPI_MAX_COOLER_LEVELS];
} NV_GPU_COOLER_POLICY_TABLE;

#define NV_GPU_COOLER_POLICY_TABLE_VER  MAKE_NVAPI_VERSION(NV_GPU_COOLER_POLICY_TABLE, 1)

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME:   NvAPI_GPU_GetCoolerSettings
//
// DESCRIPTION:     Retrieves the cooler information of all coolers or a specific cooler associated with the selected GPU.
//                  Coolers are indexed 0 to NVAPI_MAX_COOLERS_PER_GPU-1.
//                  To retrieve specific cooler info set the coolerIndex to the appropriate cooler index.
//                  To retrieve info for all cooler set coolerIndex to NVAPI_COOLER_TARGET_ALL.
//
// PARAMETERS :     hPhysicalGPU(IN) - GPU selection.
//                  coolerIndex(IN)  - Explict cooler index selection.
//                  pCoolerInfo(OUT) - Array of cooler settings.
//
///////////////////////////////////////////////////////////////////////////////
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetCoolerSettings)(_In_ NvPhysicalGpuHandle hPhysicalGpu, NvU32 coolerIndex, NV_GPU_COOLER_SETTINGS* pCoolerInfo);
_NvAPI_GPU_GetCoolerSettings NvAPI_GPU_GetCoolerSettings;

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME:   NvAPI_GPU_SetCoolerLevels
//
// DESCRIPTION:     Set the cooler levels for all coolers or a specific cooler associated with the selected GPU.
//                  Coolers are indexed 0 to NVAPI_MAX_COOLERS_PER_GPU-1. Every cooler level with non-zero currentpolicy gets applied.
//                  The new level should be in the range of minlevel and maxlevel retrieved from GetCoolerSettings API or between
//                  and NVAPI_MIN_COOLER_LEVEL to MAX_COOLER_LEVEL.
//                  To set level for a specific cooler set the coolerIndex to the appropriate cooler index.
//                  To set level for all coolers set coolerIndex to NVAPI_COOLER_TARGET_ALL.
// NOTE:            To lock the fan speed independent of the temperature or performance changes set the cooler currentPolicy to
//                  NVAPI_COOLER_POLICY_MANUAL else set it to the current policy retrieved from the GetCoolerSettings API.
// PARAMETERS:      hPhysicalGPU(IN) - GPU selection.
//                  coolerIndex(IN)  - Explict cooler index selection.
//                  pCoolerLevels(IN) - Updated cooler level and cooler policy.
//
///////////////////////////////////////////////////////////////////////////////
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_SetCoolerLevels)(_In_ NvPhysicalGpuHandle hPhysicalGpu, NvU32 coolerIndex, NV_GPU_SETCOOLER_LEVEL *pCoolerLevels);

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME:   NvAPI_GPU_RestoreCoolerSettings
//
// DESCRIPTION:     Restore the modified cooler settings to NVIDIA defaults.
//
// PARAMETERS:      hPhysicalGPU(IN) - GPU selection.
//                  pCoolerIndex(IN) - Array containing absolute cooler indexes to restore. Pass NULL restore all coolers.
//                  CoolerCount - Number of coolers to restore.
//
///////////////////////////////////////////////////////////////////////////////
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_RestoreCoolerSettings)(_In_ NvPhysicalGpuHandle hPhysicalGpu, NvU32 *pCoolerIndex, NvU32 coolerCount);

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME:   NvAPI_GPU_GetCoolerPolicyTable
//
// DESCRIPTION:     Retrieves the table of cooler and policy levels for the selected policy. Supported only for NVAPI_COOLER_POLICY_PERF.
//
// PARAMETERS:      hPhysicalGPU(IN) - GPU selection.
//                  coolerIndex(IN) - cooler index selection.
//                  pCoolerTable(OUT) - Table of policy levels and associated cooler levels.
//                  count(OUT) - Count of the number of valid levels for the selected policy.
//
///////////////////////////////////////////////////////////////////////////////
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetCoolerPolicyTable)(_In_ NvPhysicalGpuHandle hPhysicalGpu, NvU32 coolerIndex, NV_GPU_COOLER_POLICY_TABLE *pCoolerTable, NvU32 *count);

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME:   NvAPI_GPU_SetCoolerPolicyTable
//
// DESCRIPTION:     Restore the modified cooler settings to NVIDIA defaults. Supported only for NVAPI_COOLER_POLICY_PERF.
//
// PARAMETERS:      hPhysicalGPU(IN) - GPU selection.
//                  coolerIndex(IN) - cooler index selection.
//                  pCoolerTable(IN) - Updated table of policy levels and associated cooler levels. Every non-zero policy level gets updated.
//                  count(IN) - Number of valid levels in the policy table.
//
///////////////////////////////////////////////////////////////////////////////
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_SetCoolerPolicyTable)(_In_ NvPhysicalGpuHandle hPhysicalGpu, NvU32 coolerIndex, NV_GPU_COOLER_POLICY_TABLE *pCoolerTable, NvU32 count);

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME:   NvAPI_GPU_RestoreCoolerPolicyTable
//
// DESCRIPTION:     Restores the perf table policy levels to the defaults.
//
// PARAMETERS:      hPhysicalGPU(IN) - GPU selection.
//                  coolerIndex(IN) - cooler index selection.
//                  pCoolerIndex(IN) - Array containing absolute cooler indexes to restore. Pass NULL restore all coolers.
//                  coolerCount - Number of coolers to restore.
//                  policy - restore for the selected policy
//
///////////////////////////////////////////////////////////////////////////////
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_RestoreCoolerPolicyTable)(_In_ NvPhysicalGpuHandle hPhysicalGpu, NvU32 *pCoolerIndex, NvU32 coolerCount, NV_COOLER_POLICY policy);



// rev - NvAPI_GPU_GetUsages
typedef struct _NV_USAGES_INFO
{
    NvU32 version;                              //!< Structure version
    NvU32 usages[NVAPI_MAX_USAGES_PER_GPU];
} NV_USAGES_INFO;

#define NV_USAGES_INFO_VER  MAKE_NVAPI_VERSION(NV_USAGES_INFO, 1)

// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetUsages)(_In_ NvPhysicalGpuHandle hPhysicalGpu, _Inout_ NV_USAGES_INFO* pUsagesInfo);
_NvAPI_GPU_GetUsages NvAPI_GPU_GetUsages;



// rev - NvAPI_GPU_GetAllClocks
typedef struct _NV_CLOCKS_INFO
{
    NvU32 version;                              //!< Structure version
    NvU32 clocks[NVAPI_MAX_CLOCKS_PER_GPU];
} NV_CLOCKS_INFO;

#define NV_CLOCKS_INFO_VER  MAKE_NVAPI_VERSION(NV_CLOCKS_INFO, 2)

// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetAllClocks)(_In_ NvPhysicalGpuHandle hPhysicalGpu, _Inout_ NV_CLOCKS_INFO* pClocksInfo);
_NvAPI_GPU_GetAllClocks NvAPI_GPU_GetAllClocks;



// rev - NvAPI_GPU_GetVoltageDomainsStatus
typedef struct _NV_VOLTAGE_DOMAINS
{
    NvU32                                   version;        //!< Structure version
    NvU32                                   flags;          //!< Reserved for future use. Must be set to 0
    NvU32                                   max;
    struct
    {
        NV_GPU_PERF_VOLTAGE_INFO_DOMAIN_ID  domainId;       //!< ID of the voltage domain
        NvU32                               mvolt;          //!< Voltage in mV
    } domain[NVAPI_MAX_GPU_PERF_VOLTAGES];
} NV_VOLTAGE_DOMAINS;

#define NV_VOLTAGE_DOMAIN_INFO_VER  MAKE_NVAPI_VERSION(NV_VOLTAGE_DOMAINS, 1)

// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetVoltageDomainsStatus)(_In_ NvPhysicalGpuHandle hPhysicalGPU, _Inout_ NV_VOLTAGE_DOMAINS* pVoltageDomainsStatus);
_NvAPI_GPU_GetVoltageDomainsStatus NvAPI_GPU_GetVoltageDomainsStatus;



// rev - NvAPI_GPU_GetVoltages
typedef struct _NV_VOLTAGES_INFO
{
    NV_GPU_PERF_VOLTAGE_INFO_DOMAIN_ID      domainId;       //!< ID of the voltage domain
    NvU32                                   unknown1;
    NvU32                                   max;

    struct
    {
        NvU32                               unknown2;
        NvU32                               mvolt;          //!< Voltage in mV
    } info[128];
} NV_VOLTAGES_INFO;

// rev
typedef struct _NV_VOLTAGES
{
    NvU32                                   version;        //!< Structure version
    NvU32                                   flags;          //!< Reserved for future use. Must be set to 0
    NvU32                                   max;
    NV_VOLTAGES_INFO voltages[NVAPI_MAX_GPU_PERF_VOLTAGES];
} NV_VOLTAGES;

#define NV_VOLTAGES_INFO_VER  MAKE_NVAPI_VERSION(NV_VOLTAGES, 1)

// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetVoltages)(_In_ NvPhysicalGpuHandle hPhysicalGPU, _Inout_ NV_VOLTAGES* pPerfVoltages);
_NvAPI_GPU_GetVoltages NvAPI_GPU_GetVoltages;



// rev - NvAPI_GPU_GetPerfClocks
typedef struct _NV_PERF_CLOCKS_UNKNOWN_2
{
    NvU32   unknown1;
    NvU32   unknown2;
    NvU32   unknown3;
    NvU32   unknown4;
    NvU32   unknown5;
    NvU32   unknown6;
    NvU32   unknown7;
} NV_PERF_CLOCKS_UNKNOWN_2;

// rev
typedef struct _NV_PERF_CLOCKS_UNKNOWN_1
{
    NvU32                       unknown1;
    NvU32                       unknown2;
    NV_PERF_CLOCKS_UNKNOWN_2    unknown3[32];
} NV_PERF_CLOCKS_UNKNOWN_1;

// rev
typedef struct _NV_PERF_CLOCKS
{
    NvU32                           version;        //!< Structure version
    NvU32                           unknown1;
    NvU32                           unknown2;
    NvU32                           unknown3;
    NvU32                           unknown4;
    NV_PERF_CLOCKS_UNKNOWN_1        unknown5[12];
} NV_PERF_CLOCKS;

#define NV_PERF_CLOCKS_INFO_VER   MAKE_NVAPI_VERSION(NV_PERF_CLOCKS, 1)

// rev
typedef NvAPI_Status(WINAPIV *_NvAPI_GPU_GetPerfClocks)(_In_ NvPhysicalGpuHandle hPhysicalGPU, INT i, _Inout_ NV_PERF_CLOCKS* pPerfClocks);
_NvAPI_GPU_GetPerfClocks NvAPI_GPU_GetPerfClocks;



// rev - NvAPI_GPU_QueryActiveApps
typedef struct _NV_ACTIVE_APP
{
    NvU32                                 version;        //!< Structure version
    NvU32                                 processPID;
    NvAPI_LongString                      processName;
} NV_ACTIVE_APP;

#define NV_ACTIVE_APPS_INFO_VER   MAKE_NVAPI_VERSION(NV_ACTIVE_APP, 2)

// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_QueryActiveApps)(_In_ NvPhysicalGpuHandle hPhysicalGPU, _Inout_ NV_ACTIVE_APP pActiveApps[NVAPI_MAX_PROCESSES], _Inout_ NvU32* pTotal);
_NvAPI_GPU_QueryActiveApps NvAPI_GPU_QueryActiveApps;



// rev - NvAPI_GPU_GetPowerMizerInfo
typedef enum _PowerSourceInfo
{
    PowerSourceInfo_Unknown1 = 1,
    PowerSourceInfo_Unknown2,
    PowerSourceInfo_Unknown3 = 8738
} PowerSourceInfo;

// rev
typedef enum _SelectSource
{
    SelectSource_Unknown1 = 1,
    SelectSource_Unknown2,
    SelectSource_Unknown3
} SelectSource;

// rev
typedef enum _LevelInfo
{
    LevelInfo_Unknown1 = 1,
    LevelInfo_Unknown2,
    LevelInfo_Unknown3,
    LevelInfo_Unknown4,
    LevelInfo_Unknown5,
    LevelInfo_Unknown6,
    LevelInfo_Unknown7
} LevelInfo;

// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetPowerMizerInfo)(_In_ NvPhysicalGpuHandle hPhysicalGPU, PowerSourceInfo powerSourceInfo, SelectSource select, LevelInfo* pLevelInfo);
_NvAPI_GPU_GetPowerMizerInfo NvAPI_GPU_GetPowerMizerInfo;

// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GetDisplayDriverRegistryPath)(_In_ NvDisplayHandle hNvDisplay, _Inout_ NvAPI_LongString szDriverRegistryPath);
_NvAPI_GetDisplayDriverRegistryPath NvAPI_GetDisplayDriverRegistryPath;



// rev
//typedef NvAPI_Status (WINAPIV *_NvAPI_RestartDisplayDriver)(_In_ NvU32 NvDriverIndex);
//_NvAPI_RestartDisplayDriver NvAPI_RestartDisplayDriver;


typedef enum _NV_POWER_TOPOLOGY_FLAGS
{
    NV_POWER_TOPOLOGY_FLAG_UNKNOWN1,
    NV_POWER_TOPOLOGY_FLAG_UNKNOWN2
} NV_POWER_TOPOLOGY_FLAGS;


typedef struct _NV_POWER_TOPOLOGY_1
{
    NvU32 unknown1;
    NvU32 unknown2;
    NvU32 unknown3;

    //public uint UInt32_0
    //{
    //    return this.unknown1 & 1u;
    //}

    //public uint UInt32_1
    //{
    //    return (this.unknown1 & 4294967294u) / 2u;
    //}
} NV_POWER_TOPOLOGY_1;


typedef struct _NV_POWER_TOPOLOGY_2
{
    NV_POWER_TOPOLOGY_FLAGS flags;
    NV_POWER_TOPOLOGY_1 unknown;
} NV_POWER_TOPOLOGY_2;

typedef struct _NV_POWER_TOPOLOGY_STATUS
{
    NvU32 version;        //!< Structure version
    NvU32 count;

    NV_POWER_TOPOLOGY_2 unknown[4];
} NV_POWER_TOPOLOGY_STATUS;

#define NV_POWER_TOPOLOGY_STATUS_VER   MAKE_NVAPI_VERSION(NV_POWER_TOPOLOGY_STATUS, 1)

typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_ClientPowerTopologyGetStatus)(_In_ NvPhysicalGpuHandle hPhysicalGPU, _Inout_ NV_POWER_TOPOLOGY_STATUS* pClientPowerTopologyStatus);
_NvAPI_GPU_ClientPowerTopologyGetStatus NvAPI_GPU_ClientPowerTopologyGetStatus;



// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetShortName)(_In_ NvPhysicalGpuHandle hPhysicalGPU, _Inout_ NvAPI_ShortString szName);
_NvAPI_GPU_GetShortName NvAPI_GPU_GetShortName;


// rev
typedef struct _NV_ARCH_INFO
{
    NvU32 version;        //!< Structure version
    NvU32 unknown[3];
} NV_ARCH_INFO;

#define NV_ARCH_INFO_VER   MAKE_NVAPI_VERSION(NV_ARCH_INFO, 2)

// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetArchInfo)(_In_ NvPhysicalGpuHandle hPhysicalGPU, _Inout_ NV_ARCH_INFO* pArchInfo);
_NvAPI_GPU_GetArchInfo NvAPI_GPU_GetArchInfo;



// rev
typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetPartitionCount)(_In_ NvPhysicalGpuHandle hPhysicalGPU, _Out_ NvU32* pCount);
_NvAPI_GPU_GetPartitionCount NvAPI_GPU_GetPartitionCount;




typedef struct _NV_PCIE_INFO_UNKNOWN
{
    NvU32 unknown0;
    NvU32 unknown1;
    NvU32 unknown2;
    NvU32 unknown3;
    NvU32 unknown4;
    NvU32 unknown5;
    NvU32 unknown6;
    NvU32 unknown7;
} NV_PCIE_INFO_UNKNOWN;

typedef struct _NV_PCIE_INFO
{
    NvU32                   version;          //!< Structure version
    NV_PCIE_INFO_UNKNOWN    info[5];
} NV_PCIE_INFO;

#define NV_PCIE_INFO_VER   MAKE_NVAPI_VERSION(NV_PCIE_INFO, 2)

typedef NvAPI_Status (WINAPIV *_NvAPI_GPU_GetPCIEInfo)(_In_ NvPhysicalGpuHandle hPhysicalGPU, _Inout_ NV_PCIE_INFO* pPciInfo);
_NvAPI_GPU_GetPCIEInfo NvAPI_GPU_GetPCIEInfo;


NV_DECLARE_HANDLE(NvmlDeviceHandle);

typedef enum _nvmlReturn_t
{
    NVML_SUCCESS = 0,                   //!< The operation was successful
    NVML_ERROR_UNINITIALIZED = 1,       //!< NVML was not first initialized with nvmlInit()
    NVML_ERROR_INVALID_ARGUMENT = 2,    //!< A supplied argument is invalid
    NVML_ERROR_NOT_SUPPORTED = 3,       //!< The requested operation is not available on target device
    NVML_ERROR_NO_PERMISSION = 4,       //!< The current user does not have permission for operation
    NVML_ERROR_ALREADY_INITIALIZED = 5, //!< Deprecated: Multiple initializations are now allowed through ref counting
    NVML_ERROR_NOT_FOUND = 6,           //!< A query to find an object was unsuccessful
    NVML_ERROR_INSUFFICIENT_SIZE = 7,   //!< An input argument is not large enough
    NVML_ERROR_INSUFFICIENT_POWER = 8,  //!< A device's external power cables are not properly attached
    NVML_ERROR_DRIVER_NOT_LOADED = 9,   //!< NVIDIA driver is not loaded
    NVML_ERROR_TIMEOUT = 10,            //!< User provided timeout passed
    NVML_ERROR_UNKNOWN = 999            //!< An internal driver error occurred
} nvmlReturn_t;

typedef enum _NvmlPcieUtilCounter
{
    NVML_PCIE_UTIL_TX_BYTES = 0,
    NVML_PCIE_UTIL_RX_BYTES = 1,
    NVML_PCIE_UTIL_COUNT
} NvmlPcieUtilCounter;

typedef enum _nvmlClockType_t
{
    NVML_CLOCK_GRAPHICS = 0,
    NVML_CLOCK_SM = 1,
    NVML_CLOCK_MEM = 2,
    NVML_CLOCK_VIDEO = 3,
    NVML_CLOCK_COUNT
} nvmlClockType_t;

typedef enum _nvmlClockId_t
{
    NVML_CLOCK_ID_CURRENT = 0,
    NVML_CLOCK_ID_APP_CLOCK_TARGET = 1,
    NVML_CLOCK_ID_APP_CLOCK_DEFAULT = 2,
    NVML_CLOCK_ID_CUSTOMER_BOOST_MAX = 3,
    NVML_CLOCK_ID_COUNT
} nvmlClockId_t;

typedef nvmlReturn_t (WINAPIV* _nvmlInit)(VOID);
_nvmlInit NvmlInit;

typedef nvmlReturn_t (WINAPIV* _nvmlInit_v2)(VOID);
_nvmlInit_v2 NvmlInit_v2;

#define NVML_INIT_FLAG_NO_GPUS 1   //!< Don't fail nvmlInit() when no GPUs are found
#define NVML_INIT_FLAG_NO_ATTACH 2   //!< Don't attach GPUs

typedef nvmlReturn_t (WINAPIV* _nvmlInitWithFlags)(_In_ UINT32 Flags);
_nvmlInitWithFlags nvmlInitWithFlags;


typedef nvmlReturn_t (WINAPIV* _nvmlShutdown)(VOID);
_nvmlShutdown NvmlShutdown;

typedef PSTR (WINAPIV* _nvmlErrorString)(VOID);
_nvmlErrorString NvmlErrorString;

// Retrieves the globally unique immutable UUID associated with this device, as a 5 part hexadecimal string, that augments the immutable, board serial identifier.
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetUUID)(_In_ NvmlDeviceHandle DeviceHandle, _Inout_ PSTR uuid, _In_ UINT32 length);
_nvmlDeviceGetUUID NvmlDeviceGetUUID;

// Retrieves the globally unique board serial number associated with this device's board.
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetSerial)(_In_ NvmlDeviceHandle DeviceHandle, _Inout_ PSTR serial, _In_ UINT32 length);
_nvmlDeviceGetSerial NvmlDeviceGetSerial;

typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetHandleByIndex_v2)(_In_ UINT32 index, _Out_ NvmlDeviceHandle* DeviceHandle);
_nvmlDeviceGetHandleByIndex_v2 NvmlDeviceGetHandleByIndex_v2;

// Acquire the handle for a particular device, based on its PCI bus id.
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetHandleByPciBusId)(_In_ PSTR PciBusId, _Out_ NvmlDeviceHandle* DeviceHandle);
_nvmlDeviceGetHandleByPciBusId NvmlDeviceGetHandleByPciBusId;

// Acquire the handle for a particular device, based on its PCI bus id.
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetHandleByPciBusId_v2)(_In_ PSTR PciBusId, _Out_ NvmlDeviceHandle* DeviceHandle);
_nvmlDeviceGetHandleByPciBusId_v2 NvmlDeviceGetHandleByPciBusId_v2;

// Retrieves power usage for this GPU in milliwatts and its associated circuitry (e.g. memory)
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetPowerUsage)(_In_ NvmlDeviceHandle DeviceHandle, _Out_ PULONG Usage);
_nvmlDeviceGetPowerUsage NvmlDeviceGetPowerUsage;

// Retrieves the current temperature readings for the device, in degrees C.
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetTemperature)(_In_ NvmlDeviceHandle DeviceHandle, _Out_ PULONG Temperature);
_nvmlDeviceGetTemperature NvmlDeviceGetTemperature;

// Retrieves the intended operating speed of the device's fan.
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetFanSpeed)(_In_ NvmlDeviceHandle DeviceHandle, _Out_ PULONG Temperature);
_nvmlDeviceGetFanSpeed NvmlDeviceGetFanSpeed;

// Retrieves the current PCIe link width
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetCurrPcieLinkGeneration)(_In_ NvmlDeviceHandle DeviceHandle, _Out_ PULONG Generation);
_nvmlDeviceGetCurrPcieLinkGeneration NvmlDeviceGetCurrPcieLinkGeneration;

// Retrieves the maximum PCIe link generation possible with this device and system
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetMaxPcieLinkGeneration)(_In_ NvmlDeviceHandle DeviceHandle, _Out_ PULONG Generation);
_nvmlDeviceGetMaxPcieLinkGeneration NvmlDeviceGetMaxPcieLinkGeneration;

// Retrieves the current PCIe link width
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetCurrPcieLinkWidth)(_In_ NvmlDeviceHandle DeviceHandle, _Out_ PULONG Width);
_nvmlDeviceGetCurrPcieLinkWidth NvmlDeviceGetCurrPcieLinkWidth;

 // Retrieves the maximum PCIe link width possible with this device and system
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetMaxPcieLinkWidth)(_In_ NvmlDeviceHandle DeviceHandle, _Out_ PULONG Width);
_nvmlDeviceGetMaxPcieLinkWidth NvmlDeviceGetMaxPcieLinkWidth;

// Retrieve PCIe utilization information. This function is querying a byte counter over a 20ms interval and thus is the PCIe throughput over that interval.
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetPcieThroughput)(_In_ NvmlDeviceHandle DeviceHandle, _In_ NvmlPcieUtilCounter counter, _Out_ PULONG Usage);
_nvmlDeviceGetPcieThroughput NvmlDeviceGetPcieThroughput;

// Retrieves the clock speed for the clock specified by the clock type and clock ID.
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetClock)(_In_ NvmlDeviceHandle DeviceHandle, _In_ nvmlClockType_t clockType, _In_ nvmlClockId_t clockId, _Out_ PULONG clockMHz);
_nvmlDeviceGetClock NvmlDeviceGetClock;

// Retrieves total energy consumption for this GPU in millijoules (mJ) since the driver was last reloaded
typedef nvmlReturn_t (WINAPIV* _nvmlDeviceGetTotalEnergyConsumption)(_In_ NvmlDeviceHandle DeviceHandle, _Out_ PULONG64 energ);
_nvmlDeviceGetTotalEnergyConsumption NvmlDeviceGetTotalEnergyConsumption;

#include <poppack.h>
