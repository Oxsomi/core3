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

//shader_compiler/spirv_isa.c
//Offline SPIR-V -> AMD ISA by driving the bundled amdllpc + amdgpu-dis directly (rga.exe's own vk-spv-offline wrapper
// is unreliable).
//Shared by the CLI's `isa` category and the shader-compiler ISA snapshot test so both produce the exact same ISA text.

#include "shader_compiler/spirv_isa.h"
#include "shader_compiler/compiler.h"
#include "platforms/process.h"
#include "platforms/file.h"
#include "platforms/logx.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/list_impl.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"
#include "types/base/time.h"
#include "types/base/constants.h"

Bool SpvISA_stageHasOfflinePath(Buffer spirv, const Allocator *alloc) {

	//Reuse the compiler's SPIR-V entrypoint query (it maps each OpEntryPoint's execution model to an ESHPipelineStage)
	// rather than parsing the module here.
	//The compiler argument is unused for the SPIR-V path, so NULL is fine.

	ListCompilerEntrypoint entrypoints = (ListCompilerEntrypoint) { 0 };
	Error err = Error_none();

	if(!Compiler_getUniqueEntrypoints(NULL, ESHBinaryType_SPIRV, spirv, true, &entrypoints, alloc, &err)) {
		ListCompilerEntrypoint_freeUnderlying(&entrypoints, alloc);
		return false;
	}

	//amdllpc lowers these to a single hardware stage offline: raster (vertex/hull/domain/geometry/pixel), compute and
	// mesh/task.
	//Ray tracing has no single-module offline path here (amdllpc emits an empty code section without the
	// pipeline/traversal context), so a module qualifies only if every entrypoint is one of the above.

	Bool offline = entrypoints.length > 0;

	for(U64 i = 0; i < entrypoints.length && offline; ++i)
		switch(entrypoints.ptr[i].stage) {

			case ESHPipelineStage_Vertex:
			case ESHPipelineStage_Hull:
			case ESHPipelineStage_Domain:
			case ESHPipelineStage_GeometryExt:
			case ESHPipelineStage_Pixel:
			case ESHPipelineStage_Compute:
			case ESHPipelineStage_MeshExt:
			case ESHPipelineStage_TaskExt:
				break;

			default:
				offline = false;
				break;
		}

	ListCompilerEntrypoint_freeUnderlying(&entrypoints, alloc);
	return offline;
}

//Runs a tool bundled in the rga/ folder next to the executable (app-dir relative, so it resolves from any working
// directory), falling back to a same-named tool on PATH.
//Returns its exit code + captured stdout/stderr (caller frees).

static Bool SpvISA_runTool(
	CharString appRelPath, CharString sysName, const ListCharString *args,
	Buffer *stdOut, Buffer *stdErr, I32 *exitCode, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	#ifndef SUPPORTS_PROCESS
		(void) appRelPath; (void) sysName; (void) args; (void) stdOut; (void) stdErr; (void) exitCode; (void) alloc;
		retError(clean, Error_unsupportedOperation(0, "SpvISA_runTool() process spawning isn't supported on this platform"));
	#else

		Buffer out = Buffer_createNull(), err = Buffer_createNull();
		ProcessResult res = (ProcessResult) { 0 };
		Error re = Error_none();

		Bool launched = Process_run(appRelPath, true, args, NULL, 60 * SECOND, &out, &err, &res, &re);

		if(!launched) {
			Buffer_free(&out, alloc);
			Buffer_free(&err, alloc);
			out = Buffer_createNull(); err = Buffer_createNull(); res = (ProcessResult) { 0 };
			launched = Process_runSystem(sysName, args, NULL, 60 * SECOND, &out, &err, &res, &re);
		}

		if(!launched) {
			Buffer_free(&out, alloc);
			Buffer_free(&err, alloc);
			retError(clean, Error_notFound(
				0, 0, "SpvISA_runTool() couldn't launch the tool (bundle it in rga/utils next to the exe, or put it on PATH)"
			));
		}

		if(res.timedOut) {
			Buffer_free(&out, alloc);
			Buffer_free(&err, alloc);
			retError(clean, Error_timedOut(0, 60 * SECOND, "SpvISA_runTool() tool timed out"));
		}

		if(exitCode)
			*exitCode = res.exitCode;

		if(stdOut)
			*stdOut = out;
		else Buffer_free(&out, alloc);

		if(stdErr)
			*stdErr = err;
		else Buffer_free(&err, alloc);

	#endif

clean:
	return s_uccess;
}

//amdllpc's -gfxip only honors the major.minor.step form (e.g. "11.0.0"); the "gfxNNNN" name is silently ignored and
// falls back to the default target.
//Converts "gfx1100" -> "11.0.0" (last digit = step, previous = minor, rest = major).
//A value already starting with a digit is treated as an explicit major.minor.step and passed through as-is.

static Bool SpvISA_gfxipArg(CharString target, const Allocator *alloc, CharString *out, Error *e_rr) {

	Bool s_uccess = true;

	const U64 len = CharString_length(target);

	if(!len)
		retError(clean, Error_invalidParameter(0, 0, "SpvISA_gfxipArg()::target is required"));

	if(target.ptr[0] >= '0' && target.ptr[0] <= '9') {        //Already a major.minor.step
		gotoIfError3(clean, CharString_createCopy(target, alloc, out, e_rr));
		goto clean;
	}

	const CharString gfx = CharString_createRefCStrConst("gfx");

	if(!CharString_startsWithStringInsensitive(&target, &gfx, 0) || len < 3 + 3)
		retError(clean, Error_invalidParameter(
			0, 0, "SpvISA_gfxipArg() target must be a gfx target (e.g. gfx1100) or a major.minor.step (e.g. 11.0.0); "
			"offline ISA supports gfx11xx (RDNA3) and gfx12xx (RDNA4)"
		));

	const C8 *digits = target.ptr + 3;
	const U64 digitCount = len - 3;

	for(U64 i = 0; i < digitCount; ++i)
		if(digits[i] < '0' || digits[i] > '9')
			retError(clean, Error_invalidParameter(0, 0, "SpvISA_gfxipArg() gfx target must be gfx followed by digits"));

	//gfxWXYZ -> W(X).Y.Z: last digit = step, previous = minor, everything before = major

	gotoIfError3(clean, CharString_format(
		alloc, out, e_rr, "%.*s.%c.%c",
		(int) (digitCount - 2), digits, digits[digitCount - 2], digits[digitCount - 1]
	));

clean:
	return s_uccess;
}

//Parses the unsigned value (decimal or 0x-hex) after `key` on any line of the amdgpu-dis text, returning the max
// across occurrences (a pipeline can list several hardware stages) or 0 if the key never appears.

static U64 SpvISA_metaU64(Buffer text, const C8 *key) {

	const C8 *p = (const C8*) text.ptr;
	const U64 n = Buffer_length(text);

	U64 kl = 0;
	while(key[kl]) ++kl;

	U64 best = 0;

	for(U64 i = 0; i + kl <= n; ++i) {

		Bool match = true;

		for(U64 j = 0; j < kl; ++j)
			if(p[i + j] != key[j]) { match = false; break; }

		if(!match)
			continue;

		U64 k = i + kl;

		while(k < n && (p[k] == ' ' || p[k] == '\t'))
			++k;

		U64 v = 0;

		if(k + 1 < n && p[k] == '0' && (p[k + 1] == 'x' || p[k + 1] == 'X')) {
			k += 2;
			for(; k < n; ++k) {
				const C8 c = p[k];
				if(c >= '0' && c <= '9') v = (v << 4) | (U64) (c - '0');
				else if(c >= 'a' && c <= 'f') v = (v << 4) | (U64) (c - 'a' + 10);
				else if(c >= 'A' && c <= 'F') v = (v << 4) | (U64) (c - 'A' + 10);
				else break;
			}
		}

		else for(; k < n && p[k] >= '0' && p[k] <= '9'; ++k)
			v = v * 10 + (U64) (p[k] - '0');

		if(v > best)
			best = v;

		i = k;
	}

	return best;
}

//For a disassembly line "<mnemonic> ... // <hexaddr>: <8hex> [<8hex> ...]", returns hexaddr + 4 * (number of encoding
// words), i.e. the byte offset just past this instruction; 0 if the line carries no "// addr: bytes" encoding comment.

static U64 SpvISA_instrEnd(const C8 *l, U64 len) {

	U64 i = 0;

	for(; i + 1 < len; ++i)
		if(l[i] == '/' && l[i + 1] == '/') { i += 2; break; }

	if(i + 1 >= len)
		return 0;

	while(i < len && l[i] == ' ')
		++i;

	U64 addr = 0;
	Bool anyAddr = false;

	for(; i < len && l[i] != ':'; ++i) {
		const C8 c = l[i];
		if(c >= '0' && c <= '9') addr = (addr << 4) | (U64) (c - '0');
		else if(c >= 'a' && c <= 'f') addr = (addr << 4) | (U64) (c - 'a' + 10);
		else if(c >= 'A' && c <= 'F') addr = (addr << 4) | (U64) (c - 'A' + 10);
		else return 0;
		anyAddr = true;
	}

	if(!anyAddr || i >= len)
		return 0;

	++i;        //skip ':'

	U64 words = 0;

	while(i < len) {

		while(i < len && l[i] == ' ')
			++i;

		U64 digits = 0;

		while(i < len && ((l[i] >= '0' && l[i] <= '9') || (l[i] >= 'a' && l[i] <= 'f') || (l[i] >= 'A' && l[i] <= 'F'))) {
			++i;
			++digits;
		}

		if(!digits)
			break;

		++words;
	}

	return addr + words * 4;
}

//Trims raw amdgpu-dis output down to what's useful for inspection and lines it up with the mesa spirv2isa backend:
// a one-line register/resource-usage summary (SGPRs/VGPRs/code/instrs/scratch/lds, matching mesa's field set, from
// the ELF PAL metadata plus the disassembly) followed by just the instructions and function labels.
//The bulk of the raw output (assembler directives, the s_code_end 0xbf9f0000 padding, and the ELF/PAL metadata block)
// is dropped.

static Bool SpvISA_clean(Buffer raw, const Allocator *alloc, Buffer *out, Error *e_rr) {

	Bool s_uccess = true;
	CharString body = CharString_createNull();
	CharString res = CharString_createNull();

	const C8 *p = (const C8*) raw.ptr;
	const U64 n = Buffer_length(raw);

	const U64 sgpr    = SpvISA_metaU64(raw, ".sgpr_count:");
	const U64 vgpr    = SpvISA_metaU64(raw, ".vgpr_count:");
	const U64 lds     = SpvISA_metaU64(raw, ".lds_size:");
	const U64 scratch = SpvISA_metaU64(raw, ".scratch_memory_size:");

	//Target name from the leading `// llvm-mc ... -mcpu=<t> ...` line

	CharString mcpu = CharString_createRefCStrConst("gfx?");

	{
		const C8 *keyMcpu = "-mcpu=";

		for(U64 i = 0; i + 6 <= n; ++i) {

			Bool match = true;

			for(U64 j = 0; j < 6; ++j)
				if(p[i + j] != keyMcpu[j]) { match = false; break; }

			if(!match)
				continue;

			U64 s = i + 6, e = s;
			while(e < n && p[e] != ' ' && p[e] != '\t' && p[e] != '\r' && p[e] != '\n') ++e;
			mcpu = CharString_createRefSizedConst(p + s, e - s, false);
			break;
		}
	}

	//Keep only real code: instruction lines (tab + mnemonic) and _amdgpu* function labels; drop directives, the
	// s_code_end padding and the ELF/PAL metadata.
	//Tally the instruction count and code size (mesa reports both) here.

	const CharString amdgpuPfx = CharString_createRefCStrConst("_amdgpu");
	const CharString symendSfx = CharString_createRefCStrConst("_symend:");

	U64 instrCount = 0, codeSize = 0, lineStart = 0;

	for(U64 i = 0; i <= n; ++i) {

		if(i < n && p[i] != '\n')
			continue;

		U64 lineEnd = i;

		if(lineEnd > lineStart && p[lineEnd - 1] == '\r')
			--lineEnd;

		const U64 len = lineEnd - lineStart;
		const C8 *l = p + lineStart;
		lineStart = i + 1;

		//Instruction: a tab then a mnemonic letter (directives and .long padding are a tab then '.')

		const Bool isInstr =
			len >= 2 && l[0] == '\t' && ((l[1] >= 'a' && l[1] <= 'z') || (l[1] >= 'A' && l[1] <= 'Z'));

		Bool keep = isInstr;

		//Function label: `_amdgpu..._main:` (but not the `_symend:` end marker)

		if(!keep && len >= 2 && l[0] == '_' && l[len - 1] == ':') {
			const CharString lbl = CharString_createRefSizedConst(l, len, false);
			keep =
				CharString_startsWithStringInsensitive(&lbl, &amdgpuPfx, 0) &&
				!CharString_endsWithStringInsensitive(&lbl, &symendSfx, 0);
		}

		if(!keep)
			continue;

		if(isInstr) {
			++instrCount;
			const U64 end = SpvISA_instrEnd(l, len);
			if(end > codeSize)
				codeSize = end;
		}

		const CharString line = CharString_createRefSizedConst(l, len, false);
		gotoIfError3(clean, CharString_appendString(&body, &line, alloc, e_rr));
		gotoIfError3(clean, CharString_append(&body, '\n', alloc, e_rr));
	}

	gotoIfError3(clean, CharString_format(
		alloc, &res, e_rr,
		"; %.*s  SGPRs %"PRIu64"  VGPRs %"PRIu64"  code %"PRIu64" B  instrs %"PRIu64"  scratch %"PRIu64"  lds %"PRIu64"\n",
		(int) CharString_length(mcpu), mcpu.ptr, sgpr, vgpr, codeSize, instrCount, scratch, lds
	));

	gotoIfError3(clean, CharString_appendString(&res, &body, alloc, e_rr));

	gotoIfError3(clean, Buffer_createCopy(CharString_bufferConst(res), alloc, out, e_rr));

clean:
	CharString_free(&body, alloc);
	CharString_free(&res, alloc);
	return s_uccess;
}

Bool SpvISA_disassemble(
	Buffer spirv, CharString gfxTarget, CharString entrypoint, Buffer *isaOut, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	CharString tmpDir = CharString_createNull(), tmpSpv = CharString_createNull(), tmpElf = CharString_createNull();
	CharString gfxip = CharString_createNull(), gfxipArg = CharString_createNull(), inputArg = CharString_createNull();
	Buffer elf = Buffer_createNull();
	Buffer llpcOut = Buffer_createNull(), llpcErr = Buffer_createNull();
	Buffer disOut = Buffer_createNull(), disErr = Buffer_createNull();
	ListCharString llpcArgs = (ListCharString) { 0 }, disArgs = (ListCharString) { 0 };
	Bool madeTmp = false;

	if(!isaOut || isaOut->ptr)
		retError(clean, Error_invalidParameter(2, 0, "SpvISA_disassemble()::isaOut must be an empty buffer"));

	gotoIfError3(clean, SpvISA_gfxipArg(gfxTarget, alloc, &gfxip, e_rr));

	//Work in a fresh temp dir so concurrent invocations don't collide on the shader.spv / shader.elf names

	gotoIfError3(clean, CharString_format(alloc, &tmpDir, e_rr, ".oxc3_isa_%"PRIu64, (U64) Time_now()));
	gotoIfError3(clean, File_add(&tmpDir, EFileType_Folder, false, alloc, e_rr));
	madeTmp = true;

	gotoIfError3(clean, CharString_format(alloc, &tmpSpv, e_rr, "%.*s/shader.spv", (int) CharString_length(tmpDir), tmpDir.ptr));
	gotoIfError3(clean, CharString_format(alloc, &tmpElf, e_rr, "%.*s/shader.elf", (int) CharString_length(tmpDir), tmpDir.ptr));
	gotoIfError3(clean, File_write(&spirv, &tmpSpv, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));

	const Bool isWin = _PLATFORM_TYPE == PLATFORM_WINDOWS;

	//1. amdllpc -gfxip=<major.minor.step> --auto-layout-desc <shader.spv[,entrypoint]> -o <shader.elf>
	//   SPIR-V carries no pipeline layout, so --auto-layout-desc derives descriptor bindings from resource usage.
	//   The "<spv>,<entrypoint>" form selects one entrypoint of a multi-entry (library) module; without it amdllpc
	//   lowers only the module's first entrypoint, which would misrepresent every other stage in a library.

	gotoIfError3(clean, CharString_format(alloc, &gfxipArg, e_rr, "-gfxip=%.*s", (int) CharString_length(gfxip), gfxip.ptr));

	gotoIfError3(clean, ListCharString_pushBack(&llpcArgs, gfxipArg, alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&llpcArgs, CharString_createRefCStrConst("--auto-layout-desc"), alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&llpcArgs, CharString_createRefCStrConst("-o"), alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&llpcArgs, tmpElf, alloc, e_rr));

	//Only use the "<spv>,<entrypoint>" selector when `entrypoint` is genuinely one of the module's OpEntryPoints.
	//The oiSH identifier keeps the original HLSL name, but a single-entry module's SPIR-V is normalized to "main", so a
	// mismatched name would make amdllpc error ("failed to identify shader stages by entry-point").
	//When it doesn't match, dropping the selector lets amdllpc lower the sole entrypoint; the selector only
	// disambiguates a library.

	Bool useEntry = false;

	if(CharString_length(entrypoint)) {

		ListCompilerEntrypoint eps = (ListCompilerEntrypoint) { 0 };
		Error epErr = Error_none();

		if(Compiler_getUniqueEntrypoints(NULL, ESHBinaryType_SPIRV, spirv, true, &eps, alloc, &epErr))
			for(U64 i = 0; i < eps.length; ++i)
				if(CharString_equalsStringSensitive(&eps.ptr[i].name, &entrypoint)) { useEntry = true; break; }

		ListCompilerEntrypoint_freeUnderlying(&eps, alloc);
	}

	if(useEntry) {
		gotoIfError3(clean, CharString_format(
			alloc, &inputArg, e_rr, "%.*s,%.*s",
			(int) CharString_length(tmpSpv), tmpSpv.ptr, (int) CharString_length(entrypoint), entrypoint.ptr
		));
		gotoIfError3(clean, ListCharString_pushBack(&llpcArgs, inputArg, alloc, e_rr));
	}

	else gotoIfError3(clean, ListCharString_pushBack(&llpcArgs, tmpSpv, alloc, e_rr));

	I32 llpcExit = 0;
	gotoIfError3(clean, SpvISA_runTool(
		CharString_createRefCStrConst(isWin ? "rga/utils/amdllpc.exe" : "rga/utils/amdllpc"),
		CharString_createRefCStrConst(isWin ? "amdllpc.exe" : "amdllpc"),
		&llpcArgs, &llpcOut, &llpcErr, &llpcExit, alloc, e_rr
	));

	//amdllpc can exit 0 yet emit nothing (an unsupported gfxip prints "Invalid gfxip" and produces no ELF), so a
	// missing/empty ELF is a failure too.
	//Surface amdllpc's own diagnostic (stderr, else stdout).

	const Bool elfOk = !llpcExit && File_read(&tmpElf, 1 * SECOND, 0, 0, &fileHandleType, &elf, NULL) && Buffer_length(elf);

	if(!elfOk) {

		const Buffer diag = Buffer_length(llpcErr) ? llpcErr : llpcOut;

		if(Buffer_length(diag))
			Log_warnLnx("amdllpc: %.*s", (int) Buffer_length(diag), (const C8*) diag.ptr);

		retError(clean, Error_invalidState(
			1, "SpvISA_disassemble() amdllpc produced no ELF (unsupported gfxip? offline ISA supports gfx11xx/gfx12xx)"
		));
	}

	//2. amdgpu-dis <shader.elf>  ->  ISA text on stdout

	gotoIfError3(clean, ListCharString_pushBack(&disArgs, tmpElf, alloc, e_rr));

	I32 disExit = 0;
	gotoIfError3(clean, SpvISA_runTool(
		CharString_createRefCStrConst(
			isWin ? "rga/utils/lc/disassembler/amdgpu-dis.exe" : "rga/utils/lc/disassembler/amdgpu-dis"
		),
		CharString_createRefCStrConst(isWin ? "amdgpu-dis.exe" : "amdgpu-dis"),
		&disArgs, &disOut, &disErr, &disExit, alloc, e_rr
	));

	if(disExit || !Buffer_length(disOut)) {

		if(Buffer_length(disErr))
			Log_warnLnx("amdgpu-dis: %.*s", (int) Buffer_length(disErr), (const C8*) disErr.ptr);

		retError(clean, Error_invalidState(1, "SpvISA_disassemble() amdgpu-dis produced no ISA text"));
	}

	//Trim to a register-usage summary + the instructions; the raw ELF/PAL-metadata + padding dump isn't useful here

	gotoIfError3(clean, SpvISA_clean(disOut, alloc, isaOut, e_rr));

clean:
	if(madeTmp)
		File_remove(&tmpDir, 1 * SECOND, alloc, NULL);

	CharString_free(&tmpDir, alloc);
	CharString_free(&tmpSpv, alloc);
	CharString_free(&tmpElf, alloc);
	CharString_free(&gfxip, alloc);
	CharString_free(&gfxipArg, alloc);
	CharString_free(&inputArg, alloc);
	Buffer_free(&elf, alloc);
	Buffer_free(&llpcOut, alloc);
	Buffer_free(&llpcErr, alloc);
	Buffer_free(&disOut, alloc);
	Buffer_free(&disErr, alloc);
	ListCharString_free(&llpcArgs, alloc);        //Elements are refs into owned strings freed above
	ListCharString_free(&disArgs, alloc);
	return s_uccess;
}

Bool SpvISA_listSupportedTargets(const Allocator *alloc, ListCharString *out, Error *e_rr) {

	Bool s_uccess = true;

	//Candidate AMD targets across GCN..RDNA4; the probe keeps only the ones this amdllpc build accepts.
	//amdllpc validates -gfxip up front (rejecting unsupported ones with "Invalid gfxip") even with no input file, so no
	// shader is needed.

	static const struct { const C8 *gfx; const C8 *arch; } candidates[] = {
		{ "gfx900", "GCN5/Vega" }, { "gfx906", "GCN5/Vega20" },
		{ "gfx1010", "RDNA1" }, { "gfx1030", "RDNA2" }, { "gfx1032", "RDNA2" }, { "gfx1034", "RDNA2" }, { "gfx1035", "RDNA2" },
		{ "gfx1100", "RDNA3" }, { "gfx1101", "RDNA3" }, { "gfx1102", "RDNA3" }, { "gfx1103", "RDNA3" }, { "gfx1150", "RDNA3.5" },
		{ "gfx1200", "RDNA4" }, { "gfx1201", "RDNA4" }
	};

	const Bool isWin = _PLATFORM_TYPE == PLATFORM_WINDOWS;
	const CharString needle = CharString_createRefCStrConst("Invalid gfxip");

	CharString gfxip = CharString_createNull(), gfxipArg = CharString_createNull(), line = CharString_createNull();
	ListCharString args = (ListCharString) { 0 };
	Buffer llpcOut = Buffer_createNull(), llpcErr = Buffer_createNull();

	for(U64 i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {

		CharString_free(&gfxip, alloc);
		CharString_free(&gfxipArg, alloc);
		Buffer_free(&llpcOut, alloc);
		Buffer_free(&llpcErr, alloc);
		ListCharString_free(&args, alloc);

		gotoIfError3(clean, SpvISA_gfxipArg(CharString_createRefCStrConst(candidates[i].gfx), alloc, &gfxip, e_rr));
		gotoIfError3(clean, CharString_format(alloc, &gfxipArg, e_rr, "-gfxip=%.*s", (int) CharString_length(gfxip), gfxip.ptr));
		gotoIfError3(clean, ListCharString_pushBack(&args, gfxipArg, alloc, e_rr));

		I32 exitCode = 0;
		gotoIfError3(clean, SpvISA_runTool(
			CharString_createRefCStrConst(isWin ? "rga/utils/amdllpc.exe" : "rga/utils/amdllpc"),
			CharString_createRefCStrConst(isWin ? "amdllpc.exe" : "amdllpc"),
			&args, &llpcOut, &llpcErr, &exitCode, alloc, e_rr
		));

		//Supported iff amdllpc didn't reject the gfxip (it then just errors on the missing input file, which is fine)

		const CharString o = CharString_createRefSizedConst((const C8*) llpcOut.ptr, Buffer_length(llpcOut), false);
		const CharString eo = CharString_createRefSizedConst((const C8*) llpcErr.ptr, Buffer_length(llpcErr), false);

		if(CharString_containsStringSensitive(&o, &needle, 0, 0) || CharString_containsStringSensitive(&eo, &needle, 0, 0))
			continue;

		gotoIfError3(clean, CharString_format(alloc, &line, e_rr, "%s (%s)", candidates[i].gfx, candidates[i].arch));
		gotoIfError3(clean, ListCharString_pushBack(out, line, alloc, e_rr));
		line = CharString_createNull();        //Owned by `out` now
	}

clean:
	CharString_free(&gfxip, alloc);
	CharString_free(&gfxipArg, alloc);
	CharString_free(&line, alloc);
	ListCharString_free(&args, alloc);
	Buffer_free(&llpcOut, alloc);
	Buffer_free(&llpcErr, alloc);
	return s_uccess;
}
