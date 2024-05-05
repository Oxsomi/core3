/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "types/string.h"
#include "types/allocator.h"
#include "types/archive.h"
#include "types/platform_types.h"
#include "types/lock.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct VirtualSection {

	CharString path;
	void *dataExt;			//Information about how to load the virtual file
	Archive loadedData;		//If the data is in memory, this will be used
	Bool loaded;

} VirtualSection;

TList(VirtualSection);

typedef struct Platform {

	EPlatform platformType;

	ListCharString args;
	CharString workingDirectory;		//Contains a trailing slash to make file stuff easier

	Allocator alloc;

	Lock virtualSectionsLock;
	ListVirtualSection virtualSections;

	void *data;

	U32 threads;
	U32 padding;

} Platform;

extern Platform Platform_instance;
user_impl extern const Bool Platform_useWorkingDirectory;			//If false, the app directory will be used instead.

Error Platform_create(int cmdArgc, const C8 *cmdArgs[], void *data, void *allocator);

impl void Platform_cleanupExt();
impl Error Platform_initExt();

impl Bool Platform_checkCPUSupport();								//SIMD dependent: SSE, None, NEON

void Platform_cleanup();

user_impl extern I32 Program_run();
user_impl extern void Program_exit();

//Call this on allocate to make sure the platform's allocator tracks allocations

Error Platform_onAllocate(void *ptr, U64 length);
Bool Platform_onFree(void *ptr, U64 length);

//Default allocator

impl void *Platform_allocate(void *allocator, U64 length);
impl void Platform_free(void *allocator, void *ptr, U64 length);

//Debugging to see where allocations came from and how many are active

void Platform_printAllocations(U64 offset, U64 length, U64 minAllocationSize);

#ifdef __cplusplus
	}
#endif
