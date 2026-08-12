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
#include "shader_compiler/spirv_isa.h"

#ifdef CLI_GRAPHICS
	#include "graphics/generic/instance.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/pipeline_layout.h"
	#include "graphics/generic/descriptor_layout.h"
	#include "graphics/generic/device_info.h"
#endif

//Prints the AMD gfx targets the bundled amdllpc can actually compile for (probed live, so the list matches what this
//build accepts). Used by 'isa devices', the '?' shorthand, and as a hint after an unknown -asic. This is why OxC3 no
//longer spawns rga.exe at all: amdllpc + amdgpu-dis do the disassembly, and amdllpc itself reports its target set.

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

//Resolves an -asic for any ISA path. "?" prints the device list and sets *handled so the caller stops (no error).
//Any other value is left for rga to match - it accepts gfx targets, arch names and partial or full marketing names
//(e.g. "gfx1100", "9070 XT", "AMD Radeon RX 9070 XT") - so *handled stays false and the caller proceeds; a genuinely
//unknown ASIC is reported by rga when it runs (and CLI_isaDisassembleSpirv then re-prints the device list).

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

//Disassembles a SPIR-V module to AMD ISA text for `asic`, returning the ISA in `isaOut` (caller frees). The actual
//amdllpc + amdgpu-dis driving lives in the shared SpvISA_ module (so this and the corpus ISA snapshot test produce
//identical output); here we just reject stages with no offline path early, with a clearer error than amdllpc's.

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

	//Print a pipeline's executables: the driver's numeric statistics (VGPRs/SGPRs/...) then the ISA disassembly text.

	static void CLI_isaPrintExecutables(const ListPipelineExecutable *execs) {

		for(U64 i = 0; i < execs->length; ++i) {

			const PipelineExecutable *e = &execs->ptr[i];

			Log_debugLnx(
				"; %.*s  subgroup %"PRIu32, (int) CharString_length(e->name), e->name.ptr, e->subgroupSize
			);

			for(U64 j = 0; j < e->statistics.length; ++j) {

				const PipelineStatistic *s = &e->statistics.ptr[j];
				const int nl = (int) CharString_length(s->name);

				switch(s->format) {

					case EPipelineStatisticFormat_Bool:
						Log_debugLnx(";   %.*s = %s", nl, s->name.ptr, s->value ? "true" : "false"); break;

					case EPipelineStatisticFormat_I64:
						Log_debugLnx(";   %.*s = %"PRIi64, nl, s->name.ptr, (I64) s->value); break;

					case EPipelineStatisticFormat_F64: {
						F64 d = *(const F64*) &s->value;
						Log_debugLnx(";   %.*s = %f", nl, s->name.ptr, d); break;
					}

					default:
						Log_debugLnx(";   %.*s = %"PRIu64, nl, s->name.ptr, s->value); break;
				}
			}

			if(CharString_length(e->disassembly))
				Log_debugLnx("%.*s", (int) CharString_length(e->disassembly), e->disassembly.ptr);

			else
				Log_debugLnx(
					";   (this driver returned statistics only, no ISA disassembly. Whether the ISA text is exposed is "
					"driver-dependent: the open-source Mesa drivers (RADV / ANV / Turnip / NVK / PanVK, i.e. Linux) and "
					"AMD's own driver return it; other vendors' proprietary drivers keep it to their own tooling. For "
					"deterministic offline AMD ISA use '-asic <gfxN>' instead of live)"
				);
		}
	}

	//Live ISA: create a real Vulkan device, compile the oiSH's first compute entry into a pipeline with ISA capture,
	//then read back the driver's disassembly + statistics. This is the REAL driver ISA (device + driver dependent, not
	//golden-pinnable), complementing the offline amdllpc goldens.

	static Bool CLI_isaDisassembleLive(SHFile shFile, U64 deviceId, const Allocator *alloc, Error *e_rr) {

		Bool s_uccess = true;

		RefPtrType instanceType = (RefPtrType) { 0 };
		GraphicsInstanceRef *instanceRef = NULL;
		GraphicsDeviceRef *deviceRef = NULL;
		ListGraphicsDeviceInfo infos = (ListGraphicsDeviceInfo) { 0 };
		PipelineRef *pipeline = NULL;
		ListPipelineExecutable execs = (ListPipelineExecutable) { 0 };

		//Load the graphics backend DLLs + register their function tables before touching any graphics object

		Error gErr = Error_none();

		if(!GraphicsInterface_create(&gErr))
			retError(clean, Error_invalidState(0, "CLI_isaDisassembleLive() couldn't create a graphics interface (no driver/ICD?)"));

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
			instanceRef, &infos.ptr[deviceId], EGraphicsDeviceFlags_None, EGraphicsBufferingMode_Default, &deviceRef, e_rr
		));

		if(!(GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features2 & EGraphicsFeatures2_PipelineExecutableInfo))
			retError(clean, Error_unsupportedOperation(
				0, "CLI_isaDisassembleLive() device lacks VK_KHR_pipeline_executable_properties"
			));

		//Find the first compute entrypoint (compute-only for now; graphics/RT are a later slice)

		U64 computeEntry = U64_MAX;

		for(U64 i = 0; i < shFile.entries.length; ++i)
			if(shFile.entries.ptr[i].stage == ESHPipelineStage_Compute) { computeEntry = i; break; }

		if(computeEntry == U64_MAX)
			retError(clean, Error_unsupportedOperation(
				0, "CLI_isaDisassembleLive() no compute entrypoint (live disassembly currently supports compute)"
			));

		CharString entryName = shFile.entries.ptr[computeEntry].name;

		U32 entry = GraphicsDeviceRef_getFirstShaderEntry(
			deviceRef, &shFile, &entryName, NULL, NULL, ESHExtension_None, ESHExtension_None
		);

		if(entry == U32_MAX)
			retError(clean, Error_invalidState(0, "CLI_isaDisassembleLive() couldn't resolve the compute entry for this device"));

		//Create the pipeline with ISA capture + read back the executables. The device's default pipeline layout is used;
		//it may not match the shader's resources exactly (a validation warning), but the pipeline still compiles and the
		//driver reports its statistics + ISA, which is what we're after. A per-shader detected layout (for a fully valid
		//pipeline + clean disassembly) is a follow-up; layout detection is currently unstable for some corpus shaders.
		//The SPIRV entrypoint is passed as NULL (-> "main"): OxC3 normalizes a single-entry SPIR-V module's entrypoint to
		//"main" regardless of the HLSL name (e.g. an SHEntry named "mainCompute" is "main" in the SPIR-V).

		const CharString pName = CharString_createRefCStrConst("isa live pipeline");

		gotoIfError3(clean, GraphicsDeviceRef_createPipelineCompute(
			deviceRef, &shFile, &pName, entry, NULL, EPipelineFlags_CaptureISA, NULL, &pipeline, e_rr
		));

		gotoIfError3(clean, GraphicsDeviceRef_getPipelineExecutables(pipeline, alloc, &execs, e_rr));

		Log_debugLnx(
			"; Live ISA for '%.*s' on %s",
			(int) CharString_length(entryName), entryName.ptr,
			infos.ptr[deviceId].name
		);

		CLI_isaPrintExecutables(&execs);

	clean:
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
					0, 0, "CLI_isaDisassemble() -asic live:<index> device index must be a number (list them by running with 'live')"
				));
		}

		if(live) {

			gotoIfError3(clean, File_read(&inputStr, 1 * SECOND, 0, 0, &fileHandleType, &input, e_rr));
			gotoIfError3(clean, MemoryStream_createFromBuffer(
				&input, EMemoryStreamFlags_None, &memoryStreamType, &readStream, e_rr
			));

			U64 liveOff = 0;
			gotoIfError3(clean, SHFile_read((StreamRef*)readStream, &liveOff, false, alloc, &shFile, e_rr));

			gotoIfError3(clean, CLI_isaDisassembleLive(shFile, deviceId, alloc, e_rr));
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

		gotoIfError3(clean, MemoryStream_createFromBuffer(&input, EMemoryStreamFlags_None, &memoryStreamType, &readStream, e_rr));

		U64 off = 0;
		gotoIfError3(clean, SHFile_read((StreamRef*)readStream, &off, false, alloc, &shFile, e_rr));

		//Pick which binary's SPIR-V to disassemble. -entry is an INDEX into the oiSH's binaries: an entrypoint name
		//alone is ambiguous, since the same name can occur several times with different uniforms, defines or
		//extensions. Without -entry, use the sole SPIR-V binary when there's exactly one (list them via 'file data --bin').

		CharString entry = CharString_createNull();
		const Bool hasEntry = ParsedArgs_getArg(args, EOperationHasParameter_EntryShift, &entry, NULL) && CharString_length(entry);

		const SHBinaryInfo *chosen = NULL;

		if(hasEntry) {

			U64 idx = 0;

			if(!CharString_parseU64(entry, &idx))
				retError(clean, Error_invalidParameter(
					0, 0, "CLI_isaDisassemble() -entry must be a binary index (list them with 'OxC3 file data -input <oiSH> --bin')"
				));

			if(idx >= shFile.binaries.length)
				retError(clean, Error_outOfBounds(0, idx, shFile.binaries.length, "CLI_isaDisassemble() -entry index out of bounds"));

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
