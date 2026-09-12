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

//shader_compiler/compiler_helper_link.c

#include "types/container/list_impl.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/container/list_basic_types.h"
#include "types/base/string_read_helper.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
#include "formats/oiSH/sh_file.h"
#include "shader_compiler/compiler.h"
#include "compiler_helper_internal.h"

TList(ListSHEntryRuntime);
TListImpl(ListSHEntryRuntime);

TListImpl(LinkEntry);
TListImpl(CompilerLinkStep);

void ListListSHEntryRuntime_freeUnderlying(ListListSHEntryRuntime *entry, const Allocator *alloc) {

	if(!entry)
		return;

	for(U64 i = 0; i < entry->length; ++i)
		ListSHEntryRuntime_freeUnderlying(&entry->ptrNonConst[i], alloc);

	ListListSHEntryRuntime_free(entry, alloc);
}

void ListLinkEntry_freeUnderlying(ListLinkEntry* entries, const Allocator *alloc) {

	if (!entries)
		return;

	for (U64 i = 0; i < entries->length; ++i) {
		LinkEntry* entry = &entries->ptrNonConst[i];
		Buffer_free(&entry->uniformData, alloc);
		ListU16_free(&entry->runtimeEntries, alloc);
	}

	ListLinkEntry_free(entries, alloc);
}

//The matching half of Compiler_getLinkEntries, over an already discovered entrypoint list: which
// parsed entry each entrypoint is, whether this backend keeps it, which combination the binary's
// identifier maps back to (model promotion included), and one link entry per uniform permutation,
// with RT libs sharing one per permutation across their entries.
//Pure on purpose: Compiler_getLinkSteps runs this identical matching over the parse, where the
// link jobs run it over the compiled binary's reflection, so the two can't diverge.

static Bool Compiler_matchLinkEntries(
	const ListSHEntryRuntime *runtimeEntries,
	const SHBinaryIdentifier *binaryIdentifier,
	EGfxBinaryType binaryType,
	const ListCompilerEntrypoint *entrypoints,
	ListLinkEntry *linkEntries,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListU16 tmpEntries = (ListU16) { 0 };

	Bool isLibTarget =
		binaryIdentifier->stageType >= EGfxPipelineStage_RtStartExt &&
		binaryIdentifier->stageType <= EGfxPipelineStage_RtEndExt;

	ListCompilerEntrypoint entrypointL = *entrypoints;
	ListSHEntryRuntime runtimeEntryL = *runtimeEntries;
	SHBinaryIdentifier ident = *binaryIdentifier;

	for (U64 i = 0; i < entrypointL.length; ++i) {

		CompilerEntrypoint entrypoint = entrypointL.ptr[i];

		//Find entrypoint in input array and ensure it exists / is the same stage

		U64 j = 0;

		for (; j < runtimeEntryL.length; ++j)
			if (CharString_equalsStringSensitive(&entrypoint.name, &runtimeEntryL.ptr[j].entry.name))
				break;

		if (j == runtimeEntryL.length)
			retError(clean, Error_invalidState(
				0, "Compiler_getLinkEntries() had an entrypoint that wasn't defined while parsing but is present in reflection"
			));

		SHEntryRuntime entry = runtimeEntryL.ptr[j];

		if(entry.entry.stage != entrypoint.stage)
			retError(clean, Error_invalidState(
				0, "Compiler_getLinkEntries() had a reflection stage type that mismatched with what was parsed"
			));

		//Skip this entrypoint if its [[oxc::binary(...)]] mask (AND its stage/extension backend support)
		// excludes the backend we're currently compiling.
		//A shader targeting all backends thus only emits an entrypoint for the backends
		// it actually declared / can be expressed on (see SHEntryRuntime_getBinaryTypes).
		//
		//TODO: this only filters the oiSH *reflection* - the entrypoint is not reported for this backend,
		//      but the compiled DXIL/SPIRV blob still physically contains its code
		//      (it was compiled as part of the shared lib).
		//      Truly removing it requires explicitly stripping the entrypoint from the binary and re-running DCE per backend.
		//      Until then, reflection and the actual binary disagree for restricted entrypoints.
		//      (Doesn't apply to the compile-level skip in Compiler_compileShaderFile,
		//      where the whole compile is skipped so the code is genuinely absent.)

		if (!((SHEntryRuntime_getBinaryTypes(&entry) >> binaryType) & 1))
			continue;

		//Ensure we're actually present for what we're currently compiling and that we do really need linking (otherwise skip)
		//This is not relevant for single entrypoints, as they're always only compiled with the defines / extensions they need.
		//However, if you have a mix of extensions and defines, then some entrypoints might not need to be linked again.
		//Example: raygen with both SER and no SER.
		//            This would only be linked once per compilation, but any other shaders should exclude this.
		//            (We don't want to have 2x hit shaders included while only raygen needs these compilations)

		//Check extensions

		Bool containsExtension = !ident.extensions && !entry.extensions.length;
		U16 extensionId = 0;

		for(U64 k = 0; k < entry.extensions.length; ++k)
			if (entry.extensions.ptr[k] == (U32) ident.extensions) {
				containsExtension = true;
				extensionId = (U16) k;
				break;
			}

		if (!containsExtension)            //Extension not found
			continue;

		//Check shader versions

		//The same promotion SHEntryRuntime_asBinaryIdentifier applied on the way out has to be applied on the
		// way back in: the binary carries the model its EXTENSIONS forced, which can be higher than anything
		// the entry declared, so matching the declared list verbatim finds nothing and silently drops the link
		// entry - leaving the entry with fewer binaryIds than it has combinations.
		//That covers the entry declaring no model at all just as much as one declaring several: its
		// combinations sit at OISH_SHADER_MODEL_MIN, the same floor asBinaryIdentifier uses, until an
		// extension raises them.

		const U16 promoted = ESHExtension_minShaderModel(ident.extensions);

		Bool containsShaderVersion =
			!entry.shaderVersions.length && ident.shaderVersion == U16_max(OISH_SHADER_MODEL_MIN, promoted);

		U16 shaderVersion = 0;

		for(U64 k = 0; k < entry.shaderVersions.length; ++k)
			if (U16_max(entry.shaderVersions.ptr[k], promoted) == ident.shaderVersion) {
				containsShaderVersion = true;
				shaderVersion = (U16) k;
				break;
			}

		if (!containsShaderVersion)        //Shader model not found
			continue;

		//Check defines

		Bool containsDefines = !ident.defines.length && !entry.definesPerCompilation.length;
		U16 defineId = 0;

		for (U64 k = 0, l = 0; k < entry.definesPerCompilation.length; ++k) {

			U64 m = entry.definesPerCompilation.ptr[k];

			ListCharString tmp = (ListCharString) { 0 };

			//An entry written with an empty defines annotation still runs this loop once, since
			// definesPerCompilation then holds a single 0, and defineNameValues is left entirely empty.
			//Offsetting its null ptr is undefined even by the zero l is here, and createRefConst refuses both a
			// null ptr and a zero length anyway, so building the ref at all is wrong rather than merely unsafe:
			// it fails the whole compile.
			//An empty combination is just the empty list, which is what tmp already holds.
			//SHEntryRuntime_asBinaryIdentifier guards the same arithmetic the same way.

			if (m)
				gotoIfError3(clean, ListCharString_createRefConst(
					entry.defineNameValues.ptr + (l << 1), m << 1, &tmp, e_rr
				));

			Bool eq = tmp.length == ident.defines.length;        //TODO: ListCharString_equalsUnderlying

			if (eq)
				for (U64 n = 0; n < tmp.length; ++n)
					if (!CharString_equalsStringSensitive(&tmp.ptr[n], &ident.defines.ptr[n])) {
						eq = false;
						break;
					}

			if (eq) {
				defineId = (U16) k;
				containsDefines = true;
				break;
			}

			l += m;
		}

		if (!containsDefines)            //Defines not found
			continue;

		//Go through all uniforms defined by the runtime, since there may be multiple

		U16 shaderVersions = (U16)U64_max(entry.shaderVersions.length, 1);
		U16 extensions = (U16)U64_max(entry.extensions.length, 1);
		U16 defines = (U16)U64_max(entry.definesPerCompilation.length, 1);
		U64 uniformCombos = U64_safeDiv(entry.uniformData.length, entry.uniformStride);

		for (U64 k = 0; k < U64_max(1, uniformCombos); ++k) {

			U64 combinationId = ((k * defines + defineId) * extensions + extensionId) * shaderVersions + shaderVersion;

			//An entry without uniforms still runs this loop once, since uniformCombos is 0 and the max(1, ..) keeps it going.
			//Its ptr is NULL with a 0 stride.
			//Offsetting a null pointer is undefined even by zero, which UBSan flags.
			//So keep it null rather than computing NULL + 0.

			const U8 *uniformPtr =
				entry.uniformData.ptr ? entry.uniformData.ptr + entry.uniformStride * k : NULL;

			LinkEntry linkEntry = (LinkEntry) {
				.uniformData = Buffer_createRefConst(uniformPtr, entry.uniformStride),
				.combinationId = (U16) combinationId
			};

			if (!isLibTarget) {

				linkEntry.entrypointId = (U16)j;

				U64 l = 0;

				for (; l < runtimeEntryL.length; ++l)
					if (CharString_equalsStringSensitive(&runtimeEntryL.ptr[l].entry.name, &entrypoint.name))
						break;

				if(l == runtimeEntryL.length)
					retError(clean, Error_invalidState(
						0, "Compiler_getLinkEntries() had an entrypoint that couldn't be found in runtime entry"
					));

				//The ptr below is the same as linkEntry.entrypointId, except can be used as ptr to avoid intermediate ListU16
				gotoIfError3(clean, ListU16_createRefConst(
					&runtimeEntryL.ptr[l].entry.idOrPadding, 1, &linkEntry.runtimeEntries, e_rr
				));
			}

			else {

				//If RT shader, try to find a previous linkEntry
				//In that case, we just reference the same binary.

				U64 l = 0;

				for (; l < linkEntries->length; ++l) {

					LinkEntry linkEntry2 = linkEntries->ptr[l];

					if (linkEntry2.entrypointId != U16_MAX)
						continue;

					if (!Buffer_eq(linkEntry.uniformData, linkEntry2.uniformData))
						continue;

					break;
				}

				//l is the matching entry the search above found; k is the uniform combination being built,
				// which has nothing to do with an index into linkEntries (and can run past its length)

				if (l != linkEntries->length) {
					gotoIfError3(clean, ListU16_pushBack(&linkEntries->ptrNonConst[l].runtimeEntries, (U16)j, alloc, e_rr));
					continue;
				}

				linkEntry.entrypointId = U16_MAX;
				gotoIfError3(clean, ListU16_pushBack(&tmpEntries, (U16)j, alloc, e_rr));
				linkEntry.runtimeEntries = tmpEntries;
			}

			gotoIfError3(clean, ListLinkEntry_pushBack(linkEntries, linkEntry, alloc, e_rr));
			tmpEntries = (ListU16) { 0 };    //Moved
		}
	}

	if (linkEntries->length >> 32)
		retError(clean, Error_invalidState(0, "Compiler_matchLinkEntries() must return <32bit entries"));

clean:
	ListU16_free(&tmpEntries, alloc);
	return s_uccess;
}

Bool Compiler_getLinkEntries(
	const Compiler *compiler,
	const ListSHEntryRuntime *runtimeEntries,
	const SHBinaryIdentifier *binaryIdentifier,
	EGfxBinaryType binaryType,
	Buffer *binary,
	ListCompilerEntrypoint *entrypoints,
	ListLinkEntry *linkEntries,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool freeEntrypoints = false;
	Bool freeLinkEntries = false;

	Bool isRt =
		binaryIdentifier->stageType >= EGfxPipelineStage_RtStartExt &&
		binaryIdentifier->stageType <= EGfxPipelineStage_RtEndExt;

	Bool isLib = isRt || binaryIdentifier->stageType == EGfxPipelineStage_Count;

	if (!isLib) {
		
		gotoIfError3(clean, ListCompilerEntrypoint_pushBack(
			entrypoints, (CompilerEntrypoint) { .stage = binaryIdentifier->stageType }, alloc, e_rr
		));

		freeEntrypoints = true;

		gotoIfError3(clean, CharString_createCopy(
			binaryIdentifier->entrypoint,
			alloc,
			&ListCompilerEntrypoint_last(*entrypoints)->name,
			e_rr
		));
	}
	
	else gotoIfError3(clean, Compiler_getUniqueEntrypoints(compiler, binaryType, *binary, true, entrypoints, alloc, e_rr));

	freeEntrypoints = true;
	freeLinkEntries = true;

	gotoIfError3(clean, Compiler_matchLinkEntries(
		runtimeEntries, binaryIdentifier, binaryType, entrypoints, linkEntries, alloc, e_rr
	));

clean:

	if (!s_uccess) {

		if (freeEntrypoints)
			ListCompilerEntrypoint_freeUnderlying(entrypoints, alloc);

		if (freeLinkEntries)
			ListLinkEntry_freeUnderlying(linkEntries, alloc);
	}

	return s_uccess;
}

void ListCompilerLinkStep_freeUnderlying(ListCompilerLinkStep *steps, const Allocator *alloc) {

	if(!steps)
		return;

	for (U64 i = 0; i < steps->length; ++i) {

		CompilerLinkStep *step = &steps->ptrNonConst[i];

		CharString_free(&step->profile, alloc);
		CharString_free(&step->uniformsHlsl, alloc);
		ListCharString_freeUnderlying(&step->uniformsArgs, alloc);
		ListCharString_freeUnderlying(&step->libs, alloc);
		ListCharString_freeUnderlying(&step->linkArgs, alloc);
		ListCharString_freeUnderlying(&step->spirvOpt, alloc);
	}

	ListCompilerLinkStep_free(steps, alloc);
}

Bool Compiler_linkProfile(
	Bool isLib, EGfxPipelineStage stage, U16 shaderVersion, const Allocator *alloc, CharString *out, Error *e_rr
) {

	if (isLib)
		return CharString_format(
			alloc, out, e_rr, "lib_%" PRIu8 "_%" PRIu8, (U8)(shaderVersion >> 8), (U8) shaderVersion
		);

	return CharString_format(
		alloc, out, e_rr, "%s_%" PRIu8 "_%" PRIu8,
		EGfxPipelineStage_getStagePrefix(stage), (U8)(shaderVersion >> 8), (U8) shaderVersion
	);
}

Bool Compiler_getLinkSteps(
	const ListSHEntryRuntime *entries,
	const SHBinaryIdentifier *compiled,
	U16 storedEntryId,
	EGfxBinaryType type,
	Bool keepRegisters,
	ListCompilerLinkStep *steps,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListCompilerEntrypoint entrypoints = (ListCompilerEntrypoint) { 0 };
	ListLinkEntry linkEntries = (ListLinkEntry) { 0 };
	CompilerLinkStep step = (CompilerLinkStep) { 0 };
	CharString scratch = CharString_createNull();
	CharString pair = CharString_createNull();
	CharString value = CharString_createNull();

	if(!entries || !compiled || !steps)
		retError(clean, Error_nullPointer(
			!entries ? 0 : (!compiled ? 1 : 5), "Compiler_getLinkSteps()::entries, compiled and steps are required"
		));

	if(steps->ptr)
		retError(clean, Error_invalidOperation(
			0, "Compiler_getLinkSteps()::steps are non zero, could indicate memleak"
		));

	if(storedEntryId >= entries->length)
		retError(clean, Error_outOfBounds(
			2, storedEntryId, entries->length, "Compiler_getLinkSteps()::storedEntryId out of bounds"
		));

	//[[oxc::stage]] entries never link: their one link entry only drives reflection
	// (Compiler_compileLinkJob), so there is no step to describe.

	if (!entries->ptr[storedEntryId].isShaderAnnotation)
		goto clean;

	//Discovery mirrors Compiler_getLinkEntries: a non lib identifier is its own single entrypoint, and
	// a lib's entrypoints are the [shader] entries sharing this compile, which is what the compiled
	// binary's reflection lists for it.

	Bool isRt =
		compiled->stageType >= EGfxPipelineStage_RtStartExt &&
		compiled->stageType <= EGfxPipelineStage_RtEndExt;

	Bool isLib = isRt || compiled->stageType == EGfxPipelineStage_Count;

	if (!isLib) {
		gotoIfError3(clean, ListCompilerEntrypoint_pushBack(
			&entrypoints,
			(CompilerEntrypoint) {
				.name = CharString_createRefStrConst(compiled->entrypoint),
				.stage = compiled->stageType
			},
			alloc, e_rr
		));
	}

	else for (U64 e = 0; e < entries->length; ++e) {

		const SHEntryRuntime *runtime = &entries->ptr[e];

		if (!runtime->isShaderAnnotation || !((SHEntryRuntime_getBinaryTypes(runtime) >> type) & 1))
			continue;

		Bool shares = false;
		U32 compiledCombos = SHEntryRuntime_getCombinationsCompiled(runtime);

		for (U32 jc = 0; jc < compiledCombos && !shares; ++jc) {

			SHBinaryIdentifier compiledId = (SHBinaryIdentifier) { 0 };
			gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(runtime, (U16) jc, &compiledId, e_rr));

			shares = SHBinaryIdentifier_equals(&compiledId, compiled);
		}

		if (shares)
			gotoIfError3(clean, ListCompilerEntrypoint_pushBack(
				&entrypoints,
				(CompilerEntrypoint) {
					.name = CharString_createRefStrConst(runtime->entry.name),
					.stage = runtime->entry.stage
				},
				alloc, e_rr
			));
	}

	gotoIfError3(clean, Compiler_matchLinkEntries(entries, compiled, type, &entrypoints, &linkEntries, alloc, e_rr));

	for (U64 i = 0; i < linkEntries.length; ++i) {

		LinkEntry linkEntry = linkEntries.ptr[i];

		step = (CompilerLinkStep) {
			.runtimeEntryId = storedEntryId,
			.combinationId = linkEntry.combinationId
		};

		gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(
			&entries->ptr[storedEntryId], linkEntry.combinationId, &step.identifier, e_rr
		));

		//The specialization the link job applies: a named entrypoint pins its own stage, a lib link
		// keeps the lib identifier (Compiler_compileLinkJob marks it EGfxPipelineStage_Count).

		Bool isLibStep = linkEntry.entrypointId == U16_MAX;
		EGfxPipelineStage stage = EGfxPipelineStage_Count;

		if (!isLibStep) {

			const SHEntryRuntime *owner = &entries->ptr[linkEntry.entrypointId];

			step.identifier.entrypoint = CharString_createRefStrConst(owner->entry.name);
			step.identifier.stageType = owner->entry.stage;
			stage = (EGfxPipelineStage) owner->entry.stage;
		}

		gotoIfError3(clean, Compiler_linkProfile(
			isLibStep, stage, step.identifier.shaderVersion, alloc, &step.profile, e_rr
		));

		if (type == EGfxBinaryType_DXIL) {

			if (step.identifier.uniforms.length) {

				gotoIfError3(clean, Compiler_buildUniformExportsHLSL(
					&step.identifier.uniforms,
					Buffer_createRefConst(step.identifier.uniformData.ptr, step.identifier.uniformData.length),
					step.identifier.extensions, alloc, &step.uniformsHlsl, e_rr
				));

				gotoIfError3(clean, ListCharString_pushBack(
					&step.uniformsArgs, CharString_createRefCStrConst("-T"), alloc, e_rr
				));

				gotoIfError3(clean, ListCharString_pushBack(
					&step.uniformsArgs, CharString_createRefCStrConst("lib_6_3"), alloc, e_rr
				));

				if (step.identifier.extensions & ESHExtension_16BitTypes)
					gotoIfError3(clean, ListCharString_pushBack(
						&step.uniformsArgs, CharString_createRefCStrConst("-enable-16bit-types"), alloc, e_rr
					));

				gotoIfError3(clean, ListCharString_pushBack(
					&step.libs, CharString_createRefCStrConst("uniforms"), alloc, e_rr
				));
			}

			gotoIfError3(clean, ListCharString_pushBack(
				&step.libs, CharString_createRefCStrConst("0"), alloc, e_rr
			));

			if (keepRegisters)
				gotoIfError3(clean, ListCharString_pushBack(
					&step.linkArgs, CharString_createRefCStrConst(COMPILER_KEEP_ALL_BINDINGS), alloc, e_rr
				));
		}

		//Mirrors Compiler_linkSPIRV: the optimizer only runs when uniforms or an entrypoint ask for it;
		// a lib link without uniforms copies the module as is and the step stays empty.

		else if (step.identifier.uniforms.length || CharString_length(step.identifier.entrypoint)) {

			gotoIfError3(clean, CharString_format(
				alloc, &scratch, e_rr, "--target-env=%s",
				Compiler_spirvTargetEnvName(Compiler_linkSpirvVersion(stage, step.identifier.extensions))
			));

			gotoIfError3(clean, ListCharString_pushBack(&step.spirvOpt, scratch, alloc, e_rr));
			scratch = CharString_createNull();

			if (step.identifier.uniforms.length) {

				gotoIfError3(clean, CharString_createCopy(
					CharString_createRefCStrConst("--set-spec-const-default-value="), alloc, &scratch, e_rr
				));

				//constant_id i is the uniform's declaration order in the compile's spec constant preamble

				for (U64 j = 0; j < step.identifier.uniforms.length; ++j) {

					SHUniformRuntime uniform = step.identifier.uniforms.ptr[j];
					TypeId typeId = ETypeId_arr[uniform.typeIdShort];

					SHValue uniformValue = (SHValue) { 0 };
					Buffer_memcpy(
						Buffer_createRef(&uniformValue, sizeof(uniformValue)),
						Buffer_createRefConst(
							step.identifier.uniformData.ptr + uniform.dataOffset, ETypeId_getBytes(typeId)
						)
					);

					CharString_free(&value, alloc);

					if(!SHValue_stringify(&uniformValue, typeId, alloc, &value, NULL))
						value = CharString_createRefCStrConst("unknown");

					gotoIfError3(clean, CharString_format(
						alloc, &pair, e_rr, "%s%" PRIu64 ":%.*s",
						j ? " " : "", j, (int) CharString_length(value), value.ptr
					));

					gotoIfError3(clean, CharString_appendString(&scratch, &pair, alloc, e_rr));
					CharString_free(&pair, alloc);
				}

				gotoIfError3(clean, ListCharString_pushBack(&step.spirvOpt, scratch, alloc, e_rr));
				scratch = CharString_createNull();

				gotoIfError3(clean, ListCharString_pushBack(
					&step.spirvOpt, CharString_createRefCStrConst("--freeze-spec-const"), alloc, e_rr
				));
			}

			gotoIfError3(clean, ListCharString_pushBack(
				&step.spirvOpt, CharString_createRefCStrConst("-O"), alloc, e_rr
			));
		}

		gotoIfError3(clean, ListCompilerLinkStep_pushBack(steps, step, alloc, e_rr));
		step = (CompilerLinkStep) { 0 };    //Moved
	}

clean:

	if (!s_uccess) {

		CharString_free(&step.profile, alloc);
		CharString_free(&step.uniformsHlsl, alloc);
		ListCharString_freeUnderlying(&step.uniformsArgs, alloc);
		ListCharString_freeUnderlying(&step.libs, alloc);
		ListCharString_freeUnderlying(&step.linkArgs, alloc);
		ListCharString_freeUnderlying(&step.spirvOpt, alloc);
		ListCompilerLinkStep_freeUnderlying(steps, alloc);
	}

	CharString_free(&scratch, alloc);
	CharString_free(&pair, alloc);
	CharString_free(&value, alloc);
	ListCompilerEntrypoint_freeUnderlying(&entrypoints, alloc);
	ListLinkEntry_freeUnderlying(&linkEntries, alloc);
	return s_uccess;
}
