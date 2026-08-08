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

//formats/oiDL/test/test_oiDL_copy.c

#include "test_oiDL_shared.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"

Bool buildDataFile(Test *t, DLFile *f, U64 count);
Bool buildStringFile(Test *t, DLFile *f, U64 count);

extern const DLSettings kSettingsData;
extern const DLSettings kSettingsStr;

//Reference the cache in the DLFile

static Bool buildDataFileWithCacheRefs(Test *t, DLFile *f, U64 count) {

	U64 cacheSize = (count * (count - 1)) / 2;

	if (!DLFile_create(&kSettingsData, cacheSize, t->alloc, f, &t->err)) {
		Test_assert(t, "DLFile_create with cache for Data", false);
		return false;
	}

	U64 offset = 0;

	for (U64 i = 0; i < count; ++i) {

		for (U64 b = 0; b < i; ++b)
			f->cache.ptrNonConst[offset + b] = (U8)i;

		Buffer entry = i == 0
			? Buffer_createNull()
			: Buffer_createRefConst(f->cache.ptr + offset, i);

		if (!DLFile_addEntry(f, &entry, t->alloc, &t->err)) {
			Test_assert(t, "DLFile_addEntry cache ref", false);
			DLFile_free(f, t->alloc);
			return false;
		}

		offset += i;
	}

	return true;
}

static Bool buildStringFileWithCacheRefs(Test *t, DLFile *f, U64 count) {

	U64 cacheSize = (count * (count - 1)) / 2;

	if (!DLFile_create(&kSettingsStr, cacheSize, t->alloc, f, &t->err)) {
		Test_assert(t, "DLFile_create with cache for String", false);
		return false;
	}

	U64 offset = 0;

	for (U64 i = 0; i < count; ++i) {

		for (U64 b = 0; b < i; ++b)
			f->cache.ptrNonConst[offset + b] = (U8)('a' + (i % 26));

		CharString entry = i == 0
			? CharString_createNull()
			: CharString_createRefSizedConst((const C8*)f->cache.ptr + offset, i, false);

		if (!DLFile_addEntryString(f, &entry, t->alloc, &t->err)) {
			Test_assert(t, "DLFile_addEntryString cache ref", false);
			DLFile_free(f, t->alloc);
			return false;
		}

		offset += i;
	}

	return true;
}

void Test_DLCreateCopyCacheRefs(Test *t) {

	Test_setModule(t, "DLFile_createCopy (cache refs)");

	//Entries that are refs into DLFile::cache must be re-based into the copy's
	// own cache by DLFile_createCopy.  We inject cache-ref entries directly
	//(matching what DLFile_read produces for small entries) so the test has no
	// dependency on the serialization path.

	{                            //Data: copy must re-base cache refs, freeing the source must not corrupt the copy
		DLFile src  = { 0 };
		DLFile copy = { 0 };

		if (!buildDataFileWithCacheRefs(t, &src, 4)) {
			Test_assert(t, "Setup 4-entry cache-ref Data file", false);
			goto doneCacheData;
		}

		Test_assert(t, "createCopy ok",    DLFile_createCopy(&src, t->alloc, &copy, &t->err));
		Test_assert(t, "entryCount 4",     DLFile_entryCount(&copy) == 4);

		for (U64 i = 0; i < 4; ++i)
			Test_assert(t, "entrySize matches src", DLFile_entrySize(&copy, i) == DLFile_entrySize(&src, i));

		//Poison the source cache: if copy entries still point into src.cache the
		//size reads below will operate on freed/poisoned memory.

		DLFile_free(&src, t->alloc);

		Test_assert(t, "[0] size after src free == 0", DLFile_entrySize(&copy, 0) == 0);
		Test_assert(t, "[1] size after src free == 1", DLFile_entrySize(&copy, 1) == 1);
		Test_assert(t, "[2] size after src free == 2", DLFile_entrySize(&copy, 2) == 2);
		Test_assert(t, "[3] size after src free == 3", DLFile_entrySize(&copy, 3) == 3);

		//Verify that the copy's buffer pointers fall inside the copy's own cache

		for (U64 i = 1; i < 4; ++i) {
			Buffer buf = copy.entryBuffers.ptr[i];
			Test_assert(t, "copy entry ptr inside copy cache",
				buf.ptr >= copy.cache.ptr &&
				buf.ptr + Buffer_length(buf) <= copy.cache.ptr + Buffer_length(copy.cache)
			);
		}

	doneCacheData:
		DLFile_free(&src,  t->alloc);
		DLFile_free(&copy, t->alloc);
	}

	{                            //String: copy must re-base CharString refs, freeing source must not corrupt the copy
		DLFile src  = { 0 };
		DLFile copy = { 0 };

		if (!buildStringFileWithCacheRefs(t, &src, 4)) {
			Test_assert(t, "Setup 4-entry cache-ref String file", false);
			goto doneCacheStr;
		}

		Test_assert(t, "createCopy String ok", DLFile_createCopy(&src, t->alloc, &copy, &t->err));
		Test_assert(t, "entryCount 4",         DLFile_entryCount(&copy) == 4);

		for (U64 i = 0; i < 4; ++i)
			Test_assert(t, "entrySize matches src", DLFile_entrySize(&copy, i) == DLFile_entrySize(&src, i));

		DLFile_free(&src, t->alloc);

		Test_assert(t, "[0] size after src free == 0", DLFile_entrySize(&copy, 0) == 0);
		Test_assert(t, "[1] size after src free == 1", DLFile_entrySize(&copy, 1) == 1);
		Test_assert(t, "[2] size after src free == 2", DLFile_entrySize(&copy, 2) == 2);
		Test_assert(t, "[3] size after src free == 3", DLFile_entrySize(&copy, 3) == 3);

		//Verify that string pointers fall inside the copy's own cache

		for (U64 i = 1; i < 4; ++i) {
			CharString str = copy.entryStrings.ptr[i];
			Test_assert(t, "copy string ptr inside copy cache",
				(const U8*)str.ptr >= copy.cache.ptr &&
				(const U8*)str.ptr + CharString_length(str) <= copy.cache.ptr + Buffer_length(copy.cache));
		}

	doneCacheStr:
		DLFile_free(&src,  t->alloc);
		DLFile_free(&copy, t->alloc);
	}

	//Mixed: some entries are heap-allocated (own alloc), some are cache refs.
	//The fixup must re-base only the cache-ref entries and leave heap entries alone.

	{
		DLFile src  = { 0 };
		DLFile copy = { 0 };

		//Start with a cache-ref file, then append a heap-allocated entry on top.

		if (!buildDataFileWithCacheRefs(t, &src, 3)) {
			Test_assert(t, "Setup 3-entry cache-ref Data file for mixed test", false);
			goto doneMixed;
		}

		U8 heapVal[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
		Buffer heap   = Buffer_createNull();

		if (!Buffer_createCopy(Buffer_createRefConst(heapVal, 4), t->alloc, &heap, &t->err)) {
			Test_assert(t, "Buffer_createCopy heap entry", false);
			goto doneMixed;
		}

		Test_assert(t, "addEntry heap ok", DLFile_addEntry(&src, &heap, t->alloc, &t->err));
		Test_assert(t, "src entryCount 4", DLFile_entryCount(&src) == 4);

		Test_assert(t, "createCopy mixed ok", DLFile_createCopy(&src, t->alloc, &copy, &t->err));
		Test_assert(t, "copy entryCount 4",   DLFile_entryCount(&copy) == 4);

		DLFile_free(&src, t->alloc);

		//Cache-ref entries (0-2)

		Test_assert(t, "[0] size == 0 after src free", DLFile_entrySize(&copy, 0) == 0);
		Test_assert(t, "[1] size == 1 after src free", DLFile_entrySize(&copy, 1) == 1);
		Test_assert(t, "[2] size == 2 after src free", DLFile_entrySize(&copy, 2) == 2);

		//Heap entry (3) - independently deep-copied, size must still be correct

		Test_assert(t, "[3] size == 4 after src free", DLFile_entrySize(&copy, 3) == 4);

	doneMixed:
		Buffer_free(&heap, t->alloc);
		DLFile_free(&src,  t->alloc);
		DLFile_free(&copy, t->alloc);
	}

	{                            //Copy-of-copy: rebasing must be applied fresh at each generation
		DLFile src   = { 0 };
		DLFile copy1 = { 0 };
		DLFile copy2 = { 0 };

		if (!buildDataFileWithCacheRefs(t, &src, 3)) {
			Test_assert(t, "Setup 3-entry for copy-of-copy", false);
			goto doneCopyOfCopy;
		}

		Test_assert(t, "copy1 ok", DLFile_createCopy(&src, t->alloc, &copy1, &t->err));
		DLFile_free(&src, t->alloc);

		Test_assert(t, "copy2 ok", DLFile_createCopy(&copy1, t->alloc, &copy2, &t->err));
		DLFile_free(&copy1, t->alloc);

		//copy2 must be fully valid even though src and copy1 are both gone

		Test_assert(t, "copy2 entryCount 3",              DLFile_entryCount(&copy2) == 3);
		Test_assert(t, "copy2 [0] size after both frees", DLFile_entrySize(&copy2, 0) == 0);
		Test_assert(t, "copy2 [1] size after both frees", DLFile_entrySize(&copy2, 1) == 1);
		Test_assert(t, "copy2 [2] size after both frees", DLFile_entrySize(&copy2, 2) == 2);

		//Pointers must be inside copy2's own cache, not copy1's (already freed)

		for (U64 i = 1; i < 3; ++i) {
			Buffer buf = copy2.entryBuffers.ptr[i];
			Test_assert(t, "copy2 ptr inside copy2 cache",
				buf.ptr >= copy2.cache.ptr &&
				buf.ptr + Buffer_length(buf) <= copy2.cache.ptr + Buffer_length(copy2.cache));
		}

	doneCopyOfCopy:
		DLFile_free(&src,   t->alloc);
		DLFile_free(&copy1, t->alloc);
		DLFile_free(&copy2, t->alloc);
	}

	{                            //Mutating the copy must not disturb the source's cache refs
		DLFile src  = { 0 };
		DLFile copy = { 0 };

		if (!buildDataFileWithCacheRefs(t, &src, 3)) {
			Test_assert(t, "Setup 3-entry for mutate-after-copy", false);
			goto doneMutate;
		}

		Test_assert(t, "createCopy ok", DLFile_createCopy(&src, t->alloc, &copy, &t->err));

		Test_assert(t, "remove [0] from copy",     DLFile_remove(&copy, 0, t->alloc, &t->err));
		Test_assert(t, "copy entryCount 2",        DLFile_entryCount(&copy) == 2);

		//Source must be completely unaffected

		Test_assert(t, "src entryCount intact",    DLFile_entryCount(&src) == 3);
		Test_assert(t, "src [0] intact",           DLFile_entrySize(&src, 0) == 0);
		Test_assert(t, "src [1] intact",           DLFile_entrySize(&src, 1) == 1);
		Test_assert(t, "src [2] intact",           DLFile_entrySize(&src, 2) == 2);

	doneMutate:
		DLFile_free(&src,  t->alloc);
		DLFile_free(&copy, t->alloc);
	}
}

void Test_DLCreateCopyPlain(Test *t) {
	
	Test_setModule(t, "DLFile_createCopy");

	{                            //Data file: copy preserves entry count and sizes
		DLFile src  = { 0 };
		DLFile copy = { 0 };

		if (!buildDataFile(t, &src, 4)) {
			Test_assert(t, "Setup 4-entry Data file for copy", false);
			goto doneCopyData;
		}

		Test_assert(t, "createCopy ok",         DLFile_createCopy(&src, t->alloc, &copy, &t->err));
		Test_assert(t, "entryCount matches",    DLFile_entryCount(&copy) == DLFile_entryCount(&src));

		for (U64 i = 0; i < 4; ++i)
			Test_assert(t, "entrySize matches", DLFile_entrySize(&copy, i) == DLFile_entrySize(&src, i));

		//Mutating the copy must not affect the source

		Test_assert(t, "remove from copy",      DLFile_remove(&copy, 0, t->alloc, &t->err));
		Test_assert(t, "src entryCount intact", DLFile_entryCount(&src) == 4);
		Test_assert(t, "copy entryCount 3",     DLFile_entryCount(&copy) == 3);

	doneCopyData:
		DLFile_free(&src,  t->alloc);
		DLFile_free(&copy, t->alloc);
	}

	{                            //String file: copy preserves entry count and sizes
		DLFile src  = { 0 };
		DLFile copy = { 0 };

		if (!buildStringFile(t, &src, 3)) {
			Test_assert(t, "Setup 3-entry String file for copy", false);
			goto doneCopyStr;
		}

		Test_assert(t, "createCopy String ok",  DLFile_createCopy(&src, t->alloc, &copy, &t->err));
		Test_assert(t, "entryCount matches",    DLFile_entryCount(&copy) == DLFile_entryCount(&src));

		for (U64 i = 0; i < 3; ++i)
			Test_assert(t, "entrySize matches", DLFile_entrySize(&copy, i) == DLFile_entrySize(&src, i));

	doneCopyStr:
		DLFile_free(&src,  t->alloc);
		DLFile_free(&copy, t->alloc);
	}

	{                            //Copy of empty file succeeds and yields an empty file
		DLFile src  = { 0 };
		DLFile copy = { 0 };

		if (!DLFile_create(&kSettingsData, 0, t->alloc, &src, &t->err)) {
			Test_assert(t, "Create empty Data file for copy", false);
			goto doneCopyEmpty;
		}

		Test_assert(t, "createCopy empty ok",   DLFile_createCopy(&src, t->alloc, &copy, &t->err));
		Test_assert(t, "copy entryCount 0",     DLFile_entryCount(&copy) == 0);

	doneCopyEmpty:
		DLFile_free(&src,  t->alloc);
		DLFile_free(&copy, t->alloc);
	}

	{                            //Settings are preserved in the copy
		DLFile src  = { 0 };
		DLFile copy = { 0 };

		if (!buildStringFile(t, &src, 1)) {
			Test_assert(t, "Setup String file for settings check", false);
			goto doneCopySettings;
		}

		Test_assert(t, "createCopy settings ok", DLFile_createCopy(&src, t->alloc, &copy, &t->err));
		Test_assert(t, "dataType preserved",     copy.settings.dataType     == src.settings.dataType);
		Test_assert(t, "compressionType pres.",  copy.settings.compressionType == src.settings.compressionType);
		Test_assert(t, "encryptionType pres.",   copy.settings.encryptionType  == src.settings.encryptionType);

	doneCopySettings:
		DLFile_free(&src,  t->alloc);
		DLFile_free(&copy, t->alloc);
	}

	{                            //Null source (dlFile == NULL): must return empty/zeroed copy
		DLFile copy = { 0 };
		Test_assert(t, "createCopy null src ok", DLFile_createCopy(NULL, t->alloc, &copy, NULL));
		Test_assert(t, "copy entryCount 0",      DLFile_entryCount(&copy) == 0);
		DLFile_free(&copy, t->alloc);
	}

	{                            //Null copy pointer: must fail gracefully
		DLFile src = { 0 };

		if (!buildDataFile(t, &src, 1)) {
			Test_assert(t, "Setup for null copy ptr test", false);
			goto doneCopyNullDst;
		}

		Test_assert(t, "createCopy null dst fails", !DLFile_createCopy(&src, t->alloc, NULL, NULL));

	doneCopyNullDst:
		DLFile_free(&src, t->alloc);
	}

	{                            //Non-empty copy destination: must fail to prevent memory leak
		DLFile src  = { 0 };
		DLFile copy = { 0 };

		if (!buildDataFile(t, &src, 2) || !buildDataFile(t, &copy, 1)) {
			Test_assert(t, "Setup for non-empty copy dst test", false);
			goto doneCopyDirty;
		}

		Test_assert(t, "createCopy dirty dst fails", !DLFile_createCopy(&src, t->alloc, &copy, NULL));

	doneCopyDirty:
		DLFile_free(&src,  t->alloc);
		DLFile_free(&copy, t->alloc);
	}
}

void Test_DLCopy(Test *t) {
	Test_DLCreateCopyPlain(t);
	Test_DLCreateCopyCacheRefs(t);
}
