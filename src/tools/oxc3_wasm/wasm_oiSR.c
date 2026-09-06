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

//tools/oxc3_wasm/wasm_oiSR.c

#include "tools/oxc3_wasm/wasm_bridge.h"
#include "formats/oiSR/sr_file.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"
#include <inttypes.h>

//The reflection tiers, in bit order, so a features list reads the same way the CLI prints one.
//SymbolInfo sits at bit 16 and is named separately rather than being padded into this table.

static const C8 *srFeatureNames[5] = { "Basics", "Functions", "Namespaces", "UserTypes", "Scopes" };

//The descriptor class a frontend resource kind lands in, which is the grouping the symbol view sorts by.
//Frontend reflection has no space or binding, so this is all a Register node can say about where it binds.

static const C8 *WasmJson_srRegisterClass(ESRResourceType type) {

	switch (type) {

		case ESRResourceType_CBuffer:
		case ESRResourceType_TBuffer:
			return "CBV";

		case ESRResourceType_Sampler:
			return "SMP";

		case ESRResourceType_UAVRWTyped:
		case ESRResourceType_UAVRWStructured:
		case ESRResourceType_UAVRWByteAddress:
		case ESRResourceType_UAVAppendStructured:
		case ESRResourceType_UAVConsumeStructured:
		case ESRResourceType_UAVRWStructuredWithCounter:
		case ESRResourceType_UAVFeedbackTexture:
			return "UAV";

		default:
			return "SRV";
	}
}

//A node is from a builtin include if its source file's basename starts with '@' (e.g. @types.hlsli).
//Same rule SRFile_print collapses on, so the page's tree folds exactly what the CLI folds.

static Bool WasmJson_srIsBuiltin(CharString file) {

	U64 length = CharString_length(file);
	U64 base = 0;

	for(U64 i = 0; i < length; ++i)
		if(file.ptr[i] == '/' || file.ptr[i] == '\\')
			base = i + 1;

	return base < length && file.ptr[base] == '@';
}

//The first quoted argument of an annotation, e.g. compute out of oxc::stage("compute").
//The annotation's text is all the format keeps (a name id covering name and arguments both), so a stage is read
// back out of it rather than being a field of its own.

static CharString WasmJson_srAnnotationArgument(CharString annotation) {

	U64 length = CharString_length(annotation);
	U64 start = length;

	for (U64 i = 0; i < length; ++i)
		if (annotation.ptr[i] == '"') {

			if(start == length) {
				start = i + 1;
				continue;
			}

			return CharString_createRefSizedConst(annotation.ptr + start, i - start, false);
		}

	return CharString_createNull();
}

//One node's collapsed bit. False when the set was never built, which is the no-symbols case: without a
//source file per node there is no builtin include to fold in the first place.

static Bool WasmJson_srCollapsed(Buffer collapsed, U64 i) {
	Bool bit = false;
	return Buffer_getBit(collapsed, i, &bit, NULL) && bit;
}

Bool WasmJson_srFile(
	const SRFile *file,
	CharString name,
	CharString sourceName,
	CharString *out,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Buffer collapsed = Buffer_createNull();
	CharString annotationText = CharString_createNull();

	if(!file || !out)
		retError(clean, Error_nullPointer(!file ? 0 : 3, "WasmJson_srFile()::file and out are required"));

	Bool hasSymbols = (file->flags & ESRSettingsFlags_HasSymbols) != 0;

	//collapsed[i] = node i is a builtin include's symbol, or a descendant of one.
	//Nodes are stored in producer order, so a parent always precedes its children and one pass is enough.

	if(hasSymbols)
		gotoIfError3(clean, Buffer_createEmptyBytes((file->nodes.length + 7) >> 3, alloc, &collapsed, e_rr));

	for (U64 i = 0; i < file->nodes.length && collapsed.ptr; ++i) {

		SRNode node = file->nodes.ptr[i];

		Bool builtin =
			i < file->symbols.length && file->symbols.ptr[i].fileNameId != U32_MAX &&
			WasmJson_srIsBuiltin(file->names.entryStrings.ptr[file->symbols.ptr[i].fileNameId]);

		if(!builtin && node.parent != U32_MAX && node.parent < i)
			builtin = WasmJson_srCollapsed(collapsed, node.parent);

		if(builtin)
			gotoIfError3(clean, Buffer_setBit(collapsed, i, e_rr));
	}

	gotoIfError3(clean, Json_raw(out, "{", alloc, e_rr));

	Bool first = true;

	gotoIfError3(clean, Json_key(out, "name", &first, alloc, e_rr));
	gotoIfError3(clean, Json_str(out, name, alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "sourceName", &first, alloc, e_rr));
	gotoIfError3(clean, Json_str(out, sourceName, alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "compilerVersion", &first, alloc, e_rr));
	gotoIfError3(clean, Json_fmt(
		out, alloc, e_rr, "{\"major\":%u,\"minor\":%u,\"patch\":%u}", OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH
	));

	//A content hash is 64 bits, which a JSON number silently rounds past 2^53, so it travels as hex text.

	gotoIfError3(clean, Json_key(out, "hash", &first, alloc, e_rr));
	gotoIfError3(clean, Json_fmt(out, alloc, e_rr, "\"%016"PRIx64"\"", file->hash));

	gotoIfError3(clean, Json_key(out, "header", &first, alloc, e_rr));
	gotoIfError3(clean, Json_raw(out, "{\"version\":\"1.1\",\"flags\":{\"hasSymbols\":", alloc, e_rr));
	gotoIfError3(clean, Json_bool(out, hasSymbols, alloc, e_rr));
	gotoIfError3(clean, Json_raw(out, "},\"features\":[", alloc, e_rr));

	Bool firstFeature = true;

	for(U64 i = 0; i < sizeof(srFeatureNames) / sizeof(srFeatureNames[0]); ++i)
		if ((file->features >> i) & 1) {
			gotoIfError3(clean, Json_next(out, &firstFeature, alloc, e_rr));
			gotoIfError3(clean, Json_cstr(out, srFeatureNames[i], alloc, e_rr));
		}

	if (file->features & ESRFeature_SymbolInfo) {
		gotoIfError3(clean, Json_next(out, &firstFeature, alloc, e_rr));
		gotoIfError3(clean, Json_cstr(out, "SymbolInfo", alloc, e_rr));
	}

	gotoIfError3(clean, Json_fmt(
		out, alloc, e_rr,
		"],\"counts\":{\"nodes\":%"PRIu64",\"annotations\":%"PRIu64",\"registers\":%"PRIu64",\"enumValues\":%"PRIu64
		",\"types\":%"PRIu64",\"arrayDims\":%"PRIu64",\"interfaces\":%"PRIu64"}}",
		(U64) file->nodes.length, (U64) file->annotations.length, (U64) file->registers.length,
		(U64) file->enumValues.length, (U64) file->types.length, (U64) file->arrayDims.length,
		(U64) file->interfaces.length
	));

	gotoIfError3(clean, Json_key(out, "nodes", &first, alloc, e_rr));
	gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

	for (U64 i = 0; i < file->nodes.length; ++i) {

		SRNode node = file->nodes.ptr[i];

		if(i)
			gotoIfError3(clean, Json_raw(out, ",", alloc, e_rr));

		gotoIfError3(clean, Json_raw(out, "{", alloc, e_rr));

		Bool firstField = true;

		gotoIfError3(clean, Json_key(out, "id", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_fmt(out, alloc, e_rr, "%"PRIu64, i));

		gotoIfError3(clean, Json_key(out, "kind", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_cstr(out, ESRNodeType_name((ESRNodeType) node.type), alloc, e_rr));

		//The return slot has no name of its own, and an anonymous node never had one; both are spelled the way
		// the CLI spells them so the two views read the same.

		gotoIfError3(clean, Json_key(out, "name", &firstField, alloc, e_rr));

		if(node.flags & ESRNodeFlag_ParamReturn) {
			gotoIfError3(clean, Json_cstr(out, "(return)", alloc, e_rr));
		}

		else if(node.nameId == U32_MAX) {
			gotoIfError3(clean, Json_cstr(out, "(anonymous)", alloc, e_rr));
		}

		else gotoIfError3(clean, Json_str(out, file->names.entryStrings.ptr[node.nameId], alloc, e_rr));

		//A root has no parent, which the page tests for with a negative id rather than a sentinel it would have
		// to know the width of.

		gotoIfError3(clean, Json_key(out, "parent", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_fmt(
			out, alloc, e_rr, "%"PRIi64, node.parent == U32_MAX ? (I64) -1 : (I64) node.parent
		));

		//Children are found by parent id rather than stored as a range.
		//Every child travels, builtin or not: which of them to fold away is the reader's decision, and the
		// `builtin` flag below is what it decides on. Leaving them out here would make the fold permanent.

		gotoIfError3(clean, Json_key(out, "children", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

		Bool firstChild = true;

		for(U64 j = 0; j < file->nodes.length; ++j)
			if (file->nodes.ptr[j].parent == i) {
				gotoIfError3(clean, Json_next(out, &firstChild, alloc, e_rr));
				gotoIfError3(clean, Json_fmt(out, alloc, e_rr, "%"PRIu64, j));
			}

		gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

		//The source span, which is what makes the tree an outline with go to definition.
		//Absent when the file was written without the SymbolInfo tier.

		gotoIfError3(clean, Json_key(out, "loc", &firstField, alloc, e_rr));

		if(i >= file->symbols.length || file->symbols.ptr[i].fileNameId == U32_MAX) {
			gotoIfError3(clean, Json_raw(out, "null", alloc, e_rr));
		}

		else {

			SRSymbol symbol = file->symbols.ptr[i];

			gotoIfError3(clean, Json_raw(out, "{\"file\":", alloc, e_rr));
			gotoIfError3(clean, Json_str(out, file->names.entryStrings.ptr[symbol.fileNameId], alloc, e_rr));
			gotoIfError3(clean, Json_fmt(
				out, alloc, e_rr, ",\"line\":%"PRIu32",\"col\":%"PRIu32",\"len\":%"PRIu32",\"lines\":%"PRIu32"}",
				symbol.line, symbol.columnStart,
				symbol.columnEnd > symbol.columnStart ? symbol.columnEnd - symbol.columnStart : 0,
				symbol.lineCount
			));
		}

		//Annotations keep the bracket form they were written in: [name] is a builtin, [[name]] a custom attribute.

		gotoIfError3(clean, Json_key(out, "annotations", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

		for (U16 a = 0; a < node.annotationCount; ++a) {

			U64 annotationId = (U64) node.annotationStart + a;

			if(node.annotationStart == U32_MAX || annotationId >= file->annotations.length)
				break;

			SRAnnotation annotation = file->annotations.ptr[annotationId];
			CharString text = file->names.entryStrings.ptr[annotation.nameId];

			CharString_free(&annotationText, alloc);
			gotoIfError3(clean, CharString_format(
				alloc, &annotationText, e_rr, annotation.isBuiltin ? "[%.*s]" : "[[%.*s]]",
				(int) CharString_length(text), text.ptr
			));

			if(a)
				gotoIfError3(clean, Json_raw(out, ",", alloc, e_rr));

			gotoIfError3(clean, Json_str(out, annotationText, alloc, e_rr));
		}

		gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

		//The resolved type of a value node, plus the array shape that goes with it.
		//`name` is the frontend spelling the type resolves to and `display` the alias the source wrote, which is
		// what a tooltip shows.

		const SRType *type = NULL;

		for(U64 j = 0; j < file->types.length && !type; ++j)
			if(file->types.ptr[j].nodeId == i)
				type = &file->types.ptr[j];

		gotoIfError3(clean, Json_key(out, "type", &firstField, alloc, e_rr));

		if(!type) {
			gotoIfError3(clean, Json_raw(out, "null", alloc, e_rr));
		}

		else {

			//A record type often carries no spelling of its own, only the node that declares it, so the
			//definition's name is the type's name.
			//When there is neither, the name is null rather than the class: DXC's function parameter
			//reflection reports no type name at all for `inout Payload p`, and calling that type
			//"Struct" reads as a type of that name instead of as "a struct, name not reported".
			//`cls` carries the category either way.

			Bool named = true;
			CharString under = CharString_createNull();

			if(type->typeNameId != U32_MAX)
				under = file->names.entryStrings.ptr[type->typeNameId];

			else if (
				type->defNodeId != U32_MAX && type->defNodeId < file->nodes.length &&
				file->nodes.ptr[type->defNodeId].nameId != U32_MAX
			)
				under = file->names.entryStrings.ptr[file->nodes.ptr[type->defNodeId].nameId];

			else named = false;

			CharString display = type->displayNameId != U32_MAX ?
				file->names.entryStrings.ptr[type->displayNameId] : under;

			gotoIfError3(clean, Json_raw(out, "{\"name\":", alloc, e_rr));

			if (named) {
				gotoIfError3(clean, Json_str(out, under, alloc, e_rr));
			}

			else gotoIfError3(clean, Json_raw(out, "null", alloc, e_rr));

			gotoIfError3(clean, Json_raw(out, ",\"display\":", alloc, e_rr));

			if (named || type->displayNameId != U32_MAX) {
				gotoIfError3(clean, Json_str(out, display, alloc, e_rr));
			}

			else gotoIfError3(clean, Json_raw(out, "null", alloc, e_rr));
			gotoIfError3(clean, Json_raw(out, ",\"cls\":", alloc, e_rr));
			gotoIfError3(clean, Json_cstr(out, ESRTypeClass_name((ESRTypeClass) type->typeClass), alloc, e_rr));
			gotoIfError3(clean, Json_fmt(
				out, alloc, e_rr, ",\"rows\":%"PRIu8",\"cols\":%"PRIu8",\"def\":%"PRIi64",\"base\":%"PRIi64"}",
				type->rows, type->cols,
				type->defNodeId == U32_MAX ? (I64) -1 : (I64) type->defNodeId,
				type->baseNodeId == U32_MAX ? (I64) -1 : (I64) type->baseNodeId
			));
		}

		//Array lengths list per dimension when the pool holds them, otherwise the flattened element count, which
		// is the same fallback SRFile_print reads. Bounds are rechecked so a truncated file can't over read.

		gotoIfError3(clean, Json_key(out, "implements", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

		{
			Bool firstIface = true;

			for (U64 j = 0; j < file->interfaces.length; ++j) {

				if(file->interfaces.ptr[j].nodeId != i)
					continue;

				if(!firstIface)
					gotoIfError3(clean, Json_raw(out, ",", alloc, e_rr));

				firstIface = false;
				gotoIfError3(clean, Json_fmt(out, alloc, e_rr, "%"PRIu32, file->interfaces.ptr[j].interfaceNodeId));
			}
		}

		gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

		gotoIfError3(clean, Json_key(out, "arrays", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

		if (type) {

			Bool multiDim =
				type->arrayDimCount >= 2 && type->arrayDimStart != U32_MAX &&
				(U64) type->arrayDimStart + type->arrayDimCount <= file->arrayDims.length;

			if (multiDim) {
				for(U8 k = 0; k < type->arrayDimCount; ++k)
					gotoIfError3(clean, Json_fmt(
						out, alloc, e_rr, k ? ",%"PRIu32 : "%"PRIu32, file->arrayDims.ptr[type->arrayDimStart + k]
					));
			}

			else if(type->elements)
				gotoIfError3(clean, Json_fmt(out, alloc, e_rr, "%"PRIu32, type->elements));
		}

		gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

		gotoIfError3(clean, Json_key(out, "semantic", &firstField, alloc, e_rr));

		if(node.semanticId == U32_MAX) {
			gotoIfError3(clean, Json_raw(out, "null", alloc, e_rr));
		}

		else gotoIfError3(clean, Json_str(out, file->names.entryStrings.ptr[node.semanticId], alloc, e_rr));

		//A parameter's direction, written as HLSL writes it; the return slot is its own direction so the page can
		// tell it apart from an out parameter.

		gotoIfError3(clean, Json_key(out, "direction", &firstField, alloc, e_rr));

		if (node.type != ESRNodeType_Parameter) {
			gotoIfError3(clean, Json_raw(out, "null", alloc, e_rr));
		}

		else {

			Bool in = (node.flags & ESRNodeFlag_ParamIn) != 0;
			Bool outward = (node.flags & ESRNodeFlag_ParamOut) != 0;

			gotoIfError3(clean, Json_cstr(out, node.flags & ESRNodeFlag_ParamReturn ? "return" : (
				in && outward ? "inout" : outward ? "out" : "in"
			), alloc, e_rr));
		}

		gotoIfError3(clean, Json_key(out, "register", &firstField, alloc, e_rr));

		const SRRegister *reg = NULL;

		for(U64 j = 0; j < file->registers.length && !reg; ++j)
			if(file->registers.ptr[j].nodeId == i)
				reg = &file->registers.ptr[j];

		if(!reg) {
			gotoIfError3(clean, Json_raw(out, "null", alloc, e_rr));
		}

		else {

			gotoIfError3(clean, Json_raw(out, "{\"info\":", alloc, e_rr));
			gotoIfError3(clean, Json_cstr(out, ESRResourceType_name((ESRResourceType) reg->type), alloc, e_rr));
			gotoIfError3(clean, Json_fmt(out, alloc, e_rr, ",\"count\":%"PRIu32",\"cls\":", reg->bindCount));
			gotoIfError3(clean, Json_cstr(out, WasmJson_srRegisterClass((ESRResourceType) reg->type), alloc, e_rr));
			gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));
		}

		//An enumerator's value with the enum's underlying integer type, the pair the CLI prints as `= 2 (U32)`.
		//The value is 64 bits wide and a JSON number silently rounds past 2^53, so one beyond that travels as decimal
		// text instead; a reader prints either as it is.

		gotoIfError3(clean, Json_key(out, "enumValue", &firstField, alloc, e_rr));

		const SREnumValue *enumValue = NULL;

		if(node.type == ESRNodeType_EnumValue)
			for(U64 j = 0; j < file->enumValues.length && !enumValue; ++j)
				if(file->enumValues.ptr[j].nodeId == i)
					enumValue = &file->enumValues.ptr[j];

		if(!enumValue) {
			gotoIfError3(clean, Json_raw(out, "null", alloc, e_rr));
		}

		else {

			const I64 exactMax = (I64) 1 << 53;

			gotoIfError3(clean, Json_raw(out, "{\"value\":", alloc, e_rr));

			if(enumValue->value >= -exactMax && enumValue->value <= exactMax) {
				gotoIfError3(clean, Json_fmt(out, alloc, e_rr, "%"PRIi64, enumValue->value));
			}

			else gotoIfError3(clean, Json_fmt(out, alloc, e_rr, "\"%"PRIi64"\"", enumValue->value));

			gotoIfError3(clean, Json_raw(out, ",\"type\":", alloc, e_rr));
			gotoIfError3(clean, Json_cstr(out, ESREnumType_name((ESREnumType) enumValue->enumType), alloc, e_rr));
			gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));
		}

		//An entrypoint is a function carrying a stage annotation, and the bracket form says which kind: the
		// builtin [shader("...")] is the library form, [[oxc::stage("...")]] the direct one.

		gotoIfError3(clean, Json_key(out, "entry", &firstField, alloc, e_rr));

		CharString stage = CharString_createNull();
		Bool isLibEntry = false;

		if (node.type == ESRNodeType_Function && node.annotationStart != U32_MAX)
			for (U16 a = 0; a < node.annotationCount && !CharString_length(stage); ++a) {

				U64 annotationId = (U64) node.annotationStart + a;

				if(annotationId >= file->annotations.length)
					break;

				SRAnnotation annotation = file->annotations.ptr[annotationId];
				CharString text = file->names.entryStrings.ptr[annotation.nameId];

				Bool isStage =
					CharString_startsWithCStringSensitive(&text, "oxc::stage(", 0) ||
					(annotation.isBuiltin && CharString_startsWithCStringSensitive(&text, "shader(", 0));

				if(!isStage)
					continue;

				stage = WasmJson_srAnnotationArgument(text);
				isLibEntry = annotation.isBuiltin;
			}

		if(!CharString_length(stage)) {
			gotoIfError3(clean, Json_raw(out, "null", alloc, e_rr));
		}

		else {

			gotoIfError3(clean, Json_raw(out, "{\"stage\":", alloc, e_rr));
			gotoIfError3(clean, Json_str(out, stage, alloc, e_rr));
			gotoIfError3(clean, Json_raw(out, ",\"lib\":", alloc, e_rr));
			gotoIfError3(clean, Json_bool(out, isLibEntry, alloc, e_rr));
			gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));
		}

		gotoIfError3(clean, Json_key(out, "returns", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_bool(out, (node.flags & ESRNodeFlag_HasReturn) != 0, alloc, e_rr));

		//Whether the node came from a builtin include, or descends from one that did.
		//This is what the summary counts and what a reader folds on, so the same tree can be shown collapsed
		// the way the CLI prints it or expanded in full.

		gotoIfError3(clean, Json_key(out, "builtin", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_bool(out, WasmJson_srCollapsed(collapsed, i), alloc, e_rr));

		gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));
	}

	gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

	//The per include summary the collapsed nodes add up to, in the order the includes were first seen.

	gotoIfError3(clean, Json_key(out, "builtinCollapsed", &first, alloc, e_rr));
	gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

	Bool firstBuiltin = true;

	for (U64 i = 0; i < file->nodes.length && collapsed.ptr; ++i) {

		if(!WasmJson_srCollapsed(collapsed, i) || i >= file->symbols.length || file->symbols.ptr[i].fileNameId == U32_MAX)
			continue;

		U32 fileNameId = file->symbols.ptr[i].fileNameId;
		Bool seen = false;
		U64 count = 0;

		for (U64 j = 0; j < file->nodes.length; ++j) {

			if(
				!WasmJson_srCollapsed(collapsed, j) || j >= file->symbols.length ||
				file->symbols.ptr[j].fileNameId != fileNameId
			)
				continue;

			if(j < i)
				seen = true;

			++count;
		}

		if(seen)
			continue;

		gotoIfError3(clean, Json_next(out, &firstBuiltin, alloc, e_rr));
		gotoIfError3(clean, Json_raw(out, "{\"file\":", alloc, e_rr));
		gotoIfError3(clean, Json_str(out, file->names.entryStrings.ptr[fileNameId], alloc, e_rr));
		gotoIfError3(clean, Json_fmt(out, alloc, e_rr, ",\"count\":%"PRIu64"}", count));
	}

	gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

	gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));

clean:
	CharString_free(&annotationText, alloc);
	Buffer_free(&collapsed, alloc);
	return s_uccess;
}
