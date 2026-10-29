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

//shader_compiler/test/test_shader_compiler_permutations.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSH/sh_entries.h"
#include "formats/oiSH/sh_file.h"
#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"

//The permutation system (multiple entrypoints in one file, each with its OWN oxc:: annotations that fan out
// into distinct compiled binaries) is easy to get subtly wrong when entrypoints DON'T share annotations -
// one entrypoint's extension / model / backend selection must not leak onto another's binary.
//These tests compile multi-entrypoint files where the entrypoints deliberately disagree
// and assert each binary reflects only its own annotations.

//Does any produced binary carry `ext`?
//Returns how many of the binaries do.
static U64 countBinariesWithExt(const SHFile *sh, ESHExtension ext) {
	U64 n = 0;

	for (U64 i = 0; i < sh->binaries.length; ++i)
		if (sh->binaries.ptr[i].identifier.extensions & ext)
			++n;
	return n;
}

//Is there an entrypoint named `nm` in the reflected file?
static Bool hasEntry(const SHFile *sh, const C8 *nm) {
	const CharString n = CharString_createRefCStrConst(nm);

	for (U64 i = 0; i < sh->entries.length; ++i)
		if (CharString_equalsStringSensitive(&sh->entries.ptr[i].name, &n))
			return true;
	return false;
}

void Test_shaderCompilerPermutations(Test *t) {

	Test_setModule(t, "Compiler permutations");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none();

	//--- Per-entrypoint extension isolation: two standalone stages in one file, only the vertex one enables 16BitTypes.
	//--- Exactly one of the two produced binaries may carry that extension bit. ---

	{
		ListBuffer out = (ListBuffer) { 0 };
		SHFile sh = (SHFile) { 0 };

		Bool ok =
			compileFileShader(alloc, "permutations/mixed_extensions.hlsl", EGfxBinaryType_SPIRV, true, false, &out, &err) &&
			out.length == 1 && Buffer_length(out.ptr[0]) && readOiSH(alloc, out.ptr[0], &sh, &err);

		Test_assert(t, "per-entrypoint extension isolation (only 1 of 2 binaries has 16BitTypes)",
			ok && sh.entries.length == 2 && sh.binaries.length == 2 &&
			countBinariesWithExt(&sh, ESHExtension_16BitTypes) == 1
		);

		SHFile_free(&sh, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();
	}

	//--- Per-entrypoint model isolation: two stages request different shader models;
	//--- each binary must keep its own requested model rather than collapsing to one. ---

	{
		ListBuffer out = (ListBuffer) { 0 };
		SHFile sh = (SHFile) { 0 };

		Bool ok =
			compileFileShader(alloc, "permutations/mixed_models.hlsl", EGfxBinaryType_SPIRV, true, false, &out, &err) &&
			out.length == 1 && Buffer_length(out.ptr[0]) && readOiSH(alloc, out.ptr[0], &sh, &err);

		Bool has67 = false, has65 = false;

		for (U64 i = 0; ok && i < sh.binaries.length; ++i) {
			U16 v = sh.binaries.ptr[i].identifier.shaderVersion;
			has67 |= v == OISH_SHADER_MODEL(6, 7);
			has65 |= v == OISH_SHADER_MODEL(6, 5);
		}

		Test_assert(t, "per-entrypoint model isolation (one binary 6.7, the other 6.5)",
			ok && sh.entries.length == 2 && has67 && has65
		);

		SHFile_free(&sh, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();
	}

	//--- Per-entrypoint backend selection inside one shared lib compile:
	//--- three RT entrypoints, each restricted to a different backend set.
	//--- This is the trickiest disagreement - all three share the lib's compile,
	//--- so the driver must filter per entrypoint at link time.
	//--- raygen = both; miss = spv only; closesthit = dxil only.
	//--- So SPIRV keeps {raygen, miss}, DXIL keeps {raygen, closesthit}. ---

	{
		ListBuffer spv = (ListBuffer) { 0 }, dx = (ListBuffer) { 0 };
		SHFile ssh = (SHFile) { 0 }, dsh = (SHFile) { 0 };

		Bool spvOk =
			compileFileShader(alloc, "permutations/mixed_binary_lib.hlsl", EGfxBinaryType_SPIRV, true, false, &spv, &err) &&
			spv.length == 1 && Buffer_length(spv.ptr[0]) && readOiSH(alloc, spv.ptr[0], &ssh, &err);

		Test_assert(t, "SPIRV keeps raygen + spv-only miss, drops dxil-only closesthit",
			spvOk && ssh.entries.length == 2 &&
			hasEntry(&ssh, "mainRaygen") && hasEntry(&ssh, "mainMiss") && !hasEntry(&ssh, "mainCH")
		);

		Bool dxOk =
			compileFileShader(alloc, "permutations/mixed_binary_lib.hlsl", EGfxBinaryType_DXIL, true, false, &dx, &err) &&
			dx.length == 1 && Buffer_length(dx.ptr[0]) && readOiSH(alloc, dx.ptr[0], &dsh, &err);

		Test_assert(t, "DXIL keeps raygen + dxil-only closesthit, drops spv-only miss",
			dxOk && dsh.entries.length == 2 &&
			hasEntry(&dsh, "mainRaygen") && hasEntry(&dsh, "mainCH") && !hasEntry(&dsh, "mainMiss")
		);

		SHFile_free(&ssh, alloc);
		SHFile_free(&dsh, alloc);
		ListBuffer_freeUnderlying(&spv, alloc);
		ListBuffer_freeUnderlying(&dx, alloc);
		err = Error_none();
	}

	//--- Uniforms must be identical across every entrypoint of a compilation unit (they specialize one shared linking step).
	//--- Two entrypoints declaring DIFFERENT uniforms is illegal and must be rejected cleanly. ---

	{
		ListBuffer out = (ListBuffer) { 0 };
		Error e2 = Error_none();

		//enableLogging=false: the failure is expected and asserted on, so keep the compiler quiet.
		Bool compiled =
			compileFileShader(alloc, "permutations/mixed_uniforms.hlsl", EGfxBinaryType_SPIRV, false, false, &out, &e2);

		Test_assert(t, "mismatched uniforms across entrypoints rejected", !compiled);

		ListBuffer_freeUnderlying(&out, alloc);
	}

	//--- The same uniform with rows of each entrypoint's own: one library compile, every entry linked once a row, ---
	//--- each binary holding its own entry's value (mainWide 0, 1 and 2, mainDeep 3 and 5). ---

	{
		static const struct { U8 mode; const C8 *name; } targets[] = {
			{ EGfxBinaryType_SPIRV, "spirv" },
			{ EGfxBinaryType_DXIL,  "dxil"  }
		};

		for (U64 tg = 0; tg < sizeof(targets) / sizeof(targets[0]); ++tg) {

			ListBuffer out = (ListBuffer) { 0 };
			SHFile shFile = (SHFile) { 0 };
			CharString label = CharString_createNull();

			Bool ok =
				compileFileShader(
					alloc, "permutations/uniform_rows_per_entry.hlsl", targets[tg].mode, true, false, &out, &err
				) &&
				out.length == 1 && Buffer_length(out.ptr[0]) &&
				readOiSH(alloc, out.ptr[0], &shFile, &err) && shFile.entries.length == 2;

			for (U64 j = 0; ok && j < shFile.entries.length; ++j) {

				const SHEntry *entry = &shFile.entries.ptr[j];
				const Bool wide = CharString_equalsCStringSensitive(&entry->name, "mainWide");
				const U8 expected = wide ? 0x07 : 0x28;        //Bits of the rows: 0, 1 and 2, or 3 and 5
				U8 seen = 0;

				ok = wide || CharString_equalsCStringSensitive(&entry->name, "mainDeep");

				for (U64 k = 0; ok && k < entry->binaryIds.length; ++k) {

					const SHBinaryIdentifier *id = &shFile.binaries.ptr[entry->binaryIds.ptr[k]].identifier;

					if (id->uniforms.length != 1) {
						ok = false;
						break;
					}

					const U8 row = id->uniformData.ptr[id->uniforms.ptr[0].dataOffset];
					ok = row < 8 && !((seen >> row) & 1);
					seen |= ok ? (U8) (1 << row) : 0;
				}

				ok = ok && seen == expected;
			}

			if (!ok)
				Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

			if (!CharString_format(alloc, &label, &err, "uniform rows of each entrypoint's own (%s)", targets[tg].name))
				label = CharString_createRefCStrConst("uniform rows of each entrypoint's own");

			Test_assert(t, label.ptr, ok);

			CharString_free(&label, alloc);
			SHFile_free(&shFile, alloc);
			ListBuffer_freeUnderlying(&out, alloc);
			err = Error_none();
		}
	}

	//--- Disabled combinations (@variants.hlsli), on both backends: dropped at compile (OXC_DISABLE_COMBINATION) ---
	//--- or after their link (oxcDisableBinary()), the entry keeping the binaries left, and the marker's register ---
	//--- never reaching a kept binary, with --keep-registers too. A marker reached by every combination writes nothing. ---

	{
		static const struct { U8 mode; const C8 *name; } targets[] = {
			{ EGfxBinaryType_SPIRV, "spirv" },
			{ EGfxBinaryType_DXIL,  "dxil"  }
		};

		for (U64 tg = 0; tg < sizeof(targets) / sizeof(targets[0]); ++tg) {

			CharString label = CharString_createNull();

			//At compile: five of the six define and extension combinations.

			{
				ListBuffer out = (ListBuffer) { 0 };
				SHFile shFile = (SHFile) { 0 };

				Bool ok =
					compileFileShader(
						alloc, "permutations/disable_combination.hlsl", targets[tg].mode, true, false, &out, &err
					) &&
					out.length == 1 && Buffer_length(out.ptr[0]) &&
					readOiSH(alloc, out.ptr[0], &shFile, &err) &&
					shFile.binaries.length == 5 && shFile.entries.length == 1 &&
					shFile.entries.ptr[0].binaryIds.length == 5;

				for (U64 j = 0; ok && j < shFile.binaries.length; ++j) {

					const SHBinaryIdentifier *id = &shFile.binaries.ptr[j].identifier;
					const Bool has16 = !!(id->extensions & ESHExtension_16BitTypes);

					ok = id->defines.length == 2 &&
						!(has16 && CharString_equalsCStringSensitive(&id->defines.ptr[1], "2"));
				}

				if (!ok)
					Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

				if (!CharString_format(alloc, &label, &err, "combination disabled at compile (%s)", targets[tg].name))
					label = CharString_createRefCStrConst("combination disabled at compile");

				Test_assert(t, label.ptr, ok);

				CharString_free(&label, alloc);
				SHFile_free(&shFile, alloc);
				ListBuffer_freeUnderlying(&out, alloc);
				err = Error_none();
			}

			//After the link: rows 0 and 1 kept, and no marker register, whether unused registers are kept or not.

			for (U8 keep = 0; keep < 2; ++keep) {

				ListBuffer out = (ListBuffer) { 0 };
				SHFile shFile = (SHFile) { 0 };

				Bool ok =
					compileFileShader(
						alloc, "permutations/disable_binary.hlsl", targets[tg].mode, true, keep, &out, &err
					) &&
					out.length == 1 && Buffer_length(out.ptr[0]) &&
					readOiSH(alloc, out.ptr[0], &shFile, &err) &&
					shFile.binaries.length == 2 && shFile.entries.length == 1;

				U8 rowsSeen = 0;

				for (U64 j = 0; ok && j < shFile.binaries.length; ++j) {

					const SHBinaryInfo *bin = &shFile.binaries.ptr[j];
					const SHBinaryIdentifier *id = &bin->identifier;

					ok = id->uniforms.length == 1;

					if (ok)
						rowsSeen |= (U8) (1 << (id->uniformData.ptr[id->uniforms.ptr[0].dataOffset] & 7));

					for (U64 i = 0; ok && i < bin->registers.length; ++i)
						ok = !CharString_equalsCStringSensitive(&bin->registers.ptr[i].name, "_oxcDisableBinary");
				}

				ok = ok && rowsSeen == 3;

				if (!ok)
					Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

				if (!CharString_format(
					alloc, &label, &err, "binary disabled after its link (%s%s)", targets[tg].name,
					keep ? ", keep registers" : ""
				))
					label = CharString_createRefCStrConst("binary disabled after its link");

				Test_assert(t, label.ptr, ok);

				CharString_free(&label, alloc);
				SHFile_free(&shFile, alloc);
				ListBuffer_freeUnderlying(&out, alloc);
				err = Error_none();
			}

			//Every combination disabled, at compile or by a call the specialization can't fold: nothing written.

			static const C8 *nothing[] = {
				"permutations/disable_everything.hlsl", "permutations/disable_binary_unfolded.hlsl"
			};

			for (U64 n = 0; n < sizeof(nothing) / sizeof(nothing[0]); ++n) {

				ListBuffer out = (ListBuffer) { 0 };

				const Bool ok =
					compileFileShader(alloc, nothing[n], targets[tg].mode, false, false, &out, &err) &&
					out.length == 1 && !Buffer_length(out.ptr[0]);

				if (!CharString_format(alloc, &label, &err, "%s writes nothing (%s)", nothing[n], targets[tg].name))
					label = CharString_createRefCStrConst(nothing[n]);

				Test_assert(t, label.ptr, ok);

				CharString_free(&label, alloc);
				ListBuffer_freeUnderlying(&out, alloc);
				err = Error_none();
			}
		}
	}

	//--- An empty defines annotation means one combination with no defines, not zero combinations. ---
	//--- Nothing else compiles that spelling, so the link time define matching never walked an empty range. ---

	{
		ListBuffer out = (ListBuffer) { 0 };

		Test_assert(t, "empty defines annotation compiles",
			compileFileShader(alloc, "permutations/empty_defines.hlsl", EGfxBinaryType_SPIRV, true, false, &out, &err)
		);

		//An empty defines list still means one combination, so exactly one binary has to come out of it.

		Test_assert(t, "empty defines produced a binary", out.length == 1);

		ListBuffer_freeUnderlying(&out, alloc);
	}

	//--- Multiple render targets, and a non zero default semantic index, have to reflect IDENTICALLY on both ---
	//--- backends, or the two binaries can never be merged into one oiSH. ---

	{
		ListBuffer spv = (ListBuffer) { 0 };
		ListBuffer dxil = (ListBuffer) { 0 };
		SHFile spvFile = (SHFile) { 0 }, dxilFile = (SHFile) { 0 }, combined = (SHFile) { 0 };

		const Bool built =
			Test_assert(t, "mrt compiles for spirv",
				compileFileShader(alloc, "stages/pixel_mrt.hlsl", EGfxBinaryType_SPIRV, true, false, &spv, &err)
			) &&
			Test_assert(t, "mrt compiles for dxil",
				compileFileShader(alloc, "stages/pixel_mrt.hlsl", EGfxBinaryType_DXIL, true, false, &dxil, &err)
			) &&
			spv.length == 1 && dxil.length == 1 &&
			Test_assert(t, "mrt spirv oiSH readable", readOiSH(alloc, spv.ptr[0], &spvFile, &err)) &&
			Test_assert(t, "mrt dxil oiSH readable", readOiSH(alloc, dxil.ptr[0], &dxilFile, &err));

		//Combining is the operation that actually compares the two reflections field by field, which is why a
		// divergence on SV_Target1 or TEXCOORD1 surfaces here and nowhere earlier.

		if (built)
			Test_assert(t, "mrt spirv and dxil combine",
				SHFile_combine(&spvFile, &dxilFile, alloc, &combined, &err)
			);

		SHFile_free(&combined, alloc);
		SHFile_free(&spvFile, alloc);
		SHFile_free(&dxilFile, alloc);
		ListBuffer_freeUnderlying(&spv, alloc);
		ListBuffer_freeUnderlying(&dxil, alloc);
	}

	//--- Dual source blending: two outputs at LOCATION 0 on SPIR-V (Index 0 and 1) have to land on the same ---
	//--- reflected slots as DXIL's SV_Target0/1, or the backends can't merge. ---

	{
		ListBuffer spv = (ListBuffer) { 0 };
		ListBuffer dxil = (ListBuffer) { 0 };
		SHFile spvFile = (SHFile) { 0 }, dxilFile = (SHFile) { 0 }, combined = (SHFile) { 0 };

		const Bool built =
			Test_assert(t, "dualsrc compiles for spirv",
				compileFileShader(alloc, "stages/pixel_dualsrc.hlsl", EGfxBinaryType_SPIRV, true, false, &spv, &err)
			) &&
			Test_assert(t, "dualsrc compiles for dxil",
				compileFileShader(alloc, "stages/pixel_dualsrc.hlsl", EGfxBinaryType_DXIL, true, false, &dxil, &err)
			) &&
			spv.length == 1 && dxil.length == 1 &&
			Test_assert(t, "dualsrc spirv oiSH readable", readOiSH(alloc, spv.ptr[0], &spvFile, &err)) &&
			Test_assert(t, "dualsrc dxil oiSH readable", readOiSH(alloc, dxil.ptr[0], &dxilFile, &err));

		if (built)
			Test_assert(t, "dualsrc spirv and dxil combine",
				SHFile_combine(&spvFile, &dxilFile, alloc, &combined, &err)
			);

		SHFile_free(&combined, alloc);
		SHFile_free(&spvFile, alloc);
		SHFile_free(&dxilFile, alloc);
		ListBuffer_freeUnderlying(&spv, alloc);
		ListBuffer_freeUnderlying(&dxil, alloc);
	}

	//--- Declaration order: an entry's binaryIds follow its extension lines as written, not the file's binary order.
	//--- The file sorts its binaries by identifier for reproducibility, which puts the smaller extension mask first;
	//--- an entry listing its richest line first must still have that binary first, or first fit takes the baseline. ---

	{
		ListBuffer out = (ListBuffer) { 0 };
		SHFile sh = (SHFile) { 0 };

		const Bool ok =
			compileFileShader(alloc, "permutations/extension_order.hlsl", EGfxBinaryType_SPIRV, true, false, &out, &err) &&
			out.length == 1 && Buffer_length(out.ptr[0]) && readOiSH(alloc, out.ptr[0], &sh, &err);

		const ESHExtension rich = (ESHExtension)(ESHExtension_16BitTypes | ESHExtension_I64);

		//Extension lines vary fastest, so with two uniform sets each entry holds its lines twice, in its own order.

		const ESHExtension richFirst[] = { rich, ESHExtension_I64, rich, ESHExtension_I64 };
		const ESHExtension baseFirst[] = { ESHExtension_I64, rich, ESHExtension_I64, rich };

		const CharString richName = CharString_createRefCStrConst("mainRichFirst");
		const CharString baseName = CharString_createRefCStrConst("mainBaseFirst");

		Bool richOk = false, baseOk = false;

		for (U64 i = 0; ok && i < sh.entries.length; ++i) {

			const SHEntry *entry = &sh.entries.ptr[i];
			const Bool isRich = CharString_equalsStringSensitive(&entry->name, &richName);
			const Bool isBase = CharString_equalsStringSensitive(&entry->name, &baseName);
			const ESHExtension *want = isRich ? richFirst : baseFirst;

			if((!isRich && !isBase) || entry->binaryIds.length != 4)
				continue;

			Bool same = true;

			for (U64 j = 0; j < 4; ++j)
				same &= (U64) (sh.binaries.ptr[entry->binaryIds.ptr[j]].identifier.extensions & rich) == (U64) want[j];

			if(isRich)
				richOk = same;

			else baseOk = same;
		}

		Test_assert(t, "binaryIds follow declaration order (richest line first)", ok && richOk);
		Test_assert(t, "binaryIds follow declaration order (baseline line first)", ok && baseOk);

		SHFile_free(&sh, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();
	}

	//--- The same in one lib compile, where both entrypoints share every binary: each keeps its own order. ---

	{
		ListBuffer out = (ListBuffer) { 0 };
		SHFile sh = (SHFile) { 0 };

		const Bool ok =
			compileFileShader(
				alloc, "permutations/extension_order_lib.hlsl", EGfxBinaryType_SPIRV, true, false, &out, &err
			) &&
			out.length == 1 && Buffer_length(out.ptr[0]) && readOiSH(alloc, out.ptr[0], &sh, &err);

		const ESHExtension rich = (ESHExtension)(ESHExtension_16BitTypes | ESHExtension_I64);
		const ESHExtension richFirst[] = { rich, ESHExtension_I64 };
		const ESHExtension baseFirst[] = { ESHExtension_I64, rich };

		const CharString richName = CharString_createRefCStrConst("rayRichFirst");
		const CharString baseName = CharString_createRefCStrConst("missBaseFirst");

		Bool richOk = false, baseOk = false;

		for (U64 i = 0; ok && i < sh.entries.length; ++i) {

			const SHEntry *entry = &sh.entries.ptr[i];
			const Bool isRich = CharString_equalsStringSensitive(&entry->name, &richName);
			const Bool isBase = CharString_equalsStringSensitive(&entry->name, &baseName);
			const ESHExtension *want = isRich ? richFirst : baseFirst;

			if((!isRich && !isBase) || entry->binaryIds.length != 2)
				continue;

			Bool same = true;

			for (U64 j = 0; j < 2; ++j)
				same &= (U64) (sh.binaries.ptr[entry->binaryIds.ptr[j]].identifier.extensions & rich) == (U64) want[j];

			if(isRich)
				richOk = same;

			else baseOk = same;
		}

		Test_assert(t, "lib binaryIds follow declaration order (richest line first)", ok && richOk);
		Test_assert(t, "lib binaryIds follow declaration order (baseline line first)", ok && baseOk);

		SHFile_free(&sh, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();
	}

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
}
