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

//test_package/test_oxc3_package.c

#include "audio/audio_interface.h"
#include "formats/oiSR/sr_file.h"
#include "formats/wav/wav_file.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "types/container/log.h"
#include "types/math/mat.h"
#include "types/base/error.h"

#if OXC3_TEST_COMPILER
	#include "shader_compiler/compiler.h"
#endif

#if OXC3_TEST_DYNAMIC_COMPILER && !defined(SHADER_COMPILER_DYNAMIC)
	#error "The shared shader compiler must export SHADER_COMPILER_DYNAMIC to its consumers"
#endif

#if !OXC3_TEST_DYNAMIC_COMPILER && defined(SHADER_COMPILER_DYNAMIC)
	#error "A static or disabled shader compiler must not export SHADER_COMPILER_DYNAMIC"
#endif

static_assert(_ENABLE_SIMD == OXC3_TEST_SIMD, "Consumer SIMD ABI differs from the installed library");
static_assert(_OXC3_ERROR_STACKTRACE == OXC3_TEST_ERROR_FRAMES, "Consumer Error ABI differs from the installed library");

#if OXC3_TEST_COMPILER

static Bool Consumer_compile(const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	Compiler compiler = (Compiler) { 0 };
	CompileResult result = (CompileResult) { 0 };
	Bool created = false;

	CompilerSettings settings = (CompilerSettings) {
		.string = CharString_createRefCStrConst(
			"#include \"@types.hlsli\"\n"
			"RWByteAddressBuffer output;\n"
			"[numthreads(1, 1, 1)]\n"
			"void main(U32x3 id : SV_DispatchThreadID) { output.Store(id.x * 4, 42u); }\n"
		),
		.path = CharString_createRefCStrConst("installed_consumer.hlsl"),
		.format = ECompilerFormat_HLSL,
		.outputType = EGfxBinaryType_SPIRV,
		.containsGfxOrComp = true
	};

	SHBinaryIdentifier identifier = (SHBinaryIdentifier) {
		.entrypoint = CharString_createRefCStrConst("main"),
		.shaderVersion = OISH_SHADER_MODEL(6, 5),
		.stageType = EGfxPipelineStage_Compute
	};

	Compiler_setPlatform(Platform_instance);
	gotoIfError3(clean, Compiler_create(alloc, &compiler, e_rr));
	created = true;
	gotoIfError3(clean, Compiler_compile(&compiler, &settings, &identifier, alloc, &result, e_rr));

	if(!result.isSuccess || result.type != ECompileResultType_Binary || !Buffer_length(result.binary))
		retError(clean, Error_invalidState(0, "Consumer_compile()::compiler did not produce a shader"));

clean:
	CompileResult_free(&result, alloc);

	if(created)
		Compiler_free(&compiler, alloc);

	Compiler_shutdown();
	return s_uccess;
}

#endif

static Bool Consumer_check(const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	AudioInterfaceRef *audio = NULL;
	const RefPtrType audioType = AudioInterface_makeType(alloc);
	SRFile reflection = (SRFile) { 0 };
	CharString name = CharString_createRefCStrConst("consumer");
	U32 nameId = U32_MAX, repeatedId = U32_MAX;
	const F32 sample = 1;
	F32x4x4 inverse;
	Buffer shader = Buffer_createNull();

	if(WAVFile_cvt(&sample, 4, 2, 0, false, true) != 32767)
		retError(clean, Error_invalidState(0, "Consumer_check()::WAV sample conversion failed"));

	if(!F32x4x4_inverse(F32x4x4_scale3(2, 4, 8), &inverse))
		retError(clean, Error_invalidState(1, "Consumer_check()::matrix inverse failed"));

	if(!F32x4_eqApprox4(F32x4x4_transformPoint(inverse, F32x4_create3(2, 4, 8)), F32x4_one()))
		retError(clean, Error_invalidState(2, "Consumer_check()::matrix ABI differs"));

	//Creating the interface checks the OpenAL dependency without opening or playing to an audio device.
	gotoIfError3(clean, AudioInterface_create(&audio, alloc, &audioType, e_rr));

	if(AudioInterfaceRef_ptr(audio)->api != EAudioApi_OpenAL)
		retError(clean, Error_invalidState(3, "Consumer_check()::audio interface has the wrong API"));

	gotoIfError3(clean, SRFile_create(ESRSettingsFlags_None, ESRFeature_Basics, alloc, &reflection, e_rr));
	gotoIfError3(clean, SRFile_addString(&reflection, &name, alloc, &nameId, e_rr));
	gotoIfError3(clean, SRFile_addString(&reflection, &name, alloc, &repeatedId, e_rr));

	if(nameId == U32_MAX || nameId != repeatedId)
		retError(clean, Error_invalidState(4, "Consumer_check()::oiSR name interning failed"));

	#if OXC3_TEST_GRAPHICS

		const CharString section = CharString_createRefCStrConst("//OxC3_graphics");
		const CharString shaderPath = CharString_createRefCStrConst("//OxC3_graphics/shaders/image_copy.oiSH");

		gotoIfError3(clean, File_loadVirtual(&section, NULL, NULL, NULL, alloc, e_rr));
		gotoIfError3(clean, File_read(&shaderPath, U64_MAX, 0, 0, NULL, &shader, e_rr));

		if(!Buffer_length(shader))
			retError(clean, Error_invalidState(5, "Consumer_check()::installed graphics shader is empty"));

	#endif

	#if OXC3_TEST_COMPILER
		gotoIfError3(clean, Consumer_compile(alloc, e_rr));
	#endif

clean:
	Buffer_free(&shader, alloc);
	SRFile_free(&reflection, alloc);
	RefPtr_dec(&audio);
	return s_uccess;
}

Platform_defineEntrypoint() {

	Error error = Error_none();

	if(!Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, true, &error))
		Platform_return(1);

	const Allocator *alloc = Platform_instance->alloc;
	const Bool success = Consumer_check(alloc, &error);

	if(!success)
		Error_print(alloc, &error, ELogLevel_Error, ELogOptions_NewLine | ELogOptions_NoBreak);

	Platform_cleanup();
	Platform_return(success ? 0 : 1);
}
