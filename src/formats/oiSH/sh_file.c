/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#ifndef DISALLOW_SH_OXC3_PLATFORMS
	#include "platforms/ext/listx_impl.h"
#else
	#include "types/container/list_impl.h"
#endif

#include "formats/oiSH/sh_file.h"

TListImpl(SHFile);

#ifndef DISALLOW_SH_OXC3_PLATFORMS
	
	#include "platforms/platform.h"

	Bool SHFile_createx(ESHSettingsFlags flags, U32 compilerVersion, U32 sourceHash, SHFile *shFile, Error *e_rr) {
		return SHFile_create(flags, compilerVersion, sourceHash, Platform_instance->alloc, shFile, e_rr);
	}

	void SHFile_freex(SHFile *shFile) {
		SHFile_free(shFile, Platform_instance->alloc);
	}

	Bool SHFile_addBinaryx(SHFile *shFile, SHBinaryInfo *binaries, Error *e_rr) {
		return SHFile_addBinaries(shFile, binaries, Platform_instance->alloc, e_rr);
	}

	Bool SHFile_addEntrypointx(SHFile *shFile, SHEntry *entry, Error *e_rr) {
		return SHFile_addEntrypoint(shFile, entry, Platform_instance->alloc, e_rr);
	}

	Bool SHFile_addIncludex(SHFile *shFile, SHInclude *include, Error *e_rr) {
		return SHFile_addInclude(shFile, include, Platform_instance->alloc, e_rr);
	}

	Bool SHFile_writex(SHFile shFile, Buffer *result, Error *e_rr) {
		return SHFile_write(shFile, Platform_instance->alloc, result, e_rr);
	}

	Bool SHFile_readx(Buffer file, Bool isSubFile, SHFile *shFile, Error *e_rr) {
		return SHFile_read(file, isSubFile, Platform_instance->alloc, shFile, e_rr);
	}

	Bool SHFile_combinex(SHFile a, SHFile b, SHFile *combined, Error *e_rr) {
		return SHFile_combine(a, b, Platform_instance->alloc, combined, e_rr);
	}

	void ListSHInclude_freeUnderlyingx(ListSHInclude *includes) {
		ListSHInclude_freeUnderlying(includes, Platform_instance->alloc);
	}

	void SHInclude_freex(SHInclude *include) {
		SHInclude_free(include, Platform_instance->alloc);
	}
#endif

Bool SHFile_create(
	ESHSettingsFlags flags,
	U32 compilerVersion,
	U32 sourceHash,
	Allocator alloc,
	SHFile *shFile,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!shFile)
		retError(clean, Error_nullPointer(0, "SHFile_create()::shFile is required"))

	if(shFile->entries.ptr)
		retError(clean, Error_invalidOperation(0, "SHFile_create()::shFile isn't empty, might indicate memleak"))

	if(flags & ESHSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 3, "SHFile_create()::flags contained unsupported flag"))

	gotoIfError2(clean, ListSHEntry_reserve(&shFile->entries, 8, alloc))
	gotoIfError2(clean, ListSHBinaryInfo_reserve(&shFile->binaries, 4, alloc))
	gotoIfError2(clean, ListSHInclude_reserve(&shFile->includes, 16, alloc))

	shFile->flags = flags;
	shFile->compilerVersion = compilerVersion;
	shFile->sourceHash = sourceHash;

clean:
	return s_uccess;
}

void SHFile_free(SHFile *shFile, Allocator alloc) {

	if(!shFile || !shFile->entries.ptr)
		return;

	for(U64 i = 0; i < shFile->entries.length; ++i) {
		SHEntry *entry = &shFile->entries.ptrNonConst[i];
		CharString_free(&entry->name, alloc);
		ListCharString_freeUnderlying(&entry->semanticNames, alloc);
		ListU16_free(&entry->binaryIds, alloc);
	}

	for(U64 j = 0; j < shFile->binaries.length; ++j) {

		SHBinaryInfo *binary = &shFile->binaries.ptrNonConst[j];

		ListSHRegisterRuntime_freeUnderlying(&binary->registers, alloc);
		CharString_free(&binary->identifier.entrypoint, alloc);
		ListCharString_freeUnderlying(&binary->identifier.uniforms, alloc);

		for(U64 i = 0; i < ESHBinaryType_Count; ++i)
			Buffer_free(&binary->binaries[i], alloc);
	}

	ListSHEntry_freeUnderlying(&shFile->entries, alloc);
	ListSHBinaryInfo_free(&shFile->binaries, alloc);
	ListSHInclude_freeUnderlying(&shFile->includes, alloc);

	*shFile = (SHFile) { 0 };
}
