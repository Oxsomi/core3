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

//shader_compiler/test/test_shader_compiler_reflection.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "formats/oiSH/sh_registers.h"
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSB/sb_file.h"
#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"
#include "types/base/constants.h"

//Everything else in the suite asserts extension bits (features) and stage names (stages) but never a single
//reflected resource. This module compiles one resource-rich compute entrypoint and asserts the reflected
//SHRegister set - register type, read/write, descriptor-array dimension, structured-buffer stride - on BOTH
//backends, so a whole class of reflection regressions (a buffer misclassified, IsWrite dropped, a descriptor
//array collapsed, a resource silently dropped) is caught semantically instead of as an opaque corpus byte diff.

//The shader lives at reflection/resources.hlsl: one compute entrypoint with one of (almost) every
//ESHRegisterType, each actually used so DXC keeps it (RayQuery yields the AccelerationStructure register).

//Find a reflected register by name across all binaries (registers hang off each SHBinaryInfo).
static const SHRegisterRuntime *findReg(const SHFile *sh, const C8 *nm) {

	const CharString n = CharString_createRefCStrConst(nm);

	for (U64 b = 0; b < sh->binaries.length; ++b) {

		const ListSHRegisterRuntime *rs = &sh->binaries.ptr[b].registers;

		for (U64 i = 0; i < rs->length; ++i)
			if (CharString_equalsStringSensitive(&rs->ptr[i].name, &n))
				return &rs->ptr[i];
	}

	return NULL;
}

void Test_shaderCompilerReflection(Test *t) {

	Test_setModule(t, "Compiler reflection");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none();

	static const struct { U8 mode; const C8 *name; } targets[] = {
		{ ESHBinaryType_SPIRV, "spirv" },
		{ ESHBinaryType_DXIL,  "dxil"  }
	};

	for (U64 tg = 0; tg < sizeof(targets) / sizeof(targets[0]); ++tg) {

		const U8 mode = targets[tg].mode;
		const C8 *bk = targets[tg].name;

		ListBuffer out = (ListBuffer) { 0 };
		SHFile sh = (SHFile) { 0 };
		CharString label = CharString_createNull();

		Bool ok =
			compileFileShader(alloc, "reflection/resources.hlsl", mode, true, false, &out, &err) &&
			out.length == 1 && Buffer_length(out.ptr[0]) &&
			readOiSH(alloc, out.ptr[0], &sh, &err) && sh.binaries.length >= 1;

		//Assert a register's base type (+ optionally the IsWrite bit) by name. Name-matched, so it's immune
		//to register ordering differences between the SPIRV and DXIL reflectors.
		#define ASSERT_REG(nm, wantType, wantWrite) do {                                                  \
			const SHRegisterRuntime *r = ok ? findReg(&sh, nm) : NULL;                                     \
			const Bool isW = r && (r->reg.registerType & ESHRegisterType_IsWrite);                         \
			if (!CharString_format(alloc, &label, &err, "%s: %s (%s)", nm, #wantType, bk))                 \
				label = CharString_createRefCStrConst(nm);                                                 \
			Test_assert(t, label.ptr,                                                                      \
				r && (r->reg.registerType & ESHRegisterType_TypeMask) == (wantType) && isW == (wantWrite));\
			CharString_free(&label, alloc);                                                                \
		} while (0)

		ASSERT_REG("inBuf",  ESHRegisterType_StructuredBuffer,     false);
		ASSERT_REG("outBuf", ESHRegisterType_StructuredBuffer,     true);
		ASSERT_REG("rawIn",  ESHRegisterType_ByteAddressBuffer,    false);
		ASSERT_REG("rawOut", ESHRegisterType_ByteAddressBuffer,    true);
		ASSERT_REG("tex1d",  ESHRegisterType_Texture1D,            false);
		ASSERT_REG("tex",    ESHRegisterType_Texture2D,            false);
		ASSERT_REG("tex3d",  ESHRegisterType_Texture3D,            false);
		ASSERT_REG("texCube",ESHRegisterType_TextureCube,          false);
		ASSERT_REG("texMS",  ESHRegisterType_Texture2DMS,          false);
		ASSERT_REG("shadowMap", ESHRegisterType_Texture2D,         false);
		ASSERT_REG("img",    ESHRegisterType_Texture2D,            true);
		ASSERT_REG("samp",   ESHRegisterType_Sampler,              false);
		ASSERT_REG("tlas",   ESHRegisterType_AccelerationStructure, false);

		//SamplerComparisonState: DXIL has a dedicated comparison-sampler type; SPIRV only sees a plain sampler
		//(documented DXIL/SPIRV quirk in oiSH.md), so the expected register type is backend-dependent.
		{
			const SHRegisterRuntime *r = ok ? findReg(&sh, "sampCmp") : NULL;
			const U8 want = mode == ESHBinaryType_DXIL
				? ESHRegisterType_SamplerComparisonState : ESHRegisterType_Sampler;
			if (!CharString_format(alloc, &label, &err, "sampCmp comparison sampler (%s)", bk))
				label = CharString_createRefCStrConst("sampCmp");
			Test_assert(t, label.ptr, r && (r->reg.registerType & ESHRegisterType_TypeMask) == want);
			CharString_free(&label, alloc);
		}

		//Descriptor array Texture2D[4]: recorded via the arrays dimension list, not the IsArray (layered) flag.
		{
			const SHRegisterRuntime *r = ok ? findReg(&sh, "texArr") : NULL;
			if (!CharString_format(alloc, &label, &err, "texArr Texture2D[4] descriptor array (%s)", bk))
				label = CharString_createRefCStrConst("texArr");
			Test_assert(t, label.ptr,
				r && (r->reg.registerType & ESHRegisterType_TypeMask) == ESHRegisterType_Texture2D &&
				!(r->reg.registerType & ESHRegisterType_IsArray) &&
				r->arrays.length == 1 && r->arrays.ptr[0] == 4);
			CharString_free(&label, alloc);
		}

		//Structured-buffer element stride: Particle is four 4-byte members => 16 bytes.
		{
			const SHRegisterRuntime *r = ok ? findReg(&sh, "outBuf") : NULL;
			if (!CharString_format(alloc, &label, &err, "outBuf element stride is 16 (%s)", bk))
				label = CharString_createRefCStrConst("outBuf stride");
			Test_assert(t, label.ptr, r && r->shaderBuffer.bufferSize == 16);
			CharString_free(&label, alloc);
		}

		//The binding for the backend we actually compiled is populated; other backends' slots stay U32_MAX
		//(this per-backend duality is what lets SHFile_combine merge a SPIRV + DXIL binary's bindings).
		{
			const SHRegisterRuntime *r = ok ? findReg(&sh, "outBuf") : NULL;
			if (!CharString_format(alloc, &label, &err, "outBuf binding present for %s only", bk))
				label = CharString_createRefCStrConst("outBuf binding");
			Test_assert(t, label.ptr,
				r && r->reg.bindings.arr[mode].binding != U32_MAX &&
				r->reg.bindings.arr[mode].space != U32_MAX);
			CharString_free(&label, alloc);
		}

		#undef ASSERT_REG

		//The whole reflected oiSH round-trips (read -> write -> byte-identical) on this backend.
		{
			if (!CharString_format(alloc, &label, &err, "oiSH round-trips (%s)", bk))
				label = CharString_createRefCStrConst("oiSH round-trips");
			Test_assert(t, label.ptr, ok && oiSHRoundtrips(alloc, out.ptr[0], &err));
			CharString_free(&label, alloc);
		}

		if (!ok)
			Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

		SHFile_free(&sh, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();
	}

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
}
