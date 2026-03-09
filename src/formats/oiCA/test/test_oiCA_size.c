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

#include "test_oiCA_shared.h"
#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_props.h"
#include "types/base/time.h"

CAHandle addFile(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Ns time, Bool failIsSuccess);
CAHandle addFolder(Test *t, CAFile *ca, CAHandle parent, const C8 *name, Bool failIsSuccess);

static inline CAHandle CAFile_resolveCStr(CAFile *ca, const C8 *name) {
	return CAFile_resolve(ca, CharString_createRefCStrConst(name));
}

extern const CASettings kCASettings;
extern const CASettings kCASettingsDate;
extern const CASettings kCASettingsFullDate;

//Key used for all encryption tests: 32 bytes (8 x U32)
static const U32 kTestKey[8] = {
	0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210,
	0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0x9ABCDEF0
};

//Helper: run a null-stream write then a real write and compare sizes.
static Bool caCheckSizeConsistency(
	Test *t,
	CAFile *ca,
	const RefPtrType *memType,
	const RefPtrType *encStreamType
) {
	U64 predictedSize = 0;

	if (!CAFile_write(ca, encStreamType, NULL, &predictedSize, t->alloc, &t->err))
		return false;

	StreamRef *sr = NULL;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memType, &sr, &t->err))
		return false;

	U64 realSize = 0;
	Bool ok = CAFile_write(ca, encStreamType, sr, &realSize, t->alloc, &t->err);
	RefPtr_dec(&sr);

	return ok && predictedSize == realSize;
}

//Empty archive: no files, no dirs.
void Test_CAWriteSizeConsistencyEmpty(Test *t) {

	Test_setModule(t, "CAFile write: null size == real size (empty)");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		CAFile ca = { 0 };

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create empty ca size", false);
			goto doneEmptySize;
		}

		Test_assert(t, "empty size consistent", caCheckSizeConsistency(t, &ca, &type, NULL));

	doneEmptySize:
		CAFile_free(&ca, t->alloc);
	}
}

//Single file at root.
void Test_CAWriteSizeConsistencySingleFile(Test *t) {

	Test_setModule(t, "CAFile write: null size == real size (single file)");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		CAFile ca = { 0 };

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create single file ca size", false);
			goto doneSingleSize;
		}

		CAHandle hf = addFile(t, &ca, CAHandle_Root, "file.bin", 0, false);
		U8 b = 0x42;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		CAFile_setData(&ca, hf, t->alloc, &buf, NULL);

		Test_assert(t, "single file size consistent", caCheckSizeConsistency(t, &ca, &type, NULL));

	doneSingleSize:
		CAFile_free(&ca, t->alloc);
	}
}

//Deep hierarchy: multiple folders and files at different nesting levels.
void Test_CAWriteSizeConsistencyHierarchy(Test *t) {

	Test_setModule(t, "CAFile write: null size == real size (hierarchy)");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		CAFile ca = { 0 };

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create hier ca size", false);
			goto doneHierSize;
		}

		CAHandle hA  = addFolder(t, &ca, CAHandle_Root, "a", false);
		CAHandle hB  = addFolder(t, &ca, hA, "b", false);
		CAHandle hC  = addFolder(t, &ca, hB, "c", false);

		U8 vals[4] = { 0xAA, 0xBB, 0xCC, 0xDD };

		const C8 *names[4]    = { "root.bin", "a.bin", "b.bin", "c.bin" };
		CAHandle parents[4]   = { CAHandle_Root, hA, hB, hC };

		for (U64 i = 0; i < 4; ++i) {
			CAHandle hf = addFile(t, &ca, parents[i], names[i], 0, false);
			Buffer buf = Buffer_createNull();
			Buffer_createCopy(Buffer_createRefConst(&vals[i], 1), t->alloc, &buf, NULL);
			CAFile_setData(&ca, hf, t->alloc, &buf, NULL);
		}

		Test_assert(t, "hierarchy size consistent", caCheckSizeConsistency(t, &ca, &type, NULL));

	doneHierSize:
		CAFile_free(&ca, t->alloc);
	}
}

//IncludeDate flag: MS-DOS date adds 4 bytes per file entry.
void Test_CAWriteSizeConsistencyMsDosDate(Test *t) {

	Test_setModule(t, "CAFile write: null size == real size (MS-DOS date)");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		CAFile ca = { 0 };

		if (!CAFile_create(&kCASettingsDate, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create msdos ca size", false);
			goto doneMsDosSize;
		}

		Date date = (Date) { .year = 2000, .month = 6, .day = 15 };
		Ns ts = Time_date(&date, false);

		addFile(t, &ca, CAHandle_Root, "dated.bin", ts, false);

		Test_assert(t, "msdos date size consistent", caCheckSizeConsistency(t, &ca, &type, NULL));

	doneMsDosSize:
		CAFile_free(&ca, t->alloc);
	}
}

//IncludeFullDate flag: full Ns timestamp adds 8 bytes per file entry.
void Test_CAWriteSizeConsistencyFullDate(Test *t) {

	Test_setModule(t, "CAFile write: null size == real size (full date)");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		CAFile ca = { 0 };

		if (!CAFile_create(&kCASettingsFullDate, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create fulldate ca size", false);
			goto doneFullDateSize;
		}

		Ns ts = (Ns)946681200 * SECOND + 123 * MS;
		addFile(t, &ca, CAHandle_Root, "precise.bin", ts, false);

		Test_assert(t, "full date size consistent", caCheckSizeConsistency(t, &ca, &type, NULL));

	doneFullDateSize:
		CAFile_free(&ca, t->alloc);
	}
}

//Encryption: null-stream pass must still predict the correct final size including iv + tag.
void Test_CAWriteSizeConsistencyEncrypted(Test *t) {

	Test_setModule(t, "CAFile write: null size == real size (encrypted)");

	const RefPtrType type         = MemoryStream_makeType(t->alloc);
	const RefPtrType encStreamType = EncryptionStream_makeType(t->alloc);

	{
		CAFile ca = { 0 };

		CASettings settings = {
			.flags           = ECASettingsFlags_None,
			.compressionType = EXXCompressionType_None,
			.encryptionType  = EXXEncryptionType_AES256GCM
		};

		Buffer_memcpy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRefConst(kTestKey, sizeof(kTestKey))
		);

		if (!CAFile_create(&settings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create enc ca size", false);
			goto doneEncSize;
		}

		CAHandle hf = addFile(t, &ca, CAHandle_Root, "secret.bin", 0, false);
		U8 b = 0x55;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		CAFile_setData(&ca, hf, t->alloc, &buf, NULL);

		Test_assert(t, "encrypted size consistent", caCheckSizeConsistency(t, &ca, &type, &encStreamType));

	doneEncSize:
		CAFile_free(&ca, t->alloc);
	}
}

//Long directory count (>254): triggers dirCountLong path, changing ref sizes.
void Test_CAWriteSizeConsistencyLongDirCount(Test *t) {

	Test_setModule(t, "CAFile write: null size == real size (long dir count)");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		CAFile ca = { 0 };

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create long dir ca size", false);
			goto doneLongDir;
		}

		//Create 255 folders under root to cross the dirCountLong threshold

		for (U32 i = 0; i < 255; ++i) {

			C8 name[16] = { 0 };
			name[0] = 'd';
			name[1] = (C8)('0' + i / 100);
			name[2] = (C8)('0' + (i / 10) % 10);
			name[3] = (C8)('0' + i % 10);

			addFolder(t, &ca, CAHandle_Root, name, false);

			if (t->err.genericError)
				goto doneLongDir;
		}

		Test_assert(t, "long dir count size consistent", caCheckSizeConsistency(t, &ca, &type, NULL));

	doneLongDir:
		CAFile_free(&ca, t->alloc);
	}
}
