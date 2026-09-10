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

//formats/oiSH/sh_json.c

#include "formats/oiSH/sh_file.h"
#include "formats/oiSH/sh_registers.h"
#include "formats/oiSB/sb_file.h"
#include "formats/oiXX/oiXX.h"
#include "formats/json/json_writer.h"
#include "types/container/texture_format.h"
#include "types/base/type_id.h"
#include "types/base/string_read_helper.h"
#include <inttypes.h>

//The HLSL spelling of a register's type: the base name when the kind has one, otherwise the texture composition

static Bool SHFile_jsonRegisterTypeName(EGfxRegisterType type, JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	const C8 *base = SHRegister_baseTypeName(type);

	if (base) {
		gotoIfError3(clean, JsonWriter_cstr(w, base, e_rr));
		goto clean;
	}

	gotoIfError3(clean, JsonWriter_fmt(
		w,
		e_rr,
		"\"%s%s%s%s\"",
		type & EGfxRegisterType_IsWrite ? "RW" : "",
		type & EGfxRegisterType_IsCombinedSampler ? "sampler" : "Texture",
		SHRegister_textureDimension(type),
		type & EGfxRegisterType_IsArray ? "Array" : ""
	));

clean:
	return s_uccess;
}

static Bool SHFile_jsonRegister(const SHRegisterRuntime *reg, JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	EGfxRegisterType type = (EGfxRegisterType) reg->reg.registerType;
	EGfxRegisterType base = (EGfxRegisterType)(type & EGfxRegisterType_TypeMask);

	//A subpass input is texture-like and sits in the texture range, but the union it shares with a
	//texture's format holds its attachment id instead, so only a real texture reports one.

	Bool isTexture = base >= EGfxRegisterType_TextureStart && base < EGfxRegisterType_SubpassInput;
	Bool isBuffer = base >= EGfxRegisterType_BufferStart && base <= EGfxRegisterType_BufferEnd;

	gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyStr(w, "name", reg->name, e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(w, "arrays", e_rr));

	for(U64 i = 0; i < reg->arrays.length; ++i)
		gotoIfError3(clean, JsonWriter_u64(w, reg->arrays.ptr[i], e_rr));

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_key(w, "typeStr", e_rr));
	gotoIfError3(clean, SHFile_jsonRegisterTypeName(type, w, e_rr));

	gotoIfError3(clean, JsonWriter_keyCstr(w, "cls", SHRegister_className(type), e_rr));

	gotoIfError3(clean, (
		JsonWriter_keyObject(w, "flags", e_rr) &&
		JsonWriter_keyBool(w, "write", (type & EGfxRegisterType_IsWrite) != 0, e_rr) &&
		JsonWriter_keyBool(w, "array", (type & EGfxRegisterType_IsArray) != 0 || reg->arrays.length != 0, e_rr) &&
		JsonWriter_keyBool(w, "combined", (type & EGfxRegisterType_IsCombinedSampler) != 0, e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));

	//A binding of U32_MAX in both halves is how the format spells "not present"; push constants are the case
	// that reaches a reader as a null SPIR-V binding, since they are a block rather than a descriptor.

	GfxBinding spirv = reg->reg.bindings.arr[EGfxBinaryType_SPIRV];
	GfxBinding dxil = reg->reg.bindings.arr[EGfxBinaryType_DXIL];

	gotoIfError3(clean, (JsonWriter_keyObject(w, "bindings", e_rr) && JsonWriter_key(w, "spirv", e_rr)));

	if(spirv.space == U32_MAX && spirv.binding == U32_MAX) {
		gotoIfError3(clean, JsonWriter_null(w, e_rr));
	}

	else gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyU64(w, "set", spirv.space, e_rr) &&
		JsonWriter_keyU64(w, "binding", spirv.binding, e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));

	gotoIfError3(clean, JsonWriter_key(w, "dxil", e_rr));

	if(dxil.space == U32_MAX && dxil.binding == U32_MAX) {
		gotoIfError3(clean, JsonWriter_null(w, e_rr));
	}

	else {

		C8 letter = base == EGfxRegisterType_ConstantBuffer || base == EGfxRegisterType_PushConstants ? 'b' : (
			base == EGfxRegisterType_Sampler || base == EGfxRegisterType_SamplerComparisonState ? 's' : (
				type & EGfxRegisterType_IsWrite ? 'u' : 't'
			)
		);

		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_key(w, "letter", e_rr) &&
			JsonWriter_fmt(w, e_rr, "\"%c\"", letter) &&
			JsonWriter_keyU64(w, "binding", dxil.binding, e_rr) &&
			JsonWriter_keyU64(w, "space", dxil.space, e_rr) &&
			JsonWriter_endObject(w, e_rr)
		));
	}

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

	gotoIfError3(clean, (
		JsonWriter_keyObject(w, "used", e_rr) &&
		JsonWriter_keyBool(w, "spirv", (reg->reg.isUsedFlag >> EGfxBinaryType_SPIRV) & 1, e_rr) &&
		JsonWriter_keyBool(w, "dxil", (reg->reg.isUsedFlag >> EGfxBinaryType_DXIL) & 1, e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));

	gotoIfError3(clean, JsonWriter_keyBool(w, "push", base == EGfxRegisterType_PushConstants, e_rr));

	//Texture format info is split by direction: a read texture carries the primitive it approximately matches,
	// a written one the exact format id SPIR-V demands. Either half can be absent.

	gotoIfError3(clean, JsonWriter_key(w, "texture", e_rr));

	if(!isTexture) {
		gotoIfError3(clean, JsonWriter_null(w, e_rr));
	}

	else {

		gotoIfError3(clean, (JsonWriter_beginObject(w, e_rr) && JsonWriter_key(w, "primitive", e_rr)));

		if(reg->reg.texture.primitive == EGfxTexturePrimitive_Count) {
			gotoIfError3(clean, JsonWriter_null(w, e_rr));
		}

		else gotoIfError3(clean, JsonWriter_cstr(w, EGfxTexturePrimitive_name[reg->reg.texture.primitive], e_rr));

		gotoIfError3(clean, JsonWriter_key(w, "format", e_rr));

		if(!reg->reg.texture.formatId) {
			gotoIfError3(clean, JsonWriter_null(w, e_rr));
		}

		else gotoIfError3(clean, JsonWriter_cstr(w, ETextureFormatId_name[reg->reg.texture.formatId], e_rr));

		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	}

	//U16_MAX is how the format spells "no attachment", so only a bound one travels.

	gotoIfError3(clean, JsonWriter_key(w, "inputAttachment", e_rr));

	if (base == EGfxRegisterType_SubpassInput && reg->reg.inputAttachmentId != U16_MAX) {
		gotoIfError3(clean, JsonWriter_u64(w, reg->reg.inputAttachmentId, e_rr));
	}

	else gotoIfError3(clean, JsonWriter_null(w, e_rr));

	//A buffer register may carry an oiSB layout; a raw one (ByteAddressBuffer, acceleration structure) does not.

	gotoIfError3(clean, JsonWriter_key(w, "buffer", e_rr));

	if(!isBuffer || !reg->shaderBuffer.bufferSize) {
		gotoIfError3(clean, JsonWriter_null(w, e_rr));
	}

	else gotoIfError3(clean, SBFile_writeJson(&reg->shaderBuffer, w, e_rr));

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

clean:
	return s_uccess;
}

//The signature halves of a graphics entry.
//A slot is empty when its type is 0; the semantic id follows the same "0 means the stage's default" rule
// SHEntry_print uses, so a reader shows what the driver will see rather than a blank.

static Bool SHFile_jsonEntryIO(const SHEntry *entry, Bool isOutput, JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, JsonWriter_beginArray(w, e_rr));

	Bool hasSemantics =
		entry->inputSemanticNamesU64[0] | entry->inputSemanticNamesU64[1] |
		entry->outputSemanticNamesU64[0] | entry->outputSemanticNamesU64[1];

	for (U8 i = 0; i < 16; ++i) {

		ESBType type = (ESBType)(isOutput ? entry->outputs[i] : entry->inputs[i]);

		if(!type)
			continue;

		U8 semanticValue = isOutput ? entry->outputSemanticNames[i] : entry->inputSemanticNames[i];
		U64 semanticNameId = semanticValue >> 4;
		U64 semanticOff = isOutput ? entry->uniqueInputSemantics : 0;

		CharString semantic =
			semanticNameId ? entry->semanticNames.ptr[semanticNameId - 1 + semanticOff] : (
				isOutput && entry->stage == EGfxPipelineStage_Pixel ?
				CharString_createRefCStrConst("SV_TARGET") : CharString_createRefCStrConst("TEXCOORD")
			);

		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_keyCstr(w, "type", ESBType_name(type), e_rr) &&
			JsonWriter_keyStr(w, "semantic", semantic, e_rr) &&
			JsonWriter_keyU64(w, "idx", hasSemantics ? (U8)(semanticValue & 0xF) : i, e_rr) &&
			JsonWriter_endObject(w, e_rr)
		));
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

clean:
	return s_uccess;
}

static Bool SHFile_jsonEntry(const SHFile *file, U64 entryId, JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	const SHEntry *entry = &file->entries.ptr[entryId];
	EGfxPipelineStage stage = (EGfxPipelineStage) entry->stage;

	Bool isCompute = stage == EGfxPipelineStage_Compute;
	Bool isMeshy = stage == EGfxPipelineStage_MeshExt || stage == EGfxPipelineStage_TaskExt;
	Bool isRt = stage >= EGfxPipelineStage_RtStartExt && stage <= EGfxPipelineStage_RtEndExt;

	gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyStr(w, "name", entry->name, e_rr));

	gotoIfError3(clean, JsonWriter_keyCstr(w, "stage", SHEntry_stageNames[stage], e_rr));

	//An entry is a library entry when every binary it resolves to is one, which is how the identifier records
	// it: a lib binary has no entrypoint of its own and lists the entries it holds instead.

	Bool lib = entry->binaryIds.length != 0;

	for (U64 i = 0; i < entry->binaryIds.length; ++i) {

		U16 binaryId = entry->binaryIds.ptr[i];

		if(binaryId >= file->binaries.length || CharString_length(file->binaries.ptr[binaryId].identifier.entrypoint))
			lib = false;
	}

	gotoIfError3(clean, JsonWriter_keyBool(w, "lib", lib, e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(w, "binaryIds", e_rr));

	for(U64 i = 0; i < entry->binaryIds.length; ++i)
		gotoIfError3(clean, JsonWriter_u64(w, entry->binaryIds.ptr[i], e_rr));

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_key(w, "inputs", e_rr));
	gotoIfError3(clean, SHFile_jsonEntryIO(entry, false, w, e_rr));

	gotoIfError3(clean, JsonWriter_key(w, "outputs", e_rr));
	gotoIfError3(clean, SHFile_jsonEntryIO(entry, true, w, e_rr));

	//Group size and wave size only exist for the stages that dispatch; the rest report null so a reader never
	// has to know which stages carry them.

	gotoIfError3(clean, JsonWriter_key(w, "group", e_rr));

	if(isCompute || isMeshy) {
		gotoIfError3(clean, (
			JsonWriter_beginArray(w, e_rr) &&
			JsonWriter_u64(w, entry->groupX, e_rr) &&
			JsonWriter_u64(w, entry->groupY, e_rr) &&
			JsonWriter_u64(w, entry->groupZ, e_rr) &&
			JsonWriter_endArray(w, e_rr)
		));
	}

	else gotoIfError3(clean, JsonWriter_null(w, e_rr));

	gotoIfError3(clean, JsonWriter_key(w, "waveSize", e_rr));

	if(!entry->waveSize) {
		gotoIfError3(clean, JsonWriter_null(w, e_rr));
	}

	else gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyU64(w, "req", (U8)(entry->waveSize & 0xF), e_rr) &&
		JsonWriter_keyU64(w, "min", (U8)((entry->waveSize >> 4) & 0xF), e_rr) &&
		JsonWriter_keyU64(w, "max", (U8)((entry->waveSize >> 8) & 0xF), e_rr) &&
		JsonWriter_keyU64(w, "rec", (U8)((entry->waveSize >> 12) & 0xF), e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));

	if (isRt) {
		gotoIfError3(clean, JsonWriter_keyU64(w, "payloadSize", entry->payloadSize, e_rr));
		gotoIfError3(clean, JsonWriter_keyU64(w, "intersectionSize", entry->intersectionSize, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

clean:
	return s_uccess;
}

static Bool SHFile_jsonBinary(
	const SHFile *file, U64 binaryId, SHDisassemble disassemble, void *disassembleCtx, JsonWriter *w,
	const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	CharString typeName = CharString_createNull();
	CharString value = CharString_createNull();
	CharString disassembly = CharString_createNull();

	const SHBinaryInfo *binary = &file->binaries.ptr[binaryId];
	const SHBinaryIdentifier *identifier = &binary->identifier;

	Bool lib = !CharString_length(identifier->entrypoint);

	gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

	gotoIfError3(clean, JsonWriter_key(w, "entrypoint", e_rr));

	if(lib) {
		gotoIfError3(clean, JsonWriter_null(w, e_rr));
	}

	else gotoIfError3(clean, JsonWriter_str(w, identifier->entrypoint, e_rr));

	gotoIfError3(clean, JsonWriter_keyCstr(w, "stage", lib ? "lib" : SHEntry_stageNames[identifier->stageType], e_rr));

	gotoIfError3(clean, JsonWriter_keyBool(w, "lib", lib, e_rr));

	//Which entries resolve to this binary. A lib holds several, a stage binary exactly one, and a reader takes
	// the list either way rather than branching on the kind.

	gotoIfError3(clean, JsonWriter_keyArray(w, "entryNames", e_rr));

	for (U64 i = 0; i < file->entries.length; ++i) {

		const SHEntry *entry = &file->entries.ptr[i];

		for (U64 j = 0; j < entry->binaryIds.length; ++j)
			if (entry->binaryIds.ptr[j] == binaryId) {
				gotoIfError3(clean, JsonWriter_str(w, entry->name, e_rr));
				break;
			}
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_key(w, "model", e_rr));
	gotoIfError3(clean, JsonWriter_fmt(
		w, e_rr, "\"%"PRIu8".%"PRIu8"\"", (U8)(identifier->shaderVersion >> 8), (U8) identifier->shaderVersion
	));

	//Declared extensions and the subset of them the final executable never showed. A reader renders the
	// difference (active = extensions minus dormant), so both lists travel rather than only the result.
	//dormantExtensions can carry detected bits outside the declared set; the CLI implicitly masks them by
	// only ever printing declared bits, so the same mask applies here or a reader lists phantom dormants.

	gotoIfError3(clean, JsonWriter_keyArray(w, "extensions", e_rr));

	for(U64 i = 0; i < ESHExtension_Count; ++i)
		if ((identifier->extensions >> i) & 1) {
			gotoIfError3(clean, JsonWriter_cstr(w, ESHExtension_names[i], e_rr));
		}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(w, "dormant", e_rr));

	for(U64 i = 0; i < ESHExtension_Count; ++i)
		if (((binary->dormantExtensions & identifier->extensions) >> i) & 1) {
			gotoIfError3(clean, JsonWriter_cstr(w, ESHExtension_names[i], e_rr));
		}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(w, "defines", e_rr));

	for (U64 i = 0; i < identifier->defines.length / 2; ++i)
		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_keyStr(w, "name", identifier->defines.ptr[i << 1], e_rr) &&
			JsonWriter_keyStr(w, "value", identifier->defines.ptr[(i << 1) | 1], e_rr) &&
			JsonWriter_endObject(w, e_rr)
		));

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(w, "uniforms", e_rr));

	for (U64 i = 0; i < identifier->uniforms.length; ++i) {

		SHUniformRuntime uniform = identifier->uniforms.ptr[i];
		TypeId typeId = ETypeId_arr[uniform.typeIdShort];

		CharString_free(&typeName, alloc);
		CharString_free(&value, alloc);

		if(!CharString_createFromETypeId(typeId, alloc, &typeName, NULL))
			typeName = CharString_createRefCStrConst("unknown");

		SHValue uniformValue = (SHValue) { 0 };
		Buffer_memcpy(
			Buffer_createRef(&uniformValue, sizeof(uniformValue)),
			Buffer_createRefConst(identifier->uniformData.ptr + uniform.dataOffset, ETypeId_getBytes(typeId))
		);

		if(!SHValue_stringify(&uniformValue, typeId, alloc, &value, NULL))
			value = CharString_createRefCStrConst("unknown");

		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_keyStr(w, "type", typeName, e_rr) &&
			JsonWriter_keyStr(w, "name", uniform.name, e_rr) &&
			JsonWriter_keyStr(w, "value", value, e_rr) &&
			JsonWriter_endObject(w, e_rr)
		));
	}

	CharString_free(&typeName, alloc);
	CharString_free(&value, alloc);

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	//A mask covering every vendor means the binary named none, which a reader shows as "all" rather than as a
	// list of twelve.

	U16 anyVendor = ESHVendor_allMask(ESHVendor_Count);

	gotoIfError3(clean, JsonWriter_key(w, "vendors", e_rr));

	if((binary->vendorMask & anyVendor) == anyVendor) {
		gotoIfError3(clean, JsonWriter_null(w, e_rr));
	}

	else {

		gotoIfError3(clean, JsonWriter_beginArray(w, e_rr));

		for(U64 i = 0; i < ESHVendor_Count; ++i)
			if ((binary->vendorMask >> i) & 1) {
				gotoIfError3(clean, JsonWriter_cstr(w, ESHVendor_names[i], e_rr));
			}

		gotoIfError3(clean, JsonWriter_endArray(w, e_rr));
	}

	//[[oxc::binary(...)]] is an entry level annotation that never reaches a stored binary, so nothing here can
	// report it; a reader renders the annotation block without that line rather than inventing one.

	gotoIfError3(clean, JsonWriter_keyNull(w, "binAnno", e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(w, "supported", e_rr));

	if (Buffer_length(binary->binaries[EGfxBinaryType_SPIRV])) {
		gotoIfError3(clean, JsonWriter_cstr(w, "spv", e_rr));
	}

	if (Buffer_length(binary->binaries[EGfxBinaryType_DXIL])) {
		gotoIfError3(clean, JsonWriter_cstr(w, "dxil", e_rr));
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, (
		JsonWriter_keyObject(w, "sizes", e_rr) &&
		JsonWriter_keyU64(w, "spirv", Buffer_length(binary->binaries[EGfxBinaryType_SPIRV]), e_rr) &&
		JsonWriter_keyU64(w, "dxil", Buffer_length(binary->binaries[EGfxBinaryType_DXIL]), e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));

	//Only when the caller handed a disassembler in (the CLI's --verbose): text per backend, null for a
	// backend without code, and a stated omission with the size once the text outgrows what a document
	// carries. That is oiDL's rule for contents: the shape always, the bytes only when asked and small
	// enough to travel.

	if (disassemble) {

		static const U64 disassemblyCap = 4 * 1024 * 1024;

		gotoIfError3(clean, JsonWriter_keyObject(w, "disassembly", e_rr));

		for (U8 j = 0; j < 2; ++j) {

			const EGfxBinaryType type = j ? EGfxBinaryType_DXIL : EGfxBinaryType_SPIRV;
			const Buffer code = binary->binaries[type];

			gotoIfError3(clean, JsonWriter_key(w, j ? "dxil" : "spirv", e_rr));

			if (!Buffer_length(code)) {
				gotoIfError3(clean, JsonWriter_null(w, e_rr));
				continue;
			}

			CharString_free(&disassembly, alloc);
			gotoIfError3(clean, disassemble(disassembleCtx, type, code, alloc, &disassembly, e_rr));

			if (CharString_length(disassembly) > disassemblyCap) {
				gotoIfError3(clean, (
					JsonWriter_beginObject(w, e_rr) &&
					JsonWriter_keyU64(w, "omitted", CharString_length(disassembly), e_rr) &&
					JsonWriter_endObject(w, e_rr)
				));
			}

			else gotoIfError3(clean, JsonWriter_str(w, disassembly, e_rr));
		}

		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_keyArray(w, "registers", e_rr));

	for (U64 i = 0; i < binary->registers.length; ++i)
		gotoIfError3(clean, SHFile_jsonRegister(&binary->registers.ptr[i], w, e_rr));

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

clean:
	CharString_free(&typeName, alloc);
	CharString_free(&value, alloc);
	CharString_free(&disassembly, alloc);
	return s_uccess;
}

Bool SHFile_writeJsonMembers(
	const SHFile *file, SHDisassemble disassemble, void *disassembleCtx, JsonWriter *w, const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!file || !w)
		retError(clean, Error_nullPointer(!file ? 0 : 1, "SHFile_writeJsonMembers()::file and w are required"));

	gotoIfError3(clean, (
		JsonWriter_keyObject(w, "compilerVersion", e_rr) &&
		JsonWriter_keyU64(w, "major", OXC3_GET_MAJOR(file->compilerVersion), e_rr) &&
		JsonWriter_keyU64(w, "minor", OXC3_GET_MINOR(file->compilerVersion), e_rr) &&
		JsonWriter_keyU64(w, "patch", OXC3_GET_PATCH(file->compilerVersion), e_rr) &&
		JsonWriter_endObject(w, e_rr) &&
		JsonWriter_keyU64(w, "sourceHash", file->sourceHash, e_rr) &&
		JsonWriter_keyObject(w, "flags", e_rr) &&
		JsonWriter_keyBool(w, "reflectionOnly", (file->flags & ESHSettingsFlags_ReflectionOnly) != 0, e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));

	//The size types a rewrite would pick, which is what the header of this file carries: they are recomputed
	// from the binaries rather than kept on SHFile, so this reports the same thing SHFile_write would store.

	U8 sizeTypes[EGfxBinaryType_Count] = { 0 };

	for(U64 i = 0; i < file->binaries.length; ++i)
		for (U8 j = 0; j < EGfxBinaryType_Count; ++j) {

			U64 length = Buffer_length(file->binaries.ptr[i].binaries[j]);
			U8 required = length <= U8_MAX ? 0 : (length <= U16_MAX ? 1 : (length <= U32_MAX ? 2 : 3));

			if(required > sizeTypes[j])
				sizeTypes[j] = required;
		}

	gotoIfError3(clean, (
		JsonWriter_keyObject(w, "header", e_rr) &&
		JsonWriter_keyCstr(w, "version", "1.2", e_rr) &&
		JsonWriter_keyObject(w, "sizeTypes", e_rr) &&
		JsonWriter_keyCstr(
			w, "spirv", EXXDataSizeType_name((EXXDataSizeType) sizeTypes[EGfxBinaryType_SPIRV]), e_rr
		) &&
		JsonWriter_keyCstr(w, "dxil", EXXDataSizeType_name((EXXDataSizeType) sizeTypes[EGfxBinaryType_DXIL]), e_rr) &&
		JsonWriter_endObject(w, e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));

	//The highest model any binary asked for, which is the model the file as a whole requires.

	U16 model = OISH_SHADER_MODEL_MIN;

	for(U64 i = 0; i < file->binaries.length; ++i)
		if(file->binaries.ptr[i].identifier.shaderVersion > model)
			model = file->binaries.ptr[i].identifier.shaderVersion;

	gotoIfError3(clean, JsonWriter_key(w, "model", e_rr));
	gotoIfError3(clean, JsonWriter_fmt(w, e_rr, "\"%"PRIu8".%"PRIu8"\"", (U8)(model >> 8), (U8) model));

	gotoIfError3(clean, JsonWriter_keyArray(w, "entries", e_rr));

	for (U64 i = 0; i < file->entries.length; ++i)
		gotoIfError3(clean, SHFile_jsonEntry(file, i, w, e_rr));

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(w, "binaries", e_rr));

	for (U64 i = 0; i < file->binaries.length; ++i)
		gotoIfError3(clean, SHFile_jsonBinary(file, i, disassemble, disassembleCtx, w, alloc, e_rr));

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(w, "includes", e_rr));

	for (U64 i = 0; i < file->includes.length; ++i)
		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_keyStr(w, "path", file->includes.ptr[i].relativePath, e_rr) &&
			JsonWriter_keyU64(w, "crc32c", file->includes.ptr[i].crc32c, e_rr) &&
			JsonWriter_endObject(w, e_rr)
		));

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	//The file's register set, which is what a reflection diff compares two files by.
	//Registers live per binary, so this is their union in first seen order; a name can only appear once, since
	// the compiler binds one register per name across every variant of a file.

	gotoIfError3(clean, JsonWriter_keyArray(w, "registers", e_rr));

	for (U64 i = 0; i < file->binaries.length; ++i) {

		const ListSHRegisterRuntime *registers = &file->binaries.ptr[i].registers;

		for (U64 j = 0; j < registers->length; ++j) {

			const SHRegisterRuntime *reg = &registers->ptr[j];
			Bool seen = false;

			for (U64 k = 0; k < i && !seen; ++k) {

				const ListSHRegisterRuntime *prev = &file->binaries.ptr[k].registers;

				for(U64 l = 0; l < prev->length && !seen; ++l)
					seen = prev->ptr[l].nameHash == reg->nameHash &&
						CharString_equalsStringSensitive(&prev->ptr[l].name, &reg->name);
			}

			for(U64 k = 0; k < j && !seen; ++k)
				seen = registers->ptr[k].nameHash == reg->nameHash &&
					CharString_equalsStringSensitive(&registers->ptr[k].name, &reg->name);

			if(seen)
				continue;

			gotoIfError3(clean, SHFile_jsonRegister(reg, w, e_rr));
		}
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

clean:
	return s_uccess;
}

Bool SHFile_writeJson(
	const SHFile *file, SHDisassemble disassemble, void *disassembleCtx, JsonWriter *w, const Allocator *alloc,
	Error *e_rr
) {
	return
		JsonWriter_beginObject(w, e_rr) &&
		SHFile_writeJsonMembers(file, disassemble, disassembleCtx, w, alloc, e_rr) &&
		JsonWriter_endObject(w, e_rr);
}
