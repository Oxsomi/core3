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

//Finalize one spawn attempt: if it launched, hand stdout/exit code back and stop; otherwise free and let the
//caller try the next location. `done` is set when a launch succeeded (regardless of rga's own exit code).

static Bool CLI_finishRGA(
	Bool launched, Buffer out, Buffer err, ProcessResult res,
	Buffer *stdOut, Buffer *stdErr, I32 *exitCode, Bool *done, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	if(!launched) {
		Buffer_free(&out, alloc);
		Buffer_free(&err, alloc);
		goto clean;
	}

	if(res.timedOut) {
		Buffer_free(&out, alloc);
		Buffer_free(&err, alloc);
		retError(clean, Error_timedOut(0, 30 * SECOND, "CLI_runRGA() rga timed out"));
	}

	if(exitCode)
		*exitCode = res.exitCode;

	if(stdOut)
		*stdOut = out;
	else Buffer_free(&out, alloc);

	//Hand stderr back if the caller wants it (rga can log a useful error to stderr while still exiting 0);
	//otherwise surface it here only when rga exited non-zero.

	if(stdErr)
		*stdErr = err;

	else {

		if(res.exitCode && Buffer_length(err))
			Log_warnLnx("rga: %.*s", (int) Buffer_length(err), (const C8*) err.ptr);

		Buffer_free(&err, alloc);
	}

	*done = true;

clean:
	return s_uccess;
}

//Run rga with the given args. RGA can't be linked into OxC3 (it's a subprocess orchestrator over prebuilt AMD
//compilers), so it's spawned. Look for it bundled next to OxC3 first (via the sandboxed, app-dir-relative
//Process_run - the build copies it into rga/), then fall back to a system 'rga' on PATH (Process_runSystem).

static Bool CLI_runRGA(
	const ListCharString *args, Buffer *stdOut, Buffer *stdErr, I32 *exitCode, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	(void) alloc;

	#ifndef SUPPORTS_PROCESS
		(void) args; (void) stdOut; (void) stdErr; (void) exitCode;
		retError(clean, Error_unsupportedOperation(0, "CLI_runRGA() process spawning isn't supported on this platform"));
	#else

		const Bool isWin = _PLATFORM_TYPE == PLATFORM_WINDOWS;
		const CharString exeName = CharString_createRefCStrConst(isWin ? "rga.exe" : "rga");
		const CharString bundledSub = CharString_createRefCStrConst(isWin ? "rga/rga.exe" : "rga/rga");

		Bool done = false;
		Buffer out = Buffer_createNull(), err = Buffer_createNull();
		ProcessResult res = (ProcessResult) { 0 };
		Error re = Error_none();

		//1. Bundled next to OxC3 in an rga/ subfolder (app-dir relative, resolves from any working dir)

		Bool launched = Process_run(bundledSub, true, args, NULL, 30 * SECOND, &out, &err, &res, &re);
		gotoIfError3(clean, CLI_finishRGA(launched, out, err, res, stdOut, stdErr, exitCode, &done, alloc, e_rr));

		//2. Bundled flat next to OxC3 (in case it isn't in a subfolder)

		if(!done) {
			out = Buffer_createNull(); err = Buffer_createNull(); res = (ProcessResult) { 0 };
			launched = Process_run(exeName, true, args, NULL, 30 * SECOND, &out, &err, &res, &re);
			gotoIfError3(clean, CLI_finishRGA(launched, out, err, res, stdOut, stdErr, exitCode, &done, alloc, e_rr));
		}

		//3. A system 'rga' on PATH (the conan run environment prepends the package's bin to PATH)

		if(!done) {
			out = Buffer_createNull(); err = Buffer_createNull(); res = (ProcessResult) { 0 };
			launched = Process_runSystem(exeName, args, NULL, 30 * SECOND, &out, &err, &res, &re);
			gotoIfError3(clean, CLI_finishRGA(launched, out, err, res, stdOut, stdErr, exitCode, &done, alloc, e_rr));
		}

		if(!done)
			retError(clean, Error_notFound(
				0, 0, "CLI_runRGA() couldn't find rga (bundle it next to OxC3 in an rga/ folder, or put it on PATH)"
			));

	#endif

clean:
	return s_uccess;
}

//Runs `rga -s vk-offline --list-asics` (the driver-independent mode) and returns its stdout: the ASICs grouped
//by architecture, one "gfxNNNN (Arch)" line each followed by its indented marketing names. Caller frees `out`.

static Bool CLI_isaCaptureDevices(Buffer *out, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	ListCharString rgaArgs = (ListCharString) { 0 };
	I32 exitCode = 0;

	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, CharString_createRefCStrConst("-s"), alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, CharString_createRefCStrConst("vk-offline"), alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, CharString_createRefCStrConst("--list-asics"), alloc, e_rr));

	gotoIfError3(clean, CLI_runRGA(&rgaArgs, out, NULL, &exitCode, alloc, e_rr));

	if(exitCode)
		retError(clean, Error_invalidState(0, "CLI_isaCaptureDevices() rga --list-asics failed"));

clean:
	ListCharString_free(&rgaArgs, alloc);        //Elements are refs
	return s_uccess;
}

//Captures and prints rga's --list-asics output (the ASICs grouped by architecture). Used by 'isa devices', the
//'?' shorthand, and as a hint after rga rejects an -asic so the user can pick a valid one.

static Bool CLI_isaPrintDevices(const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	Buffer out = Buffer_createNull();

	gotoIfError3(clean, CLI_isaCaptureDevices(&out, alloc, e_rr));
	Log_debugLnx("%.*s", (int) Buffer_length(out), (const C8*) out.ptr);

clean:
	Buffer_free(&out, alloc);
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

//rga's analysis CSV (-a) is a header row followed by one data row. Fetches an unsigned column by header name
//(case-insensitive); returns false and leaves *out at U64_MAX when the column or a numeric value isn't present.

static Bool CLI_isaCsvGet(Buffer csv, const C8 *name, U64 *out) {

	*out = U64_MAX;

	const C8 *p = (const C8*) csv.ptr;
	const U64 n = Buffer_length(csv);
	const CharString target = CharString_createRefCStrConst(name);

	//Header line = [0, headerEnd); data line = [dataStart, dataEnd)

	U64 headerEnd = 0;
	while(headerEnd < n && p[headerEnd] != '\n' && p[headerEnd] != '\r') ++headerEnd;

	U64 dataStart = headerEnd;
	while(dataStart < n && (p[dataStart] == '\n' || p[dataStart] == '\r')) ++dataStart;

	U64 dataEnd = dataStart;
	while(dataEnd < n && p[dataEnd] != '\n' && p[dataEnd] != '\r') ++dataEnd;

	if(dataStart >= dataEnd)
		return false;

	//Find the header column index whose token matches `name`

	U64 col = 0, idx = U64_MAX, tokStart = 0;

	for(U64 i = 0; i <= headerEnd; ++i)
		if(i == headerEnd || p[i] == ',') {
			const CharString tok = CharString_createRefSizedConst(p + tokStart, i - tokStart, false);
			if(CharString_equalsStringInsensitive(&tok, &target)) { idx = col; break; }
			++col;
			tokStart = i + 1;
		}

	if(idx == U64_MAX)
		return false;

	//Walk the data row to that column index and parse it

	U64 c = 0, valStart = dataStart;

	for(U64 i = dataStart; i <= dataEnd; ++i)
		if(i == dataEnd || p[i] == ',') {
			if(c == idx) {
				const CharString v = CharString_createRefSizedConst(p + valStart, i - valStart, false);
				return CharString_parseU64(v, out);
			}
			++c;
			valStart = i + 1;
		}

	return false;
}

//SPIR-V execution model (OpEntryPoint operand 0) -> the rga vk-spv-offline per-stage flag.

static Bool CLI_spirvStageFlag(Buffer spirv, CharString *flagOut, Error *e_rr) {

	Bool s_uccess = true;

	const U64 words = Buffer_length(spirv) >> 2;

	if(words < 5)
		retError(clean, Error_invalidParameter(0, 0, "CLI_spirvStageFlag() not a SPIR-V module (too small)"));

	const U32 *w = (const U32*) spirv.ptr;

	if(w[0] != 0x07230203)
		retError(clean, Error_invalidParameter(0, 0, "CLI_spirvStageFlag() bad SPIR-V magic (big-endian modules unsupported)"));

	//Walk instructions from word 5; OpEntryPoint (opcode 15) carries the execution model as its first operand

	for(U64 i = 5; i < words; ) {

		const U32 op = w[i] & 0xFFFF;
		const U32 len = w[i] >> 16;

		if(!len || i + len > words)
			retError(clean, Error_invalidState(0, "CLI_spirvStageFlag() malformed SPIR-V instruction stream"));

		if(op == 15) {

			const U32 model = w[i + 1];
			const C8 *flag = NULL;

			switch(model) {
				case 0: flag = "--vert"; break;        //Vertex
				case 1: flag = "--tesc"; break;        //TessellationControl
				case 2: flag = "--tese"; break;        //TessellationEvaluation
				case 3: flag = "--geom"; break;        //Geometry
				case 4: flag = "--frag"; break;        //Fragment
				case 5: flag = "--comp"; break;        //GLCompute
				default: break;
			}

			if(!flag)
				retError(clean, Error_unsupportedOperation(
					1, "CLI_spirvStageFlag() stage has no offline ISA path (only vertex/hull/domain/geometry/pixel/compute)"
				));

			*flagOut = CharString_createRefCStrConst(flag);
			goto clean;
		}

		i += len;
	}

	retError(clean, Error_notFound(0, 0, "CLI_spirvStageFlag() no OpEntryPoint found in the SPIR-V"));

clean:
	return s_uccess;
}

//Collects the first file in the temp dir whose name ends with `ext` (rga templates its output names per ASIC/stage,
//so we discover them rather than knowing them up front: <asic>_isa_<stage>.txt and <asic>_stats_<stage>.csv).

typedef struct IsaFindCtx {
	CharString *out;
	CharString ext;
	const Allocator *alloc;
} IsaFindCtx;

static Bool CLI_isaFindProduced(const FileInfo *info, void *userData, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	IsaFindCtx *ctx = (IsaFindCtx*) userData;

	if(info->type != EFileType_File || CharString_length(*ctx->out))
		goto clean;

	if(CharString_endsWithStringInsensitive(&info->path, &ctx->ext, 0))
		gotoIfError3(clean, CharString_createCopy(info->path, alloc, ctx->out, e_rr));

clean:
	(void) userData;
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

//Disassembles a SPIR-V module to AMD ISA text for `asic` via rga, returning the ISA in `isaOut` (caller frees).
//`asic` is assumed already validated (see CLI_isaResolveAsic). rga can't be linked in, so this shells out to it.

Bool CLI_isaDisassembleSpirv(Buffer spirv, CharString asic, Buffer *isaOut, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	CharString tmpDir = CharString_createNull(), tmpSpv = CharString_createNull();
	CharString isaPath = CharString_createNull(), statsPath = CharString_createNull();
	CharString producedIsa = CharString_createNull(), producedStats = CharString_createNull();
	CharString summary = CharString_createNull();
	Buffer isaText = Buffer_createNull(), statsCsv = Buffer_createNull();
	Buffer rgaOut = Buffer_createNull(), rgaErr = Buffer_createNull();
	ListCharString rgaArgs = (ListCharString) { 0 };
	Bool madeTmp = false;

	if(!isaOut || isaOut->ptr)
		retError(clean, Error_invalidParameter(2, 0, "CLI_isaDisassembleSpirv()::isaOut must be an empty buffer"));

	CharString stageFlag = CharString_createNull();
	gotoIfError3(clean, CLI_spirvStageFlag(spirv, &stageFlag, e_rr));

	//rga templates its output names per ASIC/stage, so run it in a fresh temp dir and discover what it produced

	gotoIfError3(clean, CharString_format(alloc, &tmpDir, e_rr, ".oxc3_isa_%"PRIu64, (U64) Time_now()));
	gotoIfError3(clean, File_add(&tmpDir, EFileType_Folder, false, alloc, e_rr));
	madeTmp = true;

	gotoIfError3(clean, CharString_format(alloc, &tmpSpv, e_rr, "%.*s/shader.spv", (int) CharString_length(tmpDir), tmpDir.ptr));
	gotoIfError3(clean, File_write(&spirv, &tmpSpv, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));

	gotoIfError3(clean, CharString_format(alloc, &isaPath, e_rr, "%.*s/isa.txt", (int) CharString_length(tmpDir), tmpDir.ptr));
	gotoIfError3(clean, CharString_format(alloc, &statsPath, e_rr, "%.*s/stats.csv", (int) CharString_length(tmpDir), tmpDir.ptr));

	//rga -s vk-spv-offline -c <asic> --isa <out> -a <stats> --<stage> <shader.spv>

	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, CharString_createRefCStrConst("-s"), alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, CharString_createRefCStrConst("vk-spv-offline"), alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, CharString_createRefCStrConst("-c"), alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, asic, alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, CharString_createRefCStrConst("--isa"), alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, isaPath, alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, CharString_createRefCStrConst("-a"), alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, statsPath, alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, stageFlag, alloc, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(&rgaArgs, tmpSpv, alloc, e_rr));

	I32 exitCode = 0;
	gotoIfError3(clean, CLI_runRGA(&rgaArgs, &rgaOut, &rgaErr, &exitCode, alloc, e_rr));

	//Find what rga produced: the ISA (.txt) and, if present, the analysis CSV (.csv)

	IsaFindCtx isaCtx = (IsaFindCtx) {
		.out = &producedIsa, .ext = CharString_createRefCStrConst(".txt"), .alloc = alloc
	};
	gotoIfError3(clean, File_foreach(&tmpDir, false, CLI_isaFindProduced, &isaCtx, false, alloc, e_rr));

	//rga can exit 0 yet produce nothing for an unknown ASIC, so a missing ISA counts as failure too. Surface rga's
	//own message (it names the bad target) and re-print the device list (best effort) so the user can pick a valid one.

	if(exitCode || !CharString_length(producedIsa)) {

		const Buffer diag = Buffer_length(rgaErr) ? rgaErr : rgaOut;

		if(Buffer_length(diag))
			Log_warnLnx("rga: %.*s", (int) Buffer_length(diag), (const C8*) diag.ptr);

		Error listErr = Error_none();
		CLI_isaPrintDevices(alloc, &listErr);

		retError(clean, Error_invalidState(
			1, "CLI_isaDisassembleSpirv() rga produced no ISA (unknown ASIC, or shader unsupported for it); see ASICs above"
		));
	}

	IsaFindCtx statsCtx = (IsaFindCtx) {
		.out = &producedStats, .ext = CharString_createRefCStrConst(".csv"), .alloc = alloc
	};
	gotoIfError3(clean, File_foreach(&tmpDir, false, CLI_isaFindProduced, &statsCtx, false, alloc, e_rr));

	//No CSV: just return the raw ISA. Otherwise prepend a human-readable register/resource-usage summary.

	if(!CharString_length(producedStats)) {
		gotoIfError3(clean, File_read(&producedIsa, 1 * SECOND, 0, 0, &fileHandleType, isaOut, e_rr));
		goto clean;
	}

	gotoIfError3(clean, File_read(&producedStats, 1 * SECOND, 0, 0, &fileHandleType, &statsCsv, e_rr));

	U64 usedVgpr, availVgpr, usedSgpr, availSgpr, usedLds, availLds, scratch, vgprSpill, sgprSpill, isaSize;
	CLI_isaCsvGet(statsCsv, "USED_VGPRs", &usedVgpr);
	CLI_isaCsvGet(statsCsv, "AVAILABLE_VGPRs", &availVgpr);
	CLI_isaCsvGet(statsCsv, "USED_SGPRs", &usedSgpr);
	CLI_isaCsvGet(statsCsv, "AVAILABLE_SGPRs", &availSgpr);
	CLI_isaCsvGet(statsCsv, "USED_LDS_BYTES", &usedLds);
	CLI_isaCsvGet(statsCsv, "AVAILABLE_LDS_BYTES", &availLds);
	CLI_isaCsvGet(statsCsv, "SCRATCH_MEM", &scratch);
	CLI_isaCsvGet(statsCsv, "VGPR_SPILLS", &vgprSpill);
	CLI_isaCsvGet(statsCsv, "SGPR_SPILLS", &sgprSpill);
	CLI_isaCsvGet(statsCsv, "ISA_SIZE", &isaSize);

	gotoIfError3(clean, CharString_format(
		alloc, &summary, e_rr,
		"; === %.*s resource usage ===\n"
		"; VGPRs: %"PRIu64" / %"PRIu64"    SGPRs: %"PRIu64" / %"PRIu64"\n"
		"; LDS: %"PRIu64" / %"PRIu64" bytes    Scratch: %"PRIu64" bytes\n"
		"; Spills (VGPR/SGPR): %"PRIu64" / %"PRIu64"    ISA size: %"PRIu64" bytes\n\n",
		(int) CharString_length(asic), asic.ptr,
		usedVgpr, availVgpr, usedSgpr, availSgpr,
		usedLds, availLds, scratch,
		vgprSpill, sgprSpill, isaSize
	));

	gotoIfError3(clean, File_read(&producedIsa, 1 * SECOND, 0, 0, &fileHandleType, &isaText, e_rr));

	const CharString isaStr = CharString_createRefSizedConst((const C8*) isaText.ptr, Buffer_length(isaText), false);
	gotoIfError3(clean, CharString_appendString(&summary, &isaStr, alloc, e_rr));

	gotoIfError3(clean, Buffer_createCopy(CharString_bufferConst(summary), alloc, isaOut, e_rr));

clean:
	if(madeTmp)
		File_remove(&tmpDir, 1 * SECOND, alloc, NULL);

	CharString_free(&tmpDir, alloc);
	CharString_free(&tmpSpv, alloc);
	CharString_free(&isaPath, alloc);
	CharString_free(&statsPath, alloc);
	CharString_free(&producedIsa, alloc);
	CharString_free(&producedStats, alloc);
	CharString_free(&summary, alloc);
	Buffer_free(&isaText, alloc);
	Buffer_free(&statsCsv, alloc);
	Buffer_free(&rgaOut, alloc);
	Buffer_free(&rgaErr, alloc);
	ListCharString_free(&rgaArgs, alloc);        //Elements are refs into owned strings freed above
	return s_uccess;
}

Bool CLI_isaDisassemble(const ParsedArgs *args) {

	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	const RefPtrType memoryStreamType = MemoryStream_makeType(alloc);

	CharString inputStr = CharString_createNull(), outputStr = CharString_createNull(), asic = CharString_createNull();
	Buffer input = Buffer_createNull(), isa = Buffer_createNull();
	Buffer spirv = Buffer_createNull();        //A ref into `input` (.spv) or into `shFile` (.oiSH); not owned
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
	}

	else retError(clean, Error_invalidParameter(
		0, 1, "CLI_isaDisassemble() input must be .spv (SPIR-V) or .oiSH; DXIL/HLSL have no offline SPIR-V path"
	));

	gotoIfError3(clean, CLI_isaDisassembleSpirv(spirv, asic, &isa, alloc, e_rr));

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
