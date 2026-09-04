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
#include "types/math/type_cast.h"
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
//amdllpc does the disassembly directly and reports its own target set.

//The shader analyzer lives in the AMD driver and binds to a device, so a machine that mixes vendors has to pick
// the AMD adapter rather than whichever one enumerated first.
//Returns U64_MAX when there's no AMD device to bind to.

#ifdef CLI_GRAPHICS

	static U64 CLI_isaFindAmdDevice(ListGraphicsDeviceInfo infos) {

		for(U64 i = 0; i < infos.length; ++i)
			if(infos.ptr[i].vendor == EGraphicsVendorId_AMD)
				return i;

		return U64_MAX;
	}

#endif

//Targets the installed driver can compile for besides the device itself, which today means AMD on D3D12.
//Absent everywhere else, so a machine without them says nothing rather than reporting a failure.

static Bool CLI_isaPrintLiveTargets(const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	//Failures land in a local error that is deliberately dropped: a machine without these targets is the normal
	// case and shouldn't turn 'isa devices' into a failure.

	(void) e_rr;

	#ifndef CLI_GRAPHICS
		(void) alloc;
	#else

		RefPtrType instanceType = (RefPtrType) { 0 };
		GraphicsInstanceRef *instanceRef = NULL;
		GraphicsDeviceRef *deviceRef = NULL;
		ListGraphicsDeviceInfo infos = (ListGraphicsDeviceInfo) { 0 };
		ListCharString liveTargets = (ListCharString) { 0 };
		Error gErr = Error_none();

		if(!GraphicsInterface_create(&gErr) || !GraphicsInterface_supportsApi(EGraphicsApi_Direct3D12))
			goto clean;

		instanceType = GraphicsInstance_makeType(EGraphicsApi_Direct3D12, alloc);

		const GraphicsApplicationInfo appInfo = {
			.name = CharString_createRefCStrConst("OxC3 CLI isa devices"),
			.version = OXC3_MAKE_VERSION(OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH)
		};

		if(!GraphicsInstance_create(
			&appInfo, EGraphicsApi_Direct3D12, EGraphicsInstanceFlags_None, alloc, &instanceType, &instanceRef, &gErr
		))
			goto clean;

		if(!GraphicsInstance_getDeviceInfos(GraphicsInstanceRef_ptr(instanceRef), &infos, &gErr) || !infos.length)
			goto clean;

		//The list belongs to the driver rather than the adapter, so any AMD device answers for the whole generation.

		const U64 amdDevice = CLI_isaFindAmdDevice(infos);

		if(amdDevice == U64_MAX)
			goto clean;

		if(!GraphicsDeviceRef_create(
			instanceRef, &infos.ptr[amdDevice], EGraphicsDeviceFlags_None, EGraphicsBufferingMode_Default,
			NULL, NULL, &deviceRef, &gErr
		))
			goto clean;

		if(!GraphicsDeviceRef_listShaderTargets(deviceRef, alloc, &liveTargets, &gErr) || !liveTargets.length)
			goto clean;

		Log_debugLnx("");
		Log_debugLnx(
			"Live DXIL targets (%s driver; pass one as -asic with -compile-output dxil):", infos.ptr[amdDevice].name
		);

		for(U64 i = 0; i < liveTargets.length; ++i)
			Log_debugLnx("\t%.*s", (int) CharString_length(liveTargets.ptr[i]), liveTargets.ptr[i].ptr);

	clean:

		//The device holds on to the info it was created from, so the list outlives it

		ListCharString_freeUnderlying(&liveTargets, alloc);
		RefPtr_dec(&deviceRef);
		RefPtr_dec(&instanceRef);
		ListGraphicsDeviceInfo_free(&infos, alloc);

	#endif

	return s_uccess;
}

static Bool CLI_isaPrintDevices(const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	ListCharString targets = (ListCharString) { 0 };

	gotoIfError3(clean, SpvISA_listSupportedTargets(alloc, &targets, e_rr));

	Log_debugLnx("Offline ISA targets (amdllpc-supported; pass one as -asic):");

	for(U64 i = 0; i < targets.length; ++i)
		Log_debugLnx("\t%.*s", (int) CharString_length(targets.ptr[i]), targets.ptr[i].ptr);

	//An installed AMD driver compiles for its whole generation, not only the ASIC present, and that's the only
	// route DXIL has to ISA; those need a device to enumerate, so they're listed separately.

	gotoIfError3(clean, CLI_isaPrintLiveTargets(alloc, e_rr));

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
//The actual amdllpc driving lives in the shared SpvISA_ module, so this and the corpus ISA snapshot test
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
		CharString *out, const ListPipelineExecutable *execs, Bool hasIntrospection,
		const Allocator *alloc, Error *e_rr
	) {

		Bool s_uccess = true;
		CharString line = CharString_createNull();

		//A device with no introspection still validated the pipeline by creating it, so that's what's reported.
		//On D3D12 that means anything but an AMD driver, since ISA there comes from AMD's own extension.

		if (!hasIntrospection) {

			const CharString noApi = CharString_createRefCStrConst(
				";   (this device exposes no pipeline introspection, so there is nothing to read back. The pipeline "
				"compiled, which validates the binary and the state against a real driver. D3D12 has no equivalent "
				"of VK_KHR_pipeline_executable_properties, so ISA there comes from AMD's own driver extension and "
				"needs an AMD driver. For AMD ISA from SPIR-V use '-asic <gfxN>')\n"
			);

			return CharString_appendString(out, &noApi, alloc, e_rr);
		}

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
			"AMD's own driver return it, and support varies across the other closed-source drivers. For "
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

	//A texture format name (e.g. "rgba16f", case-insensitive) -> its ETextureFormatId, or Undefined if unknown.
	//Used by -pso-set for rtv.format.

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
			"#include \"@resources.hlsli\"\n"
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
				"\to._position = F32x4(_time, _deltaTime, (F32) _frameId, (F32) _swapchainCount);\n"
			);

			gotoIfError3(clean, CharString_appendString(out, &vsMid, alloc, e_rr));

			for (U8 i = 0; i < 16; ++i) {

				const ESBType type = (ESBType) vsTarget->inputs[i];

				if(!type)
					continue;

				//The values come from the per frame globals, so they aren't compile time constants and the stand-in
				// needs no layout beyond the default one.

				const C8 *source = "F32x4(_time, _deltaTime, (F32) _frameId, (F32) _swapchainCount)";

				switch(ESBType_getPrimitive(type)) {

					case ESBPrimitive_Int:
						source = "I32x4((I32) _frameId, (I32) _swapchainCount, asint(_time), asint(_deltaTime))";
						break;

					case ESBPrimitive_UInt:
						source = "U32x4(_frameId, _swapchainCount, asuint(_time), asuint(_deltaTime))";
						break;

					default:
						break;
				}

				static const C8 *swizzles[4] = { "x", "xy", "xyz", "xyzw" };
				const U8 comp = (U8) ESBType_getVector(type);

				CharString_free(&line, alloc);
				gotoIfError3(clean, CharString_format(
					alloc, &line, e_rr, "\to._v%"PRIu8" = (%s) %s.%s;\n",
					i, ESBType_name(type), source, swizzles[comp & 3]
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
		const SHEntry *vsTarget, const SHEntry *psSource, EGfxBinaryType binaryType,
		SHFile *out, const Allocator *alloc, Error *e_rr
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

		//The stand-ins join the shader's own stages in one pipeline, so they have to be the same binary type.

		gotoIfError3(clean, ListU8_pushBack(&modes, (U8) binaryType, alloc, e_rr));

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

	//Applies "path[idx]=value,..." to a derived pipeline through SPFile_supply, by the paths the report prints.
	//rtv.format takes a texture format name, the F32 fields a float literal, everything else an integer.

	static Bool CLI_isaApplyPipelineSet(SPFile *spFile, U32 pipelineId, CharString set, Error *e_rr) {

		Bool s_uccess = true;
		U64 start = 0;

		for (U64 i = 0; i <= CharString_length(set); ++i) {

			if(i != CharString_length(set) && set.ptr[i] != ',')
				continue;

			const CharString seg = CharString_createRefSizedConst(set.ptr + start, i - start, false);
			start = i + 1;

			if(!CharString_length(seg))
				continue;

			U64 eq = U64_MAX;

			for (U64 j = 0; j < CharString_length(seg); ++j)
				if (seg.ptr[j] == '=') {
					eq = j;
					break;
				}

			if(eq == U64_MAX || !eq || eq + 1 >= CharString_length(seg))
				retError(clean, Error_invalidParameter(
					0, 0, "CLI_isaDisassemble() -pso-set expects path=value entries separated by commas"
				));

			const CharString path = CharString_createRefSizedConst(seg.ptr, eq, false);
			const CharString valueStr =
				CharString_createRefSizedConst(seg.ptr + eq + 1, CharString_length(seg) - eq - 1, false);

			ESPField field = ESPField_Count;
			U8 index = 0;

			if (!ESPField_parsePath(path, &field, &index)) {
				Log_errorLnx(
					"-pso-set: '%.*s' isn't a field this pipeline reports (see the report's paths)", (int) eq, seg.ptr
				);
				retError(clean, Error_invalidParameter(0, 0, "CLI_isaDisassemble() -pso-set has an unknown field path"));
			}

			U32 value = 0;
			U64 parsed = 0;
			F32 f = 0;

			const Bool isF32 =
				field == ESPField_DepthBiasClamp || field == ESPField_DepthBiasSlope || field == ESPField_MsaaMinSampleShading;

			if (field == ESPField_RenderTargetFormat && !(valueStr.ptr[0] >= '0' && valueStr.ptr[0] <= '9')) {

				const ETextureFormatId fmt = CLI_parseTextureFormat(valueStr);

				if(fmt == ETextureFormatId_Undefined)
					retError(clean, Error_invalidParameter(
						0, 0, "CLI_isaDisassemble() -pso-set rtv.format has an unknown texture format (e.g. rgba8, rgba16f)"
					));

				value = (U32) fmt;
			}

			else if(isF32 && CharString_parseFloat(valueStr, &f))
				value = U32_fromF32Bits(f);

			//The sampler lod fields store F16 bits in the low half, so a float literal converts down

			else if (
				(
					field == ESPField_LayoutSamplerMipBias || field == ESPField_LayoutSamplerMinLod ||
					field == ESPField_LayoutSamplerMaxLod
				) &&
				CharString_parseFloat(valueStr, &f)
			)
				value = (U32) EFloatType_convert(EFloatType_F32, U32_fromF32Bits(f), EFloatType_F16);

			else if(CharString_parseU64(valueStr, &parsed) && !(parsed >> 32))
				value = (U32) parsed;

			else {
				Log_errorLnx("-pso-set: '%.*s' has no valid value", (int) CharString_length(seg), seg.ptr);
				retError(clean, Error_invalidParameter(0, 0, "CLI_isaDisassemble() -pso-set value couldn't be parsed"));
			}

			gotoIfError3(clean, SPFile_supply(spFile, pipelineId, field, index, value, e_rr));
		}

	clean:
		return s_uccess;
	}

	//Replays every field a stored oiSP carries over the derived pipeline; the stored values count as supplied.
	//The stored pipeline has to be of the same kind, since its fields belong to that kind's state.

	static Bool CLI_isaApplyPipelineInput(
		SPFile *spFile, U32 pipelineId, CharString path, const RefPtrType *fileHandleType,
		const Allocator *alloc, Error *e_rr
	) {

		Bool s_uccess = true;
		Buffer data = Buffer_createNull();
		StreamRef *stream = NULL;
		SPFile stored = (SPFile) { 0 };
		const RefPtrType memStreamType = MemoryStream_makeType(alloc);

		gotoIfError3(clean, File_read(&path, 1 * SECOND, 0, 0, fileHandleType, &data, e_rr));
		gotoIfError3(clean, MemoryStream_createFromBuffer(&data, EMemoryStreamFlags_None, &memStreamType, &stream, e_rr));

		U64 off = 0;
		gotoIfError3(clean, SPFile_read(stream, &off, false, alloc, &stored, e_rr));

		if(!stored.pipelines.length)
			retError(clean, Error_invalidState(0, "CLI_isaDisassemble() -pso-input holds no pipeline"));

		const SPPipelineBase src = stored.pipelines.ptr[0];

		if(src.type != spFile->pipelines.ptr[pipelineId].type)
			retError(clean, Error_invalidState(
				0, "CLI_isaDisassemble() -pso-input is a different kind of pipeline than the one the shader forms"
			));

		for (U32 i = 0; i < src.specializationCount; ++i) {
			const SPSpecialization spec = stored.specializations.ptr[src.specializationStart + i];
			gotoIfError3(clean, SPFile_supply(spFile, pipelineId, (ESPField) spec.field, spec.index, spec.value, e_rr));
		}

		//A stored layout replaces the derived one wholesale, since structure can't travel as field supplies.
		//Every copied row counts as supplied: the caller chose this layout, whatever produced it originally.
		//The derived layout stays in the list unreferenced, which costs bytes and nothing else.

		if (src.layoutIndex != U32_MAX) {

			PLFile copied = (PLFile) { 0 };
			gotoIfError3(clean, PLFile_copy(&stored.layouts.ptr[src.layoutIndex], alloc, &copied, e_rr));

			for(U64 i = 0; i < copied.bindings.length; ++i)
				copied.bindings.ptrNonConst[i].name24_source8 = PLDescriptorBinding_pack(
					PLDescriptorBinding_name(copied.bindings.ptr[i]), EPLSource_Supplied
				);

			copied.pushConstant.name24_source8 = PLDescriptorBinding_pack(
				PLDescriptorBinding_name(copied.pushConstant), EPLSource_Supplied
			);

			const U32 copiedAt = (U32) spFile->layouts.length;

			if (!ListPLFile_pushBack(&spFile->layouts, copied, alloc, e_rr)) {
				PLFile_free(&copied, alloc);
				s_uccess = false;
				goto clean;
			}

			spFile->pipelines.ptrNonConst[pipelineId].layoutIndex = copiedAt;
		}

	clean:
		SPFile_free(&stored, alloc);
		RefPtr_dec(&stream);
		Buffer_free(&data, alloc);
		return s_uccess;
	}

	//A single entry module has its SPIR-V entrypoint normalized to "main" while a library keeps the HLSL names, so
	// the name the oiSH recorded is only handed to the backend when the module genuinely carries it.
	//Passing a name the module doesn't have resolves to nothing and the driver reports it as an opaque failure.
	//The returned string references the oiSH, so it lives as long as the caller's SHFile.

	static CharString CLI_isaSpirvEntry(const SHFile *shFile, U32 entry, CharString recorded, const Allocator *alloc) {

		if(!CharString_length(recorded) || (U16) entry >= shFile->entries.length)
			return CharString_createNull();

		const SHEntry *shEntry = &shFile->entries.ptr[(U16) entry];
		const U16 binaryIndex = (U16)(entry >> 16);

		if(binaryIndex >= shEntry->binaryIds.length)
			return CharString_createNull();

		const U16 binaryId = shEntry->binaryIds.ptr[binaryIndex];

		if(binaryId >= shFile->binaries.length)
			return CharString_createNull();

		const Buffer spirv = shFile->binaries.ptr[binaryId].binaries[EGfxBinaryType_SPIRV];

		if(!Buffer_length(spirv))
			return CharString_createNull();

		ListCompilerEntrypoint eps = (ListCompilerEntrypoint) { 0 };
		Error epErr = Error_none();
		Bool found = false;

		if(Compiler_getUniqueEntrypoints(NULL, EGfxBinaryType_SPIRV, spirv, true, &eps, alloc, &epErr))
			for(U64 i = 0; i < eps.length && !found; ++i)
				found = CharString_equalsStringSensitive(&eps.ptr[i].name, &recorded);

		ListCompilerEntrypoint_freeUnderlying(&eps, alloc);
		return found ? recorded : CharString_createNull();
	}

	static Bool CLI_isaDisassembleLive(
		SHFile shFile, EGfxBinaryType binaryType, U64 deviceId, Bool hasOutput, CharString outputStr,
		const RefPtrType *fileHandleType, Bool assumeDefaults,
		Bool hasPipelineOutput, CharString pipelineOutputStr, CharString psoSet, CharString psoInput,
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
		PipelineLayoutRef *pipelineLayout = NULL;
		ListCharString runtimeNames = (ListCharString) { 0 };
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

		//SPIR-V is compiled by Vulkan and DXIL by D3D12, so the binary the oiSH holds picks the backend.
		//Everything below is the generic graphics interface, so only this choice differs between them.

		const EGraphicsApi api = binaryType == EGfxBinaryType_DXIL ? EGraphicsApi_Direct3D12 : EGraphicsApi_Vulkan;
		const C8 *apiName = EGraphicsApi_name[api];

		if (!GraphicsInterface_supportsApi(api)) {

			Log_errorLnx("%s isn't available on this machine, which is what %s binaries are compiled by.", apiName,
				EGfxBinaryType_names[binaryType]);

			retError(clean, Error_unsupportedOperation(0, "CLI_isaDisassembleLive() the required graphics API is unavailable"));
		}

		instanceType = GraphicsInstance_makeType(api, alloc);

		const GraphicsApplicationInfo appInfo = {
			.name = CharString_createRefCStrConst("OxC3 CLI isa live"),
			.version = OXC3_MAKE_VERSION(OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH)
		};

		gotoIfError3(clean, GraphicsInstance_create(
			&appInfo, api, EGraphicsInstanceFlags_None, alloc, &instanceType, &instanceRef, e_rr
		));

		gotoIfError3(clean, GraphicsInstance_getDeviceInfos(GraphicsInstanceRef_ptr(instanceRef), &infos, e_rr));

		if(!infos.length)
			retError(clean, Error_invalidState(0, "CLI_isaDisassembleLive() the graphics API reported no devices"));

		//List the devices so the user can pick one with -asic live:<index> (multi-GPU / multi-vendor machines)

		Log_debugLnx("%s devices (select with -asic live:<index>):", apiName);

		for(U64 i = 0; i < infos.length; ++i)
			Log_debugLnx("\t%"PRIu64": %s", i, infos.ptr[i].name);

		//Plain "live" picks for the caller: DXIL is only introspectable through AMD's driver extension, so an AMD
		// adapter is preferred over whichever one enumerated first, while SPIR-V is cross-vendor and takes device 0.

		if (deviceId == U64_MAX) {

			const U64 amdDevice = api == EGraphicsApi_Direct3D12 ? CLI_isaFindAmdDevice(infos) : U64_MAX;
			deviceId = amdDevice == U64_MAX ? 0 : amdDevice;
		}

		if(deviceId >= infos.length)
			retError(clean, Error_outOfBounds(
				0, deviceId, infos.length, "CLI_isaDisassembleLive() -asic live:<index> device index out of range"
			));

		gotoIfError3(clean, GraphicsDeviceRef_create(
			instanceRef, &infos.ptr[deviceId], EGraphicsDeviceFlags_None, EGraphicsBufferingMode_Default,
			NULL, NULL, &deviceRef, e_rr
		));

		//Without introspection the run still builds the pipeline, which validates the binary and the state; it just
		// has nothing to disassemble afterwards, so it's said here rather than refused.

		if(!(GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features2 & EGraphicsFeatures2_PipelineExecutableInfo))
			Log_warnLnx(
				"%s on this device exposes no pipeline introspection, so this run validates the pipeline without "
				"disassembling it.", apiName
			);

		//The device is created with its default bindless layout (@resources.hlsli), which is what OxC3 compiles
		// shaders against; a shader declaring registers of its own additionally gets the pipeline layout its oiSP
		// derivation describes, while one declaring nothing custom keeps NULL, meaning that same default.

		const CharString pName = CharString_createRefCStrConst("isa live pipeline");

		ListSHFile fileList = (ListSHFile) { 0 };
		ListSHFile_createRefConst(&shFile, 1, &fileList, NULL);

		CharString entryName = CharString_createNull();
		U8 boundStageCount = 0;                //Reported, since a dropped stage would otherwise be invisible

		//Pick the stages that form ONE pipeline.
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

			if(stage == EGfxPipelineStage_Compute)
				++kindCounts[0];

			else if(
				stage == EGfxPipelineStage_Vertex || stage == EGfxPipelineStage_Pixel || stage == EGfxPipelineStage_Hull ||
				stage == EGfxPipelineStage_Domain || stage == EGfxPipelineStage_GeometryExt
			)
				++kindCounts[1];

			else if(stage >= EGfxPipelineStage_RtStartExt && stage <= EGfxPipelineStage_RtEndExt)
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
				"This file holds %s, %s and %s stages, which are separate pipelines; only the %s one is compiled, "
				"since the live path takes compute over graphics over ray tracing. Pass a file holding a single kind "
				"to inspect the others.",
				kindCounts[0] ? "compute" : "no compute", kindCounts[1] ? "graphics" : "no graphics",
				kindCounts[2] ? "ray tracing" : "no ray tracing", kindNames[chosenKind]
			);
		}

		for (U64 i = 0; i < shFile.entries.length; ++i) {

			const U8 stage = shFile.entries.ptr[i].stage;
			U64 *slot = NULL;

			switch (stage) {
				case EGfxPipelineStage_Compute:      if(chosenKind == 0) slot = &computeE;  break;
				case EGfxPipelineStage_Vertex:       if(chosenKind == 1) slot = &vertexE;   break;
				case EGfxPipelineStage_Pixel:        if(chosenKind == 1) slot = &pixelE;    break;
				case EGfxPipelineStage_Hull:         if(chosenKind == 1) slot = &hullE;     break;
				case EGfxPipelineStage_Domain:       if(chosenKind == 1) slot = &domainE;   break;
				case EGfxPipelineStage_GeometryExt:  if(chosenKind == 1) slot = &geomE;     break;
				default:                                                                    break;
			}

			const Bool isRt = stage >= EGfxPipelineStage_RtStartExt && stage <= EGfxPipelineStage_RtEndExt;

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

		//The runtime's own registers belong to the device's layout, so derivation must not describe them

		gotoIfError3(clean, GraphicsDeviceRef_runtimeRegisterNames(deviceRef, &runtimeNames, alloc, e_rr));

		gotoIfError3(clean, SPFile_derivePipeline(
			&spFile, &fileList, &shaderNames, CharString_createNull(), stageRefs, stageRefCount, &runtimeNames,
			alloc, &pipelineId, e_rr
		));

		//-pso-input replays a stored pipeline's values over the derived one, so a run can be repeated or edited from
		// the oiSP a previous -pso-output wrote; every value it carries counts as supplied.

		if(CharString_length(psoInput))
			gotoIfError3(clean, CLI_isaApplyPipelineInput(&spFile, pipelineId, psoInput, fileHandleType, alloc, e_rr));

		//-pso-set supplies any reported field by the path the report prints, so nothing has to stay assumed.

		if(CharString_length(psoSet))
			gotoIfError3(clean, CLI_isaApplyPipelineSet(&spFile, pipelineId, psoSet, e_rr));

		//The layout the file now describes becomes a real one exactly once, after every override had its say.

		gotoIfError3(clean, SPFile_createPipelineLayout(deviceRef, &spFile, pipelineId, alloc, &pipelineLayout, e_rr));

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

			Log_errorLnx(
				"Supply them with -pso-set \"path=value,...\" (any field, by the path above), replay an oiSP with -pso-input, "
				"or pass --assume-defaults for a quick look."
			);

			retError(clean, Error_invalidState(
				1, "CLI_isaDisassembleLive() pipeline state is missing; supply it or pass -assume-defaults"
			));
		}

		//The record is copied once the overrides are in, so the lowering below reads final values.

		const SPPipelineBase pipelineBase = spFile.pipelines.ptr[pipelineId];
		const SPRaytracingState *rtState = SPFile_raytracingState(&spFile, pipelineId);

		if(pipelineBase.type == (U8) ESPPipelineType_Compute) {

			//Compute: a single entry.
			//The SPIRV entrypoint is passed as NULL, which resolves to "main": OxC3 normalizes a single-entry SPIR-V
			// module's entrypoint to "main" regardless of the HLSL name (an SHEntry named "mainCompute" is "main").

			entryName = shFile.entries.ptr[computeE].name;

			const U32 entry = GraphicsDeviceRef_getFirstShaderEntry(
				deviceRef, &shFile, &entryName, NULL, NULL, ESHExtension_None, ESHExtension_None
			);

			//A stage that resolves to nothing usually failed the device's own feature check, which prints its reason
			// just above this; the common one is a shader built against its own bindless layout, since the pipeline is
			// created with the device's default one.

			if(entry == U32_MAX)
				retError(clean, Error_invalidState(
					0,
					"CLI_isaDisassembleLive() no compatible binary for the compute stage (see the reason above; "
					"a shader needing its own BINDLESS layout still fails the device's feature check)"
				));

			const CharString spvEntry = CLI_isaSpirvEntry(&shFile, entry, entryName, alloc);

			gotoIfError3(clean, GraphicsDeviceRef_createPipelineCompute(
				deviceRef, &shFile, &pName, entry, CharString_length(spvEntry) ? &spvEntry : NULL,
				EPipelineFlags_CaptureISA, pipelineLayout, &pipeline, e_rr
			));
		}

		else if (
			vertexE != U64_MAX || pixelE != U64_MAX || hullE != U64_MAX || domainE != U64_MAX || geomE != U64_MAX
		) {

			//Bind every graphics stage the shader declares, in chain order, and generate only the ends it's missing.
			//Silently dropping a declared stage would report a pipeline the shader never described.

			const U64 chainEntry[5] = { vertexE, hullE, domainE, geomE, pixelE };

			static const EGfxPipelineStage chainStage[5] = {
				EGfxPipelineStage_Vertex, EGfxPipelineStage_Hull, EGfxPipelineStage_Domain,
				EGfxPipelineStage_GeometryExt, EGfxPipelineStage_Pixel
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

				gotoIfError3(clean, CLI_isaCompileStandIns(vsTarget, psSource, binaryType, &standIns, alloc, e_rr));

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
						"a shader needing its own BINDLESS layout still fails the device's feature check)"
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

			gotoIfError3(clean, GraphicsDeviceRef_createPipelineGraphics(
				deviceRef, &fileList, &stageList, &info, &pName, EPipelineFlags_CaptureISA, pipelineLayout, &pipeline, e_rr
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

				if(entry->stage < EGfxPipelineStage_RtStartExt || entry->stage > EGfxPipelineStage_RtEndExt)
					continue;

				CharString name = entry->name;

				const U32 id = GraphicsDeviceRef_getFirstShaderEntry(
					deviceRef, &shFile, &name, NULL, NULL, ESHExtension_None, ESHExtension_None
				);

				if(id == U32_MAX)
					retError(clean, Error_invalidState(
						0,
						"CLI_isaDisassembleLive() no compatible binary for a ray tracing stage (see the reason above; "
						"a shader needing its own BINDLESS layout still fails the device's feature check)"
					));

				//A hit shader is remembered by kind so the groups can reference its stage index.

				U8 kind = 0xFF;

				switch (entry->stage) {
					case EGfxPipelineStage_ClosestHitExt:    kind = 0;  break;
					case EGfxPipelineStage_AnyHitExt:        kind = 1;  break;
					case EGfxPipelineStage_IntersectionExt:  kind = 2;  break;
					default:                                            break;
				}

				if(kind != 0xFF && hitCount[kind] < 8)
					hitIndex[kind][hitCount[kind]++] = stageCount;

				if(entry->stage == EGfxPipelineStage_RaygenExt && !CharString_length(entryName))
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
				deviceRef, &stageList, &fileList, &groupList, &info, &pName, EPipelineFlags_CaptureISA, pipelineLayout,
				&pipeline, e_rr
			));
		}

		else retError(clean, Error_unsupportedOperation(
			0, "CLI_isaDisassembleLive() needs a compute, a vertex+pixel, or a raygen+miss+closesthit shader"
		));

		//Creating the pipeline is itself the check that this binary and this state are valid on a real driver.
		//Reading the compiled shader back is a separate capability that only Vulkan exposes, so it's asked for
		// only where it exists and its absence is reported rather than treated as a failure.

		const Bool hasIntrospection =
			(GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features2 &
			EGraphicsFeatures2_PipelineExecutableInfo) != 0;

		if(hasIntrospection)
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

		gotoIfError3(clean, CLI_isaAppendExecutables(&text, &execs, hasIntrospection, alloc, e_rr));

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
		ListCharString_free(&runtimeNames, alloc);
		RefPtr_dec(&pipelineLayout);
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

		//"live" lets the function choose; "live:<index>" targets a specific GPU (it lists them all first)

		Bool live = CharString_equalsStringInsensitive(&asic, &liveStr);
		U64 deviceId = U64_MAX;

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

			gotoIfError3(clean, File_read(&inputStr, 1 * SECOND, 0, 0, &fileHandleType, &input, e_rr));
			gotoIfError3(clean, MemoryStream_createFromBuffer(
				&input, EMemoryStreamFlags_None, &memoryStreamType, &readStream, e_rr
			));

			U64 liveOff = 0;
			gotoIfError3(clean, SHFile_read((StreamRef*)readStream, &liveOff, false, alloc, &shFile, e_rr));

			//Which binary is run decides the backend: SPIR-V goes to Vulkan, DXIL to D3D12.
			//-compile-output picks when the file holds both; otherwise the one that's present is used, preferring
			// SPIR-V because it's the only one any driver disassembles today.

			EGfxBinaryType liveType = EGfxBinaryType_Count;
			CharString liveMode = CharString_createNull();

			if (ParsedArgs_getArg(args, EOperationHasParameter_ShaderOutputModeShift, &liveMode, NULL)) {

				if(CharString_equalsCStringInsensitive(&liveMode, "DXIL"))
					liveType = EGfxBinaryType_DXIL;

				else if(CharString_equalsCStringInsensitive(&liveMode, "SPV"))
					liveType = EGfxBinaryType_SPIRV;

				else retError(clean, Error_invalidParameter(
					0, 0, "CLI_isaDisassemble() -compile-output has to be spv or dxil"
				));
			}

			else for (U64 i = 0; i < shFile.binaries.length && liveType == EGfxBinaryType_Count; ++i) {

				if(Buffer_length(shFile.binaries.ptr[i].binaries[EGfxBinaryType_SPIRV]))
					liveType = EGfxBinaryType_SPIRV;

				else if(Buffer_length(shFile.binaries.ptr[i].binaries[EGfxBinaryType_DXIL]))
					liveType = EGfxBinaryType_DXIL;
			}

			if(liveType == EGfxBinaryType_Count)
				retError(clean, Error_notFound(0, 0, "CLI_isaDisassemble() the oiSH holds no SPIR-V or DXIL binary"));

			const Bool assumeDefaults = (args->flags & EOperationFlags_AssumeDefaults) != 0;

			CharString pipelineOutputStr = CharString_createNull();
			CharString psoSetStr = CharString_createNull();
			CharString psoInputStr = CharString_createNull();
			ParsedArgs_getArg(args, EOperationHasParameter_PipelineSetShift, &psoSetStr, NULL);
			ParsedArgs_getArg(args, EOperationHasParameter_PipelineInputShift, &psoInputStr, NULL);
			const Bool hasPipelineOutput =
				ParsedArgs_getArg(args, EOperationHasParameter_PipelineOutputShift, &pipelineOutputStr, NULL) &&
				CharString_length(pipelineOutputStr);

			gotoIfError3(clean, CLI_isaDisassembleLive(
				shFile, liveType, deviceId, hasOutput, outputStr, &fileHandleType, assumeDefaults,
				hasPipelineOutput, pipelineOutputStr, psoSetStr, psoInputStr, inputStr, alloc, e_rr
			));
			goto clean;
		}

	#endif

	//Everything below is the offline path, which hands amdllpc a module and a gfxip and never forms a pipeline.
	//The pipeline flags therefore have nothing to act on here, so they're refused rather than silently dropped.

	{
		CharString psoArg = CharString_createNull();

		const Bool hasPsoArg =
			ParsedArgs_getArg(args, EOperationHasParameter_PipelineOutputShift, &psoArg, NULL) ||
			ParsedArgs_getArg(args, EOperationHasParameter_PipelineSetShift, &psoArg, NULL) ||
			ParsedArgs_getArg(args, EOperationHasParameter_PipelineInputShift, &psoArg, NULL);

		if(hasPsoArg)
			retError(clean, Error_invalidParameter(
				0, 0,
				"CLI_isaDisassemble() -pso-output/-pso-set/-pso-input describe a pipeline state object, which only "
				"'-asic live' builds"
			));

		if(args->flags & EOperationFlags_AssumeDefaults)
			retError(clean, Error_invalidParameter(
				0, 0,
				"CLI_isaDisassemble() --assume-defaults fills in pipeline state that only '-asic live' derives"
			));
	}

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

			if(!Buffer_length(chosen->binaries[EGfxBinaryType_SPIRV]))
				retError(clean, Error_invalidParameter(
					0, 1, "CLI_isaDisassemble() binary at -entry has no SPIR-V; the offline path lowers SPIR-V "
					"only, so use '-asic live' "
					"to run this DXIL on a real device"
				));
		}

		else {

			//No -entry: require exactly one SPIR-V binary across the whole oiSH

			U64 spvCount = 0;

			for(U64 i = 0; i < shFile.binaries.length; ++i)
				if(Buffer_length(shFile.binaries.ptr[i].binaries[EGfxBinaryType_SPIRV])) {
					chosen = &shFile.binaries.ptr[i];
					++spvCount;
				}

			if(!spvCount)
				retError(clean, Error_notFound(
					0, 0, "CLI_isaDisassemble() oiSH has no SPIR-V binary; the offline path lowers SPIR-V "
					"only, so use '-asic live' "
					"to run this DXIL on a real device"
				));

			if(spvCount > 1)
				retError(clean, Error_invalidParameter(
					0, 1, "CLI_isaDisassemble() oiSH has multiple binaries; pass -entry <index> "
					"(list them with 'OxC3 file data -input <oiSH> --bin')"
				));
		}

		const Buffer spv = chosen->binaries[EGfxBinaryType_SPIRV];
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
