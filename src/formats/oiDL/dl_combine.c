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

#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/base/string.h"
#include "types/container/list_basic_types.h"
#include "types/container/ref_ptr.h"
#include "types/base/mathi.h"

Bool DLFile_combine(const DLFile *a, const DLFile *b, const Allocator *alloc, DLFile *combined, Error *e_rr) {

	Bool s_uccess = true;
	CharString tmpStr = CharString_createNull();
	Buffer tmp = Buffer_createNull();
	U64 cacheIt = 0;

	if(!a || !b)
		retError(clean, Error_nullPointer(!a ? 0 : 1, "DLFile_combine()::a and b are required"));

	const void *aSettingsPtr = &a->settings;
	const void *bSettingsPtr = &b->settings;

	for(U64 i = 0; i < 7; ++i)
		if(((const U64*)aSettingsPtr)[i] != ((const U64*)bSettingsPtr)[i])
			retError(clean, Error_invalidParameter(1, 0, "DLFile_combine()::a is incompatible with b"));

	//"Combine" cache, but keep it limited to 1 MiB

	U64 cacheSize = U64_min(Buffer_length(a->cache) + Buffer_length(b->cache), 4 * MIBI);
	gotoIfError3(clean, DLFile_create(&a->settings, cacheSize, alloc, combined, e_rr));

	//Merge stream ids and stream refs

	U64 streamCounta = a->entryStreams.length;
	U64 streamCount = streamCounta + b->entryStreams.length;
	gotoIfError3(clean, ListDLEntryStream_reserve(&combined->entryStreams, streamCount, alloc, e_rr));

	for (U64 i = 0; i < a->entryStreams.length; ++i) {

		DLEntryStream entryStream = a->entryStreams.ptr[i];
		combined->entryStreams.ptrNonConst[i] = entryStream;

		if (entryStream.stream)
			RefPtr_inc(entryStream.stream);
	}

	for (U64 i = 0; i < b->entryStreams.length; ++i) {

		DLEntryStream entryStream = b->entryStreams.ptr[i];
		combined->entryStreams.ptrNonConst[i + streamCounta] = entryStream;

		if(entryStream.stream)
			RefPtr_inc(entryStream.stream);
	}

	//Combine

	EDLDataType dataType = a->settings.dataType;

	if(dataType == EDLDataType_String) {

		ListCharString entryStringsa = a->entryStrings;
		ListCharString entryStringsb = b->entryStrings;

		gotoIfError3(clean, ListCharString_reserve(
			&combined->entryStrings, entryStringsa.length + entryStringsb.length, alloc, e_rr
		));

		U64 alen = entryStringsa.length;
		U64 j = alen + entryStringsb.length;

		for (U64 i = 0; i < j; ++i) {

			if (!DLFile_isFullyLoaded(i < alen ? a : b, i < alen ? i : i - alen))	//Not in memory
				continue;

			CharString str = i < alen ? entryStringsa.ptr[i] : entryStringsb.ptr[i - alen];
			U64 strl = CharString_length(str);

			//Put into cache

			if (strl < DLFile_smallLen && cacheIt + strl <= cacheSize) {

				Buffer_memcpy(
					Buffer_createRef(combined->cache.ptrNonConst + cacheIt, strl),
					Buffer_createRefConst(str.ptr, strl)
				);

				tmpStr = CharString_createRefSized((C8*)combined->cache.ptrNonConst + cacheIt, strl, false);
				cacheIt += strl;
			}

			else gotoIfError3(clean, CharString_createCopy(str, alloc, &tmpStr, e_rr));

			gotoIfError3(clean, DLFile_addEntryString(combined, &tmpStr, alloc, e_rr));
		}
	}

	else {

		ListBuffer entryBuffersa = a->entryBuffers;
		ListBuffer entryBuffersb = b->entryBuffers;

		gotoIfError3(clean, ListBuffer_reserve(
			&combined->entryBuffers, entryBuffersa.length + entryBuffersb.length, alloc, e_rr
		));

		U64 alen = entryBuffersa.length;
		U64 j = alen + entryBuffersb.length;

		for (U64 i = 0; i < j; ++i) {

			if (!DLFile_isFullyLoaded(i < alen ? a : b, i < alen ? i : i - alen))	//Not in memory
				continue;

			Buffer buf = i < alen ? entryBuffersa.ptr[i] : entryBuffersb.ptr[i - alen];
			U64 bufl = Buffer_length(buf);

			//Put into cache

			if (bufl < DLFile_smallLen && cacheIt + bufl <= cacheSize) {

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
	CharString_free(&tmpStr, alloc);
	Buffer_free(&tmp, alloc);
	return s_uccess;
}
