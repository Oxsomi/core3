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

//formats/oiCA/test/test_oiCA_serialization.c

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

static const U32 kWrongKey[8] = {
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static Bool writeAndRead(
	Test *t,
	CAFile *caIn,
	const U32 key[8],
	const RefPtrType *memType,
	const RefPtrType *encStreamType,
	StreamRef **streamOut,
	CAFile *caOut
) {
	StreamRef *sr = NULL;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memType, &sr, &t->err))
		return false;

	U64 startOffset = 0;
	if (!CAFile_write(caIn, encStreamType, sr, &startOffset, t->alloc, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	if (!CAFile_read(sr, NULL, 0, key, t->alloc, caOut, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	*streamOut = sr;
	return true;
}

//An empty archive (no files, no dirs) must write and read back cleanly.
void Test_CASerializeEmpty(Test *t) {

	Test_setModule(t, "CAFile serialize: empty archive");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);

	{
		CAFile ca = { 0 };
		CAFile ca2 = { 0 };
		StreamRef *sr = NULL;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create empty ca", false);
			goto doneEmpty;
		}

		Test_assert(t, "write + read empty", writeAndRead(t, &ca, NULL, &memType, NULL, &sr, &ca2));
		Test_assert(t, "fileCount == 0", CAFile_fileCount(&ca2, CAHandle_Root, false) == 0);
		Test_assert(t, "dirCount == 0", CAFile_dirCount(&ca2, CAHandle_Root, false) == 0);

	doneEmpty:
		RefPtr_dec(&sr);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&ca2, t->alloc);
	}
}

//One file at root, content must survive the round-trip exactly.
void Test_CASerializeSingleFile(Test *t) {

	Test_setModule(t, "CAFile serialize: single file at root");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);

	{
		CAFile ca = { 0 };
		CAFile ca2 = { 0 };
		StreamRef *sr = NULL;
		Buffer buf = Buffer_createNull();

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create ca single", false);
			goto doneSingle;
		}

		const U8 payload[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x11, 0x22, 0x33 };

		CAHandle hf = addFile(t, &ca, CAHandle_Root, "hello.bin", 0, false);

		if (!Buffer_createCopy(Buffer_createRefConst(payload, sizeof payload), t->alloc, &buf, &t->err)) {
			Test_assert(t, "createCopy single", false);
			goto doneSingle;
		}

		Test_assert(t, "setData single", CAFile_setData(&ca, hf, t->alloc, &buf, &t->err));

		Test_assert(t, "write + read single", writeAndRead(t, &ca, NULL, &memType, NULL, &sr, &ca2));

		Test_assert(t, "fileCount == 1", CAFile_fileCount(&ca2, CAHandle_Root, false) == 1);
		Test_assert(t, "dirCount  == 0", CAFile_dirCount(&ca2, CAHandle_Root, false) == 0);

		CAHandle hf2 = CAFile_resolveCStr(&ca2, "hello.bin");
		Test_assert(t, "resolve hello.bin", hf2 != CAHandle_Invalid);
		Test_assert(t, "fileSize == 8", CAFile_fileSize(&ca2, hf2) == sizeof(payload));

		Bool valid = false;
		Buffer got = CAFile_getDataConst(&ca2, hf2, &valid);
		Test_assert(t, "getData valid", valid);
		Test_assert(t, "content matches", Buffer_eq(got, Buffer_createRefConst(payload, sizeof(payload))));

	doneSingle:
		Buffer_free(&buf, t->alloc);
		RefPtr_dec(&sr);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&ca2, t->alloc);
	}
}

//Deep nested hierarchy: root/a/b/c/ with files at each level.
//Verifies parent resolution survives write/read.
void Test_CASerializeHierarchy(Test *t) {

	const RefPtrType memType = MemoryStream_makeType(t->alloc);

	Test_setModule(t, "CAFile serialize: nested hierarchy");

	{
		CAFile ca = { 0 };
		CAFile ca2 = { 0 };
		StreamRef *sr = NULL;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create ca hier", false);
			goto doneHier;
		}

		CAHandle hA = addFolder(t, &ca, CAHandle_Root, "a", false);
		CAHandle hB = addFolder(t, &ca, hA, "b", false);
		CAHandle hC = addFolder(t, &ca, hB, "c", false);

		//Files at each level

		CAHandle hR = addFile(t, &ca, CAHandle_Root, "root.txt", 0, false);
		CAHandle hAf = addFile(t, &ca, hA, "a.txt", 0, false);
		CAHandle hBf = addFile(t, &ca, hB, "b.txt", 0, false);
		CAHandle hCf = addFile(t, &ca, hC, "c.txt", 0, false);

		//Give each file distinct 1-byte content so we can tell them apart

		U8 bR = 0xAA, bA = 0xBB, bB = 0xCC, bC = 0xDD;
		Buffer bufs[4] = { 0 };
		Buffer_createCopy(Buffer_createRefConst(&bR, 1), t->alloc, &bufs[0], NULL);
		Buffer_createCopy(Buffer_createRefConst(&bA, 1), t->alloc, &bufs[1], NULL);
		Buffer_createCopy(Buffer_createRefConst(&bB, 1), t->alloc, &bufs[2], NULL);
		Buffer_createCopy(Buffer_createRefConst(&bC, 1), t->alloc, &bufs[3], NULL);
		CAFile_setData(&ca, hR, t->alloc, &bufs[0], NULL);
		CAFile_setData(&ca, hAf, t->alloc, &bufs[1], NULL);
		CAFile_setData(&ca, hBf, t->alloc, &bufs[2], NULL);
		CAFile_setData(&ca, hCf, t->alloc, &bufs[3], NULL);

		Test_assert(t, "write + read hierachy", writeAndRead(t, &ca, NULL, &memType, NULL, &sr, &ca2));

		Test_assert(t, "fileCount == 4 (recursion)", CAFile_fileCount(&ca2, CAHandle_Root, true) == 4);
		Test_assert(t, "dirCount  == 3 (recursion)", CAFile_dirCount(&ca2, CAHandle_Root, true) == 3);
		Test_assert(t, "fileCount == 1", CAFile_fileCount(&ca2, CAHandle_Root, false) == 1);
		Test_assert(t, "dirCount  == 1", CAFile_dirCount(&ca2, CAHandle_Root, false) == 1);

		//Resolve by full path

		CAHandle h2R = CAFile_resolveCStr(&ca2, "root.txt");
		CAHandle h2Af = CAFile_resolveCStr(&ca2, "a/a.txt");
		CAHandle h2Bf = CAFile_resolveCStr(&ca2, "a/b/b.txt");
		CAHandle h2Cf = CAFile_resolveCStr(&ca2, "a/b/c/c.txt");

		Test_assert(t, "resolve root.txt", h2R != CAHandle_Invalid);
		Test_assert(t, "resolve a/a.txt", h2Af != CAHandle_Invalid);
		Test_assert(t, "resolve a/b/b.txt", h2Bf != CAHandle_Invalid);
		Test_assert(t, "resolve a/b/c/c.txt", h2Cf != CAHandle_Invalid);

		//Content check

		Bool v = false;
		Buffer g = CAFile_getDataConst(&ca2, h2R, &v);
		Test_assert(t, "root.txt content", v && Buffer_length(g) == 1 && g.ptr[0] == 0xAA);

		g = CAFile_getDataConst(&ca2, h2Af, &v);
		Test_assert(t, "a.txt content", v && Buffer_length(g) == 1 && g.ptr[0] == 0xBB);

		g = CAFile_getDataConst(&ca2, h2Bf, &v);
		Test_assert(t, "b.txt content", v && Buffer_length(g) == 1 && g.ptr[0] == 0xCC);

		g = CAFile_getDataConst(&ca2, h2Cf, &v);
		Test_assert(t, "c.txt content", v && Buffer_length(g) == 1 && g.ptr[0] == 0xDD);

	doneHier:
		RefPtr_dec(&sr);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&ca2, t->alloc);
	}
}

//MS-DOS date has 2-second resolution.
//Write with IncludeDate, read back, verify the timestamp matches
void Test_CASerializeMsDosDate(Test *t) {

	const RefPtrType memType = MemoryStream_makeType(t->alloc);

	Test_setModule(t, "CAFile serialize: MS-DOS date round-trip");

	{
		CAFile ca = { 0 };
		CAFile ca2 = { 0 };
		StreamRef *sr = NULL;

		if (!CAFile_create(&kCASettingsDate, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create ca msdos", false);
			goto doneMsDos;
		}

		Date date = (Date) { .year = 2000, .month = 1, .day = 1, };
		Ns ts = Time_date(&date, false);

		addFile(t, &ca, CAHandle_Root, "dated.txt", ts, false);

		Test_assert(t, "write + read msdos", writeAndRead(t, &ca, NULL, &memType, NULL, &sr, &ca2));

		CAHandle hf2 = CAFile_resolveCStr(&ca2, "dated.txt");
		Test_assert(t, "resolve dated.txt", hf2 != CAHandle_Invalid);

		Ns got = CAFile_fileTime(&ca2, hf2);

		Test_assert(t, "msdos timestamp equal", ts == got);

	doneMsDos:
		RefPtr_dec(&sr);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&ca2, t->alloc);
	}
}

//Full 64-bit Ns timestamp (ECASettingsFlags_IncludeFullDate), exact round-trip.
void Test_CASerializeFullDate(Test *t) {

	const RefPtrType memType = MemoryStream_makeType(t->alloc);

	Test_setModule(t, "CAFile serialize: full date round-trip");

	{
		CAFile ca = { 0 };
		CAFile ca2 = { 0 };
		StreamRef *sr = NULL;

		if (!CAFile_create(&kCASettingsFullDate, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create ca fulldate", false);
			goto doneFullDate;
		}

		Ns ts = (Ns)946681200 * SECOND + 123 * MS;        //We can only store up to MSI

		addFile(t, &ca, CAHandle_Root, "precise.txt", ts, false);

		Test_assert(t, "write+read fulldate", writeAndRead(t, &ca, NULL, &memType, NULL, &sr, &ca2));

		CAHandle hf2 = CAFile_resolveCStr(&ca2, "precise.txt");
		Test_assert(t, "resolve precise.txt", hf2 != CAHandle_Invalid);

		Test_assert(t, "exact ns timestamp", CAFile_fileTime(&ca2, hf2) == ts);

	doneFullDate:
		RefPtr_dec(&sr);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&ca2, t->alloc);
	}
}

// Write with encryption, read back with correct key, wrong key, and no key.
void Test_CASerializeEncrypted(Test *t) {

	Test_setModule(t, "CAFile serialize: encryption");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	const RefPtrType encStreamType = EncryptionStream_makeType(t->alloc);

	CASettings kCASettingsEncrypted = {
		.flags = ECASettingsFlags_None,
		.compressionType = EXXCompressionType_None,
		.encryptionType = EXXEncryptionType_AES256GCM
	};

	Buffer_memcpy(
		Buffer_createRef(kCASettingsEncrypted.encryptionKey, sizeof(kCASettingsEncrypted.encryptionKey)),
		Buffer_createRefConst(kTestKey, sizeof(kTestKey))
	);

	//Correct key: full round-trip must succeed and content must match

	{
		CAFile ca = { 0 };
		CAFile ca2 = { 0 };
		StreamRef *sr = NULL;

		if (!CAFile_create(&kCASettingsEncrypted, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create ca encrypted", false);
			goto doneEncCorrect;
		}

		const U8 secret[] = { 0x53, 0x65, 0x63, 0x72, 0x65, 0x74 };    //"Secret"

		CAHandle hf = addFile(t, &ca, CAHandle_Root, "secret.bin", 0, false);
		Buffer buf = Buffer_createNull();

		if (!Buffer_createCopy(Buffer_createRefConst(secret, sizeof(secret)), t->alloc, &buf, &t->err)) {
			Test_assert(t, "createCopy enc", false);
			goto doneEncCorrect;
		}

		Test_assert(t, "setData enc", CAFile_setData(&ca, hf, t->alloc, &buf, &t->err));

		//Write to stream

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &sr, &t->err)) {
			Test_assert(t, "MemoryStream enc create", false);
			goto doneEncCorrect;
		}

		U64 streamOff = 0;
		Test_assert(t, "write encrypted", CAFile_write(&ca, &encStreamType, sr, &streamOff, t->alloc, &t->err));

		//Read with correct key

		Test_assert(t, "read enc correct key", CAFile_read(sr, &encStreamType, 0, kTestKey, t->alloc, &ca2, &t->err));

		CAHandle hf2 = CAFile_resolveCStr(&ca2, "secret.bin");
		Test_assert(t, "resolve secret.bin", hf2 != CAHandle_Invalid);

		Bool valid = false;
		Buffer got = CAFile_getDataConst(&ca2, hf2, &valid);
		Test_assert(t, "enc content valid", valid);
		Test_assert(t, "enc content matches", Buffer_eq(got, Buffer_createRefConst(secret, sizeof(secret))));

	doneEncCorrect:
		Buffer_free(&buf, t->alloc);
		RefPtr_dec(&sr);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&ca2, t->alloc);
	}

	//Wrong key: read must fail (AAD or content decryption mismatch)

	{
		CAFile ca = { 0 };
		CAFile ca2 = { 0 };
		StreamRef *sr = NULL;

		if (!CAFile_create(&kCASettingsEncrypted, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create ca enc wrong key", false);
			goto doneEncWrong;
		}

		CAHandle hf = addFile(t, &ca, CAHandle_Root, "secret2.bin", 0, false);

		U8 b = 0x42;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		CAFile_setData(&ca, hf, t->alloc, &buf, NULL);

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &sr, &t->err)) {
			Test_assert(t, "MemoryStream enc wrong key", false);
			goto doneEncWrong;
		}

		U64 streamOff = 0;
		Test_assert(t, "write enc wrong key", CAFile_write(&ca, &encStreamType, sr, &streamOff, t->alloc, &t->err));

		Test_assert(t, "read with wrong key fails", !CAFile_read(sr, &encStreamType, 0, kWrongKey, t->alloc, &ca2, NULL));
		Test_assert(t, "caOut not allocated after fail", !ca2.folders.ptr);

	doneEncWrong:
		RefPtr_dec(&sr);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&ca2, t->alloc);
	}

	//No key: read must fail with unauthorized error before any decryption is attempted

	{
		CAFile ca = { 0 };
		CAFile ca2 = { 0 };
		StreamRef *sr = NULL;

		if (!CAFile_create(&kCASettingsEncrypted, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create ca enc no key", false);
			goto doneEncNoKey;
		}

		CAHandle hf = addFile(t, &ca, CAHandle_Root, "secret3.bin", 0, false);

		U8 b = 0x99;
		Buffer buf = Buffer_createNull();
		Buffer_createCopy(Buffer_createRefConst(&b, 1), t->alloc, &buf, NULL);
		CAFile_setData(&ca, hf, t->alloc, &buf, NULL);

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &sr, &t->err)) {
			Test_assert(t, "MemoryStream enc no key", false);
			goto doneEncNoKey;
		}

		U64 streamOff = 0;
		Test_assert(t, "write enc no key", CAFile_write(&ca, &encStreamType, sr, &streamOff, t->alloc, &t->err));

		Test_assert(t, "read with no key fails", !CAFile_read(sr, &encStreamType, 0, NULL, t->alloc, &ca2, NULL));
		Test_assert(t, "caOut not allocated no key", !ca2.folders.ptr);

	doneEncNoKey:
		RefPtr_dec(&sr);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&ca2, t->alloc);
	}
}

//Several files in one directory, all names and contents must be distinct after read.
void Test_CASerializeMultipleFiles(Test *t) {

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	Test_setModule(t, "CAFile serialize: multiple files in one folder");

	{
		CAFile ca = { 0 };
		CAFile ca2 = { 0 };
		StreamRef *sr = NULL;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create ca multi", false);
			goto doneMulti;
		}

		CAHandle hDir = addFolder(t, &ca, CAHandle_Root, "files", false);

		enum { N = 5 };
		const C8 *names[N] = { "a.bin", "b.bin", "c.bin", "d.bin", "e.bin" };
		const U8  vals[N] = { 0x01,    0x02,    0x03,    0x04,    0x05 };

		for (U64 i = 0; i < N; ++i) {
			CAHandle hf = addFile(t, &ca, hDir, names[i], 0, false);
			Buffer buf = Buffer_createNull();
			Buffer_createCopy(Buffer_createRefConst(&vals[i], 1), t->alloc, &buf, NULL);
			CAFile_setData(&ca, hf, t->alloc, &buf, NULL);
		}

		Test_assert(t, "write + read multi", writeAndRead(t, &ca, NULL, &type, NULL, &sr, &ca2));
		Test_assert(t, "fileCount == 5", CAFile_fileCount(&ca2, CAHandle_Root, true) == N);

		CAHandle hDir2 = CAFile_resolveCStr(&ca2, "files");
		Test_assert(t, "'files' folder should be present", hDir2 != CAHandle_Invalid);

		for (U64 i = 0; i < N; ++i) {

			CAHandle hf2 = CAFile_resolveSubObject(&ca2, hDir2, CharString_createRefCStrConst(names[i]));
			Test_assert(t, "resolve file", hf2 != CAHandle_Invalid);

			Bool v = false;
			Buffer g = CAFile_getDataConst(&ca2, hf2, &v);
			Test_assert(t, "content valid", v);
			Test_assert(t, "content distinct", Buffer_length(g) == 1 && g.ptr[0] == vals[i]);
		}

	doneMulti:
		RefPtr_dec(&sr);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&ca2, t->alloc);
	}
}

//A stream-backed file (set via setDataStream) must survive write/read.
//After reading back the content must match the original stream bytes.
void Test_CASerializeStreamBacked(Test *t) {

	Test_setModule(t, "CAFile serialize: stream-backed file");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		CAFile ca = { 0 };
		CAFile ca2 = { 0 };
		StreamRef *dataStream = NULL;
		StreamRef *sr = NULL;

		if (!CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)) {
			Test_assert(t, "create ca streambacked", false);
			goto doneStreamBacked;
		}

		//Create a MemoryStream with known content

		const U8 streamPayload[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 };
		const U64 payloadLen = sizeof streamPayload;

		if (!MemoryStream_create(payloadLen, EMemoryStreamFlags_IsWritable, &type, &dataStream, &t->err)) {
			Test_assert(t, "create dataStream", false);
			goto doneStreamBacked;
		}

		OxStream *dataStreamUnderlying = RefPtr_data(dataStream, OxStream);

		//Write payload into the stream

		Buffer payloadBuf = Buffer_createRefConst(streamPayload, payloadLen);
		Test_assert(
			t, "write payload to stream",
			dataStreamUnderlying->write(dataStreamUnderlying, 0, 0, payloadBuf, t->alloc, &t->err)
		);

		//Attach to the CAFile entry via setDataStream

		CAHandle hf = addFile(t, &ca, CAHandle_Root, "streamed.bin", 0, false);
		Test_assert(t, "setDataStream", CAFile_setDataStream(&ca, hf, t->alloc, &dataStream, 0, payloadLen, &t->err));
		Test_assert(t, "dataStream moved", dataStream == NULL);

		//Write the archive, read it back

		Test_assert(t, "write + read streambacked", writeAndRead(t, &ca, NULL, &type, NULL, &sr, &ca2));

		CAHandle hf2 = CAFile_resolveCStr(&ca2, "streamed.bin");
		Test_assert(t, "resolve streamed.bin", hf2 != CAHandle_Invalid);
		Test_assert(t, "fileSize matches", CAFile_fileSize(&ca2, hf2) == payloadLen);

		//Grab loaded content and compare data (this works because the data is small enough to land in cache on reload)

		Bool valid = false;
		Buffer got = CAFile_getDataConst(&ca2, hf2, &valid);
		Test_assert(t, "streambacked content valid", valid);
		Test_assert(t, "streambacked content matches", Buffer_eq(got, Buffer_createRefConst(streamPayload, payloadLen)));

	doneStreamBacked:
		RefPtr_dec(&dataStream);
		RefPtr_dec(&sr);
		CAFile_free(&ca, t->alloc);
		CAFile_free(&ca2, t->alloc);
	}
}
