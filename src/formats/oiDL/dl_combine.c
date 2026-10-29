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

//formats/oiDL/dl_combine.c

#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/base/string_base.h"
#include "types/container/list_basic_types.h"
#include "types/container/ref_ptr.h"
#include "types/base/mathi.h"

Bool DLFile_combine(const DLFile *a, const DLFile *b, const Allocator *alloc, DLFile *combined, Error *e_rr) {

	Bool s_uccess = true;
	CharString tmpStr = CharString_createNull();
	Buffer tmp = Buffer_createNull();
	U64 cacheIt = 0;
	Bool allocated = false;
	StreamRef *pendingRef = NULL;

	if(!a || !b)
		retError(clean, Error_nullPointer(!a ? 0 : 1, "DLFile_combine()::a and b are required"));

	//DLSettings only guarantees U32 alignment, so comparing it in place as U64s can be a misaligned load;
	// a byte compare over the same span avoids that.

	if(Buffer_neq(
		Buffer_createRefConst(&a->settings, sizeof(U64) * 7),
		Buffer_createRefConst(&b->settings, sizeof(U64) * 7)
	))
		retError(clean, Error_invalidParameter(1, 0, "DLFile_combine()::a is incompatible with b"));

	//"Combine" cache, but keep it limited to 1 MiB

	U64 cacheSize = U64_min(Buffer_length(a->cache) + Buffer_length(b->cache), 4 * MIBI);
	gotoIfError3(clean, DLFile_create(&a->settings, cacheSize, alloc, combined, e_rr));

	allocated = true;

	//Every entry of a, then every entry of b, in order.
	//Entries that are still in a stream stay in a stream, and the combined file holds its own reference to them.

	const U64 alen = a->entryStreams.length;
	const U64 total = alen + b->entryStreams.length;
	const Bool isString = a->settings.dataType == EDLDataType_String;

	gotoIfError3(clean, DLFile_reserve(combined, total, alloc, e_rr));

	for (U64 i = 0; i < total; ++i) {

		const DLFile *src = i < alen ? a : b;
		const U64 k = i < alen ? i : i - alen;

		if (!DLFile_isFullyLoaded(src, k)) {

			const DLEntryStream entryStream = src->entryStreams.ptr[k];

			pendingRef = entryStream.stream;
			RefPtr_inc(pendingRef);

			gotoIfError3(clean, DLFile_addEntryStream(
				combined, &pendingRef, entryStream.dataOff, entryStream.len, alloc, e_rr
			));

			continue;
		}

		if (isString) {

			CharString str = src->entryStrings.ptr[k];
			U64 strl = CharString_length(str);

			//Put into cache
			//cacheSize is checked first because two cache-less files still satisfy the rest of this condition
			// for a zero length entry (0 < smallLen and 0 + 0 <= 0), and the cache base is then null, which
			// makes offsetting it undefined.
			//There is no cache to place anything in at that point, so such an entry takes the copy path below
			// and lands as the same empty string either way.

			if (cacheSize && strl < DLFile_smallLen && cacheIt + strl <= cacheSize) {

				Buffer_memcpy(
					Buffer_createRef(combined->cache.ptrNonConst + cacheIt, strl),
					Buffer_createRefConst(str.ptr, strl)
				);

				tmpStr = CharString_createRefSized((C8*) combined->cache.ptrNonConst + cacheIt, strl, false);
				cacheIt += strl;
			}

			else gotoIfError3(clean, CharString_createCopy(str, alloc, &tmpStr, e_rr));

			gotoIfError3(clean, DLFile_addEntryString(combined, &tmpStr, alloc, e_rr));
		}

		else {

			Buffer buf = src->entryBuffers.ptr[k];
			U64 bufl = Buffer_length(buf);

			//Put into cache
			//Same reason as the string branch above: without a cache the base is null, and a zero length entry
			// otherwise satisfies this condition and offsets it.

			if (cacheSize && bufl < DLFile_smallLen && cacheIt + bufl <= cacheSize) {

				Buffer_memcpy(
					Buffer_createRef(combined->cache.ptrNonConst + cacheIt, bufl),
					Buffer_createRefConst(buf.ptr, bufl)
				);

				tmp = Buffer_createRef(combined->cache.ptrNonConst + cacheIt, bufl);
				cacheIt += bufl;
			}

			else gotoIfError3(clean, Buffer_createCopy(buf, alloc, &tmp, e_rr));

			gotoIfError3(clean, DLFile_addEntry(combined, &tmp, alloc, e_rr));
		}
	}

clean:
	RefPtr_dec(&pendingRef);
	CharString_free(&tmpStr, alloc);

	if (!s_uccess && allocated)
		DLFile_free(combined, alloc);

	Buffer_free(&tmp, alloc);
	return s_uccess;
}
