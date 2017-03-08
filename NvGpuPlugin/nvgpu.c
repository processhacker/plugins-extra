/*
 * Process Hacker Extra Plugins -
 *   Nvidia GPU Plugin
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

PH_CIRCULAR_BUFFER_FLOAT GpuUtilizationHistory;
PH_CIRCULAR_BUFFER_ULONG GpuMemoryHistory;
PH_CIRCULAR_BUFFER_FLOAT GpuBoardHistory;
PH_CIRCULAR_BUFFER_FLOAT GpuBusHistory;

VOID NvGpuInitialize(
    VOID
    )
{
    ULONG sampleCount;

    sampleCount = PhGetIntegerSetting(L"SampleCount");
    PhInitializeCircularBuffer_FLOAT(&GpuUtilizationHistory, sampleCount);
    PhInitializeCircularBuffer_ULONG(&GpuMemoryHistory, sampleCount);
    PhInitializeCircularBuffer_FLOAT(&GpuBoardHistory, sampleCount);
    PhInitializeCircularBuffer_FLOAT(&GpuBusHistory, sampleCount);
}

VOID NvGpuUpdate(
    VOID
    )
{
    static ULONG runCount = 0; // MUST keep in sync with runCount in process provider

    if (runCount != 0)
    {
        NvGpuUpdateValues();

        PhAddItemCircularBuffer_FLOAT(&GpuUtilizationHistory, GpuCurrentGpuUsage);
        PhAddItemCircularBuffer_ULONG(&GpuMemoryHistory, GpuCurrentMemUsage);
        PhAddItemCircularBuffer_FLOAT(&GpuBoardHistory, GpuCurrentCoreUsage);
        PhAddItemCircularBuffer_FLOAT(&GpuBusHistory, GpuCurrentBusUsage);
    }

    runCount++;
}