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

//formats/oiSR/sr_file.c

#include "formats/oiSR/sr_file.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "formats/oiDL/dl_load.h"
#include "types/container/list_impl.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/constants.h"
#include "types/base/error.h"

TListImpl(SRNode);
TListImpl(SRSymbol);
TListImpl(SRAnnotation);
TListImpl(SRFile);

const C8 *ESRNodeType_name(ESRNodeType type) {

	static const C8 *names[] = {
		"Register", "Function", "Enum", "EnumValue", "Namespace",
		"Variable", "Typedef", "Struct", "Union", "StaticVariable",
		"Interface", "Parameter", "IfRoot", "Scope", "Do",
		"Switch", "While", "For", "GroupsharedVariable", "Case",
		"Default", "Using", "IfFirst", "ElseIf", "Else"
	};

	if(type >= ESRNodeType_Count)
		return "Invalid";

	return names[type];
}

Bool SRFile_create(ESRSettingsFlags flags, U32 features, const Allocator *alloc, SRFile *srFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(!srFile)
		retError(clean, Error_nullPointer(3, "SRFile_create()::srFile is required"));

	if(srFile->nodes.length || srFile->names.entryStrings.length)
		retError(clean, Error_invalidOperation(0, "SRFile_create()::srFile isn't empty, might indicate memleak"));

	Bool avoidReserve = flags & ESRSettingsFlags_CreateNoReserve;
	flags &= ~ESRSettingsFlags_CreateNoReserve;

	if(flags & ESRSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 0, "SRFile_create()::flags contained unsupported flag"));

	if(features & ~(U32)ESRFeature_All & ~(U32)ESRFeature_SymbolInfo)
		retError(clean, Error_invalidParameter(1, 0, "SRFile_create()::features contained unsupported bits"));

	DLSettings settings = (DLSettings) {
		.dataType = EDLDataType_String,
		.flags = EDLSettingsFlags_HideMagicNumber
	};

	gotoIfError3(clean, DLFile_create(&settings, avoidReserve ? 0 : KIBI, alloc, &srFile->names, e_rr));
	allocated = true;

	if(!avoidReserve) {
		gotoIfError3(clean, DLFile_reserve(&srFile->names, 16, alloc, e_rr));
		gotoIfError3(clean, ListSRNode_reserve(&srFile->nodes, 16, alloc, e_rr));
		gotoIfError3(clean, ListSRAnnotation_reserve(&srFile->annotations, 4, alloc, e_rr));
	}

	srFile->flags = flags;
	srFile->features = features;
	srFile->hash = 0;

clean:

	if(allocated && !s_uccess)
		SRFile_free(srFile, alloc);

	return s_uccess;
}

Bool SRFile_createCopy(const SRFile *src, const Allocator *alloc, SRFile *srFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(!srFile || !src)
		retError(clean, Error_nullPointer(!src ? 0 : 2, "SRFile_createCopy()::srFile and src are required"));

	if(srFile->nodes.length || srFile->names.entryStrings.length)
		retError(clean, Error_invalidParameter(2, 0, "SRFile_createCopy()::srFile is filled, indicates memleak"));

	gotoIfError3(clean, DLFile_createCopy(&src->names, alloc, &srFile->names, e_rr));
	allocated = true;
	gotoIfError3(clean, ListSRNode_createCopy(src->nodes, alloc, &srFile->nodes, e_rr));
	gotoIfError3(clean, ListSRSymbol_createCopy(src->symbols, alloc, &srFile->symbols, e_rr));
	gotoIfError3(clean, ListSRAnnotation_createCopy(src->annotations, alloc, &srFile->annotations, e_rr));

	srFile->flags = src->flags;
	srFile->features = src->features;
	srFile->hash = src->hash;

clean:

	if(allocated && !s_uccess)
		SRFile_free(srFile, alloc);

	return s_uccess;
}

void SRFile_free(SRFile *srFile, const Allocator *alloc) {

	if(!srFile)
		return;

	DLFile_free(&srFile->names, alloc);
	ListSRNode_free(&srFile->nodes, alloc);
	ListSRSymbol_free(&srFile->symbols, alloc);
	ListSRAnnotation_free(&srFile->annotations, alloc);

	*srFile = (SRFile) { 0 };
}

void ListSRFile_freeUnderlying(ListSRFile *files, const Allocator *alloc) {

	if(!files)
		return;

	for(U64 i = 0; i < files->length; ++i)
		SRFile_free(&files->ptrNonConst[i], alloc);

	ListSRFile_free(files, alloc);
}

Bool SRFile_addString(SRFile *srFile, CharString *str, const Allocator *alloc, U32 *id, Error *e_rr) {

	Bool s_uccess = true;
	CharString copy = CharString_createNull();

	if(!srFile || !str || !id)
		retError(clean, Error_nullPointer(!srFile ? 0 : (!str ? 1 : 3), "SRFile_addString() requires srFile, str and id"));

	//No string

	if(!CharString_length(*str)) {
		*id = U32_MAX;
		goto clean;
	}

	//Deduplicate against what's already in the pool

	U64 found = DLFile_findLoadedString(&srFile->names, 0, U64_MAX, str);

	if(found != U64_MAX) {
		*id = (U32) found;
		goto clean;
	}

	if(srFile->names.entryStrings.length >= U32_MAX - 1)
		retError(clean, Error_outOfBounds(0, srFile->names.entryStrings.length, U32_MAX - 1, "SRFile_addString() too many strings"));

	//The pool owns its strings, so store a copy (DLFile_addEntryString moves it in)

	gotoIfError3(clean, CharString_createCopy(*str, alloc, &copy, e_rr));
	gotoIfError3(clean, DLFile_addEntryString(&srFile->names, &copy, alloc, e_rr));
	copy = CharString_createNull();        //Ownership moved into the DLFile

	*id = (U32) (srFile->names.entryStrings.length - 1);

clean:
	CharString_free(&copy, alloc);
	return s_uccess;
}

Bool SRFile_finalize(SRFile *srFile, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	(void) alloc;        //No allocation needed; kept for signature consistency and future use

	if(!srFile)
		retError(clean, Error_nullPointer(0, "SRFile_finalize()::srFile is required"));

	//Symbols, if present, must be parallel to nodes

	Bool hasSymbols = srFile->flags & ESRSettingsFlags_HasSymbols;

	if(hasSymbols && srFile->symbols.length != srFile->nodes.length)
		retError(clean, Error_invalidState(0, "SRFile_finalize() symbols must be parallel to nodes when HasSymbols is set"));

	if(!hasSymbols && srFile->symbols.length)
		retError(clean, Error_invalidState(0, "SRFile_finalize() symbols present but HasSymbols isn't set"));

	//Hash content deterministically (flags without HideMagicNumber, features, then the POD arrays and strings)

	U32 hashedFlags = srFile->flags & ~(U32)ESRSettingsFlags_HideMagicNumber;

	U64 hash = Buffer_fnv1a64Single(((U64)srFile->features << 32) | hashedFlags, Buffer_fnv1a64Offset);

	hash = Buffer_fnv1a64(ListSRNode_bufferConst(srFile->nodes), hash);
	hash = Buffer_fnv1a64(ListSRSymbol_bufferConst(srFile->symbols), hash);
	hash = Buffer_fnv1a64(ListSRAnnotation_bufferConst(srFile->annotations), hash);

	for(U64 i = 0; i < srFile->names.entryStrings.length; ++i)
		hash = Buffer_fnv1a64(CharString_bufferConst(srFile->names.entryStrings.ptr[i]), hash);

	srFile->hash = hash;

clean:
	return s_uccess;
}

void SRFile_print(const SRFile *srFile, U64 indenting, const Allocator *alloc) {

	if(!srFile) {
		Log_debugLn(alloc, "SRFile_print() invalid srFile");
		return;
	}

	Bool hasSymbols = srFile->flags & ESRSettingsFlags_HasSymbols;

	for(U64 i = 0; i < srFile->nodes.length; ++i) {

		SRNode node = srFile->nodes.ptr[i];

		//Walk parents to compute the indentation depth

		U64 depth = indenting;
		U32 parent = node.parent;

		while(parent != U32_MAX && depth < SHORTSTRING_LEN - 1) {
			++depth;
			parent = parent < srFile->nodes.length ? srFile->nodes.ptr[parent].parent : U32_MAX;
		}

		ShortString indent;
		for(U64 j = 0; j < depth && j < SHORTSTRING_LEN - 1; ++j) indent[j] = '\t';
		indent[depth < SHORTSTRING_LEN - 1 ? depth : SHORTSTRING_LEN - 1] = '\0';

		CharString name =
			node.nameId == U32_MAX ? CharString_createRefCStrConst("(anonymous)") :
			srFile->names.entryStrings.ptr[node.nameId];

		Log_debug(
			alloc, ELogOptions_None,
			"%s%s %.*s",
			indent,
			ESRNodeType_name((ESRNodeType) node.type),
			(int) CharString_length(name),
			name.ptr
		);

		if(node.semanticId != U32_MAX) {
			CharString semantic = srFile->names.entryStrings.ptr[node.semanticId];
			Log_debug(alloc, ELogOptions_None, " : %.*s", (int) CharString_length(semantic), semantic.ptr);
		}

		if(hasSymbols && i < srFile->symbols.length) {

			SRSymbol sym = srFile->symbols.ptr[i];

			if(sym.fileNameId != U32_MAX) {
				CharString file = srFile->names.entryStrings.ptr[sym.fileNameId];
				Log_debug(
					alloc, ELogOptions_None,
					" (%.*s:%"PRIu32":%"PRIu32")",
					(int) CharString_length(file), file.ptr, sym.line, sym.columnStart
				);
			}
		}

		Log_debug(alloc, ELogOptions_NewLine, "");
	}
}
