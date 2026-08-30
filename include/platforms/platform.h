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

//platforms/platform.h

#pragma once
#include "formats/oiCA/ca_file.h"
#include "types/container/string.h"
#include "types/base/platform_types.h"
#include "types/base/string_base.h"
#include "types/base/lock.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Thread Thread;
typedef struct CAFile CAFile;

typedef struct VirtualSection {

	CharString path;

	const void *dataExt;       //Information about how to load the virtual file

	U64 loadedAndId;           //(loaded << 63) | id (0 indicates not loaded)

	U64 lenExt;                //Dependent on platform if it contains the length of the section

} VirtualSection;

TList(VirtualSection);

typedef enum ECPUVendor {
	ECPUVendor_Unknown,
	ECPUVendor_Intel,
	ECPUVendor_AMD,
	ECPUVendor_Apple,
	ECPUVendor_Arm,
	ECPUVendor_Qualcomm,
	ECPUVendor_Nvidia,
	ECPUVendor_Ampere,          //Ampere Computing (ARM64 server CPUs: Altra/AmpereOne)
	ECPUVendor_Samsung,
	ECPUVendor_Count
} ECPUVendor;

//Richer CPU topology, gathered once at Platform_create (see Platform_detectCPUInfo).
//Fields that couldn't be determined on the current OS/arch are left 0.
//Cache sizes are in bytes; hybrid P/E counts are 0 when the CPU isn't hybrid (or the split is unknown).

typedef struct PlatformCPUInfo {

	ECPUVendor vendor;
	U32 numaNodes;                      //NUMA node count (0/1 if not applicable)

	U32 logicalCores;                   //Hardware threads (== Platform_getThreads)
	U32 physicalCores;                  //Physical cores (0 if unknown)

	U32 performanceCores;               //Hybrid P-cores (0 if not hybrid / unknown)
	U32 efficiencyCores;                //Hybrid E-cores

	U64 l1DataCacheBytes;               //Per physical core L1 data cache
	U64 l2CacheBytes;                   //Per physical core / per cluster L2 cache
	U64 l3CacheBytes;                   //Total shared L3 cache

	C8 brand[48];                       //CPU brand string (null-terminated; empty if unknown)

} PlatformCPUInfo;

typedef struct Platform {

	EPlatform platformType;
	ECPUFeatures cpuFeatures;            //Detected once at Platform_create (see Platform_detectCPUFeatures)

	Bool useWorkingDir;        //TODO: Find a better solution for this for dlls
	U8 pad1[7];

	PlatformCPUInfo cpuInfo;             //Detected once at Platform_create (see Platform_detectCPUInfo)

	ListCharString args;
	CharString defaultDir;              //Either workDir or appDir based on 'useWorkingDir'
	CharString workDirectory;           //Contains a trailing slash to make file stuff easier
	CharString appDirectory;            //Installation location if useWorkingDir, otherwise same as workDirectory

	const Allocator *alloc;

	SpinLock virtualSectionsLock;

	ListVirtualSection virtualSections;
	ListCAFile archives;

	void *data;

	void *data1;                        //If present can contain the executable file
	U64 size1;

} Platform;

extern Platform *Platform_instance;     //DLLs that call this need to also call Platform_create or get the same pointer passed.

Bool Platform_create(int cmdArgc, const C8 *cmdArgs[], void *data, void *allocator, Bool useWorkingDir, Error *e_rr);

impl void Platform_cleanupExt();
impl Bool Platform_initExt(Error *e_rr);

impl Bool Platform_checkCPUSupport();   //SIMD dependent: SSE, None, NEON
impl U64 Platform_getThreads();
impl U64 Platform_getPhysicalRAM();     //Total installed physical memory in bytes (0 if unknown)
impl U64 Platform_getAvailableRAM();    //Currently free/available physical memory in bytes (0 if unknown)
impl void Platform_detectCPUInfo(PlatformCPUInfo *out);   //Fills topology (called once at Platform_create)

//Whether the user has a hardware keyboard they can type on right now.
//True on platforms that have no on screen keyboard at all, since then there's nothing to weigh it against.
//Query it per use rather than caching: a bluetooth keyboard can appear or disappear mid session.
impl Bool Platform_hasPhysicalKeyboard();

//If the device has an on screen keyboard (e.g. Android) you need to call this before handling text input.
//Also make sure to hide it later with isVisible=false to avoid having a keyboard taking up half of your screen.
//Showing is skipped when a physical keyboard is usable, which is what you want by default: covering half the
// screen with a second keyboard is worse than not offering one.
//force overrides that for the cases where the on screen keyboard is the point rather than a fallback.
//Hiding always goes through, so a hide can never be left stranded by a keyboard being plugged in meanwhile.
impl Bool Platform_setKeyboardVisibleForced(Bool isVisible, Bool force);

static inline Bool Platform_setKeyboardVisible(Bool isVisible) {
	return Platform_setKeyboardVisibleForced(isVisible, false);
}

void Platform_cleanup();                //Call on exit

impl void *Platform_getDataImpl(void *ptr);

#if _PLATFORM_TYPE == PLATFORM_ANDROID
	typedef struct android_app android_app;
	#define Platform_defineEntrypoint() void android_main(android_app *state)
	#define Platform_getData() (state)
	#define Platform_argc (0)
	#define Platform_argv (NULL)

	//android_main returns nothing, so the exit code is normally dropped.
	//A test bundled into the android test APK (see OXC3_TEST_ENTRY in types/test/test.h) isn't the entry point though;
	// it hands its result back to the runner, which is what reports pass/fail.

	#ifdef _OXC3_TEST_BUNDLED
		#define Platform_return(...) return __VA_ARGS__
	#else
		#define Platform_return(x) return
	#endif
#elif _PLATFORM_TYPE == PLATFORM_WEB && defined(_OXC3_TEST_BUNDLED)

	//A bundled web suite is entered through OXC3_TEST_ENTRY's (void *state) signature, not through main, so
	// argc/argv are genuinely not in scope and state is the parameter - the same situation android's bundle
	// is in, which is why these three agree with the branch above.
	//Platform_defineEntrypoint is deliberately NOT defined: unlike android there is no single platform entry
	// to generate, because test/web/wtest_main.c owns main() and calls the suites itself. Leaving it
	// undefined turns any attempt to use it here into a compile error rather than a wrong entry point.

	#define Platform_getData() (state)
	#define Platform_argc (0)
	#define Platform_argv (NULL)
	#define Platform_return(...) return __VA_ARGS__
#else
	#define Platform_defineEntrypoint() int main(int argc, const char *argv[])
	#define Platform_getData() Platform_getDataImpl(NULL)
	#define Platform_argc argc
	#define Platform_argv argv
	#define Platform_return(...) return __VA_ARGS__
#endif

//Call this on allocate to make sure the platform's allocator tracks allocations

Bool Platform_onAllocate(void *ptr, U64 length, Error *e_rr);
Bool Platform_onFree(void *ptr, U64 length);

//Default allocator

impl void *Platform_allocate(void *allocator, U64 length);
impl void Platform_free(void *allocator, void *ptr, U64 length);

//Debugging to see where allocations came from and how many are active

void Platform_printAllocations(U64 offset, U64 length, U64 minAllocationSize);

//Get active allocations (to detect leaks) minAllocationSize is only respected in Debug mode.
U64 Platform_getActiveAllocations(U64 minAllocationSize);

#ifdef __cplusplus
	}
#endif
