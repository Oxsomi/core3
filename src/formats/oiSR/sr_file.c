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
#include "types/base/string_read_helper.h"        //CharString_equalsStringSensitive (SRFile_findNodeByName)
#include "types/base/constants.h"
#include "types/base/error.h"

TListImpl(SRNode);
TListImpl(SRSymbol);
TListImpl(SRAnnotation);
TListImpl(SRRegister);
TListImpl(SREnumValue);
TListImpl(SRType);
TListImpl(SRInterface);
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

const C8 *ESRResourceType_name(ESRResourceType type) {

	static const C8 *names[] = {
		"cbuffer", "tbuffer", "Texture", "SamplerState", "RWTexture",
		"StructuredBuffer", "RWStructuredBuffer", "ByteAddressBuffer", "RWByteAddressBuffer",
		"AppendStructuredBuffer", "ConsumeStructuredBuffer", "RWStructuredBuffer(counter)",
		"RaytracingAccelerationStructure", "FeedbackTexture"
	};

	if(type >= ESRResourceType_Count)
		return "Invalid";

	return names[type];
}

const C8 *ESREnumType_name(ESREnumType type) {

	static const C8 *names[] = { "uint", "int", "uint64_t", "int64_t", "uint16_t", "int16_t" };

	if(type >= ESREnumType_Count)
		return "Invalid";

	return names[type];
}

const C8 *ESRTypeClass_name(ESRTypeClass type) {

	static const C8 *names[] = {
		"Scalar", "Vector", "MatrixRows", "MatrixColumns", "Object", "Struct", "InterfaceClass", "InterfacePointer"
	};

	if(type >= ESRTypeClass_Count)
		return "Invalid";

	return names[type];
}

const C8 *ESRInterpolation_name(ESRInterpolation mode) {

	static const C8 *names[] = {
		"Undefined", "Constant", "Linear", "LinearCentroid", "LinearNoperspective",
		"LinearNoperspectiveCentroid", "LinearSample", "LinearNoperspectiveSample"
	};

	if(mode >= ESRInterpolation_Count)
		return "Invalid";

	return names[mode];
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
	gotoIfError3(clean, ListSRRegister_createCopy(src->registers, alloc, &srFile->registers, e_rr));
	gotoIfError3(clean, ListSREnumValue_createCopy(src->enumValues, alloc, &srFile->enumValues, e_rr));
	gotoIfError3(clean, ListSRType_createCopy(src->types, alloc, &srFile->types, e_rr));
	gotoIfError3(clean, ListU32_createCopy(src->arrayDims, alloc, &srFile->arrayDims, e_rr));
	gotoIfError3(clean, ListSRInterface_createCopy(src->interfaces, alloc, &srFile->interfaces, e_rr));

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
	ListSRRegister_free(&srFile->registers, alloc);
	ListSREnumValue_free(&srFile->enumValues, alloc);
	ListSRType_free(&srFile->types, alloc);
	ListU32_free(&srFile->arrayDims, alloc);
	ListSRInterface_free(&srFile->interfaces, alloc);

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

U32 SRFile_findNodeByName(const SRFile *srFile, CharString name, ESRNodeType type) {

	if(!srFile)
		return U32_MAX;

	for(U64 i = 0; i < srFile->nodes.length; ++i) {

		U8 t = srFile->nodes.ptr[i].type;
		U32 nameId = srFile->nodes.ptr[i].nameId;

		//Struct and Union are the two record kinds, so a Struct query matches either

		Bool typeMatch = t == type || (type == ESRNodeType_Struct && t == ESRNodeType_Union);

		if(typeMatch && nameId != U32_MAX &&
			CharString_equalsStringSensitive(&srFile->names.entryStrings.ptr[nameId], &name))
			return (U32) i;
	}

	return U32_MAX;
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
	hash = Buffer_fnv1a64(ListSRRegister_bufferConst(srFile->registers), hash);
	hash = Buffer_fnv1a64(ListSREnumValue_bufferConst(srFile->enumValues), hash);
	hash = Buffer_fnv1a64(ListSRType_bufferConst(srFile->types), hash);
	hash = Buffer_fnv1a64(ListU32_bufferConst(srFile->arrayDims), hash);
	hash = Buffer_fnv1a64(ListSRInterface_bufferConst(srFile->interfaces), hash);

	for(U64 i = 0; i < srFile->names.entryStrings.length; ++i)
		hash = Buffer_fnv1a64(CharString_bufferConst(srFile->names.entryStrings.ptr[i]), hash);

	srFile->hash = hash;

clean:
	return s_uccess;
}

//A node is from a builtin include if its source file's basename starts with '@' (e.g. @types.hlsli).

static Bool srFileIsBuiltin(CharString file) {

	U64 len = CharString_length(file);
	U64 base = 0;

	for(U64 i = 0; i < len; ++i)
		if(file.ptr[i] == '/' || file.ptr[i] == '\\')
			base = i + 1;

	return base < len && file.ptr[base] == '@';
}

//Prints a string with control characters (newline, carriage return, tab) escaped, so a multi-line value such as an
//oxc::uniforms annotation stays on a single line and doesn't break the indented tree layout.

static void srPrintEscaped(const Allocator *alloc, CharString str) {

	U64 start = 0;
	const U64 n = CharString_length(str);

	for(U64 i = 0; i < n; ++i) {

		const C8 c = str.ptr[i];
		const C8 *esc = c == '\n' ? "\\n" : c == '\r' ? "\\r" : c == '\t' ? "\\t" : NULL;

		if(!esc)
			continue;

		if(i > start)
			Log_debug(alloc, ELogOptions_None, "%.*s", (int) (i - start), str.ptr + start);

		Log_debug(alloc, ELogOptions_None, "%s", esc);
		start = i + 1;
	}

	if(n > start)
		Log_debug(alloc, ELogOptions_None, "%.*s", (int) (n - start), str.ptr + start);
}

void SRFile_print(const SRFile *srFile, U64 indenting, Bool isVerbose, Bool collapseBuiltins, const Allocator *alloc) {

	if(!srFile) {
		Log_debugLn(alloc, "SRFile_print() invalid srFile");
		return;
	}

	Bool hasSymbols = srFile->flags & ESRSettingsFlags_HasSymbols;

	//Collapse nodes from builtin includes (@types.hlsli etc.) into a per-file summary at the end, so the shader's
	//own symbols aren't buried under the couple hundred builtin symbols the includes pull in. Needs source locations.

	collapseBuiltins = collapseBuiltins && hasSymbols;

	//collapsedMask[j] = node j was folded into the builtin summary (a builtin-include node, or a descendant of one)

	Buffer collapsedMask = Buffer_createNull();

	if(collapseBuiltins)
		Buffer_createEmptyBytes(srFile->nodes.length, alloc, &collapsedMask, NULL);

	U32 builtinFileIds[16];
	U64 builtinCounts[16] = { 0 };
	U64 builtinFileCount = 0, builtinTotal = 0;

	for(U64 i = 0; i < srFile->nodes.length; ++i) {

		SRNode node = srFile->nodes.ptr[i];

		//Collapse builtin-include nodes (and their descendants) into a per-file summary, skipping the inline dump

		if(collapseBuiltins && collapsedMask.ptr) {

			Bool builtin = false;
			U32 fileId = U32_MAX;

			if(
				i < srFile->symbols.length && srFile->symbols.ptr[i].fileNameId != U32_MAX &&
				srFileIsBuiltin(srFile->names.entryStrings.ptr[srFile->symbols.ptr[i].fileNameId])
			) {
				builtin = true;
				fileId = srFile->symbols.ptr[i].fileNameId;
			}

			else if(node.parent != U32_MAX && collapsedMask.ptr[node.parent])
				builtin = true;        //A descendant of a collapsed builtin node (e.g. a builtin function's parameters)

			if(builtin) {

				collapsedMask.ptrNonConst[i] = 1;
				++builtinTotal;

				if(fileId != U32_MAX) {

					U64 k = 0;

					for(; k < builtinFileCount; ++k)
						if(builtinFileIds[k] == fileId)
							break;

					if(k == builtinFileCount && builtinFileCount < 16) {
						builtinFileIds[builtinFileCount] = fileId;
						k = builtinFileCount++;
					}

					if(k < 16)
						++builtinCounts[k];
				}

				continue;
			}
		}

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
			node.flags & ESRNodeFlag_ParamReturn ? CharString_createRefCStrConst("(return)") :
			node.nameId == U32_MAX ? CharString_createRefCStrConst("(anonymous)") :
			srFile->names.entryStrings.ptr[node.nameId];

		//Parameter direction (in / out / inout), printed like HLSL as a qualifier before the name

		const C8 *dir = "";

		if(node.type == ESRNodeType_Parameter) {
			Bool pin = node.flags & ESRNodeFlag_ParamIn, pout = node.flags & ESRNodeFlag_ParamOut;
			dir = pin && pout ? "inout " : pout ? "out " : pin ? "in " : "";
		}

		Log_debug(
			alloc, ELogOptions_None,
			"%s%s %s%.*s",
			indent,
			ESRNodeType_name((ESRNodeType) node.type),
			dir,
			(int) CharString_length(name),
			name.ptr
		);

		//Resolved type of a value node (Variable/Parameter/Typedef/Struct/...), in parens: "(float3)", "(uint2[4])".
		//Verbose adds the structured fields so a serialized oiSR can be reviewed exactly.

		for(U64 tI = 0; tI < srFile->types.length; ++tI)
			if(srFile->types.ptr[tI].nodeId == i) {

				SRType ty = srFile->types.ptr[tI];

				//Underlying (resolved) name; falls back to the type class for nameless struct/object parameters.

				CharString under = ty.typeNameId != U32_MAX ?
					srFile->names.entryStrings.ptr[ty.typeNameId] :
					CharString_createRefCStrConst(ESRTypeClass_name((ESRTypeClass) ty.typeClass));

				//Display name is the alias the user actually wrote (what a tooltip shows); falls back to the underlying.

				CharString disp = ty.displayNameId != U32_MAX ?
					srFile->names.entryStrings.ptr[ty.displayNameId] : under;

				//Multi-dimensional arrays list their per-dim lengths ("[2][3]"); single arrays fall back to the
				//flattened `elements` ("[6]"). Bounds are re-checked here so a truncated file can't over-read.

				Bool multiDim =
					ty.arrayDimCount >= 2 && ty.arrayDimStart != U32_MAX &&
					(U64) ty.arrayDimStart + ty.arrayDimCount <= srFile->arrayDims.length;

				if(isVerbose && ty.displayNameId != U32_MAX)
					Log_debug(
						alloc, ELogOptions_None, " (%.*s : %s %"PRIu8"x%"PRIu8" elems=%"PRIu32" aka %.*s",
						(int) CharString_length(disp), disp.ptr,
						ESRTypeClass_name((ESRTypeClass) ty.typeClass), ty.rows, ty.cols, ty.elements,
						(int) CharString_length(under), under.ptr
					);

				else if(isVerbose)
					Log_debug(
						alloc, ELogOptions_None, " (%.*s : %s %"PRIu8"x%"PRIu8" elems=%"PRIu32,
						(int) CharString_length(under), under.ptr,
						ESRTypeClass_name((ESRTypeClass) ty.typeClass), ty.rows, ty.cols, ty.elements
					);

				else {

					Log_debug(alloc, ELogOptions_None, " (%.*s", (int) CharString_length(disp), disp.ptr);

					if(multiDim)
						for(U8 k = 0; k < ty.arrayDimCount; ++k)
							Log_debug(alloc, ELogOptions_None, "[%"PRIu32"]", srFile->arrayDims.ptr[ty.arrayDimStart + k]);

					else if(ty.elements)
						Log_debug(alloc, ELogOptions_None, "[%"PRIu32"]", ty.elements);

					Log_debug(alloc, ELogOptions_None, ")");
				}

				//Verbose closes the paren after the array shape and the go-to-definition target node.

				if(isVerbose) {

					if(multiDim) {
						Log_debug(alloc, ELogOptions_None, " dims=");
						for(U8 k = 0; k < ty.arrayDimCount; ++k)
							Log_debug(alloc, ELogOptions_None, "[%"PRIu32"]", srFile->arrayDims.ptr[ty.arrayDimStart + k]);
					}

					if(ty.defNodeId != U32_MAX)
						Log_debug(alloc, ELogOptions_None, " def=#%"PRIu32, ty.defNodeId);

					Log_debug(alloc, ELogOptions_None, ")");
				}

				//Inheritance, shown in both modes after the type as ": Base, IFace1, IFace2" (concrete base first, then
				//each implemented interface).

				Bool printedColon = false;

				if(ty.baseNodeId != U32_MAX && ty.baseNodeId < srFile->nodes.length) {
					U32 baseName = srFile->nodes.ptr[ty.baseNodeId].nameId;
					if(baseName != U32_MAX) {
						CharString bn = srFile->names.entryStrings.ptr[baseName];
						Log_debug(alloc, ELogOptions_None, " : %.*s", (int) CharString_length(bn), bn.ptr);
						printedColon = true;
					}
				}

				for(U64 iF = 0; iF < srFile->interfaces.length; ++iF) {

					if(srFile->interfaces.ptr[iF].nodeId != i)
						continue;

					U32 ifaceNode = srFile->interfaces.ptr[iF].interfaceNodeId;

					if(ifaceNode >= srFile->nodes.length || srFile->nodes.ptr[ifaceNode].nameId == U32_MAX)
						continue;

					CharString iName = srFile->names.entryStrings.ptr[srFile->nodes.ptr[ifaceNode].nameId];
					Log_debug(
						alloc, ELogOptions_None, printedColon ? ", %.*s" : " : %.*s",
						(int) CharString_length(iName), iName.ptr
					);
					printedColon = true;
				}

				break;
			}

		if(node.semanticId != U32_MAX) {
			CharString semantic = srFile->names.entryStrings.ptr[node.semanticId];
			Log_debug(alloc, ELogOptions_None, " : %.*s", (int) CharString_length(semantic), semantic.ptr);
		}

		//Detail for specific node kinds: resource kind, enum value, function return

		if(node.type == ESRNodeType_Register) {

			for(U64 r = 0; r < srFile->registers.length; ++r)
				if(srFile->registers.ptr[r].nodeId == i) {

					SRRegister reg = srFile->registers.ptr[r];

					//Multi-dimensional resource arrays (Texture2D tex[4][2]) list their per-dim lengths; bindCount is the
					//flattened total.

					Bool multiDim =
						reg.arrayDimCount >= 2 && reg.arrayDimStart != U32_MAX &&
						(U64) reg.arrayDimStart + reg.arrayDimCount <= srFile->arrayDims.length;

					if(isVerbose) {

						Log_debug(
							alloc, ELogOptions_None, " [%s dim=%"PRIu8" ret=%"PRIu8" count=%"PRIu32,
							ESRResourceType_name((ESRResourceType) reg.type), reg.dimension, reg.returnType, reg.bindCount
						);

						if(multiDim) {
							Log_debug(alloc, ELogOptions_None, " dims=");
							for(U8 k = 0; k < reg.arrayDimCount; ++k)
								Log_debug(alloc, ELogOptions_None, "[%"PRIu32"]", srFile->arrayDims.ptr[reg.arrayDimStart + k]);
						}

						Log_debug(alloc, ELogOptions_None, "]");
					}

					else {

						Log_debug(alloc, ELogOptions_None, " [%s]", ESRResourceType_name((ESRResourceType) reg.type));

						if(multiDim)
							for(U8 k = 0; k < reg.arrayDimCount; ++k)
								Log_debug(alloc, ELogOptions_None, "[%"PRIu32"]", srFile->arrayDims.ptr[reg.arrayDimStart + k]);
					}

					break;
				}
		}

		else if(node.type == ESRNodeType_EnumValue) {

			for(U64 v = 0; v < srFile->enumValues.length; ++v)
				if(srFile->enumValues.ptr[v].nodeId == i) {

					SREnumValue ev = srFile->enumValues.ptr[v];

					if(isVerbose)
						Log_debug(
							alloc, ELogOptions_None, " = %"PRIi64" (%s)",
							ev.value, ESREnumType_name((ESREnumType) ev.enumType)
						);

					else Log_debug(alloc, ELogOptions_None, " = %"PRIi64, ev.value);

					break;
				}
		}

		else if(node.type == ESRNodeType_Function && (node.flags & ESRNodeFlag_HasReturn))
			Log_debug(alloc, ELogOptions_None, " -> returns");

		if(hasSymbols && i < srFile->symbols.length) {

			SRSymbol sym = srFile->symbols.ptr[i];

			if(sym.fileNameId != U32_MAX) {

				CharString file = srFile->names.entryStrings.ptr[sym.fileNameId];

				if(isVerbose)
					Log_debug(
						alloc, ELogOptions_None,
						" (%.*s:%"PRIu32"+%"PRIu32":%"PRIu32"-%"PRIu32")",
						(int) CharString_length(file), file.ptr, sym.line, sym.lineCount, sym.columnStart, sym.columnEnd
					);

				else Log_debug(
					alloc, ELogOptions_None,
					" (%.*s:%"PRIu32":%"PRIu32")",
					(int) CharString_length(file), file.ptr, sym.line, sym.columnStart
				);
			}
		}

		//Verbose: dump every remaining field so a serialized oiSR can be reviewed exactly

		if(isVerbose) {

			Log_debug(
				alloc, ELogOptions_None,
				"  {#%"PRIu64" localId=0x%08"PRIx32" parent=%"PRIi64" children=%"PRIu32,
				i, node.localId, node.parent == U32_MAX ? (I64)-1 : (I64)node.parent, node.childCount
			);

			if(node.flags & ESRNodeFlag_IsFwdDeclare)
				Log_debug(alloc, ELogOptions_None, " fwdDeclare->%"PRIu32, node.fwdBckDeclareNode);

			if(node.interpolation)
				Log_debug(alloc, ELogOptions_None, " interp=%s", ESRInterpolation_name((ESRInterpolation) node.interpolation));

			Log_debug(alloc, ELogOptions_None, "}");
		}

		Log_debug(alloc, ELogOptions_NewLine, "");

		//Annotations render as indented pseudo-children (one per line) right under the node instead of trailing its
		//header, so they're easy to spot and a multi-line value (e.g. oxc::uniforms) doesn't wreck the tree layout.

		if(isVerbose && node.annotationCount) {

			const U64 annDepth = depth + 1 < SHORTSTRING_LEN - 1 ? depth + 1 : SHORTSTRING_LEN - 1;

			ShortString annIndent;
			for(U64 j = 0; j < annDepth; ++j) annIndent[j] = '\t';
			annIndent[annDepth] = '\0';

			for(U16 a = 0; a < node.annotationCount; ++a) {

				SRAnnotation an = srFile->annotations.ptr[node.annotationStart + a];
				CharString anName = srFile->names.entryStrings.ptr[an.nameId];

				Log_debug(alloc, ELogOptions_None, "%s%s", annIndent, an.isBuiltin ? "[" : "[[");
				srPrintEscaped(alloc, anName);
				Log_debug(alloc, ELogOptions_NewLine, "%s", an.isBuiltin ? "]" : "]]");
			}
		}
	}

	//Collapsed builtin-include subsection (verbose only)

	if(builtinTotal) {

		Log_debugLn(alloc, "\t... %"PRIu64" builtin-include symbols collapsed:", builtinTotal);

		for(U64 k = 0; k < builtinFileCount; ++k) {
			CharString f = srFile->names.entryStrings.ptr[builtinFileIds[k]];
			Log_debugLn(alloc, "\t\t%.*s: %"PRIu64" symbols", (int) CharString_length(f), f.ptr, builtinCounts[k]);
		}
	}

	Buffer_free(&collapsedMask, alloc);
}
