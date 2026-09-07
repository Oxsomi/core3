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

//formats/oiSB/sb_json.c

#include "formats/oiSB/sb_file.h"
#include "formats/json/json_writer.h"
#include "types/container/list_basic_types.h"

//The lengths of a variable's array dimensions.
//A dimension either fits inline or is an id into the shared pool, split on the top bit exactly as SBFile_print
// reads it; a zero means the variable isn't an array at all.

static ListU32 SBFile_jsonArrays(const SBFile *sbFile, const SBVar *var, U32 *inlineDim) {

	ListU32 dims = (ListU32) { 0 };

	if(!var->arrayDimOrArrayId)
		return dims;

	if(var->arrayDimOrArrayId >> 15)
		return sbFile->arrays.ptr[var->arrayDimOrArrayId & (U16) I16_MAX];

	*inlineDim = var->arrayDimOrArrayId;
	ListU32_createRefConst(inlineDim, 1, &dims, NULL);
	return dims;
}

//What a variable occupies in its parent, which is its element size times every array dimension.
//This is what makes the buffer's padding visible: the difference between it and the declared buffer size is
// what the layout spent on alignment.

static U64 SBFile_jsonVarSize(const SBFile *sbFile, const SBVar *var) {

	U64 size = var->structId != U16_MAX ? sbFile->structs.ptr[var->structId].stride : ESBType_getSize(
		(ESBType) var->type, (sbFile->flags & ESBSettingsFlags_IsTightlyPacked) != 0
	);

	U32 inlineDim = 0;
	ListU32 dims = SBFile_jsonArrays(sbFile, var, &inlineDim);

	for(U64 i = 0; i < dims.length; ++i)
		size *= dims.ptr[i];

	return size;
}

//One level of the variable tree, which is the layout view of a constant buffer.
//Children are found by parent id rather than being stored as a range, so this recurses over the flat list the
// same way SBFile_print walks it.

static Bool SBFile_writeJsonVars(const SBFile *sbFile, U16 parent, JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, JsonWriter_beginArray(w, e_rr));

	for (U64 i = 0; i < sbFile->vars.length; ++i) {

		SBVar var = sbFile->vars.ptr[i];

		if(var.parentId != parent)
			continue;

		gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

		//Variable names sit in the second half of the name pool, behind the struct names.

		gotoIfError3(clean, JsonWriter_keyStr(w, "name", sbFile->names.entryStrings.ptr[sbFile->structs.length + i], e_rr));

		gotoIfError3(clean, JsonWriter_keyU64(w, "offset", var.offset, e_rr));

		//A variable is either a struct instance or a plain type; the struct case reports the stride the layout
		// gives it, the plain one the size of its own type.

		gotoIfError3(clean, JsonWriter_key(w, "type", e_rr));

		if(var.structId != U16_MAX) {
			gotoIfError3(clean, JsonWriter_str(w, sbFile->names.entryStrings.ptr[var.structId], e_rr));
		}

		else gotoIfError3(clean, JsonWriter_cstr(w, ESBType_name((ESBType) var.type), e_rr));

		gotoIfError3(clean, JsonWriter_key(w, "stride", e_rr));

		if(var.structId != U16_MAX) {
			gotoIfError3(clean, JsonWriter_u64(w, sbFile->structs.ptr[var.structId].stride, e_rr));
		}

		else gotoIfError3(clean, JsonWriter_u64(
			w, ESBType_getSize((ESBType) var.type, (sbFile->flags & ESBSettingsFlags_IsTightlyPacked) != 0), e_rr
		));

		gotoIfError3(clean, JsonWriter_keyArray(w, "arrays", e_rr));

		U32 inlineDim = 0;
		ListU32 dims = SBFile_jsonArrays(sbFile, &var, &inlineDim);

		for(U64 j = 0; j < dims.length; ++j)
			gotoIfError3(clean, JsonWriter_u64(w, dims.ptr[j], e_rr));

		gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

		gotoIfError3(clean, (
			JsonWriter_keyObject(w, "used", e_rr) &&
			JsonWriter_keyBool(w, "spirv", (var.flags & ESBVarFlag_IsUsedVarSPIRV) != 0, e_rr) &&
			JsonWriter_keyBool(w, "dxil", (var.flags & ESBVarFlag_IsUsedVarDXIL) != 0, e_rr) &&
			JsonWriter_endObject(w, e_rr)
		));

		gotoIfError3(clean, JsonWriter_key(w, "children", e_rr));

		if(var.structId != U16_MAX) {
			gotoIfError3(clean, SBFile_writeJsonVars(sbFile, (U16) i, w, e_rr));
		}

		else gotoIfError3(clean, (JsonWriter_beginArray(w, e_rr) && JsonWriter_endArray(w, e_rr)));

		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

clean:
	return s_uccess;
}

Bool SBFile_writeJson(const SBFile *sbFile, JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!sbFile || !w)
		retError(clean, Error_nullPointer(!sbFile ? 0 : 1, "SBFile_writeJson()::sbFile and w are required"));

	//Padding is what the layout spends on alignment: the buffer's own size minus what its root variables take,
	// which is what makes --warn-buffer-padding's finding visible in a view.

	U64 used = 0;

	for (U64 i = 0; i < sbFile->vars.length; ++i) {

		SBVar var = sbFile->vars.ptr[i];

		if(var.parentId != U16_MAX)
			continue;

		U64 end = (U64) var.offset + SBFile_jsonVarSize(sbFile, &var);

		if(end > used)
			used = end;
	}

	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyU64(w, "size", sbFile->bufferSize, e_rr) &&
		JsonWriter_keyU64(w, "padding", sbFile->bufferSize > used ? sbFile->bufferSize - used : 0, e_rr) &&
		JsonWriter_keyBool(w, "packed", (sbFile->flags & ESBSettingsFlags_IsTightlyPacked) != 0, e_rr) &&
		JsonWriter_key(w, "vars", e_rr) &&
		SBFile_writeJsonVars(sbFile, U16_MAX, w, e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));

clean:
	return s_uccess;
}
