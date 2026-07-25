/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//tools/oxc3_cli/cpu.c

#include "tools/oxc3_cli/cli.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/base/error.h"
#include "types/base/constants.h"

//Lists the current CPU: logical cores, physical memory, and the ECPUFeatures capability flags detected once
//at Platform_create. This lets you see, at a glance, which operations have a hardware / wide-SIMD "full speed"
//path on this machine (mirrors "OxC3 graphics devices" for the CPU side).

Bool CLI_cpuDevices(const ParsedArgs *args) {

	if(!args)
		return false;

	const ECPUFeatures f = Platform_instance->cpuFeatures;
	const PlatformCPUInfo *info = &Platform_instance->cpuInfo;

	static const C8 *vendorNames[] = {
		"Unknown", "Intel", "AMD", "Apple", "ARM", "Qualcomm", "NVIDIA", "Ampere", "Samsung"
	};

	Log_debugLnx("CPU:");

	if(info->brand[0])
		Log_debugLnx("\t%s", info->brand);

	Log_debugLnx("\tVendor: %s", vendorNames[info->vendor < ECPUVendor_Count ? info->vendor : ECPUVendor_Unknown]);

	if(info->physicalCores)
		Log_debugLnx("\tCores: %"PRIu32" physical, %"PRIu32" logical", info->physicalCores, info->logicalCores);
	else
		Log_debugLnx("\tLogical cores: %"PRIu32, info->logicalCores);

	if(info->performanceCores || info->efficiencyCores)
		Log_debugLnx(
			"\tHybrid: %"PRIu32" performance + %"PRIu32" efficiency cores",
			info->performanceCores, info->efficiencyCores
		);

	if(info->numaNodes > 1)
		Log_debugLnx("\tNUMA nodes: %"PRIu32, info->numaNodes);

	if(info->l1DataCacheBytes)
		Log_debugLnx("\tL1 data cache: %"PRIu64" KiB (per core)", info->l1DataCacheBytes / 1024);

	if(info->l2CacheBytes)
		Log_debugLnx("\tL2 cache: %"PRIu64" KiB (per core)", info->l2CacheBytes / 1024);

	if(info->l3CacheBytes)
		Log_debugLnx("\tL3 cache: %"PRIu64" MiB (shared)", info->l3CacheBytes / (1024 * 1024));

	const U64 ram = Platform_getPhysicalRAM();
	const U64 avail = Platform_getAvailableRAM();

	if(ram)
		Log_debugLnx("\tMemory: %"PRIu64" MiB total", ram / (1024 * 1024));

	if(avail)
		Log_debugLnx("\tMemory available: %"PRIu64" MiB", avail / (1024 * 1024));

	Log_debugLnx("\tCapabilities (full-speed hardware paths):");

	static const struct { ECPUFeatures bit; const C8 *name; } caps[] = {
		{ ECPUFeatures_HwCRC32C,  "CRC32C (hardware)" },
		{ ECPUFeatures_HwAES,     "AES128 (hardware)" },
		{ ECPUFeatures_HwAES256,  "AES256 (full-speed, wide)" },
		{ ECPUFeatures_HwSHA256,  "SHA-256 (hardware)" },
		{ ECPUFeatures_F16C,      "F16 <-> F32 conversion" },
		{ ECPUFeatures_Vec8i,     "256-bit integer SIMD (vec8i / AVX2)" },
		{ ECPUFeatures_Vec16i,    "512-bit integer SIMD (vec16i / AVX512)" },
		{ ECPUFeatures_Int8Dot,   "int8 dot-product (VNNI / DotProd)" },
		{ ECPUFeatures_PCLMULQDQ, "Carry-less multiply (GHASH / GCM)" },
		{ ECPUFeatures_FMA,       "Fused multiply-add" },
		{ ECPUFeatures_AVX,       "256-bit float SIMD (AVX)" }
	};

	for(U64 i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i)
		Log_debugLnx("\t\t%-40s %s", caps[i].name, (f & caps[i].bit) ? "yes" : "no");

	return true;
}
