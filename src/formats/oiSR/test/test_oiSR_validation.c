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

//formats/oiSR/test/test_oiSR_validation.c

#include "test_oiSR_shared.h"
#include "formats/oiSR/sr_file.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/container/buffer.h"
#include "types/base/string_base.h"
#include "types/base/string_read_helper.h"

//A node with all "none" sentinels; caller overrides what it needs.
static SRNode srNode(ESRNodeType type, U32 nameId, U32 parent) {
	return (SRNode) {
		.nameId = nameId, .semanticId = U32_MAX, .localId = U32_MAX, .parent = parent,
		.fwdBckDeclareNode = U32_MAX, .childCount = 0, .annotationStart = U32_MAX, .annotationCount = 0,
		.type = (U8) type, .interpolation = ESRInterpolation_Undefined, .flags = ESRNodeFlag_None
	};
}

//Build a minimal valid 2-node no-symbol SRFile: Namespace(root, 1 child) > Struct.
//One string "n".
static Bool srBuildMinimal(Test *t, SRFile *sr) {

	if(!SRFile_create(ESRSettingsFlags_None, ESRFeature_All, t->alloc, sr, &t->err))
		return false;

	U32 id = 0;
	CharString n = CharString_createRefCStrConst("n");

	if(!SRFile_addString(sr, &n, t->alloc, &id, &t->err))
		return false;

	SRNode root = srNode(ESRNodeType_Namespace, id, U32_MAX);
	root.childCount = 1;
	SRNode child = srNode(ESRNodeType_Struct, id, 0);

	return
		ListSRNode_pushBack(&sr->nodes, root, t->alloc, &t->err) &&
		ListSRNode_pushBack(&sr->nodes, child, t->alloc, &t->err);
}

//Serialize sr (write is expected to succeed) then read back expecting FAILURE (NULL e_rr swallows the error).
static void srExpectReadFails(Test *t, const SRFile *sr, const RefPtrType *type, const C8 *msg) {

	StreamRef *s = NULL;
	SRFile result = { 0 };
	U64 off = 0;
	Bool wrote = false, readOk = false;

	if(MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &s, &t->err))
		wrote = SRFile_write(sr, t->alloc, s, &off, &t->err);

	if(wrote) {
		U64 ro = 0;
		readOk = SRFile_read(s, &ro, false, t->alloc, &result, NULL);
	}

	Test_assert(t, msg, wrote && !readOk);

	SRFile_free(&result, t->alloc);
	RefPtr_dec(&s);
}

//Serialize a minimal file, OR `orVal` into serialized byte `byteOff`, then read expecting FAILURE.
static void srExpectReadFailsTamper(Test *t, const RefPtrType *type, U64 byteOff, U8 orVal, const C8 *msg) {

	SRFile sr = { 0 };
	StreamRef *s = NULL;
	SRFile result = { 0 };
	U64 off = 0;
	Bool wrote = false, readOk = false, tampered = false;

	if(srBuildMinimal(t, &sr) && MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &s, &t->err))
		wrote = SRFile_write(&sr, t->alloc, s, &off, &t->err);

	if(wrote) {

		MemoryStream *ms = RefPtr_data(s, MemoryStream);

		if(byteOff < Buffer_length(ms->data)) {
			ms->data.ptrNonConst[byteOff] |= orVal;
			tampered = true;
		}

		U64 ro = 0;
		readOk = SRFile_read(s, &ro, false, t->alloc, &result, NULL);
	}

	Test_assert(t, msg, wrote && tampered && !readOk);

	SRFile_free(&result, t->alloc);
	SRFile_free(&sr, t->alloc);
	RefPtr_dec(&s);
}

//Non-subfile layout: [magic U32][SRHeader: version@4, flags@5, pad@6-7, features@8-11, nodeCount@12-15, annoCount@16-19]

void Test_SRReadHeaderTamper(Test *t) {

	Test_setModule(t, "SRFile read: header rejection paths");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	srExpectReadFailsTamper(t, &type, 0,  0xFF, "corrupt magic rejected");        //byte 0 of magic
	srExpectReadFailsTamper(t, &type, 4,  0xFF, "wrong version rejected");        //version -> 0xFF
	srExpectReadFailsTamper(t, &type, 5,  0x02, "unsupported flag bit rejected"); //flags: set an ESRFlag_Unsupported bit
	srExpectReadFailsTamper(t, &type, 10, 0x02, "unknown feature bit rejected");  //byte 10 = bit 17, outside All|SymbolInfo
	srExpectReadFailsTamper(t, &type, 5,  0x01, "HasSymbols without SymbolInfo rejected"); //flag/feature disagreement
}

void Test_SRReadNodeFieldBounds(Test *t) {

	Test_setModule(t, "SRFile read: per-node field bounds");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	//Each case: build a valid file, corrupt one field of node[1] (or node[0]), expect read failure.

	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) { sr.nodes.ptrNonConst[1].type = ESRNodeType_Count;
		srExpectReadFails(t, &sr, &type, "invalid node type rejected"); } SRFile_free(&sr, t->alloc); }

	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) { sr.nodes.ptrNonConst[1].interpolation = ESRInterpolation_Count;
		srExpectReadFails(t, &sr, &type, "invalid interpolation rejected"); } SRFile_free(&sr, t->alloc); }

	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) { sr.nodes.ptrNonConst[1].flags = 0x04;    //an ESRNodeFlag_Invalid bit
		srExpectReadFails(t, &sr, &type, "invalid node flag rejected"); } SRFile_free(&sr, t->alloc); }

	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) { sr.nodes.ptrNonConst[1].nameId = 1;       //nameCount == 1
		srExpectReadFails(t, &sr, &type, "nameId out of bounds rejected"); } SRFile_free(&sr, t->alloc); }

	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) { sr.nodes.ptrNonConst[1].semanticId = 1;
		srExpectReadFails(t, &sr, &type, "semanticId out of bounds rejected"); } SRFile_free(&sr, t->alloc); }

	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) { sr.nodes.ptrNonConst[1].parent = 1;       //parent == i (self)
		srExpectReadFails(t, &sr, &type, "self-parent (parent == i) rejected"); } SRFile_free(&sr, t->alloc); }

	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) { sr.nodes.ptrNonConst[1].parent = 9;       //>= nodes.length
		srExpectReadFails(t, &sr, &type, "parent out of bounds rejected"); } SRFile_free(&sr, t->alloc); }

	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) { sr.nodes.ptrNonConst[0].childCount = 9;   //0 + 9 > 2
		srExpectReadFails(t, &sr, &type, "childCount out of bounds rejected"); } SRFile_free(&sr, t->alloc); }

	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) { sr.nodes.ptrNonConst[1].fwdBckDeclareNode = 9;
		srExpectReadFails(t, &sr, &type, "fwdBckDeclareNode out of bounds rejected"); } SRFile_free(&sr, t->alloc); }

	//fwd-declare flag on a non-declarable node type (Struct IS declarable, so switch to Variable first)
	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) {
		sr.nodes.ptrNonConst[1].type = ESRNodeType_Variable;
		sr.nodes.ptrNonConst[1].flags = ESRNodeFlag_IsFwdDeclare;
		srExpectReadFails(t, &sr, &type, "fwd-declare flag on non-declarable type rejected"); } SRFile_free(&sr, t->alloc); }

	//annotationCount > 0 but no annotation table
	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) {
		sr.nodes.ptrNonConst[1].annotationCount = 1; sr.nodes.ptrNonConst[1].annotationStart = 0;
		srExpectReadFails(t, &sr, &type, "annotation range out of bounds rejected"); } SRFile_free(&sr, t->alloc); }

	//annotationCount > 0 with U32_MAX start
	{ SRFile sr = { 0 }; if(srBuildMinimal(t, &sr)) {
		sr.nodes.ptrNonConst[1].annotationCount = 1; sr.nodes.ptrNonConst[1].annotationStart = U32_MAX;
		srExpectReadFails(t, &sr, &type, "annotationStart U32_MAX with count>0 rejected"); } SRFile_free(&sr, t->alloc); }
}

void Test_SRReadSymbolAndAnnotationBounds(Test *t) {

	Test_setModule(t, "SRFile read: symbol + annotation bounds");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	//Symbol fileNameId out of bounds (needs a HasSymbols file: 2 nodes, 2 parallel symbols)
	{
		SRFile sr = { 0 };
		if(SRFile_create(ESRSettingsFlags_HasSymbols, ESRFeature_All | ESRFeature_SymbolInfo, t->alloc, &sr, &t->err)) {

			U32 id = 0; CharString n = CharString_createRefCStrConst("n");
			SRFile_addString(&sr, &n, t->alloc, &id, &t->err);

			SRNode root = srNode(ESRNodeType_Namespace, id, U32_MAX); root.childCount = 1;
			SRNode child = srNode(ESRNodeType_Struct, id, 0);
			ListSRNode_pushBack(&sr.nodes, root, t->alloc, &t->err);
			ListSRNode_pushBack(&sr.nodes, child, t->alloc, &t->err);

			SRSymbol ok = (SRSymbol) { .fileNameId = id, .line = 1, .lineCount = 1, .columnStart = 1, .columnEnd = 2 };
			SRSymbol bad = ok; bad.fileNameId = 9;    //nameCount == 1
			ListSRSymbol_pushBack(&sr.symbols, ok, t->alloc, &t->err);
			ListSRSymbol_pushBack(&sr.symbols, bad, t->alloc, &t->err);

			srExpectReadFails(t, &sr, &type, "symbol fileNameId out of bounds rejected");
		}
		SRFile_free(&sr, t->alloc);
	}

	//Annotation nameId out of bounds
	{
		SRFile sr = { 0 };
		if(srBuildMinimal(t, &sr)) {

			SRAnnotation bad = (SRAnnotation) { .nameId = 9, .isBuiltin = 1 };    //nameCount == 1
			ListSRAnnotation_pushBack(&sr.annotations, bad, t->alloc, &t->err);
			sr.nodes.ptrNonConst[1].annotationStart = 0;
			sr.nodes.ptrNonConst[1].annotationCount = 1;

			srExpectReadFails(t, &sr, &type, "annotation nameId out of bounds rejected");
		}
		SRFile_free(&sr, t->alloc);
	}
}

//Build a valid forward-declared function pair: [0] fwd decl "f" (-> 1), [1] definition "f" (-> 0).
static Bool srBuildFwdPair(Test *t, SRFile *sr) {

	if(!SRFile_create(ESRSettingsFlags_None, ESRFeature_All, t->alloc, sr, &t->err))
		return false;

	U32 id = 0; CharString f = CharString_createRefCStrConst("f");
	if(!SRFile_addString(sr, &f, t->alloc, &id, &t->err))
		return false;

	SRNode fwd = srNode(ESRNodeType_Function, id, U32_MAX);
	fwd.flags = ESRNodeFlag_IsFwdDeclare;
	fwd.fwdBckDeclareNode = 1;

	SRNode def = srNode(ESRNodeType_Function, id, U32_MAX);
	def.fwdBckDeclareNode = 0;

	return
		ListSRNode_pushBack(&sr->nodes, fwd, t->alloc, &t->err) &&
		ListSRNode_pushBack(&sr->nodes, def, t->alloc, &t->err);
}

void Test_SRReadFwdDeclare(Test *t) {

	Test_setModule(t, "SRFile read: forward/backward declaration links");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	//Positive: a valid fwd/def pair round-trips
	{
		SRFile sr = { 0 }, result = { 0 };
		StreamRef *s = NULL;

		if(srBuildFwdPair(t, &sr) && SRFile_finalize(&sr, t->alloc, &t->err) &&
			MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &s, &t->err)) {

			U64 off = 0;
			Bool ok = SRFile_write(&sr, t->alloc, s, &off, &t->err);
			U64 ro = 0;
			ok = ok && SRFile_read(s, &ro, false, t->alloc, &result, &t->err);

			Test_assert(t, "valid fwd/def pair round-trips", ok);
			Test_assert(t, "fwd flag survives", ok && (result.nodes.ptr[0].flags & ESRNodeFlag_IsFwdDeclare));
			Test_assert(t, "fwd link survives", ok && result.nodes.ptr[0].fwdBckDeclareNode == 1);
		}
		else Test_assert(t, "build fwd pair", false);

		SRFile_free(&result, t->alloc);
		SRFile_free(&sr, t->alloc);
		RefPtr_dec(&s);
	}

	//self-loop
	{ SRFile sr = { 0 }; if(srBuildFwdPair(t, &sr)) { sr.nodes.ptrNonConst[0].fwdBckDeclareNode = 0;
		srExpectReadFails(t, &sr, &type, "fwd/bck self-loop rejected"); } SRFile_free(&sr, t->alloc); }

	//type mismatch between the linked pair
	{ SRFile sr = { 0 }; if(srBuildFwdPair(t, &sr)) { sr.nodes.ptrNonConst[1].type = ESRNodeType_Struct;
		srExpectReadFails(t, &sr, &type, "fwd/bck type mismatch rejected"); } SRFile_free(&sr, t->alloc); }

	//wrong direction: clear the fwd flag on node 0 so it becomes a "definition" whose link points forward
	{ SRFile sr = { 0 }; if(srBuildFwdPair(t, &sr)) { sr.nodes.ptrNonConst[0].flags = ESRNodeFlag_None;
		srExpectReadFails(t, &sr, &type, "fwd/bck wrong direction rejected"); } SRFile_free(&sr, t->alloc); }
}

void Test_SRReadStreamShape(Test *t) {

	Test_setModule(t, "SRFile read: stream shape (truncation / trailing / alignment)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	//Write a valid file into a buffer we control
	SRFile sr = { 0 };
	StreamRef *s = NULL;
	Buffer whole = Buffer_createNull();
	U64 total = 0;

	if(srBuildMinimal(t, &sr) && MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &s, &t->err)) {

		U64 off = 0;
		if(SRFile_write(&sr, t->alloc, s, &off, &t->err)) {
			total = off;
			MemoryStream *ms = RefPtr_data(s, MemoryStream);
			Buffer_createCopy(Buffer_createRefConst(ms->data.ptr, total), t->alloc, &whole, &t->err);
		}
	}

	Test_assert(t, "prepared valid buffer", total > 0 && Buffer_length(whole) == total);

	if(total > 0) {

		//Truncated at a few points: mid-header, mid-nodes, before names
		U64 cutoffs[3] = { 8, sizeof(U32) + 20 /*past header, into nodes*/, total > 16 ? total - 16 : total / 2 };

		for(U64 c = 0; c < 3; ++c) {

			U64 len = cutoffs[c] < total ? cutoffs[c] : total - 1;
			StreamRef *trunc = NULL;

			//Pass a borrowed ref (not owned `whole`): the region stream must not own/free the backing buffer.

			Bool made = MemoryStream_createFromBufferRegion(
				Buffer_createRefConst(whole.ptr, total), 0, len, EMemoryStreamFlags_None, &type, &trunc, &t->err
			);

			SRFile result = { 0 };
			Bool readOk = false;

			if(made) {
				U64 ro = 0;
				readOk = SRFile_read(trunc, &ro, false, t->alloc, &result, NULL);
			}

			Test_assert(t, "truncated stream rejected", made && !readOk);
			SRFile_free(&result, t->alloc);
			RefPtr_dec(&trunc);
		}

		//Trailing data: a region longer than the file (extra zero bytes) -> non-subfile end check fails
		{
			Buffer padded = Buffer_createNull();
			Buffer_createUninitializedBytes(total + 16, t->alloc, &padded, &t->err);
			for(U64 i = 0; i < total; ++i) padded.ptrNonConst[i] = whole.ptr[i];
			for(U64 i = total; i < total + 16; ++i) padded.ptrNonConst[i] = 0;

			StreamRef *trailing = NULL;
			Bool made = MemoryStream_createFromBufferRegion(
				Buffer_createRefConst(padded.ptr, total + 16), 0, total + 16, EMemoryStreamFlags_None, &type, &trailing, &t->err);

			SRFile result = { 0 };
			Bool readOk = false;
			if(made) { U64 ro = 0; readOk = SRFile_read(trailing, &ro, false, t->alloc, &result, NULL); }

			Test_assert(t, "trailing data rejected (non-subfile)", made && !readOk);
			SRFile_free(&result, t->alloc);
			RefPtr_dec(&trailing);
			Buffer_free(&padded, t->alloc);
		}
	}

	//Misaligned offset on both read and write
	{
		StreamRef *s2 = NULL;
		if(MemoryStream_create(64, EMemoryStreamFlags_WriteResize, &type, &s2, &t->err)) {
			U64 badWrite = 8;
			Test_assert(t, "misaligned write rejected", !SRFile_write(&sr, t->alloc, s2, &badWrite, NULL));
			U64 badRead = 8;
			SRFile result = { 0 };
			Test_assert(t, "misaligned read rejected", !SRFile_read(s2, &badRead, false, t->alloc, &result, NULL));
			SRFile_free(&result, t->alloc);
		}
		RefPtr_dec(&s2);
	}

	Buffer_free(&whole, t->alloc);
	SRFile_free(&sr, t->alloc);
	RefPtr_dec(&s);
}

//Round-trip helper returning the read-back file (caller frees both).
static Bool srRoundTrip(Test *t, const SRFile *src, const RefPtrType *type, StreamRef **s, SRFile *result) {

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, s, &t->err))
		return false;

	U64 off = 0;
	if(!SRFile_write(src, t->alloc, *s, &off, &t->err))
		return false;

	U64 ro = 0;
	return SRFile_read(*s, &ro, false, t->alloc, result, &t->err);
}

void Test_SRFileStructuralRoundTrips(Test *t) {

	Test_setModule(t, "SRFile: structural round-trips (empty / anonymous / boundary / fields)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	//Empty (0 nodes)
	{
		SRFile sr = { 0 }, result = { 0 };
		StreamRef *s = NULL;

		if(SRFile_create(ESRSettingsFlags_None, ESRFeature_All, t->alloc, &sr, &t->err) &&
			SRFile_finalize(&sr, t->alloc, &t->err)) {

			Bool ok = srRoundTrip(t, &sr, &type, &s, &result);
			Test_assert(t, "empty file round-trips", ok);
			Test_assert(t, "empty file has 0 nodes", ok && result.nodes.length == 0);
			Test_assert(t, "empty file hash preserved", ok && result.hash == sr.hash);
		}
		else Test_assert(t, "create empty", false);

		SRFile_free(&result, t->alloc);
		SRFile_free(&sr, t->alloc);
		RefPtr_dec(&s);
	}

	//Anonymous node (nameId == U32_MAX): round-trip + print must not crash
	{
		SRFile sr = { 0 }, result = { 0 };
		StreamRef *s = NULL;

		if(SRFile_create(ESRSettingsFlags_None, ESRFeature_All, t->alloc, &sr, &t->err)) {

			SRNode anon = srNode(ESRNodeType_Struct, U32_MAX, U32_MAX);
			ListSRNode_pushBack(&sr.nodes, anon, t->alloc, &t->err);
			SRFile_finalize(&sr, t->alloc, &t->err);

			Bool ok = srRoundTrip(t, &sr, &type, &s, &result);
			Test_assert(t, "anonymous node round-trips", ok);
			Test_assert(t, "anonymous node stays anonymous", ok && result.nodes.length == 1 && result.nodes.ptr[0].nameId == U32_MAX);
			SRFile_print(&result, 0, true, true, t->alloc);        //verbose must not crash on (anonymous)
		}
		else Test_assert(t, "create anon", false);

		SRFile_free(&result, t->alloc);
		SRFile_free(&sr, t->alloc);
		RefPtr_dec(&s);
	}

	//Boundary + interpolation/fwd fields: a wide tree of N nodes, node[1] carries interpolation, a fwd/def pair at the end
	{
		const U32 N = 300;
		SRFile sr = { 0 }, result = { 0 };
		StreamRef *s = NULL;

		if(SRFile_create(ESRSettingsFlags_None, ESRFeature_All, t->alloc, &sr, &t->err)) {

			U32 id = 0; CharString n = CharString_createRefCStrConst("x");
			SRFile_addString(&sr, &n, t->alloc, &id, &t->err);

			SRNode root = srNode(ESRNodeType_Namespace, id, U32_MAX);
			root.childCount = N - 1;
			ListSRNode_pushBack(&sr.nodes, root, t->alloc, &t->err);

			for(U32 i = 1; i < N; ++i) {
				SRNode c = srNode(ESRNodeType_Variable, id, 0);
				if(i == 1) c.interpolation = ESRInterpolation_LinearNoperspective;
				ListSRNode_pushBack(&sr.nodes, c, t->alloc, &t->err);
			}

			SRFile_finalize(&sr, t->alloc, &t->err);

			Bool ok = srRoundTrip(t, &sr, &type, &s, &result);
			Test_assert(t, "large-N file round-trips", ok);
			Test_assert(t, "large-N node count", ok && result.nodes.length == N);
			Test_assert(t, "interpolation field survives",
				ok && result.nodes.length == N && result.nodes.ptr[1].interpolation == ESRInterpolation_LinearNoperspective);
			Test_assert(t, "large-N hash preserved", ok && result.hash == sr.hash);
		}
		else Test_assert(t, "create large-N", false);

		SRFile_free(&result, t->alloc);
		SRFile_free(&sr, t->alloc);
		RefPtr_dec(&s);
	}
}

void Test_SRFileHashAndName(Test *t) {

	Test_setModule(t, "SRFile: hash sensitivity + ESRNodeType_name");

	//Hash: two identical builds equal; a changed localId or name differs
	{
		SRFile a = { 0 }, b = { 0 }, c = { 0 };
		Bool ok =
			srBuildMinimal(t, &a) && srBuildMinimal(t, &b) && srBuildMinimal(t, &c);

		if(ok) {
			SRFile_finalize(&a, t->alloc, &t->err);
			SRFile_finalize(&b, t->alloc, &t->err);

			c.nodes.ptrNonConst[1].localId = 12345;        //benign, unvalidated field
			SRFile_finalize(&c, t->alloc, &t->err);

			Test_assert(t, "identical builds -> equal hash", a.hash == b.hash);
			Test_assert(t, "changed localId -> different hash", a.hash != c.hash);
		}
		else Test_assert(t, "build a/b/c", false);

		SRFile_free(&a, t->alloc);
		SRFile_free(&b, t->alloc);
		SRFile_free(&c, t->alloc);
	}

	//ESRNodeType_name: every enum value maps to a non-empty distinct-ish string, and out-of-range -> "Invalid"
	{
		Bool allNamed = true;
		for(U32 i = 0; i < ESRNodeType_Count; ++i) {
			const C8 *nm = ESRNodeType_name((ESRNodeType) i);
			if(!nm || !nm[0]) allNamed = false;
		}
		Test_assert(t, "every ESRNodeType has a name", allNamed);

		CharString ns = CharString_createRefCStrConst(ESRNodeType_name(ESRNodeType_Namespace));
		CharString nsExp = CharString_createRefCStrConst("Namespace");
		Test_assert(t, "Namespace name", CharString_equalsStringSensitive(&ns, &nsExp));

		CharString el = CharString_createRefCStrConst(ESRNodeType_name(ESRNodeType_Else));
		CharString elExp = CharString_createRefCStrConst("Else");
		Test_assert(t, "Else name", CharString_equalsStringSensitive(&el, &elExp));

		CharString inv = CharString_createRefCStrConst(ESRNodeType_name(ESRNodeType_Count));
		CharString invExp = CharString_createRefCStrConst("Invalid");
		Test_assert(t, "out-of-range name is Invalid", CharString_equalsStringSensitive(&inv, &invExp));
	}
}

void Test_SRFileCreateAndWriteGuards(Test *t) {

	Test_setModule(t, "SRFile: create + write/finalize guards");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	//create rejects invalid flags and unknown features
	{
		SRFile sr = { 0 };
		Test_assert(t, "create rejects invalid flags",
			!SRFile_create((ESRSettingsFlags) 0x80000000, ESRFeature_All, t->alloc, &sr, NULL));
		SRFile_free(&sr, t->alloc);
	}
	{
		SRFile sr = { 0 };
		Test_assert(t, "create rejects unknown feature bits",
			!SRFile_create(ESRSettingsFlags_None, (U32) (1 << 20), t->alloc, &sr, NULL));
		SRFile_free(&sr, t->alloc);
	}

	//create rejects a non-empty target (memleak guard)
	{
		SRFile sr = { 0 };
		if(srBuildMinimal(t, &sr))
			Test_assert(t, "create rejects non-empty target",
				!SRFile_create(ESRSettingsFlags_None, ESRFeature_All, t->alloc, &sr, NULL));
		SRFile_free(&sr, t->alloc);
	}

	//CreateNoReserve path still works
	{
		SRFile sr = { 0 };
		Test_assert(t, "CreateNoReserve succeeds",
			SRFile_create(ESRSettingsFlags_CreateNoReserve, ESRFeature_All, t->alloc, &sr, &t->err));
		SRFile_free(&sr, t->alloc);
	}

	//write + finalize reject a symbols/nodes parallelism mismatch
	{
		SRFile sr = { 0 };
		if(SRFile_create(ESRSettingsFlags_HasSymbols, ESRFeature_All | ESRFeature_SymbolInfo, t->alloc, &sr, &t->err)) {

			U32 id = 0; CharString n = CharString_createRefCStrConst("n");
			SRFile_addString(&sr, &n, t->alloc, &id, &t->err);
			ListSRNode_pushBack(&sr.nodes, srNode(ESRNodeType_Struct, id, U32_MAX), t->alloc, &t->err);
			//No symbol pushed -> symbols.length (0) != nodes.length (1) while HasSymbols set

			Test_assert(t, "finalize rejects symbol/node mismatch", !SRFile_finalize(&sr, t->alloc, NULL));

			StreamRef *s = NULL;
			if(MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &s, &t->err)) {
				U64 off = 0;
				Test_assert(t, "write rejects symbol/node mismatch", !SRFile_write(&sr, t->alloc, s, &off, NULL));
			}
			RefPtr_dec(&s);
		}
		SRFile_free(&sr, t->alloc);
	}
}
