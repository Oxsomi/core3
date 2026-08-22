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

//tools/oxc3_cli/isa.c

#include "tools/oxc3_cli/cli.h"

#ifdef CLI_RGA

#include "platforms/process.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "platforms/logx.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/memory_stream.h"
#include "types/container/list_impl.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_read.h"
#include "types/base/error.h"
#include "types/base/time.h"
#include "types/base/constants.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSH/sh_entries.h"
#include "formats/oiSB/sb_variable.h"
#include "graphics/generic/pipeline_serialize.h"
#include "shader_compiler/spirv_isa.h"
#include "shader_compiler/compiler.h"

#ifdef CLI_GRAPHICS
	#include "graphics/generic/instance.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/pipeline_layout.h"
	#include "graphics/generic/descriptor_layout.h"
	#include "graphics/generic/device_info.h"
#endif

//Prints the AMD gfx targets the bundled amdllpc can actually compile for, probed live so the list matches what this
// build accepts.
//Used by 'isa devices', the '?' shorthand, and as a hint after an unknown -asic.
//amdllpc + amdgpu-dis do the disassembly directly, and amdllpc reports its own target set.

static Bool CLI_isaPrintDevices(const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	ListCharString targets = (ListCharString) { 0 };

	gotoIfError3(clean, SpvISA_listSupportedTargets(alloc, &targets, e_rr));

	Log_debugLnx("Offline ISA targets (amdllpc-supported; pass one as -asic):");

	for(U64 i = 0; i < targets.length; ++i)
		Log_debugLnx("\t%.*s", (int) CharString_length(targets.ptr[i]), targets.ptr[i].ptr);

clean:
	ListCharString_freeUnderlying(&targets, alloc);
	return s_uccess;
}

Bool CLI_isaDevices(const ParsedArgs *args) {

	(void) args;

	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;
	const Allocator *alloc = Platform_instance->alloc;

	gotoIfError3(clean, CLI_isaPrintDevices(alloc, e_rr));

clean:
	if(!s_uccess)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	return s_uccess;
}

//Resolves an -asic for any ISA path.
//"?" prints the device list and sets *handled so the caller stops (no error).
//Any other value is passed through to the offline disassembler (amdllpc), which accepts a gfx target or a
// major.minor.step form (e.g. "gfx1100" or "11.0.0"), so *handled stays false and the caller proceeds.
//A genuinely unknown target is reported by amdllpc when it runs.

Bool CLI_isaResolveAsic(CharString asic, Bool *handled, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(handled)
		*handled = false;

	if(!CharString_length(asic))
		retError(clean, Error_invalidParameter(0, 0, "CLI_isaResolveAsic()::asic is required (see 'OxC3 isa devices')"));

	const CharString q = CharString_createRefCStrConst("?");

	if(CharString_equalsStringInsensitive(&asic, &q)) {
		gotoIfError3(clean, CLI_isaPrintDevices(alloc, e_rr));
		if(handled)
			*handled = true;
	}

clean:
	return s_uccess;
}

//Disassembles a SPIR-V module to AMD ISA text for `asic`, returning the ISA in `isaOut` (caller frees).
//The actual amdllpc + amdgpu-dis driving lives in the shared SpvISA_ module, so this and the corpus ISA snapshot test
// produce identical output; here we just reject stages with no offline path early, with a clearer error than amdllpc's.

Bool CLI_isaDisassembleSpirv(
	Buffer spirv, CharString asic, CharString entrypoint, Buffer *isaOut, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	if(!SpvISA_stageHasOfflinePath(spirv, alloc))
		retError(clean, Error_unsupportedOperation(
			0, "CLI_isaDisassembleSpirv() stage has no offline ISA path (only vertex/hull/domain/geometry/pixel/compute)"
		));

	gotoIfError3(clean, SpvISA_disassemble(spirv, asic, entrypoint, isaOut, alloc, e_rr));

clean:
	return s_uccess;
}

#ifdef CLI_GRAPHICS

	//Append a pipeline's executables to `out`: the driver's numeric statistics (VGPRs/SGPRs/...) then the ISA text.
	//The same text is used for the console and for -output, so both paths look identical.

	static Bool CLI_isaAppendExecutables(
		CharString *out, const ListPipelineExecutable *execs, const Allocator *alloc, Error *e_rr
	) {

		Bool s_uccess = true;
		CharString line = CharString_createNull();

		if(!execs->length) {

			const CharString none = CharString_createRefCStrConst(
				";   (the driver returned no executables for this pipeline; some drivers expose none for certain "
				"pipeline types, e.g. ray tracing on NVIDIA)\n"
			);

			return CharString_appendString(out, &none, alloc, e_rr);
		}

		const CharString noIsa = CharString_createRefCStrConst(
			";   (this driver returned statistics only, no ISA disassembly. Whether the ISA text is exposed is "
			"driver-dependent: the open-source Mesa drivers (RADV / ANV / Turnip / NVK / PanVK, i.e. Linux) and "
			"AMD's own driver return it; other vendors' proprietary drivers keep it to their own tooling. For "
			"deterministic offline AMD ISA use '-asic <gfxN>' instead of live)\n"
		);

		for(U64 i = 0; i < execs->length; ++i) {

			const PipelineExecutable *e = &execs->ptr[i];

			CharString_free(&line, alloc);
			gotoIfError3(clean, CharString_format(
				alloc, &line, e_rr, "; %.*s  subgroup %"PRIu32"\n",
				(int) CharString_length(e->name), e->name.ptr, e->subgroupSize
			));
			gotoIfError3(clean, CharString_appendString(out, &line, alloc, e_rr));

			for(U64 j = 0; j < e->statistics.length; ++j) {

				const PipelineStatistic *s = &e->statistics.ptr[j];
				const int nl = (int) CharString_length(s->name);

				CharString_free(&line, alloc);

				switch(s->format) {

					case EPipelineStatisticFormat_Bool:
						gotoIfError3(clean, CharString_format(
							alloc, &line, e_rr, ";   %.*s = %s\n", nl, s->name.ptr, s->value ? "true" : "false"
						));
						break;

					case EPipelineStatisticFormat_I64:
						gotoIfError3(clean, CharString_format(
							alloc, &line, e_rr, ";   %.*s = %"PRIi64"\n", nl, s->name.ptr, (I64) s->value
						));
						break;

					case EPipelineStatisticFormat_F64: {
						F64 d = *(const F64*) &s->value;
						gotoIfError3(clean, CharString_format(alloc, &line, e_rr, ";   %.*s = %f\n", nl, s->name.ptr, d));
						break;
					}

					default:
						gotoIfError3(clean, CharString_format(
							alloc, &line, e_rr, ";   %.*s = %"PRIu64"\n", nl, s->name.ptr, s->value
						));
						break;
				}

				gotoIfError3(clean, CharString_appendString(out, &line, alloc, e_rr));
			}

			if(CharString_length(e->disassembly)) {
				gotoIfError3(clean, CharString_appendString(out, &e->disassembly, alloc, e_rr));
				gotoIfError3(clean, CharString_append(out, '\n', alloc, e_rr));
			}

			else gotoIfError3(clean, CharString_appendString(out, &noIsa, alloc, e_rr));
		}

	clean:
		CharString_free(&line, alloc);
		return s_uccess;
	}

	//First SHEntry with the given pipeline stage, or U64_MAX if none.

	//A vertex input's ESBType (SHEntry.inputs[]) -> the matching vertex-buffer ETextureFormatId, or Undefined if none.
	//Vertex inputs are scalars/vectors; HLSL float/uint/int are 32-bit and half is 16-bit, so 8/64-bit have no format.
	//16-bit has no 3-component format, so a 3-component 16-bit input falls back to the 4-component (RGBA) one.

	static ETextureFormatId CLI_esbTypeToVertexFormat(U8 esbType) {

		if(!esbType)
			return ETextureFormatId_Undefined;

		const ESBStride stride = ESBType_getStride(esbType);

		if(stride != ESBStride_X16 && stride != ESBStride_X32)
			return ETextureFormatId_Undefined;

		const U8 comp = (U8) ESBType_getVector(esbType);        //0..3 = 1..4 components

		static const ETextureFormatId f32[4] =
			{ ETextureFormatId_R32f, ETextureFormatId_RG32f, ETextureFormatId_RGB32f, ETextureFormatId_RGBA32f };
		static const ETextureFormatId u32[4] =
			{ ETextureFormatId_R32u, ETextureFormatId_RG32u, ETextureFormatId_RGB32u, ETextureFormatId_RGBA32u };
		static const ETextureFormatId i32[4] =
			{ ETextureFormatId_R32i, ETextureFormatId_RG32i, ETextureFormatId_RGB32i, ETextureFormatId_RGBA32i };
		static const ETextureFormatId f16[4] =
			{ ETextureFormatId_R16f, ETextureFormatId_RG16f, ETextureFormatId_RGBA16f, ETextureFormatId_RGBA16f };
		static const ETextureFormatId u16[4] =
			{ ETextureFormatId_R16u, ETextureFormatId_RG16u, ETextureFormatId_RGBA16u, ETextureFormatId_RGBA16u };
		static const ETextureFormatId i16[4] =
			{ ETextureFormatId_R16i, ETextureFormatId_RG16i, ETextureFormatId_RGBA16i, ETextureFormatId_RGBA16i };

		const Bool is16 = stride == ESBStride_X16;

		switch(ESBType_getPrimitive(esbType)) {
			case ESBPrimitive_Float: return (is16 ? f16 : f32)[comp];
			case ESBPrimitive_UInt:  return (is16 ? u16 : u32)[comp];
			case ESBPrimitive_Int:   return (is16 ? i16 : i32)[comp];
			default:                 return ETextureFormatId_Undefined;
		}
	}

	//A pixel output's ESBType (SHEntry.outputs[]) -> a standard render target format matching its primitive.
	//Float outputs take the common 8-bit unorm color target; integer outputs keep full precision.
	//The pixel shader writes a prefix of the 4 channels, so a fixed RGBA target is always compatible.

	static ETextureFormatId CLI_esbTypeToRenderTarget(U8 esbType) {

		if(!esbType)
			return ETextureFormatId_Undefined;

		switch(ESBType_getPrimitive(esbType)) {
			case ESBPrimitive_Float: return ETextureFormatId_RGBA8;
			case ESBPrimitive_UInt:  return ETextureFormatId_RGBA32u;
			case ESBPrimitive_Int:   return ETextureFormatId_RGBA32i;
			default:                 return ETextureFormatId_Undefined;
		}
	}

	//A texture format name (e.g. "rgba16f", case-insensitive) -> its ETextureFormatId, or Undefined if unknown.
	//Used to parse the -rtv override.

	static ETextureFormatId CLI_parseTextureFormat(CharString name) {

		for(U64 i = 0; i < ETextureFormatId_Count; ++i) {

			const CharString candidate = CharString_createRefCStrConst(ETextureFormatId_name[i]);

			if(CharString_equalsStringInsensitive(&name, &candidate))
				return (ETextureFormatId) i;
		}

		return ETextureFormatId_Undefined;
	}

	//Live ISA: create a real Vulkan device, compile a pipeline (compute, graphics or ray tracing) with ISA capture,
	// then read back the driver's disassembly + statistics.
	//This is the REAL driver ISA, device and driver dependent so it isn't golden-pinnable, complementing the offline
	// amdllpc goldens.

	//A graphics pipeline is a chain (vertex, optional tessellation, optional geometry, optional pixel), and only some
	// links of it have to exist for the shader to be worth inspecting.
	//Missing ends of the chain are generated so the stages the shader really declares can be compiled and measured;
	// every generated stage is disclosed, since its own disassembly means nothing.
	//Two hazards shape how the stand-ins are written, both of them the same problem from opposite ends: a driver may
	// optimize across a stage boundary, so a stand-in vertex stage reads its values from the app data buffer rather
	// than using literals (which would be folded into the pixel stage), and a stand-in pixel stage consumes every
	// input it receives (otherwise the preceding stage's output writes are dead and get eliminated).

	//Emits one struct member per signature entry, matching type and semantic so the interfaces link.
	//Integer varyings can't be interpolated, so those are flat.

	static Bool CLI_isaStandInMembers(
		const SHEntry *entry, const U8 *types, CharString *out, const Allocator *alloc, Error *e_rr
	) {

		Bool s_uccess = true;
		CharString line = CharString_createNull();

		const Bool hasSemantics =
			entry->inputSemanticNamesU64[0] | entry->inputSemanticNamesU64[1] |
			entry->outputSemanticNamesU64[0] | entry->outputSemanticNamesU64[1];

		const Bool isOutput = types == entry->outputs;
		const U64 semanticOff = isOutput ? entry->uniqueInputSemantics : 0;

		for (U8 i = 0; i < 16; ++i) {

			const ESBType type = (ESBType) types[i];

			if(!type)
				continue;

			const U8 semanticValue = isOutput ? entry->outputSemanticNames[i] : entry->inputSemanticNames[i];
			const U64 semanticNameId = semanticValue >> 4;

			const CharString semantic =
				semanticNameId ?
				entry->semanticNames.ptr[semanticNameId - 1 + semanticOff] : CharString_createRefCStrConst("TEXCOORD");

			const ESBPrimitive prim = ESBType_getPrimitive(type);
			const Bool isInteger = prim == ESBPrimitive_Int || prim == ESBPrimitive_UInt;

			CharString_free(&line, alloc);
			gotoIfError3(clean, CharString_format(
				alloc, &line, e_rr, "\t%s%s _v%"PRIu8" : %.*s%"PRIu8";\n",
				isInteger ? "nointerpolation " : "",
				ESBType_name(type),
				i,
				(int) CharString_length(semantic), semantic.ptr,
				hasSemantics ? (U8) (semanticValue & 0xF) : i
			));

			gotoIfError3(clean, CharString_appendString(out, &line, alloc, e_rr));
		}

	clean:
		CharString_free(&line, alloc);
		return s_uccess;
	}

	//Builds the HLSL for whichever stand-ins the chain is missing.
	//vsTarget is the stage a generated vertex stage has to feed (its inputs become the vertex outputs) and psSource is
	// the stage a generated pixel stage receives from (its outputs become the pixel inputs); either may be NULL.

	static Bool CLI_isaStandInSource(
		const SHEntry *vsTarget, const SHEntry *psSource, CharString *out, const Allocator *alloc, Error *e_rr
	) {

		Bool s_uccess = true;
		CharString line = CharString_createNull();

		const CharString head = CharString_createRefCStrConst(
			"#include \"@types.hlsli\"\n"
			"#include \"@appdata.hlsli\"\n"
			"\n"
			"//Generated stand-ins for stages this shader doesn't declare; only the shader's own stages are meaningful.\n"
			"\n"
		);

		gotoIfError3(clean, CharString_appendString(out, &head, alloc, e_rr));

		if (vsTarget) {

			const CharString vsHead = CharString_createRefCStrConst(
				"struct StandInVertexOut {\n"
				"\tF32x4 _position : SV_POSITION;\n"
			);

			gotoIfError3(clean, CharString_appendString(out, &vsHead, alloc, e_rr));
			gotoIfError3(clean, CLI_isaStandInMembers(vsTarget, vsTarget->inputs, out, alloc, e_rr));

			const CharString vsMid = CharString_createRefCStrConst(
				"};\n"
				"\n"
				"[shader(\"vertex\")]\n"
				"StandInVertexOut main(U32 _vertexId : SV_VertexID) {\n"
				"\n"
				"\tStandInVertexOut o;\n"
				"\to._position = getAppData4f(0);\n"
			);

			gotoIfError3(clean, CharString_appendString(out, &vsMid, alloc, e_rr));

			for (U8 i = 0; i < 16; ++i) {

				const ESBType type = (ESBType) vsTarget->inputs[i];

				if(!type)
					continue;

				//The values come from the app data buffer so they aren't compile time constants.

				const C8 *getter = "getAppData4f";

				switch(ESBType_getPrimitive(type)) {
					case ESBPrimitive_Int:   getter = "getAppData4i";  break;
					case ESBPrimitive_UInt:  getter = "getAppData4u";  break;
					default:                                           break;
				}

				static const C8 *swizzles[4] = { "x", "xy", "xyz", "xyzw" };
				const U8 comp = (U8) ESBType_getVector(type);

				CharString_free(&line, alloc);
				gotoIfError3(clean, CharString_format(
					alloc, &line, e_rr, "\to._v%"PRIu8" = (%s) %s(%"PRIu8").%s;\n",
					i, ESBType_name(type), getter, (U8) ((i + 1) * 4), swizzles[comp & 3]
				));

				gotoIfError3(clean, CharString_appendString(out, &line, alloc, e_rr));
			}

			const CharString vsTail = CharString_createRefCStrConst("\n\treturn o;\n}\n\n");
			gotoIfError3(clean, CharString_appendString(out, &vsTail, alloc, e_rr));
		}

		if (psSource) {

			const CharString psHead = CharString_createRefCStrConst(
				"struct StandInPixelIn {\n"
				"\tF32x4 _position : SV_POSITION;\n"
			);

			gotoIfError3(clean, CharString_appendString(out, &psHead, alloc, e_rr));
			gotoIfError3(clean, CLI_isaStandInMembers(psSource, psSource->outputs, out, alloc, e_rr));

			const CharString psMid = CharString_createRefCStrConst(
				"};\n"
				"\n"
				"[shader(\"pixel\")]\n"
				"F32x4 mainStandInPixel(StandInPixelIn i) : SV_TARGET {\n"
				"\n"
				"\tF32x4 acc = i._position;\n"
			);

			gotoIfError3(clean, CharString_appendString(out, &psMid, alloc, e_rr));

			//Every input is consumed, so the stage feeding this one keeps its output writes instead of having them
			// eliminated as dead.

			for (U8 i = 0; i < 16; ++i) {

				if(!psSource->outputs[i])
					continue;

				CharString_free(&line, alloc);
				gotoIfError3(clean, CharString_format(alloc, &line, e_rr, "\tacc.x += (F32) i._v%"PRIu8".x;\n", i));
				gotoIfError3(clean, CharString_appendString(out, &line, alloc, e_rr));
			}

			const CharString psTail = CharString_createRefCStrConst("\n\treturn acc;\n}\n");
			gotoIfError3(clean, CharString_appendString(out, &psTail, alloc, e_rr));
		}

	clean:
		CharString_free(&line, alloc);
		return s_uccess;
	}

	//Compiles the stand-ins to an oiSH entirely in memory, so nothing is written next to the user's files.

	static Bool CLI_isaCompileStandIns(
		const SHEntry *vsTarget, const SHEntry *psSource, SHFile *out, const Allocator *alloc, Error *e_rr
	) {

		Bool s_uccess = true;

		CharString source = CharString_createNull();
		ListCharString files = (ListCharString) { 0 };
		ListCharString texts = (ListCharString) { 0 };
		ListCharString outputs = (ListCharString) { 0 };
		ListCharString includeDirs = (ListCharString) { 0 };
		ListU8 modes = (ListU8) { 0 };
		ListBuffer buffers = (ListBuffer) { 0 };
		MemoryStreamRef *stream = NULL;

		gotoIfError3(clean, CLI_isaStandInSource(vsTarget, psSource, &source, alloc, e_rr));

		gotoIfError3(clean, ListCharString_pushBack(
			&files, CharString_createRefCStrConst("oxc3_isa_stand_ins.hlsl"), alloc, e_rr
		));

		gotoIfError3(clean, ListCharString_pushBack(&texts, source, alloc, e_rr));

		gotoIfError3(clean, ListCharString_pushBack(
			&outputs, CharString_createRefCStrConst("oxc3_isa_stand_ins.oiSH"), alloc, e_rr
		));

		gotoIfError3(clean, ListU8_pushBack(&modes, (U8) ESHBinaryType_SPIRV, alloc, e_rr));

		gotoIfError3(clean, Compiler_compileShaders(
			&files, &texts, &outputs, &modes,
			1,                                  //threadCount: one tiny shader
			false,                              //isDebug
			false,                              //keepRegisters
			(ECompilerWarning) 0,
			false,                              //ignoreEmptyFiles
			ECompileType_Compile,
			&includeDirs,
			false,                              //enableLogging: its own diagnostics would only confuse the ISA output
			alloc,
			&buffers,
			e_rr
		));

		if(!buffers.length || !Buffer_length(buffers.ptr[0]))
			retError(clean, Error_invalidState(
				0, "CLI_isaCompileStandIns() the generated stand-ins produced no binary"
			));

		//Read it straight back out of the buffer the compiler produced.

		const RefPtrType streamType = MemoryStream_makeType(alloc);
		U64 offset = 0;

		gotoIfError3(clean, MemoryStream_createFromBufferRegion(
			Buffer_createRefFromBuffer(buffers.ptr[0], true), 0, Buffer_length(buffers.ptr[0]),
			EMemoryStreamFlags_None, &streamType, &stream, e_rr
		));

		gotoIfError3(clean, SHFile_read((StreamRef*) stream, &offset, false, alloc, out, e_rr));

	clean:
		RefPtr_dec(&stream);
		ListBuffer_freeUnderlying(&buffers, alloc);
		ListU8_free(&modes, alloc);
		ListCharString_free(&includeDirs, alloc);
		ListCharString_freeUnderlying(&outputs, alloc);
		ListCharString_free(&texts, alloc);                 //Holds a ref to source
		ListCharString_freeUnderlying(&files, alloc);
		CharString_free(&source, alloc);
		return s_uccess;
	}

	static Bool CLI_isaDisassembleLive(
		SHFile shFile, U64 deviceId, Bool hasOutput, CharString outputStr, const RefPtrType *fileHandleType,
		const U8 *overrideRtv, U8 overrideRtvCount, Bool hasRecursionDepth, U8 recursionDepth, Bool assumeDefaults,
		Bool hasPipelineOutput, CharString pipelineOutputStr,
		CharString shaderName,
		const Allocator *alloc, Error *e_rr
	) {

		Bool s_uccess = true;

		RefPtrType instanceType = (RefPtrType) { 0 };
		GraphicsInstanceRef *instanceRef = NULL;
		GraphicsDeviceRef *deviceRef = NULL;
		ListGraphicsDeviceInfo infos = (ListGraphicsDeviceInfo) { 0 };
		PipelineRef *pipeline = NULL;
		ListPipelineExecutable execs = (ListPipelineExecutable) { 0 };
		CharString text = CharString_createNull();
		CharString stageLine = CharString_createNull();
		SPFile spFile = (SPFile) { 0 };
		StreamRef *pipelineStream = NULL;
		const RefPtrType memStreamType = MemoryStream_makeType(alloc);
		U32 pipelineId = U32_MAX;
		ListCharString issues = (ListCharString) { 0 };
		SHFile standIns = (SHFile) { 0 };
		SHFile standInFiles[2];

		//Load the graphics backend DLLs + register their function tables before touching any graphics object

		Error gErr = Error_none();

		if(!GraphicsInterface_create(&gErr))
			retError(clean, Error_invalidState(
				0, "CLI_isaDisassembleLive() couldn't create a graphics interface (no driver/ICD?)"
			));

		if(!GraphicsInterface_supportsApi(EGraphicsApi_Vulkan))
			retError(clean, Error_unsupportedOperation(0, "CLI_isaDisassembleLive() Vulkan isn't available on this machine"));

		instanceType = GraphicsInstance_makeType(EGraphicsApi_Vulkan, alloc);

		const GraphicsApplicationInfo appInfo = {
			.name = CharString_createRefCStrConst("OxC3 CLI isa live"),
			.version = OXC3_MAKE_VERSION(OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH)
		};

		gotoIfError3(clean, GraphicsInstance_create(
			&appInfo, EGraphicsApi_Vulkan, EGraphicsInstanceFlags_None, alloc, &instanceType, &instanceRef, e_rr
		));

		gotoIfError3(clean, GraphicsInstance_getDeviceInfos(GraphicsInstanceRef_ptr(instanceRef), &infos, e_rr));

		if(!infos.length)
			retError(clean, Error_invalidState(0, "CLI_isaDisassembleLive() no Vulkan devices found"));

		//List the Vulkan devices so the user can pick one with -asic live:<index> (multi-GPU / multi-vendor machines)

		Log_debugLnx("Vulkan devices (select with -asic live:<index>):");

		for(U64 i = 0; i < infos.length; ++i)
			Log_debugLnx("\t%"PRIu64": %s", i, infos.ptr[i].name);

		if(deviceId >= infos.length)
			retError(clean, Error_outOfBounds(
				0, deviceId, infos.length, "CLI_isaDisassembleLive() -asic live:<index> device index out of range"
			));

		gotoIfError3(clean, GraphicsDeviceRef_create(
			instanceRef, &infos.ptr[deviceId], EGraphicsDeviceFlags_None, EGraphicsBufferingMode_Default,
			NULL, &deviceRef, e_rr
		));

		if(!(GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features2 & EGraphicsFeatures2_PipelineExecutableInfo))
			retError(clean, Error_unsupportedOperation(
				0, "CLI_isaDisassembleLive() device lacks VK_KHR_pipeline_executable_properties"
			));

		//A NULL layout takes the device's default bindless layout (@resources.hlsli), which is what OxC3 compiles
		// shaders against, so the pipeline is valid as-is; a per-shader detected layout would instead omit the bindless
		// set and fail validation, so NULL is the correct choice here.

		const CharString pName = CharString_createRefCStrConst("isa live pipeline");

		ListSHFile fileList = (ListSHFile) { 0 };
		ListSHFile_createRefConst(&shFile, 1, &fileList, NULL);

		CharString entryName = CharString_createNull();
		U8 boundStageCount = 0;                //Reported, since a dropped stage would otherwise be invisible

		//Pick the stages that form ONE pipelineState.
		//A file may hold compute, graphics and ray tracing stages side by side; those are separate pipelines, so only
		// the first kind present is taken (compute, then graphics, then ray tracing) and the rest is reported rather
		// than silently folded in or silently dropped.
		//Within a kind every stage of the chain is taken, and a second entry of the same stage kind is refused, since
		// guessing which of two vertex shaders was meant would be another silent choice.

		SPStageRef stageRefs[16];
		U8 stageRefCount = 0;
		U64 kindCounts[3] = { 0, 0, 0 };        //compute, graphics, ray tracing entries in the file
		U64 computeE = U64_MAX, vertexE = U64_MAX, pixelE = U64_MAX, hullE = U64_MAX, domainE = U64_MAX, geomE = U64_MAX;

		for (U64 i = 0; i < shFile.entries.length; ++i) {

			const U8 stage = shFile.entries.ptr[i].stage;

			if(stage == ESHPipelineStage_Compute)
				++kindCounts[0];

			else if(
				stage == ESHPipelineStage_Vertex || stage == ESHPipelineStage_Pixel || stage == ESHPipelineStage_Hull ||
				stage == ESHPipelineStage_Domain || stage == ESHPipelineStage_GeometryExt
			)
				++kindCounts[1];

			else if(stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt)
				++kindCounts[2];
		}

		const U8 chosenKind = kindCounts[0] ? 0 : kindCounts[1] ? 1 : kindCounts[2] ? 2 : 3;

		if(chosenKind == 3)
			retError(clean, Error_unsupportedOperation(
				0, "CLI_isaDisassembleLive() needs a compute stage, a graphics stage or a ray tracing stage"
			));

		if((kindCounts[0] != 0) + (kindCounts[1] != 0) + (kindCounts[2] != 0) > 1) {

			static const C8 *kindNames[3] = { "compute", "graphics", "ray tracing" };

			Log_warnLnx(
				"This file holds %s, %s and %s stages, which are separate pipelines; only the %s one is compiled. "
				"Use -entry to pick another.",
				kindCounts[0] ? "compute" : "no compute", kindCounts[1] ? "graphics" : "no graphics",
				kindCounts[2] ? "ray tracing" : "no ray tracing", kindNames[chosenKind]
			);
		}

		for (U64 i = 0; i < shFile.entries.length; ++i) {

			const U8 stage = shFile.entries.ptr[i].stage;
			U64 *slot = NULL;

			switch (stage) {
				case ESHPipelineStage_Compute:      if(chosenKind == 0) slot = &computeE;  break;
				case ESHPipelineStage_Vertex:       if(chosenKind == 1) slot = &vertexE;   break;
				case ESHPipelineStage_Pixel:        if(chosenKind == 1) slot = &pixelE;    break;
				case ESHPipelineStage_Hull:         if(chosenKind == 1) slot = &hullE;     break;
				case ESHPipelineStage_Domain:       if(chosenKind == 1) slot = &domainE;   break;
				case ESHPipelineStage_GeometryExt:  if(chosenKind == 1) slot = &geomE;     break;
				default:                                                                   break;
			}

			const Bool isRt = stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt;

			if (slot) {

				if(*slot != U64_MAX)
					retError(clean, Error_invalidState(
						0,
						"CLI_isaDisassembleLive() this file has two entries of the same stage kind; use -entry to "
						"pick which one forms the pipeline"
					));

				*slot = i;
			}

			else if(!isRt || chosenKind != 2)
				continue;

			if(stageRefCount < 16)
				stageRefs[stageRefCount++] = (SPStageRef) { .fileId = 0, .entryId = (U16) i };
		}

		//The descriptor is built from exactly those stages, so what a disassembly was compiled against is always
		// explicit and the same derivation can serve the offline backend.
		//Compute derives exactly, ray tracing leaves only its own limits, and graphics needs state that no shader
		// signature carries, which is refused rather than invented unless -assume-defaults says otherwise.

		gotoIfError3(clean, SPFile_create(ESPSettingsFlags_None, alloc, &spFile, e_rr));

		//The shader is named in the pipeline, so a stored one can be resolved back to the oiSH it came from.
		//The list is parallel to fileList, so any generated stand-in file after it stays unnamed.

		ListCharString shaderNames = (ListCharString) { 0 };
		gotoIfError3(clean, ListCharString_createRefConst(&shaderName, 1, &shaderNames, e_rr));

		gotoIfError3(clean, SPFile_derivePipeline(
			&spFile, &fileList, &shaderNames, CharString_createNull(), stageRefs, stageRefCount, alloc, &pipelineId, e_rr
		));

		//An override is a supplied value, so it drops out of the specialization report.

		for(U8 i = 0; i < overrideRtvCount; ++i)
			gotoIfError3(clean, SPFile_supply(
				&spFile, pipelineId, ESPField_RenderTargetFormat, i, overrideRtv[i], e_rr
			));

		//-recursion-depth is the one ray tracing field with no sensible default; the flags default to none, which
		// is what a lib without special skip/null rules means, so supplying the depth makes the state exact.

		if (hasRecursionDepth) {
			gotoIfError3(clean, SPFile_supply(&spFile, pipelineId, ESPField_MaxRecursionDepth, 0, recursionDepth, e_rr));
			gotoIfError3(clean, SPFile_supply(&spFile, pipelineId, ESPField_RaytracingFlags, 0, 0, e_rr));
		}

		//Structural validation runs before the driver sees the pipeline, so a mismatch names itself instead of
		// surfacing as an opaque driver error.

		gotoIfError3(clean, SPFile_validate(&spFile, pipelineId, &fileList, stageRefs, alloc, &issues, e_rr));

		if (issues.length) {

			Log_errorLnx("The pipeline state doesn't match the shader:");

			for(U64 i = 0; i < issues.length; ++i)
				Log_errorLnx("\t%.*s", (int) CharString_length(issues.ptr[i]), issues.ptr[i].ptr);

			retError(clean, Error_invalidState(
				0, "CLI_isaDisassembleLive() the pipeline state is invalid for this shader"
			));
		}

		//Inventing state produces a real but unrelated disassembly, which is worse than no answer, so the fields that
		// have to be specialized are listed instead.

		if (!assumeDefaults && !SPFile_isExact(&spFile, pipelineId)) {

			Log_errorLnx("This shader needs pipeline state that no shader signature carries:");

			const SPPipelineBase reported = spFile.pipelines.ptr[pipelineId];

			for (U32 i = 0; i < reported.specializationCount; ++i) {

				const SPSpecialization *spec = &spFile.specializations.ptr[reported.specializationStart + i];
				const C8 *name = ESPField_name((ESPField) spec->field);
				const C8 *reason = ESPField_reason((ESPField) spec->field);
				const C8 *domain = ESPField_domain((ESPField) spec->field);

				if(spec->source != ESPFieldSource_Assumed)
					continue;

				if(ESPField_isIndexed((ESPField) spec->field))
					Log_errorLnx("\t%s[%"PRIu32"]: %s (legal: %s)", name, (U32) spec->index, reason, domain);

				else Log_errorLnx("\t%s: %s (legal: %s)", name, reason, domain);
			}

			retError(clean, Error_invalidState(
				1, "CLI_isaDisassembleLive() pipeline state is missing; supply it or pass -assume-defaults"
			));
		}

		//The record is copied once the overrides are in, so the lowering below reads final values.

		const SPPipelineBase pipelineBase = spFile.pipelines.ptr[pipelineId];
		const SPGraphicsState *gfxState = SPFile_graphicsState(&spFile, pipelineId);
		const SPRaytracingState *rtState = SPFile_raytracingState(&spFile, pipelineId);

		if(pipelineBase.type == (U8) ESPPipelineType_Compute) {

			//Compute: a single entry.
			//The SPIRV entrypoint is passed as NULL, which resolves to "main": OxC3 normalizes a single-entry SPIR-V
			// module's entrypoint to "main" regardless of the HLSL name (an SHEntry named "mainCompute" is "main").

			entryName = shFile.entries.ptr[computeE].name;

			const U32 entry = GraphicsDeviceRef_getFirstShaderEntry(
				deviceRef, &shFile, &entryName, NULL, NULL, ESHExtension_None, ESHExtension_None
			);

			if(entry == U32_MAX)
				retError(clean, Error_invalidState(0, "CLI_isaDisassembleLive() couldn't resolve the compute entry"));

			gotoIfError3(clean, GraphicsDeviceRef_createPipelineCompute(
				deviceRef, &shFile, &pName, entry, NULL, EPipelineFlags_CaptureISA, NULL, &pipeline, e_rr
			));
		}

		else if (
			vertexE != U64_MAX || pixelE != U64_MAX || hullE != U64_MAX || domainE != U64_MAX || geomE != U64_MAX
		) {

			//Bind every graphics stage the shader declares, in chain order, and generate only the ends it's missing.
			//Silently dropping a declared stage would report a pipeline the shader never described.

			const U64 chainEntry[5] = { vertexE, hullE, domainE, geomE, pixelE };

			static const ESHPipelineStage chainStage[5] = {
				ESHPipelineStage_Vertex, ESHPipelineStage_Hull, ESHPipelineStage_Domain,
				ESHPipelineStage_GeometryExt, ESHPipelineStage_Pixel
			};

			//Tessellation can't be expressed here yet.
			//Vulkan requires a patch list topology whenever tessellation stages are present, and ETopologyMode has no
			// patch list, so the pipeline would violate VUID-VkGraphicsPipelineCreateInfo-pStages-08888 (and
			// patchControlPoints-01214 for the control point count).
			//A driver may still accept it and hand back statistics, which is exactly the kind of authoritative looking
			// but invalid number this command exists to avoid, so it's refused instead.

			if(hullE != U64_MAX || domainE != U64_MAX)
				retError(clean, Error_unsupportedOperation(
					1,
					"CLI_isaDisassembleLive() tessellation isn't supported yet: a patch list topology is required and "
					"ETopologyMode has no patch list, so the pipeline would be invalid"
				));

			//A generated vertex stage has to feed whichever stage comes first, and a generated pixel stage receives
			// from whichever comes last.

			const SHEntry *vsTarget = NULL, *psSource = NULL;

			if (vertexE == U64_MAX)
				for(U8 i = 1; i < 5; ++i)
					if (chainEntry[i] != U64_MAX) {
						vsTarget = &shFile.entries.ptr[chainEntry[i]];
						break;
					}

			if (pixelE == U64_MAX)
				for(U8 i = 4; i > 0; --i)
					if (chainEntry[i - 1] != U64_MAX) {
						psSource = &shFile.entries.ptr[chainEntry[i - 1]];
						break;
					}

			if (vsTarget || psSource) {

				gotoIfError3(clean, CLI_isaCompileStandIns(vsTarget, psSource, &standIns, alloc, e_rr));

				if(!standIns.entries.length)
					retError(clean, Error_invalidState(
						1, "CLI_isaDisassembleLive() the generated stand-ins have no entrypoint"
					));

				standInFiles[0] = shFile;
				standInFiles[1] = standIns;

				//The list already refers to the single input file, and a ref list won't be re-pointed in place.

				fileList = (ListSHFile) { 0 };
				gotoIfError3(clean, ListSHFile_createRefConst(standInFiles, 2, &fileList, e_rr));

				if(vsTarget)
					spFile.pipelines.ptrNonConst[pipelineId].flags |= ESPPipelineFlag_GeneratedVertexStage;

				if(psSource)
					spFile.pipelines.ptrNonConst[pipelineId].flags |= ESPPipelineFlag_GeneratedPixelStage;
			}

			//Resolve each stage in chain order, taking generated ones from the second file.

			PipelineStage stages[5];
			U8 stageCount = 0;

			for (U8 i = 0; i < 5; ++i) {

				const Bool generated =
					(i == 0 && vsTarget) ||
					(i == 4 && psSource);

				if(chainEntry[i] == U64_MAX && !generated)
					continue;

				const SHFile *from = generated ? &standIns : &shFile;
				CharString name = CharString_createNull();

				if (generated) {

					for(U64 j = 0; j < standIns.entries.length; ++j)
						if (standIns.entries.ptr[j].stage == chainStage[i]) {
							name = standIns.entries.ptr[j].name;
							break;
						}

					if(!CharString_length(name))
						retError(clean, Error_invalidState(
							2, "CLI_isaDisassembleLive() a generated stand-in is missing the stage it was asked for"
						));
				}

				else name = shFile.entries.ptr[chainEntry[i]].name;

				const U32 id = GraphicsDeviceRef_getFirstShaderEntry(
					deviceRef, from, &name, NULL, NULL, ESHExtension_None, ESHExtension_None
				);

				//A stage that resolves to nothing usually failed the device's own feature check, which prints its
				// reason just above this; the common one is a shader built against its own bindless layout, since the
				// pipeline is created with the device's default one.

				if(id == U32_MAX)
					retError(clean, Error_invalidState(
						0,
						"CLI_isaDisassembleLive() no compatible binary for a graphics stage (see the reason above; "
						"a custom descriptor layout can't be supplied yet)"
					));

				stages[stageCount++] = (PipelineStage) { .binaryId = id, .shFileId = generated ? 1 : 0 };

				//The reported entry name is the shader's own stage, not a generated one.

				if(!generated && !CharString_length(entryName))
					entryName = name;
			}

			boundStageCount = stageCount;

			ListPipelineStage stageList = (ListPipelineStage) { 0 };
			ListPipelineStage_createRefConst(stages, stageCount, &stageList, NULL);

			PipelineGraphicsInfo info = (PipelineGraphicsInfo) { 0 };

			//The descriptor already holds every field, so this is the same lowering a loaded oiSP goes through.

			gotoIfError3(clean, SPFile_toGraphicsInfo(&spFile, pipelineId, &info, e_rr));

			//A pixel shader writing nothing still needs one attachment for the pipeline to create.

			if (!gfxState->renderTargetCount) {
				info.attachmentFormatsExt[0] = (U8) ETextureFormatId_RGBA8;
				info.attachmentCountExt = 1;
			}

			gotoIfError3(clean, GraphicsDeviceRef_createPipelineGraphics(
				deviceRef, &fileList, &stageList, &info, &pName, EPipelineFlags_CaptureISA, NULL, &pipeline, e_rr
			));
		}

		else if(pipelineBase.type == (U8) ESPPipelineType_Raytracing) {

			//Ray tracing: bind every stage the lib declares, then pair the hit shaders into groups.
			//No shader binding table or dispatch is needed just to compile the pipeline and read per-stage statistics.

			PipelineStage stages[16];
			U32 hitIndex[3][8];                  //Stage index per hit kind: closest hit, any hit, intersection
			U8 hitCount[3] = { 0, 0, 0 };
			U8 stageCount = 0;

			for (U64 i = 0; i < shFile.entries.length && stageCount < 16; ++i) {

				const SHEntry *entry = &shFile.entries.ptr[i];

				if(entry->stage < ESHPipelineStage_RtStartExt || entry->stage > ESHPipelineStage_RtEndExt)
					continue;

				CharString name = entry->name;

				const U32 id = GraphicsDeviceRef_getFirstShaderEntry(
					deviceRef, &shFile, &name, NULL, NULL, ESHExtension_None, ESHExtension_None
				);

				if(id == U32_MAX)
					retError(clean, Error_invalidState(
						0,
						"CLI_isaDisassembleLive() no compatible binary for a ray tracing stage (see the reason above; "
						"a custom descriptor layout can't be supplied yet)"
					));

				//A hit shader is remembered by kind so the groups can reference its stage index.

				U8 kind = 0xFF;

				switch (entry->stage) {
					case ESHPipelineStage_ClosestHitExt:    kind = 0;  break;
					case ESHPipelineStage_AnyHitExt:        kind = 1;  break;
					case ESHPipelineStage_IntersectionExt:  kind = 2;  break;
					default:                                           break;
				}

				if(kind != 0xFF && hitCount[kind] < 8)
					hitIndex[kind][hitCount[kind]++] = stageCount;

				if(entry->stage == ESHPipelineStage_RaygenExt && !CharString_length(entryName))
					entryName = entry->name;

				stages[stageCount++] = (PipelineStage) { .binaryId = id };
			}

			//One group per hit slot, taking the i-th shader of each kind; with a single hit shader of each kind this
			// is exact, and with more the pairing is inferred, which the state print says.

			U8 groupCount = hitCount[0];

			if(hitCount[1] > groupCount)
				groupCount = hitCount[1];

			if(hitCount[2] > groupCount)
				groupCount = hitCount[2];

			PipelineRaytracingGroup groups[8];

			for (U8 i = 0; i < groupCount && i < 8; ++i)
				groups[i] = (PipelineRaytracingGroup) {
					.closestHit   = i < hitCount[0] ? hitIndex[0][i] : U32_MAX,
					.anyHit       = i < hitCount[1] ? hitIndex[1][i] : U32_MAX,
					.intersection = i < hitCount[2] ? hitIndex[2][i] : U32_MAX
				};

			if(hitCount[0] > 1 || hitCount[1] > 1 || hitCount[2] > 1)
				spFile.pipelines.ptrNonConst[pipelineId].flags |= ESPPipelineFlag_AssumedHitGrouping;

			boundStageCount = stageCount;

			ListPipelineStage stageList = (ListPipelineStage) { 0 };
			ListPipelineStage_createRefConst(stages, stageCount, &stageList, NULL);

			ListPipelineRaytracingGroup groupList = (ListPipelineRaytracingGroup) { 0 };

			if(groupCount)
				ListPipelineRaytracingGroup_createRefConst(groups, groupCount, &groupList, NULL);

			const PipelineRaytracingInfo info = (PipelineRaytracingInfo) {
				.flags = rtState->raytracingFlags,
				.maxRecursionDepth = rtState->maxRecursionDepth
			};

			gotoIfError3(clean, GraphicsDeviceRef_createPipelineRaytracingExt(
				deviceRef, &stageList, &fileList, &groupList, &info, &pName, EPipelineFlags_CaptureISA, NULL, &pipeline, e_rr
			));
		}

		else retError(clean, Error_unsupportedOperation(
			0, "CLI_isaDisassembleLive() needs a compute, a vertex+pixel, or a raygen+miss+closesthit shader"
		));

		gotoIfError3(clean, GraphicsDeviceRef_getPipelineExecutables(pipeline, alloc, &execs, e_rr));

		gotoIfError3(clean, CharString_format(
			alloc, &text, e_rr, "; Live ISA for '%.*s' on %s\n",
			(int) CharString_length(entryName), entryName.ptr, infos.ptr[deviceId].name
		));

		//How many stages the pipeline was built from, since a driver that exposes no executables (ray tracing on
		// NVIDIA) would otherwise give no sign of what was actually compiled.

		if (boundStageCount) {

			gotoIfError3(clean, CharString_format(
				alloc, &stageLine, e_rr, "; %"PRIu32" stage(s) bound\n", (U32) boundStageCount
			));

			gotoIfError3(clean, CharString_appendString(&text, &stageLine, alloc, e_rr));
		}

		//The state that produced this disassembly is printed with each field's provenance, so an assumed value is
		// never mistaken for something the shader itself declared.

		gotoIfError3(clean, SPFile_print(&spFile, pipelineId, alloc, &text, e_rr));

		//The pipeline that produced this disassembly can be stored, so it can be inspected or loaded again later.

		if (hasPipelineOutput) {

			gotoIfError3(clean, SPFile_finalize(&spFile, alloc, e_rr));
			gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memStreamType, &pipelineStream, e_rr));

			U64 pipelineOff = 0;
			gotoIfError3(clean, SPFile_write(&spFile, alloc, pipelineStream, &pipelineOff, e_rr));

			const MemoryStream *pipelineMs = RefPtr_data(pipelineStream, MemoryStream);
			const Buffer written = Buffer_createRefConst(pipelineMs->data.ptr, pipelineOff);

			gotoIfError3(clean, File_write(&written, &pipelineOutputStr, 0, 0, 1 * SECOND, true, fileHandleType, e_rr));

			Log_debugLnx(
				"Wrote the pipeline -> %.*s",
				(int) CharString_length(pipelineOutputStr), pipelineOutputStr.ptr
			);
		}

		gotoIfError3(clean, CLI_isaAppendExecutables(&text, &execs, alloc, e_rr));

		if(hasOutput) {

			const Buffer textBuf = CharString_bufferConst(text);
			gotoIfError3(clean, File_write(&textBuf, &outputStr, 0, 0, 1 * SECOND, true, fileHandleType, e_rr));

			Log_debugLnx(
				"Wrote live ISA for '%.*s' -> %.*s",
				(int) CharString_length(entryName), entryName.ptr,
				(int) CharString_length(outputStr), outputStr.ptr
			);
		}

		else Log_debugLnx("%.*s", (int) CharString_length(text), text.ptr);

	clean:
		CharString_free(&stageLine, alloc);
		RefPtr_dec(&pipelineStream);
		SPFile_free(&spFile, alloc);
		SHFile_free(&standIns, alloc);
		ListCharString_freeUnderlying(&issues, alloc);
		CharString_free(&text, alloc);
		ListPipelineExecutable_freeUnderlying(&execs, alloc);
		RefPtr_dec(&pipeline);
		RefPtr_dec(&deviceRef);
		RefPtr_dec(&instanceRef);
		ListGraphicsDeviceInfo_free(&infos, alloc);
		return s_uccess;
	}

#endif

Bool CLI_isaDisassemble(const ParsedArgs *args) {

	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	const RefPtrType memoryStreamType = MemoryStream_makeType(alloc);

	CharString inputStr = CharString_createNull(), outputStr = CharString_createNull(), asic = CharString_createNull();
	Buffer input = Buffer_createNull(), isa = Buffer_createNull();
	Buffer spirv = Buffer_createNull();        //A ref into `input` (.spv) or into `shFile` (.oiSH); not owned
	CharString entrypoint = CharString_createNull();        //oiSH: the chosen binary's entrypoint (selects it from a lib)
	SHFile shFile = (SHFile) { 0 };
	MemoryStreamRef *readStream = NULL;

	gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &inputStr, e_rr));

	//-output is optional: with it we write the ISA to a file, without it we print the ISA to the console

	const Bool hasOutput =
		ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &outputStr, NULL) && CharString_length(outputStr);

	if(!ParsedArgs_getArg(args, EOperationHasParameter_ISAAsicShift, &asic, NULL) || !CharString_length(asic)) {

		//No ASIC given: print the device list so the user can pick one, then error

		Error listErr = Error_none();
		CLI_isaPrintDevices(alloc, &listErr);

		retError(clean, Error_invalidParameter(
			0, 0, "CLI_isaDisassemble() -asic is required; pick one of the ASICs above (or run 'OxC3 isa devices')"
		));
	}

	//"-asic live" bypasses the offline amdllpc path: it creates a real Vulkan device and reads the driver's own ISA
	//back for the oiSH's compute entry (cross-vendor, but device + driver dependent).

	#ifdef CLI_GRAPHICS

		const CharString liveStr = CharString_createRefCStrConst("live");
		const CharString livePfx = CharString_createRefCStrConst("live:");

		//"live" targets device 0; "live:<index>" targets a specific GPU (the function lists them all first)

		Bool live = CharString_equalsStringInsensitive(&asic, &liveStr);
		U64 deviceId = 0;

		if(!live && CharString_startsWithStringInsensitive(&asic, &livePfx, 0)) {

			live = true;

			const CharString idxStr = CharString_createRefSizedConst(
				asic.ptr + CharString_length(livePfx), CharString_length(asic) - CharString_length(livePfx), false
			);

			if(!CharString_parseU64(idxStr, &deviceId))
				retError(clean, Error_invalidParameter(
					0, 0,
					"CLI_isaDisassemble() -asic live:<index> device index must be a number "
					"(list them by running with 'live')"
				));
		}

		if(live) {

			//-rtv <fmt>[,<fmt>...] overrides the reflected render target formats for a live graphics gfxState->

			U8 overrideRtv[8] = { 0 };
			U8 overrideRtvCount = 0;

			CharString rtvStr = CharString_createNull();

			if(ParsedArgs_getArg(args, EOperationHasParameter_RTVFormatShift, &rtvStr, NULL) && CharString_length(rtvStr)) {

				U64 start = 0;

				for(U64 i = 0; i <= CharString_length(rtvStr) && overrideRtvCount < 8; ++i)

					if(i == CharString_length(rtvStr) || rtvStr.ptr[i] == ',') {

						const CharString seg = CharString_createRefSizedConst(rtvStr.ptr + start, i - start, false);
						const ETextureFormatId fmt = CLI_parseTextureFormat(seg);

						if(fmt == ETextureFormatId_Undefined)
							retError(clean, Error_invalidParameter(
								0, 0, "CLI_isaDisassemble() -rtv has an unknown texture format (e.g. rgba8, rgba16f, r32f)"
							));

						overrideRtv[overrideRtvCount++] = (U8) fmt;
						start = i + 1;
					}
			}

			gotoIfError3(clean, File_read(&inputStr, 1 * SECOND, 0, 0, &fileHandleType, &input, e_rr));
			gotoIfError3(clean, MemoryStream_createFromBuffer(
				&input, EMemoryStreamFlags_None, &memoryStreamType, &readStream, e_rr
			));

			U64 liveOff = 0;
			gotoIfError3(clean, SHFile_read((StreamRef*)readStream, &liveOff, false, alloc, &shFile, e_rr));

			//Recursion depth is the one ray tracing field a shader never declares, so it's an override rather than a
			// derived value; -assume-defaults lets the remaining assumptions through for a quick look.

			CharString depthStr = CharString_createNull();
			U64 recursionDepth = 0;
			const Bool hasRecursionDepth =
				ParsedArgs_getArg(args, EOperationHasParameter_RecursionDepthShift, &depthStr, NULL) &&
				CharString_length(depthStr);

			const Bool badDepth =
				hasRecursionDepth &&
				(!CharString_parseU64(depthStr, &recursionDepth) || !recursionDepth || recursionDepth > 255);

			if(badDepth)
				retError(clean, Error_invalidParameter(
					0, 0, "CLI_isaDisassemble() -recursion-depth has to be a number in 1..255"
				));

			const Bool assumeDefaults = (args->flags & EOperationFlags_AssumeDefaults) != 0;

			CharString pipelineOutputStr = CharString_createNull();
			const Bool hasPipelineOutput =
				ParsedArgs_getArg(args, EOperationHasParameter_PipelineOutputShift, &pipelineOutputStr, NULL) &&
				CharString_length(pipelineOutputStr);

			gotoIfError3(clean, CLI_isaDisassembleLive(
				shFile, deviceId, hasOutput, outputStr, &fileHandleType, overrideRtv, overrideRtvCount,
				hasRecursionDepth, (U8) recursionDepth, assumeDefaults,
				hasPipelineOutput, pipelineOutputStr, inputStr, alloc, e_rr
			));
			goto clean;
		}

	#endif

	//Validate the ASIC first; '?' or an unknown device prints the device list so the user can pick a valid one

	Bool handled = false;
	gotoIfError3(clean, CLI_isaResolveAsic(asic, &handled, alloc, e_rr));

	if(handled)                                  //'?' just listed the devices; nothing to disassemble
		goto clean;

	gotoIfError3(clean, File_read(&inputStr, 1 * SECOND, 0, 0, &fileHandleType, &input, e_rr));

	//Get the SPIR-V to disassemble: a raw .spv is used as-is, an oiSH has its SPIR-V binary extracted (per -entry).
	//DXIL has no offline path here (that's the live-AMD-device route, a separate step), so DXIL-only input errors.

	const CharString spvExt = CharString_createRefCStrConst(".spv");
	const CharString oiSHExt = CharString_createRefCStrConst(".oiSH");

	if(CharString_endsWithStringInsensitive(&inputStr, &spvExt, 0))
		spirv = Buffer_createRefConst(input.ptr, Buffer_length(input));

	else if(CharString_endsWithStringInsensitive(&inputStr, &oiSHExt, 0)) {

		gotoIfError3(clean, MemoryStream_createFromBuffer(
			&input, EMemoryStreamFlags_None, &memoryStreamType, &readStream, e_rr
		));

		U64 off = 0;
		gotoIfError3(clean, SHFile_read((StreamRef*)readStream, &off, false, alloc, &shFile, e_rr));

		//Pick which binary's SPIR-V to disassemble.
		//-entry is an INDEX into the oiSH's binaries: an entrypoint name alone is ambiguous, since the same name can
		// occur several times with different uniforms, defines or extensions.
		//Without -entry, use the sole SPIR-V binary when there's exactly one (list them via 'file data --bin').

		CharString entry = CharString_createNull();
		const Bool hasEntry =
			ParsedArgs_getArg(args, EOperationHasParameter_EntryShift, &entry, NULL) && CharString_length(entry);

		const SHBinaryInfo *chosen = NULL;

		if(hasEntry) {

			U64 idx = 0;

			if(!CharString_parseU64(entry, &idx))
				retError(clean, Error_invalidParameter(
					0, 0,
					"CLI_isaDisassemble() -entry must be a binary index "
					"(list them with 'OxC3 file data -input <oiSH> --bin')"
				));

			if(idx >= shFile.binaries.length)
				retError(clean, Error_outOfBounds(
					0, idx, shFile.binaries.length, "CLI_isaDisassemble() -entry index out of bounds"
				));

			chosen = &shFile.binaries.ptr[idx];

			if(!Buffer_length(chosen->binaries[ESHBinaryType_SPIRV]))
				retError(clean, Error_invalidParameter(
					0, 1, "CLI_isaDisassemble() binary at -entry has no SPIR-V (DXIL-only; use the live AMD device path)"
				));
		}

		else {

			//No -entry: require exactly one SPIR-V binary across the whole oiSH

			U64 spvCount = 0;

			for(U64 i = 0; i < shFile.binaries.length; ++i)
				if(Buffer_length(shFile.binaries.ptr[i].binaries[ESHBinaryType_SPIRV])) {
					chosen = &shFile.binaries.ptr[i];
					++spvCount;
				}

			if(!spvCount)
				retError(clean, Error_notFound(
					0, 0, "CLI_isaDisassemble() oiSH has no SPIR-V binary (DXIL-only; use the live AMD device path instead)"
				));

			if(spvCount > 1)
				retError(clean, Error_invalidParameter(
					0, 1, "CLI_isaDisassemble() oiSH has multiple binaries; pass -entry <index> "
					"(list them with 'OxC3 file data -input <oiSH> --bin')"
				));
		}

		const Buffer spv = chosen->binaries[ESHBinaryType_SPIRV];
		spirv = Buffer_createRefConst(spv.ptr, Buffer_length(spv));
		entrypoint = chosen->identifier.entrypoint;        //Selects this stage from a multi-entry (library) module
	}

	else retError(clean, Error_invalidParameter(
		0, 1, "CLI_isaDisassemble() input must be .spv (SPIR-V) or .oiSH; DXIL/HLSL have no offline SPIR-V path"
	));

	gotoIfError3(clean, CLI_isaDisassembleSpirv(spirv, asic, entrypoint, &isa, alloc, e_rr));

	if(hasOutput) {

		gotoIfError3(clean, File_write(&isa, &outputStr, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));

		Log_debugLnx(
			"Disassembled %.*s to AMD ISA for %.*s -> %.*s",
			(int) CharString_length(inputStr), inputStr.ptr,
			(int) CharString_length(asic), asic.ptr,
			(int) CharString_length(outputStr), outputStr.ptr
		);
	}

	else Log_debugLnx("%.*s", (int) Buffer_length(isa), (const C8*) isa.ptr);

clean:
	if(!s_uccess)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	SHFile_free(&shFile, alloc);
	RefPtr_dec(&readStream);
	Buffer_free(&input, alloc);
	Buffer_free(&isa, alloc);
	return s_uccess;
}

#endif
