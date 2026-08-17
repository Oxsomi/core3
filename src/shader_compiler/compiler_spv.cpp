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

//shader_compiler/compiler_spv.cpp

#include "shader_compiler/compiler.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/base/string_read_helper.h"
#include "types/base/allocator.h"
#include "types/base/c8.h"
#include "types/base/mathi.h"

#include "optimizer.hpp"
#include "linker.hpp"
#include "SPIRV-Reflect/spirv_reflect.h"
#include "compiler_spv_internal.hpp"

Bool Compiler_spvToolsCallback(
	spv_message_level_t level,
	const C8 *source,
	const spv_position_t &position,
	const C8 *msg,
	ListCompileError *errors,
	Bool *success,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	CharString error = CharString_createNull();
	CharString file = CharString_createNull();

	const C8 *format = "%s:L#%zu:%zu (index: %zu): %s";

	switch(level) {

		case SPV_MSG_FATAL:
		case SPV_MSG_INTERNAL_ERROR:
		case SPV_MSG_ERROR:
		case SPV_MSG_WARNING: {

			if(position.column > U8_MAX || position.line >= (1 << (7 + 16)))
				retError(clean, Error_invalidState(0, "Compiler_linkSPIRV() referenced line or colum out of bounds"));

			gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(source), alloc, &file, e_rr));
			gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(msg), alloc, &error, e_rr));

			CompileError err = CompileError{
						
				.lineId = (U16) position.line,

				.typeLineId = (U8)(
					((level == SPV_MSG_WARNING ? ECompileErrorType_Warn : ECompileErrorType_Error) << 7) |
					(position.line >> 16)
				),

				.lineOffset = (U8) position.column,

				.error = error,
				.file = file
			};

			gotoIfError3(clean, ListCompileError_pushBack(errors, err, alloc, e_rr));
			error = CharString_createNull();
			file = CharString_createNull();
			break;
		}

		default:
		case SPV_MSG_INFO:
		case SPV_MSG_DEBUG:
			Log_debugLn(alloc, format, source, position.line, position.column, position.index, msg);
			break;
	}

clean:

	CharString_free(&error, alloc);
	CharString_free(&file, alloc);

	if(!s_uccess)
		Log_errorLn(alloc,
			"Couldn't return error as CompileError: %s:L#%zu:%zu (index: %zu): %s",
			format, source, position.line, position.column, position.index, msg
		);

	*success = s_uccess;
	return s_uccess;
}

extern "C" Bool Compiler_processSPIRV(
	Buffer *result,
	ListSHRegisterRuntime *registers,
	Bool isDebug,
	Bool keepRegisters,
	const SHBinaryIdentifier *toCompile,
	SpinLock *lock,
	const ListSHEntryRuntime *entries,
	Bool isLib,
	ESHExtension *demotions,
	ListCompileError *errors,
	const Allocator *alloc,
	Error *e_rr
) {

	//Ensure we have a valid SPIRV file

	const void *resultPtr = result->ptr;
	U64 binLen = Buffer_length(*result);

	Bool s_uccess = true;
	SpvReflectResult res = SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
	ESHExtension exts = ESHExtension_None;
	SpvReflectShaderModule spvMod{};
	Bool isRt = !!(toCompile->extensions & ESHExtension_RayQuery);

	//Mesh/task shaders also need the >= 1.4 optimizer (SPV_EXT_mesh_shader).
	//Detected from the execution model below.

	Bool isMeshTask = false;

	//Linalg (cooperative vectors) is compiled at vulkan1.3 (SPIR-V 1.6),
	// so its storage buffers get the StorageBuffer class the matmul requires,
	// so its optimizer must run at a matching (>= 1.6) environment.

	ESHExtension linalg = (ESHExtension) (
		ESHExtension_CoopVec | ESHExtension_CoopMat | ESHExtension_CoopFP8 | ESHExtension_CoopVecTraining
	);

	Bool isLinalg = !!(toCompile->extensions & linalg);

	spvtools::Optimizer optimizerRt{ SPV_ENV_UNIVERSAL_1_4 };
	spvtools::Optimizer optimizerNoRt{ SPV_ENV_UNIVERSAL_1_3 };
	spvtools::Optimizer optimizerLinalg{ SPV_ENV_UNIVERSAL_1_6 };

	std::vector<U32> tmp;
	std::vector<U32> copied;

	SBFile sbFile{};
	ListCharString strings{};
	U8 inputSemanticCount = 0;

	if(
		binLen < 0x8 ||
		(binLen & 3) ||
		Buffer_readU32(*result, 0, NULL, NULL) != 0x07230203
	)
		retError(clean, Error_invalidState(2, "Compiler_processSPIRV() SPIRV returned is invalid"));

	if(!demotions || !result || !registers)
		retError(clean, Error_nullPointer(0, "Compiler_processSPIRV() demotions, result and registers are required"));

	//Fix alignment up front: the caller's Buffer can be a ref at any address, and the optimizer below indexes
	// it as U32s.

	if ((U64)resultPtr & 3) {
		copied.resize(binLen >> 2);
		Buffer_memcpy(Buffer_createRef(copied.data(), binLen), Buffer_createRefConst(resultPtr, binLen));
		resultPtr = copied.data();
	}

	//Reflect binary information, since our own parser doesn't have the info yet.

	//Deliberately NOT SPV_REFLECT_MODULE_FLAG_NO_COPY.
	//SPIRV-Reflect reads literal strings (OpName/OpString/OpSource) with an unbounded strlen and never
	// checks that an instruction fits inside the module, so it can read past the end of what it is given.
	//With NO_COPY that is our own exactly sized buffer and the read leaves the mapping outright, which is
	// what segfaults on linux arm64; letting it copy into its own allocation puts ordinary heap slack
	// behind the module instead. A few KB of memcpy per shader is nothing next to reflecting it.
	//Both reported upstream, restore NO_COPY once they're fixed:
	// https://github.com/KhronosGroup/SPIRV-Reflect/issues/351 (reads the header before validating size)
	// https://github.com/KhronosGroup/SPIRV-Reflect/issues/352 (unbounded strlen on literal strings)

	res = spvReflectCreateShaderModule2(0, binLen, resultPtr, &spvMod);

	if(res != SPV_REFLECT_RESULT_SUCCESS)
		retError(clean, Error_invalidState(2, "Compiler_processSPIRV() SPIRV returned couldn't be reflected"));

	//Validate capabilities.
	//This makes sure that we only output a binary that's supported by oiSH and no unknown extensions are used.

	for (U64 i = 0; i < spvMod.capability_count; ++i) {

		ESHExtension ext = (ESHExtension)(1 << ESHExtension_Count);
		gotoIfError3(clean, spvMapCapabilityToESHExtension(spvMod.capabilities[i].value, &ext, e_rr));

		//Check if extension was known to oiSH

		if(!(ext >> ESHExtension_Count))
			exts = (ESHExtension) (exts | ext);
	}

	if((toCompile->extensions & exts) != exts)
		retError(clean, Error_invalidState(
			2, "Compiler_processSPIRV() SPIRV contained capability that wasn't enabled by oiSH file (use annotations)"
		));

	//Extensions that can be generated by spvMapCapabilityToESHExtension.
	//This is used to see if demotion is possible

	*demotions = (ESHExtension)((~exts) & ESHExtension_SpirvNative);

	for(U64 i = 0; i < spvMod.push_constant_block_count; ++i)
		if(spvMod.push_constant_blocks[i].offset)
			retError(clean, Error_invalidState(
				2, "Compiler_processSPIRV() oiSH doesn't support push constants with an offset"
			));

	if(spvMod.spec_constant_count)
		retError(clean, Error_invalidState(2, "Compiler_processSPIRV() doesn't support spec constants"));

	if(!isLib && spvMod.entry_point_count != 1)
		retError(clean, Error_invalidState(
			2, "Compiler_processSPIRV() requires to have only 1 entrypoint in binary for compute/gfx"
		));

	//Check entrypoints

	for(U64 i = 0; i < spvMod.entry_point_count; ++i) {

		SpvReflectEntryPoint entrypoint = spvMod.entry_points[i];
		Bool searchPayload = false;
		Bool searchIntersection = false;

		U8 payloadSize = 0, intersectSize = 0;

		CharString name = CharString_createRefCStrConst(entrypoint.name);

		if(!isLib && !CharString_equalsCStringSensitive(&name, "main"))
			retError(clean, Error_invalidState(
				2, "Compiler_processSPIRV() with gfx/compute must have 1 entrypoint named \"main\""
			));

		if(!isLib)
			name = CharString_createRefStrConst(toCompile->entrypoint);

		U32 localSize[3] = { 0 };

		ESHPipelineStage stage = ESHPipelineStage_Count;

		switch (entrypoint.spirv_execution_model) {

			case SpvExecutionModelIntersectionKHR:
				searchIntersection = true;        //Intersection shaders carry only a hit attribute (ReportHit), never a ray payload.
				stage = ESHPipelineStage_IntersectionExt;
				break;

			case SpvExecutionModelAnyHitKHR:
				searchPayload = true;
				stage = ESHPipelineStage_AnyHitExt;
				break;

			case SpvExecutionModelClosestHitKHR:
				searchPayload = true;
				stage = ESHPipelineStage_ClosestHitExt;
				break;

			case SpvExecutionModelMissKHR:
				searchPayload = true;
				stage = ESHPipelineStage_MissExt;
				break;

			case SpvExecutionModelCallableKHR:
				searchPayload = true;
				stage = ESHPipelineStage_CallableExt;
				break;

			case SpvExecutionModelMeshEXT:
			case SpvExecutionModelTaskEXT:
			case SpvExecutionModelGLCompute: {

				switch (entrypoint.spirv_execution_model) {
					case SpvExecutionModelMeshEXT:  stage = ESHPipelineStage_MeshExt;  isMeshTask = true;  break;
					case SpvExecutionModelTaskEXT:  stage = ESHPipelineStage_TaskExt;  isMeshTask = true;  break;
					default:                        stage = ESHPipelineStage_Compute;                      break;

				}

				localSize[0] = entrypoint.local_size.x;
				localSize[1] = entrypoint.local_size.y;
				localSize[2] = entrypoint.local_size.z;
				gotoIfError3(clean, Compiler_validateGroupSize(localSize, e_rr));
				break;
			}

			case SpvExecutionModelRayGenerationKHR:        stage = ESHPipelineStage_RaygenExt;    break;
			case SpvExecutionModelVertex:                  stage = ESHPipelineStage_Vertex;       break;
			case SpvExecutionModelFragment:                stage = ESHPipelineStage_Pixel;        break;
			case SpvExecutionModelGeometry:                stage = ESHPipelineStage_GeometryExt;  break;
			case SpvExecutionModelTessellationControl:     stage = ESHPipelineStage_Hull;         break;
			case SpvExecutionModelTessellationEvaluation:  stage = ESHPipelineStage_Domain;       break;

			default:
				retError(clean, Error_invalidState(
					2, "Compiler_processSPIRV() SPIRV contained unsupported execution model"
				));
		}

		if (searchPayload || searchIntersection)
			for (U64 j = 0; j < entrypoint.interface_variable_count; ++j) {

				SpvReflectInterfaceVariable var = entrypoint.interface_variables[j];

				//Hit/miss shaders carry an IncomingRayPayload; callable shaders carry IncomingCallableData instead.
				//Both are reflected as the entry's payloadSize.

				Bool isPayload =
					var.storage_class == SpvStorageClassIncomingRayPayloadKHR ||
					var.storage_class == SpvStorageClassIncomingCallableDataKHR;

				Bool isIntersection = var.storage_class == SpvStorageClassHitAttributeKHR;

				if(!isPayload && !isIntersection)
					continue;

				//Get struct size

				if(
					!var.type_description ||
					var.type_description->op != SpvOpTypeStruct ||
					var.type_description->type_flags != (SPV_REFLECT_TYPE_FLAG_STRUCT | SPV_REFLECT_TYPE_FLAG_EXTERNAL_BLOCK)
				)
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() struct payload or intersection isn't a struct"
					));

				U64 structSize = 0;
				gotoIfError3(clean, SpvCalculateStructLength(var.type_description, &structSize, e_rr));

				//Validate payload/intersect size

				if (isPayload) {

					if(structSize > 128)
						retError(clean, Error_outOfBounds(
							0, structSize, 128, "Compiler_processSPIRV() payload out of bounds"
						));

					payloadSize = (U8) structSize;
					continue;
				}

				if(structSize > 32)
					retError(clean, Error_outOfBounds(
						0, structSize, 32, "Compiler_processSPIRV() intersection attribute out of bounds"
					));

				intersectSize = (U8) structSize;
			}

		if(searchPayload && !payloadSize)
			retError(clean, Error_invalidState(0, "Compiler_processSPIRV() payload wasn't found in SPIRV"));

		if(searchIntersection && !intersectSize)
			retError(clean, Error_invalidState(0, "Compiler_processSPIRV() intersection attribute wasn't found in SPIRV"));

		if(searchPayload || searchIntersection)
			isRt = true;

		if(stage == ESHPipelineStage_Count)
			retError(clean, Error_invalidState(
				0, "Compiler_processSPIRV() SPIRV entrypoint couldn't be mapped to ESHPipelineStage"
			));

		Bool isStageRt = stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt;
		Bool isGfx = !isStageRt && stage != ESHPipelineStage_WorkgraphExt && stage != ESHPipelineStage_Compute;

		isRt |= isStageRt;

		//Reflect inputs & outputs

		ESBType inputs[16] = {};
		ESBType outputs[16] = {};
		U8 inputSemantics[16] = {};
		U8 outputSemantics[16] = {};

		if (isGfx) {

			for (U64 j = 0; j < (U64)entrypoint.input_variable_count + entrypoint.output_variable_count; ++j) {

				Bool isOutput = j >= entrypoint.input_variable_count;

				const SpvReflectInterfaceVariable *input =
					isOutput ? entrypoint.output_variables[j - entrypoint.input_variable_count] :
					entrypoint.input_variables[j];

				if(input->built_in != (SpvBuiltIn)-1)        //We don't care about builtins
					continue;

				ESBType *inputType = isOutput ? outputs : inputs;
				U8 *inputSemantic = isOutput ? outputSemantics : inputSemantics;

				//A dual source second output shares location 0 with the first and is distinguished by the Index
				// decoration (surfaced by the spirv-reflect fork; U32_MAX when absent).
				//Its slot is location + index, which is exactly where DXIL's reflection puts SV_Target1, and
				// matching slots is what keeps the two backends' reflections combinable into one oiSH.

				U32 slot = input->location;

				if(isOutput && input->index != U32_MAX)
					slot += input->index;

				if(slot >= 16)
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() input/output location out of bounds (allowed up to 16)"
					));

				if(inputType[slot])
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() input/output location is already defined"
					));

				gotoIfError3(clean, SpvReflectFormatToESBType(input->format, &inputType[slot], e_rr));

				//Grab and parse semantic

				if(!input->name || !input->name[0])
					continue;

				CharString semantic = CharString_createRefCStrConst(input->name);
				CharString inVar = CharString_createRefCStrConst("in.var.");
				CharString outVar = CharString_createRefCStrConst("out.var.");

				Bool isInVar = CharString_startsWithStringSensitive(&semantic, &inVar, 0);
				Bool isOutVar = CharString_startsWithStringSensitive(&semantic, &outVar, 0);

				if(!isInVar && !isOutVar)
					continue;

				U64 offset = isInVar ? CharString_length(inVar) : CharString_length(outVar);
				semantic.ptr += offset;
				semantic.lenAndNullTerminated -= offset;

				U64 semanticValue = 0;

				U64 semanticl = CharString_length(semantic);
				U64 firstSemanticId = semanticl;

				while(firstSemanticId && C8_isDec(CharString_getAt(semantic, firstSemanticId - 1))) {
					--firstSemanticId;
				}

				if(!firstSemanticId)
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() bitfield accidentally detected as semantic"
					));

				CharString semanticValueStr = CharString_createRefSizedConst(
					semantic.ptr + firstSemanticId, semanticl - firstSemanticId, true
				);

				CharString realSemanticName = CharString_createRefSizedConst(
					semantic.ptr, firstSemanticId, firstSemanticId == semanticl
				);

				if (firstSemanticId != semanticl && !CharString_parseDec(semanticValueStr, &semanticValue))
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() couldn't parse semantic value"
					));

				U64 semanticName = 0;

				if(
					!CharString_equalsCStringInsensitive(&realSemanticName, "SV_TARGET") &&
					!CharString_equalsCStringInsensitive(&realSemanticName, "TEXCOORD")
				) {
					U64 start = isOutput ? inputSemanticCount : 0;
					U64 end = isOutput ? strings.length : inputSemanticCount;
					U64 k = start;

					for(; k < end; ++k)
						if(CharString_equalsStringInsensitive(&strings.ptr[k], &realSemanticName))
							break;

					if(k == end)
						gotoIfError3(clean, ListCharString_pushBack(&strings, realSemanticName, alloc, e_rr));

					if(!isOutput && k == end)
						++inputSemanticCount;

					semanticName = (k - start) + 1;
				}

				if(semanticName >= 16)
					retError(clean, Error_invalidState(1, "Compiler_processSPIRV() unique semantic name out of bounds"));

				if(semanticValue >= 15)
					retError(clean, Error_invalidState(1, "Compiler_processSPIRV() unique semantic id out of bounds"));

				//The index is kept even for a default semantic name.
				//Zeroing it here threw away the 1 in SV_TARGET1 (and the 3 in TEXCOORD3), while the DXIL path
				// keeps signature.SemanticIndex unconditionally, so the two backends recorded different
				// outputSemanticNames for the same shader and SHFile_combine refused to merge them.
				//semanticName is already folded into the high nibble above, so this matches DXIL's encoding in
				// both cases: (name << 4) | index for a named semantic, plain index for a default one.
				//SV_TARGET0 still encodes as 0, so nothing single target changes.

				semanticValue |= (U8)(semanticName << 4);
				inputSemantic[slot] = (U8) semanticValue;
			}
		}

		//Grab resources

		for (U64 j = 0; j < entrypoint.descriptor_set_count; ++j) {

			SpvReflectDescriptorSet descriptorSet = entrypoint.descriptor_sets[j];

			for (U64 k = 0; k < descriptorSet.binding_count; ++k) {

				SpvReflectDescriptorBinding *binding = descriptorSet.bindings[k];

				if(!binding)
					continue;

				gotoIfError3(clean, Compiler_convertRegisterSPIRV(registers, binding, descriptorSet.set, alloc, e_rr));
			}
		}

		if(entrypoint.used_push_constant_count > 1)
			retError(clean, Error_invalidState(
				2, "Compiler_processSPIRV() not supporting more than 1 set of push constants per entrypoint"
			));

		for (U64 j = 0; j < entrypoint.used_push_constant_count; ++j) {

			//Find push constant

			U64 k = 0;

			for(; k < spvMod.push_constant_block_count; ++k)
				if(entrypoint.used_push_constants[j] == spvMod.push_constant_blocks[k].spirv_id)
					break;
		
			if(k == spvMod.push_constant_block_count)
				retError(clean, Error_invalidState(
					2, "Compiler_processSPIRV() push constants not found"
				));

			SpvReflectBlockVariable var = spvMod.push_constant_blocks[k];

			gotoIfError3(clean, Compiler_convertShaderBufferSPIRV(
				&var,
				false,
				alloc,
				&sbFile,
				e_rr
			));

			CharString bufferName = CharString_createRefCStrConst(var.name);
			SHBindings bindings = SHBindings{};

			for(U64 l = 0; l < ESHBinaryType_Count; ++l)
				bindings.arrU64[l] = U64_MAX;

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_PushConstants,
				false,
				(U8)(1 << ESHBinaryType_SPIRV),
				&bufferName,
				NULL,
				&sbFile,
				bindings,
				alloc,
				e_rr
			));
		}

		gotoIfError3(clean, Compiler_finalizeEntrypoint(
			localSize, payloadSize, intersectSize, 0,
			inputs, outputs,
			inputSemanticCount, &strings, inputSemantics, outputSemantics,
			&name,
			lock, entries,
			alloc, e_rr
		));
	}

	//keep-all: unused resources aren't referenced by any entrypoint, so the per entrypoint walks above missed them.
	//Convert every module level binding that wasn't already claimed; their variables survive both optimizers
	// (DXC through -fspv-preserve-bindings, ours through the preserve option below).

	if (keepRegisters)
		for (U64 i = 0; i < spvMod.descriptor_binding_count; ++i) {

			SpvReflectDescriptorBinding *binding = &spvMod.descriptor_bindings[i];

			if(!binding)
				continue;

			Bool found = false;

			for (U64 j = 0; j < registers->length && !found; ++j) {

				SHBinding shBinding = registers->ptr[j].reg.bindings.arr[ESHBinaryType_SPIRV];
				found = shBinding.space == binding->set && shBinding.binding == binding->binding;
			}

			if(!found)
				gotoIfError3(clean, Compiler_convertRegisterSPIRV(registers, binding, binding->set, alloc, e_rr));
		}

	//Strip debug and optimize

	{
		ESpirvVersion spvVer = Compiler_requiredSpirvVersion(isRt, isLinalg, isMeshTask);
		spvtools::Optimizer &optimizer =
			spvVer == ESpirvVersion_1_6 ? optimizerLinalg : (spvVer == ESpirvVersion_1_4 ? optimizerRt : optimizerNoRt);

		optimizer.SetMessageConsumer(
			[alloc, errors, &s_uccess, e_rr](
				spv_message_level_t level, const C8 *source, const spv_position_t &position, const C8 *msg
			) -> void {
				(void) Compiler_spvToolsCallback(level, source, position, msg, errors, &s_uccess, alloc, e_rr);
			}
		);

		optimizer.RegisterPassesFromFlags({ "-O", "--legalize-hlsl" });

		if (!isDebug)
			optimizer.RegisterPass(spvtools::CreateStripDebugInfoPass()).RegisterPass(spvtools::CreateStripReflectInfoPass());

		//Alignment was already fixed at the top of the function, before the decoration scan.

		//keep-all: our own optimizer would otherwise strip the unused bindings DXC just preserved

		spvtools::OptimizerOptions optimizerOptions;
		optimizerOptions.set_preserve_bindings(keepRegisters);

		if(!optimizer.Run((const U32*)resultPtr, binLen >> 2, &tmp, optimizerOptions))
			retError(clean, Error_invalidState(0, "Compiler_processSPIRV() stripping spirv failed"));
	}

	//The module is destroyed before *result is freed and reallocated below.
	//Its strings point into its own copy of the SPIR-V now, but the ordering was a real use after free
	// back when it was created with NO_COPY, and it stays correct rather than incidentally correct.
	//Zeroing it keeps the destroy at clean a no-op, since it early outs on a NULL _internal.

	spvReflectDestroyShaderModule(&spvMod);
	spvMod = SpvReflectShaderModule{};

	Buffer_free(result, alloc);
	gotoIfError3(clean, Buffer_createCopy(Buffer_createRefConst(tmp.data(), (U64)tmp.size() << 2), alloc, result, e_rr));

clean:

	ListCharString_freeUnderlying(&strings, alloc);
	SBFile_free(&sbFile, alloc);

	spvReflectDestroyShaderModule(&spvMod);
	return s_uccess;
}

extern "C" Bool Compiler_disassembleSPIRV(Buffer buf, const Allocator *alloc, CharString *result, Error *e_rr) {
	
	Bool s_uccess = true;
	U64 binLen = Buffer_length(buf);
	const void *resultPtr = buf.ptr;

	//Disassemble at the highest env so SPIR-V 1.6 modules (cooperative vectors/matrix) and their opcodes are known;
	//disassembling lower-version modules at a higher env is backward-compatible.
	spvtools::SpirvTools tool{ SPV_ENV_UNIVERSAL_1_6 };
	std::string str;

	spv_binary_to_text_options_t opts = (spv_binary_to_text_options_t) (
		SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES |
		SPV_BINARY_TO_TEXT_OPTION_NESTED_INDENT |
		SPV_BINARY_TO_TEXT_OPTION_REORDER_BLOCKS |
		SPV_BINARY_TO_TEXT_OPTION_COMMENT |
		SPV_BINARY_TO_TEXT_OPTION_SHOW_BYTE_OFFSET |
		SPV_BINARY_TO_TEXT_OPTION_INDENT
	);

	std::vector<U32> copied;

	if(
		binLen < 0x8 ||
		(binLen & 3) ||
		Buffer_readU32(buf, 0, NULL, NULL) != 0x07230203
	)
		retError(clean, Error_invalidState(0, "Compiler_createDisassembly() SPIRV is invalid"));

	if ((U64)resultPtr & 3) {        //Fix alignment
		copied.resize(binLen >> 2);
		Buffer_memcpy(Buffer_createRef(copied.data(), binLen), Buffer_createRefConst(resultPtr, binLen));
		resultPtr = copied.data();
	}

	if(!tool.Disassemble((const U32*)resultPtr, binLen >> 2, &str, opts))
		retError(clean, Error_invalidOperation(0, "Compiler_createDisassembly() SPIRV couldn't be disassembled"));

	gotoIfError3(clean, CharString_createCopy(CharString_createRefSizedConst(str.c_str(), str.size(), true), alloc, result, e_rr));

clean:
	return s_uccess;
}

extern "C" Bool Compiler_assembleSPIRV(CharString text, const Allocator *alloc, Buffer *result, Error *e_rr) {

	Bool s_uccess = true;

	//Assemble at the highest env (superset grammar) so text from SPIR-V 1.6 modules (cooperative vectors/matrix)
	//parses; the assembler stamps the version the text itself requests.
	spvtools::SpirvTools tool{ SPV_ENV_UNIVERSAL_1_6 };
	std::vector<U32> spirv;

	if(!result)
		retError(clean, Error_nullPointer(2, "Compiler_assembleSPIRV()::result is required"));

	if(!CharString_length(text))
		retError(clean, Error_invalidParameter(0, 0, "Compiler_assembleSPIRV()::text is empty"));

	if(!tool.Assemble(std::string(text.ptr, (size_t) CharString_length(text)), &spirv))
		retError(clean, Error_invalidOperation(0, "Compiler_assembleSPIRV() SPIRV text couldn't be assembled"));

	gotoIfError3(clean, Buffer_createCopy(
		Buffer_createRefConst(spirv.data(), (U64) spirv.size() * sizeof(U32)), alloc, result, e_rr
	));

clean:
	return s_uccess;
}

extern "C" Bool Compiler_getUniqueEntrypointsSPIRV(
	const Compiler *compiler,
	Buffer binary,
	Bool showAll,
	ListCompilerEntrypoint *uniqueEntrypoints,
	const Allocator *alloc,
	Error *e_rr
) {

	(void) compiler;

	Bool s_uccess = true;
	const void *resultPtr = binary.ptr;
	U64 binLen = Buffer_length(binary);

	SpvReflectResult res = SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
	SpvReflectShaderModule spvMod{};
	Bool alreadyContainsLib = false;    //Avoid re-inserting uniqueEntrypoint of lib
	
	if(
		binLen < 0x8 ||
		(binLen & 3) ||
		Buffer_readU32(binary, 0, NULL, NULL) != 0x07230203
	)
		retError(clean, Error_invalidState(2, "Compiler_getUniqueEntrypointsSPIRV() SPIRV returned is invalid"));

	//Reflect binary information, since our own parser doesn't have the info yet.

	//Deliberately NOT SPV_REFLECT_MODULE_FLAG_NO_COPY.
	//SPIRV-Reflect reads literal strings (OpName/OpString/OpSource) with an unbounded strlen and never
	// checks that an instruction fits inside the module, so it can read past the end of what it is given.
	//With NO_COPY that is our own exactly sized buffer and the read leaves the mapping outright, which is
	// what segfaults on linux arm64; letting it copy into its own allocation puts ordinary heap slack
	// behind the module instead. A few KB of memcpy per shader is nothing next to reflecting it.
	//Both reported upstream, restore NO_COPY once they're fixed:
	// https://github.com/KhronosGroup/SPIRV-Reflect/issues/351 (reads the header before validating size)
	// https://github.com/KhronosGroup/SPIRV-Reflect/issues/352 (unbounded strlen on literal strings)

	res = spvReflectCreateShaderModule2(0, binLen, resultPtr, &spvMod);

	if(res != SPV_REFLECT_RESULT_SUCCESS)
		retError(clean, Error_invalidState(2, "Compiler_getUniqueEntrypointsSPIRV() SPIRV returned couldn't be reflected"));

	for(U32 i = 0; i < spvMod.entry_point_count; ++i) {

		SpvReflectEntryPoint entrypoint = spvMod.entry_points[i];
		
		const C8 *name = entrypoint.name;

		ESHPipelineStage stage = ESHPipelineStage_Count;

		switch (entrypoint.spirv_execution_model) {

			case SpvExecutionModelRayGenerationKHR:        stage = ESHPipelineStage_RaygenExt;        break;
			case SpvExecutionModelIntersectionKHR:         stage = ESHPipelineStage_IntersectionExt;  break;
			case SpvExecutionModelAnyHitKHR:               stage = ESHPipelineStage_AnyHitExt;        break;
			case SpvExecutionModelClosestHitKHR:           stage = ESHPipelineStage_ClosestHitExt;    break;
			case SpvExecutionModelMissKHR:                 stage = ESHPipelineStage_MissExt;          break;
			case SpvExecutionModelCallableKHR:             stage = ESHPipelineStage_CallableExt;      break;

			case SpvExecutionModelVertex:                  stage = ESHPipelineStage_Vertex;           break;
			case SpvExecutionModelFragment:                stage = ESHPipelineStage_Pixel;            break;
			case SpvExecutionModelGeometry:                stage = ESHPipelineStage_GeometryExt;      break;
			case SpvExecutionModelTessellationControl:     stage = ESHPipelineStage_Hull;             break;
			case SpvExecutionModelTessellationEvaluation:  stage = ESHPipelineStage_Domain;           break;
		
			case SpvExecutionModelGLCompute:               stage = ESHPipelineStage_Compute;          break;

			case SpvExecutionModelTaskEXT:
			case SpvExecutionModelTaskNV:                  stage = ESHPipelineStage_TaskExt;          break;

			case SpvExecutionModelMeshEXT:
			case SpvExecutionModelMeshNV:                  stage = ESHPipelineStage_MeshExt;          break;

			default:
				retError(clean, Error_invalidState(0, "Compiler_getUniqueEntrypointsSPIRV() had an invalid shader type"));
		}

		Bool insertPlain = false;

		if(showAll)
			insertPlain = true;

		else {

			if((stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt) || stage == ESHPipelineStage_WorkgraphExt) {

				if(!alreadyContainsLib)
					gotoIfError3(clean, ListCompilerEntrypoint_pushBack(
						uniqueEntrypoints, CompilerEntrypoint{ .stage = ESHPipelineStage_Count }, alloc, e_rr));

				alreadyContainsLib = true;
			}

			else insertPlain = true;
		}

		if(insertPlain) {

			gotoIfError3(clean, ListCompilerEntrypoint_pushBack(
				uniqueEntrypoints, CompilerEntrypoint{ .stage = stage }, alloc, e_rr));

			gotoIfError3(clean, CharString_createCopy(
				CharString_createRefCStrConst(name), alloc, &ListCompilerEntrypoint_last(*uniqueEntrypoints)->name, e_rr
			));
		}
	}

clean:
	return s_uccess;
}

extern "C" Bool Compiler_linkSPIRV(
	const Compiler *compiler,
	const ListBuffer *inputs,
	const ListSHUniformRuntime *uniforms,
	Buffer uniformData,
	const CharString *entrypoint,
	ESHPipelineStage stage,
	ESHExtension exts,
	ListCompileError *errors,
	Buffer *result,
	const Allocator *alloc,
	Error *e_rr
) {

	(void) compiler;
	Bool s_uccess = true;

	Bool isRt = stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt;

	isRt |= !!(exts & ESHExtension_RayQuery);

	//Cooperative vectors/matrix compile at vulkan1.3 (SPIR-V 1.6),
	// so their optimizer/validator must match (mirrors Compiler_processSPIRV).
	//1.6 is a superset of 1.4, so RT + coop is safe too.
	Bool isLinalg = !!(exts & (ESHExtension_CoopVec | ESHExtension_CoopMat | ESHExtension_CoopFP8 | ESHExtension_CoopVecTraining));

	Bool isMeshTask = stage == ESHPipelineStage_MeshExt || stage == ESHPipelineStage_TaskExt;

	ESpirvVersion spvVer = Compiler_requiredSpirvVersion(isRt, isLinalg, isMeshTask);
	spv_target_env env =
		spvVer == ESpirvVersion_1_6 ? SPV_ENV_UNIVERSAL_1_6 :
		(spvVer == ESpirvVersion_1_4 ? SPV_ENV_UNIVERSAL_1_4 : SPV_ENV_UNIVERSAL_1_3);

	spvtools::Optimizer opt(env);

	//Link (or just use the already linked binary)

	std::vector<U32> linkedBin;
	std::vector<U32> alignedBin;
	const U32 *linkedBinPtr = NULL;
	U64 linkedBinSiz = 0;

	if (inputs->length > 1)
		retError(clean, Error_invalidParameter(1, 0, "Compiler_linkSPIRV() linking multiple spirv modules isn't supported"));

	linkedBinPtr = (const U32*) inputs->ptr[0].ptr;
	linkedBinSiz = Buffer_length(inputs->ptr[0]);

	if (linkedBinSiz & 3)
		retError(clean, Error_invalidState(0, "Compiler_linkSPIRV() binary provided was not a U32[]"));

	linkedBinSiz >>= 2;

	//Fix alignment the same way Compiler_processSPIRV and Compiler_disassembleSPIRV do: the input is a caller
	// Buffer that can be a ref at any address, and the optimizer below reads it as U32s.

	if ((U64)(const void*)linkedBinPtr & 3) {
		alignedBin.resize(linkedBinSiz);
		Buffer_memcpy(Buffer_createRef(alignedBin.data(), linkedBinSiz << 2), inputs->ptr[0]);
		linkedBinPtr = alignedBin.data();
	}

	//Run optimizer to get rid of uniforms

	if (uniforms->length || CharString_length(*entrypoint)) {

		//TODO: Think about this; what if a binary with RT enabled produces a non RT entrypoint.
		//We need to produce SPV1.4 but afterwards we need to go back to SPV1.3?
		//Maybe we need to disallow this since we can't go backwards.

		opt.SetMessageConsumer(
			[alloc, errors, e_rr, &s_uccess](
				spv_message_level_t level, const C8 *source, const spv_position_t &position, const C8 *msg
			) -> void {
				(void) Compiler_spvToolsCallback(level, source, position, msg, errors, &s_uccess, alloc, e_rr);
			}
		);
		
		//TODO:
		//if (needsEntrypointStrip)
		//    opt.RegisterPass(spvtools::CreateStripAllButOneEntryPointPass(as string entrypoint));        //Entrypoint strip

		//Resolve spec constants to real constants

		if (uniforms->length) {

			std::unordered_map<U32, std::vector<U32>> uniformMap;

			for (U32 i = 0; i < (U32) uniforms->length; ++i) {
				
				//Uniform info

				SHUniformRuntime uniform = uniforms->ptr[i];

				if(uniform.typeIdShort >= ETypeId_Max)
					retError(clean, Error_invalidState(2, "Compiler_linkSPIRV() typeIdShort out of bounds"));

				TypeId typeId = ETypeId_arr[uniform.typeIdShort];
				U64 len = ETypeId_getBytes(typeId);
			
				if(uniform.dataOffset + len > Buffer_length(uniformData))
					retError(clean, Error_invalidState(2, "Compiler_linkSPIRV() uniformData out of bounds"));

				U8 width = ETypeId_getWidth(typeId);
				U8 height = ETypeId_getHeight(typeId);
				EDataType dt = ETypeId_getDataType(typeId);
				EDataTypeStride dts = ETypeId_getDataTypeStride(typeId);

				Bool isBool = dt == EDataType_Bool;

				Bool isX8 = dts == EDataTypeStride_8;
				Bool isX16 = dts == EDataTypeStride_16;

				Bool is16 = (exts & ESHExtension_16BitTypes) && (isX16 || (isX8 && !isBool));

				Bool is64 = dts == EDataTypeStride_64;

				U32 stride16 = is64 ? 4 : (is16 ? 1 : 2);        //width in U16s
				uniformMap[i].resize((width * height * stride16 + 1) >> 1);

				//Copy into uniformMap; normally this is quite straightforward.
				//Except for bools and 8-bit types; not natively supported.
				//Another exception is 16-bit while 16-bit types aren't supported.
				//These all get expanded to either 16-bit (if supported, unless bool) or 32-bit.

				U32 *asU32 = uniformMap[i].data();
				U16 *asU16 = (U16*) asU32;

				const U8 *inputAsU8 = uniformData.ptr + uniform.dataOffset;

				//dataOffset is an unpadded running byte sum, so a U16 view of the input can be misaligned
				// (e.g. a U8 uniform declared before a U16 one puts it at an odd offset).
				//The words are copied to an aligned local instead; 16 elements is the widest expandable
				// uniform (a 4x4 matrix) and Buffer_memcpy clamps to the smaller side.

				U16 inputU16Copy[16] = {};
				Buffer_memcpy(
					Buffer_createRef(inputU16Copy, sizeof(inputU16Copy)),
					Buffer_createRefConst(inputAsU8, len)
				);

				const U16 *inputAsU16 = inputU16Copy;

				if (isBool || isX8 || (!is16 && isX16))
					for (U64 j = 0; j < width * height; ++j) {

						if (is16)                         //Expand 8-bit to 16-bit
							asU16[j] = inputAsU8[j];

						else if (isX8)                    //Expand 8-bit to 32-bit
							asU32[j] = inputAsU8[j];

						else if (isBool)                  //Expand B1 to 32-bit
							asU32[j] = (inputAsU16[0] >> j) & 1;

						else asU32[j] = inputAsU16[j];    //Expand 16-bit to 32-bit
					}

				//Size of this uniform's own storage, not the number of uniforms in the map.
				//Buffer_memcpy clamps to the smaller side, so getting this wrong silently truncated
				// anything wider than the uniform count (e.g. a lone F64x4 kept only its first 4 bytes).

				else Buffer_memcpy(
					Buffer_createRef(asU32, uniformMap[i].size() << 2),
					Buffer_createRefConst(uniformData.ptr + uniform.dataOffset, len)
				);
			}

			opt.RegisterPass(spvtools::CreateSetSpecConstantDefaultValuePass(uniformMap));
			opt.RegisterPass(spvtools::CreateFreezeSpecConstantValuePass());
		}

		//Remove dead code and resolve branches generated by spec constants or entrypoint strip

		opt.RegisterPerformancePasses();

		std::vector<U32> tmp;
		if(!opt.Run(linkedBinPtr, linkedBinSiz, &tmp))
			retError(clean, Error_invalidState(0, "Compiler_linkSPIRV() couldn't run optimizer"));

		//Note: we don't strip reflection here, Compiler_process will handle that.

		linkedBin = std::move(tmp);
		linkedBinPtr = linkedBin.data();
		linkedBinSiz = linkedBin.size();
	}
	
	//Output to final target

	gotoIfError3(clean, Buffer_resize(result, linkedBinSiz << 2, false, false, alloc, e_rr));

	//Length comes from the source, not the destination: taking it from *result only happens to be correct
	// while callers guarantee an empty result (Buffer_resize early outs when the length already matches).

	Buffer_memcpy(
		*result,
		Buffer_createRefConst(linkedBinPtr, linkedBinSiz << 2)
	);

clean:
	return s_uccess;
}
