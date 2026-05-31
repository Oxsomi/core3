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

//formats/oiCA/test/test_oiCA_combine.c

#include "test_oiCA_shared.h"
#include "types/container/memory_stream.h"
#include "formats/oiCA/ca_combine.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_edit.h"

static inline CAHandle CAFile_resolveCStr(CAFile *ca, const C8 *name) {
	return CAFile_resolve(ca, CharString_createRefCStrConst(name));
}

static Bool addFile(
	CAFile *caFile,
	const C8 *name,
	const void *data,
	U64 dataLen,
	CAHandle parent,        //Set to CAHandle_Root by default
	CAHandle *handle,
	Test *t
) {
	CharString nameCopy = CharString_createNull();
	CharString nameRef = CharString_createRefCStrConst(name);

	if (!CharString_createCopy(nameRef, t->alloc, &nameCopy, &t->err)) {
		Test_assert(t, "addFile: copy name", false);
		return false;
	}

	*handle = CAFile_addFile(caFile, parent, &nameCopy, 0, t->alloc, &t->err);

	if (*handle == CAHandle_Invalid) {
		CharString_free(&nameCopy, t->alloc);
		Test_assert(t, "addFile: add file", false);
		return false;
	}

	Buffer buf = Buffer_createNull();

	if (
		!Buffer_createCopy(Buffer_createRefConst(data, dataLen), t->alloc, &buf, &t->err) ||
		!CAFile_setData(caFile, *handle, t->alloc, &buf, &t->err)
	) {
		Test_assert(t, "addFile: set data", false);
		Buffer_free(&buf, t->alloc);
		CAFile_removeFile(caFile, *handle, t->alloc, &t->err);
		return false;
	}

	return true;
}

void Test_CACombine(Test *t) {

	Test_setModule(t, "CAFile_combine");
	
	CAFile c = (CAFile) { 0 };
	CAFile ca = (CAFile) { 0 };
	CAFile cb = (CAFile) { 0 };

	CharString tmp0 = CharString_createNull();
	CharString tmp1 = CharString_createNull();

	Buffer buf0 = Buffer_createNull();
	Buffer buf1 = Buffer_createNull();

	CASettings settings = (CASettings) { 0 };

	if (!CAFile_create(&settings, 4, 2, t->alloc, &ca, &t->err)) {
		Test_assert(t, "Create ca", false);
		goto clean;
	}

	if (!CAFile_create(&settings, 4, 2, t->alloc, &cb, &t->err)) {
		Test_assert(t, "Create cb", false);
		goto clean;
	}

	U8 srcA[64], srcB[64];

	for (U8 i = 0; i < 64; ++i) {
		srcA[i] = i;
		srcB[i] = i + 128;
	}

	//Test 1: no conflict, b-only file is inserted

	CAHandle hA = CAHandle_Invalid;
	CAHandle hB = CAHandle_Invalid;

	if (!addFile(&ca, "a.bin", srcA, 64, CAHandle_Root, &hA, t))
		goto clean;

	if (!addFile(&cb, "b.bin", srcB, 64, CAHandle_Root, &hB, t))
		goto clean;

	if (!CAFile_combine(&ca, &cb, EArchiveCombineMode_RequireSame, EArchiveCombineFlags_None, t->alloc, &c, &t->err)) {
		Test_assert(t, "No conflict combine", false);
		goto clean;
	}

	Test_assert(t, "No conflict: a.bin present", CAFile_resolveCStr(&c, "a.bin") != CAHandle_Invalid);
	Test_assert(t, "No conflict: b.bin present", CAFile_resolveCStr(&c, "b.bin") != CAHandle_Invalid);
	Test_assert(t, "No conflict: entry count", CAFile_fileCount(&c, CAHandle_Root, false) == 2);

	CAFile_free(&c, t->alloc);

	//Test 2: RequireSame, identical data -> ok

	CAHandle hA2 = CAHandle_Invalid;
	CAHandle hB2 = CAHandle_Invalid;

	if (!addFile(&ca, "same.bin", srcA, 64, CAHandle_Root, &hA2, t))
		goto clean;

	if (!addFile(&cb, "same.bin", srcA, 64, CAHandle_Root, &hB2, t))
		goto clean;

	if (!CAFile_combine(&ca, &cb, EArchiveCombineMode_RequireSame, EArchiveCombineFlags_None, t->alloc, &c, &t->err)) {
		Test_assert(t, "RequireSame identical: ok", false);
		goto clean;
	}

	Test_assert(t, "RequireSame identical: entry count", CAFile_fileCount(&c, CAHandle_Root, false) == 3);

	CAFile_free(&c, t->alloc);

	//Test 3: RequireSame, differing data -> error

	CAHandle hB3 = CAHandle_Invalid;

	if (!addFile(&cb, "a.bin", srcB, 64, CAHandle_Root, &hB3, t))
		goto clean;

	Test_assert(t, "RequireSame conflict: rejects",
		!CAFile_combine(&ca, &cb, EArchiveCombineMode_RequireSame, EArchiveCombineFlags_None, t->alloc, &c, NULL)
	);

	CAFile_free(&c, t->alloc);

	//Test 4: AcceptA on conflict

	if (!CAFile_combine(&ca, &cb, EArchiveCombineMode_AcceptA, EArchiveCombineFlags_None, t->alloc, &c, &t->err)) {
		Test_assert(t, "AcceptA combine", false);
		goto clean;
	}

	{
		CAHandle h = CAFile_resolveCStr(&c, "a.bin");
		Test_assert(t, "AcceptA: a.bin present", h != CAHandle_Invalid);

		if (h != CAHandle_Invalid) {
			Bool valid = false;
			Buffer data = CAFile_getDataConst(&c, h, &valid);
			Test_assert(t, "AcceptA: kept a data", valid && Buffer_length(data) == 64 && data.ptr[0] == srcA[0]);
		}
	}

	CAFile_free(&c, t->alloc);

	//Test 5: AcceptB on conflict

	if (!CAFile_combine(&ca, &cb, EArchiveCombineMode_AcceptB, EArchiveCombineFlags_None, t->alloc, &c, &t->err)) {
		Test_assert(t, "AcceptB combine", false);
		goto clean;
	}

	{
		CAHandle h = CAFile_resolveCStr(&c, "a.bin");
		Test_assert(t, "AcceptB: a.bin present", h != CAHandle_Invalid);

		if (h != CAHandle_Invalid) {
			Bool valid = false;
			Buffer data = CAFile_getDataConst(&c, h, &valid);
			Test_assert(t, "AcceptB: replaced with b data", valid && Buffer_length(data) == 64 && data.ptr[0] == srcB[0]);
		}
	}

	CAFile_free(&c, t->alloc);

	//Test 6: Rename on conflict

	if (!CAFile_combine(&ca, &cb, EArchiveCombineMode_Rename, EArchiveCombineFlags_None, t->alloc, &c, &t->err)) {
		Test_assert(t, "Rename combine", false);
		goto clean;
	}

	{
		CAHandle hOrig   = CAFile_resolveCStr(&c, "a.bin");
		CAHandle hRenamed = CAFile_resolveCStr(&c, "a-1.bin");

		Test_assert(t, "Rename: original a.bin present",  hOrig    != CAHandle_Invalid);
		Test_assert(t, "Rename: renamed a-1.bin present", hRenamed != CAHandle_Invalid);

		if (hOrig != CAHandle_Invalid && hRenamed != CAHandle_Invalid) {
			Bool vOrig = false, vRenamed = false;
			Buffer dOrig    = CAFile_getDataConst(&c, hOrig,    &vOrig);
			Buffer dRenamed = CAFile_getDataConst(&c, hRenamed, &vRenamed);
			Test_assert(t, "Rename: original has a data",  vOrig    && dOrig.ptr[0]    == srcA[0]);
			Test_assert(t, "Rename: renamed has b data",   vRenamed && dRenamed.ptr[0] == srcB[0]);
		}
	}

	CAFile_free(&c, t->alloc);

	//Test 7: Rename, counter increments past existing a-1.bin

	CAHandle hA1 = CAHandle_Invalid;

	if (!addFile(&ca, "a-1.bin", srcA, 64, CAHandle_Root, &hA1, t))
		goto clean;

	if (!CAFile_combine(&ca, &cb, EArchiveCombineMode_Rename, EArchiveCombineFlags_None, t->alloc, &c, &t->err)) {
		Test_assert(t, "Rename counter increment combine", false);
		goto clean;
	}

	Test_assert(t, "Rename counter: a-2.bin present", CAFile_resolveCStr(&c, "a-2.bin") != CAHandle_Invalid);

	CAFile_free(&c, t->alloc);

	CAFile_free(&ca, t->alloc);        //Last use of ca, cb in this state
	CAFile_free(&cb, t->alloc);

	//Test 8: ResolveLatestTimestamp, same data different timestamps -> keep latest

	if (
		!CAFile_create(&settings, 2, 0, t->alloc, &ca, &t->err) ||
		!CAFile_create(&settings, 2, 0, t->alloc, &cb, &t->err)
	) {
		Test_assert(t, "Create ca/cb", false);
		goto clean;
	}

	{
		CharString nameRef   = CharString_createRefCStrConst("ts.bin");

		if (
			!CharString_createCopy(nameRef, t->alloc, &tmp0, &t->err) ||
			!CharString_createCopy(nameRef, t->alloc, &tmp1, &t->err)
		) {
			Test_assert(t, "Copy ts.bin names", false);
			goto clean;
		}

		CAHandle hTs1 = CAFile_addFile(&ca, CAHandle_Root, &tmp0, 1000 * MS, t->alloc, &t->err);
		CAHandle hTs2 = CAFile_addFile(&cb, CAHandle_Root, &tmp1, 2000 * MS, t->alloc, &t->err);

		if (hTs1 == CAHandle_Invalid || hTs2 == CAHandle_Invalid) {
			Test_assert(t, "Add ts.bin files", false);
			goto clean;
		}

		if (
			!Buffer_createCopy(Buffer_createRefConst(srcA, 64), t->alloc, &buf0, &t->err) ||
			!Buffer_createCopy(Buffer_createRefConst(srcA, 64), t->alloc, &buf1, &t->err) ||
			!CAFile_setData(&ca, hTs1, t->alloc, &buf0, &t->err) ||
			!CAFile_setData(&cb, hTs2, t->alloc, &buf1, &t->err)
		) {
			Test_assert(t, "Set ts.bin data", false);
			goto clean;
		}

		if (!CAFile_combine(
			&ca, &cb, EArchiveCombineMode_RequireSame, EArchiveCombineFlags_ResolveLatestTimestamp, t->alloc, &c, &t->err
		)) {
			Test_assert(t, "ResolveLatestTimestamp combine", false);
			goto clean;
		}

		CAHandle hTs = CAFile_resolveCStr(&c, "ts.bin");
		Test_assert(t, "ResolveLatestTimestamp: file present", hTs != CAHandle_Invalid);

		if (hTs != CAHandle_Invalid)
			Test_assert(t, "ResolveLatestTimestamp: timestamp is latest", CAFile_fileTime(&c, hTs) == 2000 * MS);

		CAFile_free(&c, t->alloc);
	}

	//Test 9: ResolveAcceptLatest, differing data -> keep newer

	{
		CharString nameRef   = CharString_createRefCStrConst("latest.bin");

		if (
			!CharString_createCopy(nameRef, t->alloc, &tmp0, &t->err) ||
			!CharString_createCopy(nameRef, t->alloc, &tmp1, &t->err)
		) {
			Test_assert(t, "Copy latest.bin names", false);
			goto clean;
		}

		CAHandle hL1 = CAFile_addFile(&ca, CAHandle_Root, &tmp0, 1000 * MS, t->alloc, &t->err);
		CAHandle hL2 = CAFile_addFile(&cb, CAHandle_Root, &tmp1, 2000 * MS, t->alloc, &t->err);

		if (hL1 == CAHandle_Invalid || hL2 == CAHandle_Invalid) {
			Test_assert(t, "Add latest.bin files", false);
			goto clean;
		}

		if (
			!Buffer_createCopy(Buffer_createRefConst(srcA, 64), t->alloc, &buf0, &t->err) ||
			!Buffer_createCopy(Buffer_createRefConst(srcB, 64), t->alloc, &buf1, &t->err) ||
			!CAFile_setData(&ca, hL1, t->alloc, &buf0, &t->err) ||
			!CAFile_setData(&cb, hL2, t->alloc, &buf1, &t->err)
		) {
			Test_assert(t, "Set latest.bin data", false);
			goto clean;
		}

		if (!CAFile_combine(
			&ca, &cb, EArchiveCombineMode_AcceptA, EArchiveCombineFlags_ResolveAcceptLatest, t->alloc, &c, &t->err
		)) {
			Test_assert(t, "ResolveAcceptLatest combine", false);
			goto clean;
		}

		CAHandle hL = CAFile_resolveCStr(&c, "latest.bin");
		Test_assert(t, "ResolveAcceptLatest: file present", hL != CAHandle_Invalid);

		if (hL != CAHandle_Invalid) {
			Bool valid = false;
			Buffer data = CAFile_getDataConst(&c, hL, &valid);
			Test_assert(t, "ResolveAcceptLatest: has newer b data", valid && data.ptr[0] == srcB[0]);
			Test_assert(t, "ResolveAcceptLatest: timestamp is latest", CAFile_fileTime(&c, hL) == 2000 * MS);
		}

		CAFile_free(&c, t->alloc);
	}

	CAFile_free(&ca, t->alloc);        //Last use of ca, cb in this state
	CAFile_free(&cb, t->alloc);

	//Test 10: folder merging

	{
		if (
			!CAFile_create(&settings, 2, 2, t->alloc, &ca, &t->err) ||
			!CAFile_create(&settings, 2, 2, t->alloc, &cb, &t->err)
		) {
			Test_assert(t, "Create ca/cb", false);
			goto clean;
		}

		CharString dirRef   = CharString_createRefCStrConst("subdir");

		if (
			!CharString_createCopy(dirRef, t->alloc, &tmp0, &t->err) ||
			!CharString_createCopy(dirRef, t->alloc, &tmp1, &t->err)
		) {
			Test_assert(t, "Copy subdir names", false);
			goto clean;
		}

		CAHandle hDirE = CAFile_addFolder(&ca, CAHandle_Root, &tmp0, t->alloc, &t->err);
		CAHandle hDirF = CAFile_addFolder(&cb, CAHandle_Root, &tmp1, t->alloc, &t->err);

		if (hDirE == CAHandle_Invalid || hDirF == CAHandle_Invalid) {
			Test_assert(t, "Add subdir folders", false);
			goto clean;
		}

		//Add different files inside the shared subdir

		CAHandle hFE = CAHandle_Invalid, hFF = CAHandle_Invalid;

		if (
			!addFile(&ca, "e.bin", srcA, 64, hDirE, &hFE, t) ||
			!addFile(&cb, "f.bin", srcB, 64, hDirF, &hFF, t)
		) {
			goto clean;
		}

		if (!CAFile_combine(&ca, &cb, EArchiveCombineMode_RequireSame, EArchiveCombineFlags_None, t->alloc, &c, &t->err)) {
			Test_assert(t, "Folder merge combine", false);
			goto clean;
		}

		Test_assert(t, "Folder merge: subdir present",CAFile_resolveCStr(&c, "subdir") != CAHandle_Invalid);
		Test_assert(t, "Folder merge: subdir/e.bin present", CAFile_resolveCStr(&c, "subdir/e.bin") != CAHandle_Invalid);
		Test_assert(t, "Folder merge: subdir/f.bin present", CAFile_resolveCStr(&c, "subdir/f.bin") != CAHandle_Invalid);

		CAFile_free(&c, t->alloc);
	}

clean:
	CAFile_free(&c, t->alloc);
	CAFile_free(&ca, t->alloc);
	CAFile_free(&cb, t->alloc);
	CharString_free(&tmp0, t->alloc);
	CharString_free(&tmp1, t->alloc);
	Buffer_free(&buf0, t->alloc);
	Buffer_free(&buf1, t->alloc);
}
