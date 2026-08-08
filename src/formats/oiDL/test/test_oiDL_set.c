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

//formats/oiDL/test/test_oiDL_set.c

#include "test_oiDL_shared.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "types/container/memory_stream.h"

Bool buildDataFile(Test *t, DLFile *f, U64 count);
Bool buildStringFile(Test *t, DLFile *f, U64 count);

extern const DLSettings kSettingsData;
extern const DLSettings kSettingsStr;

void Test_DLSetEntry(Test *t) {

	Test_setModule(t, "DLFile_setEntry");

	{                            //Replace an existing entry with a larger buffer
		DLFile f   = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildDataFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry Data file for setEntry", false);
			goto doneSetBasic;
		}

		U8 val[5] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };

		if (!Buffer_createCopy(Buffer_createRefConst(val, 5), t->alloc, &buf, &t->err)) {
			Test_assert(t, "Buffer_createCopy for setEntry", false);
			goto doneSetBasic;
		}

		Test_assert(t, "setEntry id = 1 ok",   DLFile_setEntry(&f, 1, &buf, t->alloc, &t->err));
		Test_assert(t, "buf zeroed out",       !buf.ptr);
		Test_assert(t, "entryCount unchanged", DLFile_entryCount(&f) == 3);
		Test_assert(t, "[1] size == 5",        DLFile_entrySize(&f, 1) == 5);

		//Neighbours must be untouched

		Test_assert(t, "[0] size unchanged",   DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[2] size unchanged",   DLFile_entrySize(&f, 2) == 2);

	doneSetBasic:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                            //Replace with an empty (zero-length) buffer, effectively clears the entry
		DLFile f   = { 0 };
		Buffer empty = Buffer_createNull();

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup 2-entry for setEntry empty", false);
			goto doneSetEmpty;
		}

		Test_assert(t, "setEntry empty ok",  DLFile_setEntry(&f, 1, &empty, t->alloc, &t->err));
		Test_assert(t, "empty zeroed out",   !empty.ptr);
		Test_assert(t, "[1] size == 0",      DLFile_entrySize(&f, 1) == 0);

	doneSetEmpty:
		Buffer_free(&empty, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                            //Set at index 0 (first entry)
		DLFile f   = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildDataFile(t, &f, 3)) {
			Test_assert(t, "Setup for setEntry id=0", false);
			goto doneSetFront;
		}

		U8 val[2] = { 0x01, 0x02 };

		if (!Buffer_createCopy(Buffer_createRefConst(val, 2), t->alloc, &buf, &t->err)) {
			Test_assert(t, "Buffer_createCopy id=0", false);
			goto doneSetFront;
		}

		Test_assert(t, "setEntry id = 0 ok", DLFile_setEntry(&f, 0, &buf, t->alloc, &t->err));
		Test_assert(t, "[0] size == 2",      DLFile_entrySize(&f, 0) == 2);
		Test_assert(t, "[1] size unchanged", DLFile_entrySize(&f, 1) == 1);

	doneSetFront:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                            //Set at last valid index
		DLFile f   = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildDataFile(t, &f, 3)) {
			Test_assert(t, "Setup for setEntry last", false);
			goto doneSetLast;
		}

		U8 val = 0xFF;

		if (!Buffer_createCopy(Buffer_createRefConst(&val, 1), t->alloc, &buf, &t->err)) {
			Test_assert(t, "Buffer_createCopy last", false);
			goto doneSetLast;
		}

		Test_assert(t, "setEntry id = 2 ok", DLFile_setEntry(&f, 2, &buf, t->alloc, &t->err));
		Test_assert(t, "[2] size == 1",      DLFile_entrySize(&f, 2) == 1);

	doneSetLast:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                            //Set replaces a previously-set entry (double set at same id)
		DLFile f    = { 0 };
		Buffer buf1 = Buffer_createNull();
		Buffer buf2 = Buffer_createNull();

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup for double setEntry", false);
			goto doneDoubleSet;
		}

		U8 v1[3] = { 0x11, 0x22, 0x33 };
		U8 v2[1] = { 0x44 };

		if (
			!Buffer_createCopy(Buffer_createRefConst(v1, 3), t->alloc, &buf1, &t->err) ||
			!Buffer_createCopy(Buffer_createRefConst(v2, 1), t->alloc, &buf2, &t->err)
		) {
			Test_assert(t, "Buffer_createCopy double set", false);
			goto doneDoubleSet;
		}

		Test_assert(t, "first setEntry ok",  DLFile_setEntry(&f, 0, &buf1, t->alloc, &t->err));
		Test_assert(t, "[0] size == 3",      DLFile_entrySize(&f, 0) == 3);
		Test_assert(t, "second setEntry ok", DLFile_setEntry(&f, 0, &buf2, t->alloc, &t->err));
		Test_assert(t, "[0] size == 1",      DLFile_entrySize(&f, 0) == 1);

	doneDoubleSet:
		Buffer_free(&buf1, t->alloc);
		Buffer_free(&buf2, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                            //OOB id must fail
		DLFile f   = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup for OOB setEntry", false);
			goto doneSetOOB;
		}

		U8 val = 0x01;
		Buffer_createCopy(Buffer_createRefConst(&val, 1), t->alloc, &buf, NULL);

		Test_assert(t, "setEntry OOB fails",     !DLFile_setEntry(&f, 5, &buf, t->alloc, NULL));
		Test_assert(t, "entryCount unchanged",   DLFile_entryCount(&f) == 2);

	doneSetOOB:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                            //Type mismatch: String file
		DLFile f   = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildStringFile(t, &f, 1)) {
			Test_assert(t, "Setup String file for setEntry type mismatch", false);
			goto doneSetMismatch;
		}

		U8 val = 0x01;
		Buffer_createCopy(Buffer_createRefConst(&val, 1), t->alloc, &buf, NULL);

		Test_assert(t, "setEntry on String file fails", !DLFile_setEntry(&f, 0, &buf, t->alloc, NULL));

	doneSetMismatch:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                            //Null guards
		DLFile f   = { 0 };
		Buffer buf = Buffer_createNull();

		if (!buildDataFile(t, &f, 1)) {
			Test_assert(t, "Setup for null guard setEntry", false);
			goto doneSetNull;
		}

		U8 val = 0x01;
		Buffer_createCopy(Buffer_createRefConst(&val, 1), t->alloc, &buf, NULL);

		Test_assert(t, "setEntry null dlFile fails", !DLFile_setEntry(NULL, 0, &buf,  t->alloc, NULL));
		Test_assert(t, "setEntry null entry fails",  !DLFile_setEntry(&f,   0, NULL,  t->alloc, NULL));

	doneSetNull:
		Buffer_free(&buf, t->alloc);
		DLFile_free(&f, t->alloc);
	}
}

void Test_DLSetEntryString(Test *t) {

	Test_setModule(t, "DLFile_setEntryString");

	{                        //Replace an existing string entry with a longer string
		DLFile f = { 0 };
		CharString s = CharString_createNull();

		if (!buildStringFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry String file for setEntryString", false);
			goto doneSetStrBasic;
		}

		if (!CharString_createCopy(CharString_createRefCStrConst("Hello!"), t->alloc, &s, &t->err)) {
			Test_assert(t, "CharString_createCopy Hello!", false);
			goto doneSetStrBasic;
		}

		Test_assert(t, "setEntryString id = 1 ok",  DLFile_setEntryString(&f, 1, &s, t->alloc, &t->err));
		Test_assert(t, "str zeroed out",            !s.ptr);
		Test_assert(t, "entryCount unchanged",      DLFile_entryCount(&f) == 3);
		Test_assert(t, "[1] size == 6",             DLFile_entrySize(&f, 1) == 6);

		//Neighbours must be untouched

		Test_assert(t, "[0] size unchanged",        DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[2] size unchanged",        DLFile_entrySize(&f, 2) == 2);

	doneSetStrBasic:
		CharString_free(&s, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                        //Replace with an empty string, clears the entry
		DLFile f = { 0 };
		CharString empty = CharString_createNull();

		if (!buildStringFile(t, &f, 2)) {
			Test_assert(t, "Setup for setEntryString empty", false);
			goto doneSetStrEmpty;
		}

		Test_assert(t, "setEntryString empty ok", DLFile_setEntryString(&f, 1, &empty, t->alloc, &t->err));
		Test_assert(t, "[1] size == 0",           DLFile_entrySize(&f, 1) == 0);

	doneSetStrEmpty:
		CharString_free(&empty, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                        //Unicode (UTF-8) replacement
		DLFile f = { 0 };
		CharString uni = CharString_createNull();

		if (!buildStringFile(t, &f, 2)) {
			Test_assert(t, "Setup for setEntryString unicode", false);
			goto doneSetStrUni;
		}

		//Chinese: Guang ji (6 bytes, 2 code points)

		CharString_createCopy(
			CharString_createRefSizedConst("\xE5\x85\x89\xE8\xBF\xB9", 6, true),
			t->alloc, &uni, NULL
		);

		Test_assert(t, "setEntryString unicode ok", DLFile_setEntryString(&f, 0, &uni, t->alloc, &t->err));
		Test_assert(t, "[0] size == 6",             DLFile_entrySize(&f, 0) == 6);

	doneSetStrUni:
		CharString_free(&uni, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                            //Double set at same index
		DLFile f  = { 0 };
		CharString s1 = CharString_createNull();
		CharString s2 = CharString_createNull();

		if (!buildStringFile(t, &f, 2)) {
			Test_assert(t, "Setup for double setEntryString", false);
			goto doneSetStrDouble;
		}

		CharString_createCopy(CharString_createRefCStrConst("first"),  t->alloc, &s1, NULL);
		CharString_createCopy(CharString_createRefCStrConst("second"), t->alloc, &s2, NULL);

		Test_assert(t, "first setEntryString ok",  DLFile_setEntryString(&f, 0, &s1, t->alloc, &t->err));
		Test_assert(t, "[0] size == 5",            DLFile_entrySize(&f, 0) == 5);
		Test_assert(t, "second setEntryString ok", DLFile_setEntryString(&f, 0, &s2, t->alloc, &t->err));
		Test_assert(t, "[0] size == 6",            DLFile_entrySize(&f, 0) == 6);

	doneSetStrDouble:
		CharString_free(&s1, t->alloc);
		CharString_free(&s2, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                        //OOB id must fail
		DLFile f = { 0 };
		CharString s = CharString_createNull();

		if (!buildStringFile(t, &f, 2)) {
			Test_assert(t, "Setup for OOB setEntryString", false);
			goto doneSetStrOOB;
		}

		CharString_createCopy(CharString_createRefCStrConst("X"), t->alloc, &s, NULL);

		Test_assert(t, "setEntryString OOB fails",   !DLFile_setEntryString(&f, 5, &s, t->alloc, NULL));
		Test_assert(t, "entryCount unchanged",       DLFile_entryCount(&f) == 2);

	doneSetStrOOB:
		CharString_free(&s, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                        //Type mismatch: Data file
		DLFile f = { 0 };
		CharString s = CharString_createNull();

		if (!buildDataFile(t, &f, 1)) {
			Test_assert(t, "Setup Data file for setEntryString type mismatch", false);
			goto doneSetStrMismatch;
		}

		CharString_createCopy(CharString_createRefCStrConst("x"), t->alloc, &s, NULL);
		Test_assert(t, "setEntryString on Data file fails", !DLFile_setEntryString(&f, 0, &s, t->alloc, NULL));

	doneSetStrMismatch:
		CharString_free(&s, t->alloc);
		DLFile_free(&f, t->alloc);
	}

	{                        //Null guards
		DLFile f = { 0 };
		CharString s = CharString_createNull();

		if (!buildStringFile(t, &f, 1)) {
			Test_assert(t, "Setup for null guard setEntryString", false);
			goto doneSetStrNull;
		}

		CharString_createCopy(CharString_createRefCStrConst("G"), t->alloc, &s, NULL);

		Test_assert(t, "setEntryString null dlFile fails", !DLFile_setEntryString(NULL, 0, &s,   t->alloc, NULL));
		Test_assert(t, "setEntryString null entry fails",  !DLFile_setEntryString(&f,   0, NULL, t->alloc, NULL));

	doneSetStrNull:
		CharString_free(&s, t->alloc);
		DLFile_free(&f, t->alloc);
	}
}

void Test_DLSetStream(Test *t) {

	Test_setModule(t, "DLFile_setStream");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{                            //Set a stream entry on a Data file
		DLFile f      = { 0 };
		StreamRef *sr = NULL;

		if (!buildDataFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry Data file for setStream", false);
			goto doneSr;
		}

		if (!MemoryStream_create(16, EMemoryStreamFlags_None, &type, &sr, &t->err)) {
			Test_assert(t, "MemoryStream_create", false);
			goto doneSr;
		}

		Test_assert(t, "setStream id = 1 ok",  DLFile_setStream(&f, 1, &sr, 0, 8, t->alloc, &t->err));
		Test_assert(t, "sr zeroed out",        !sr);
		Test_assert(t, "entryCount unchanged", DLFile_entryCount(&f) == 3);
		Test_assert(t, "[1] stream len == 8",  DLFile_entrySize(&f, 1) == 8);

		//Neighbours must be unaffected

		Test_assert(t, "[0] unchanged",        DLFile_entrySize(&f, 0) == 0);
		Test_assert(t, "[2] unchanged",        DLFile_entrySize(&f, 2) == 2);

	doneSr:
		RefPtr_dec(&sr);
		DLFile_free(&f, t->alloc);
	}

	{                            //Set a stream entry on a String file
		DLFile f      = { 0 };
		StreamRef *srStr = NULL;

		if (!buildStringFile(t, &f, 3)) {
			Test_assert(t, "Setup 3-entry String file for setStream", false);
			goto doneSrStr;
		}

		if (!MemoryStream_create(32, EMemoryStreamFlags_None, &type, &srStr, &t->err)) {
			Test_assert(t, "MemoryStream_create", false);
			goto doneSrStr;
		}

		Test_assert(t, "setStream String id = 2 ok", DLFile_setStream(&f, 2, &srStr, 4, 12, t->alloc, &t->err));
		Test_assert(t, "sr zeroed out",              !srStr);
		Test_assert(t, "[2] stream len == 12",       DLFile_entrySize(&f, 2) == 12);
		Test_assert(t, "[1] unchanged",              DLFile_entrySize(&f, 1) == 1);

	doneSrStr:
		RefPtr_dec(&srStr);
		DLFile_free(&f, t->alloc);
	}

	{                            //Set stream replaces a previously-loaded buffer
		DLFile f      = { 0 };
		StreamRef *sr = NULL;

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup for setStream over buffer", false);
			goto doneSrOverBuf;
		}

		//Entry [1] currently has a 1-byte heap buffer; replace it with a stream.

		if (!MemoryStream_create(64, EMemoryStreamFlags_None, &type, &sr, &t->err)) {
			Test_assert(t, "MemoryStream_create", false);
			goto doneSrOverBuf;
		}

		Test_assert(t, "setStream over buffer ok", DLFile_setStream(&f, 1, &sr, 0, 64, t->alloc, &t->err));
		Test_assert(t, "[1] stream len == 64",     DLFile_entrySize(&f, 1) == 64);
		Test_assert(t, "entry is a stream",        f.entryStreams.ptr[1].stream);

	doneSrOverBuf:
		RefPtr_dec(&sr);
		DLFile_free(&f, t->alloc);
	}

	{                                //Set stream then replace again with a second stream
		DLFile f       = { 0 };
		StreamRef *sr1 = NULL;
		StreamRef *sr2 = NULL;

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup for double setStream", false);
			goto doneSrDouble;
		}

		if (!MemoryStream_create(8, EMemoryStreamFlags_None, &type, &sr1, &t->err)) {
			Test_assert(t, "MemoryStream_create", false);
			goto doneSrDouble;
		}

		if (!MemoryStream_create(24, EMemoryStreamFlags_None, &type, &sr2, &t->err)) {
			Test_assert(t, "MemoryStream_create", false);
			goto doneSrDouble;
		}

		Test_assert(t, "first setStream ok",  DLFile_setStream(&f, 0, &sr1, 0, 8,  t->alloc, &t->err));
		Test_assert(t, "[0] len == 8",        DLFile_entrySize(&f, 0) == 8);
		Test_assert(t, "second setStream ok", DLFile_setStream(&f, 0, &sr2, 0, 20, t->alloc, &t->err));
		Test_assert(t, "[0] len == 20",       DLFile_entrySize(&f, 0) == 20);

	doneSrDouble:
		RefPtr_dec(&sr1);
		RefPtr_dec(&sr2);
		DLFile_free(&f, t->alloc);
	}

	{                            //NULL stream pointer clears the entry back to no-stream / zero-length
		DLFile f      = { 0 };
		StreamRef *sr = NULL;    //Passing sr == NULL means "clear"

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup for setStream null stream", false);
			goto doneSrNull;
		}

		Test_assert(t, "setStream null stream ok", DLFile_setStream(&f, 1, &sr, 0, 0, t->alloc, &t->err));
		Test_assert(t, "entry stream is NULL",     !f.entryStreams.ptr[1].stream);
		Test_assert(t, "[1] size == 0",            DLFile_entrySize(&f, 1) == 0);

	doneSrNull:
		RefPtr_dec(&sr);
		DLFile_free(&f, t->alloc);
	}

	{                            //OOB id must fail
		DLFile f      = { 0 };
		StreamRef *sr = NULL;

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup for OOB setStream", false);
			goto doneSrOOB;
		}

		if (!MemoryStream_create(4, EMemoryStreamFlags_None, &type, &sr, &t->err)) {
			Test_assert(t, "MemoryStream_create", false);
			goto doneSrOOB;
		}

		Test_assert(t, "setStream OOB fails",    !DLFile_setStream(&f, 5, &sr, 0, 4, t->alloc, NULL));
		Test_assert(t, "entryCount unchanged",   DLFile_entryCount(&f) == 2);
		Test_assert(t, "sr not consumed",        sr);

	doneSrOOB:
		RefPtr_dec(&sr);
		DLFile_free(&f, t->alloc);
	}

	{                                //dataOff + len out of bounds within the stream must fail
		DLFile f      = { 0 };
		StreamRef *sr = NULL;

		if (!buildDataFile(t, &f, 2)) {
			Test_assert(t, "Setup for setStream off+len OOB", false);
			goto doneSrLenOOB;
		}

		if (!MemoryStream_create(8, EMemoryStreamFlags_None, &type, &sr, &t->err)) {
			Test_assert(t, "MemoryStream_create", false);
			goto doneSrLenOOB;
		}

		Test_assert(t, "setStream off + len OOB fails", !DLFile_setStream(&f, 0, &sr, 4, 8, t->alloc, NULL));
		Test_assert(t, "sr not consumed",               sr);

	doneSrLenOOB:
		RefPtr_dec(&sr);
		DLFile_free(&f, t->alloc);
	}

	{                                //Null guards
		DLFile f      = { 0 };
		StreamRef *sr = NULL;

		if (!buildDataFile(t, &f, 1)) {
			Test_assert(t, "Setup for null guard setStream", false);
			goto doneSrNullGuard;
		}

		if (!MemoryStream_create(4, EMemoryStreamFlags_None, &type, &sr, &t->err)) {
			Test_assert(t, "MemoryStream_create", false);
			goto doneSrNullGuard;
		}

		Test_assert(t, "setStream null dlFile fails",  !DLFile_setStream(NULL, 0, &sr,  0, 4, t->alloc, NULL));
		Test_assert(t, "setStream null stream** fails", !DLFile_setStream(&f,  0, NULL, 0, 4, t->alloc, NULL));

	doneSrNullGuard:
		RefPtr_dec(&sr);
		DLFile_free(&f, t->alloc);
	}

	#undef MAKE_STREAM
}

void Test_DLSet(Test *t) {
	Test_DLSetEntry(t);
	Test_DLSetEntryString(t);
	Test_DLSetStream(t);
}
