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

#include "test_types_container_shared.h"
#include "types/container/test/stream.h"
#include "types/container/encryption_stream.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"
#include "types/base/mathi.h"

static const U32 encTestKey[8] = {
	0xCD00324F, 0x4CBAAE34, 0x67924E05, 0x78012F15,
	0x5A8F573A, 0xA066652D, 0xDDB8C2E1, 0xF76AF7FE
};

RefPtrType memType;

static Bool EncStream_harnessCreate(
	const StreamHarness *h, U64 size, Bool isResizable, RefPtr **out, Test *t
) {
	(void)h;

	const I32x4      rootIV  = I32x4_create4(0x11223344, 0x55667788, 0x99AABBCC, 0);
	static const U8 empty[65536] = { 0 };
	static const U64 chunkSize = sizeof(empty);

	RefPtr *backing = NULL;
	EMemoryStreamFlags flags = EMemoryStreamFlags_IsWritable;
	
	U64 realSize = EncryptionStream_underlyingSize(chunkSize, size);

	if (isResizable)
		flags |= EMemoryStreamFlags_IsResizable;

	if (!MemoryStream_create(realSize, flags, &memType, &backing, &t->err)) {
		Test_assert(t, "EncStream backing MemStream create", false);
		return false;
	}

	Bool ok = EncryptionStream_create(
		backing, 0, encTestKey, rootIV, chunkSize, 0, h->type, out, &t->err
	);

	RefPtr_dec(&backing);

	if (!ok) {
		Test_assert(t, "EncryptionStream_create", false);
		return false;
	}

	if (size) {

		Stream *s = RefPtr_data(*out, Stream);

		//Clear which also sets s->size without having garbage.
		//Garbage is fine with MemoryStream but not with others like EncryptionStream

		for(U64 i = 0; i < size; i += chunkSize)
			if (!s->write(s, i, U64_min(size - i, chunkSize), Buffer_createRefConst(empty, chunkSize), t->alloc, &t->err)) {
				Test_assert(t, "EncStream write empty bytes", false);
				RefPtr_dec(out);
				return false;
			}
	}

	return true;
}

void Test_encryptionStream(Test *t) {

	const RefPtrType type = EncryptionStream_makeType(t->alloc);
	memType = MemoryStream_makeType(t->alloc);

	StreamHarness h = {
		.create = EncStream_harnessCreate,
		.type = &type,
		.name = "EncryptionStream"
	};

	StreamHarness_testStream(&h, t);
	StreamHarness_testCursor(&h, t);
	Test_setModule(t, NULL);
}
