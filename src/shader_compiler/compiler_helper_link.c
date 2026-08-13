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

Bool Compiler_getLinkEntries(
	const Compiler *compiler,
	const ListSHEntryRuntime *runtimeEntries,
	const SHBinaryIdentifier *binaryIdentifier,
	ESHBinaryType binaryType,
	Buffer *binary,
	ListCompilerEntrypoint *entrypoints,
	ListLinkEntry *linkEntries,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListU16 tmpEntries = (ListU16) { 0 };
	Bool freeEntrypoints = false;
	Bool freeLinkEntries = false;

	Bool isRt =
		binaryIdentifier->stageType >= ESHPipelineStage_RtStartExt &&
		binaryIdentifier->stageType <= ESHPipelineStage_RtEndExt;

	Bool isLib =
		binaryIdentifier->stageType == ESHPipelineStage_WorkgraphExt ||
		isRt;

	Bool isLibTarget = isLib;
	isLib = isLibTarget || (binaryIdentifier->stageType == ESHPipelineStage_Count);

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

		Bool containsShaderVersion = ident.shaderVersion == OISH_SHADER_MODEL(6, 5) && !entry.shaderVersions.length;
		U16 shaderVersion = 0;

		for(U64 k = 0; k < entry.shaderVersions.length; ++k)
			if (entry.shaderVersions.ptr[k] == ident.shaderVersion) {
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
			gotoIfError3(clean, ListCharString_createRefConst(entry.defineNameValues.ptr + (l << 1), m << 1, &tmp, e_rr));

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

			LinkEntry linkEntry = (LinkEntry) {
				.uniformData = Buffer_createRefConst(entry.uniformData.ptr + entry.uniformStride * k, entry.uniformStride),
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
				gotoIfError3(clean, ListU16_createRefConst(&runtimeEntryL.ptr[l].entry.idOrPadding, 1, &linkEntry.runtimeEntries, e_rr));
			}

			else {

				//If RT/workgraph shader, try to find a previous linkEntry
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
			freeLinkEntries = true;
		}
	}

	if (linkEntries->length >> 32)
		retError(clean, Error_invalidState(0, "Compiler_getLinkEntries() must return <32bit entries"));

clean:

	if (!s_uccess) {

		if (freeEntrypoints)
			ListCompilerEntrypoint_freeUnderlying(entrypoints, alloc);

		if (freeLinkEntries)
			ListLinkEntry_freeUnderlying(linkEntries, alloc);
	}

	ListU16_free(&tmpEntries, alloc);
	return s_uccess;
}
