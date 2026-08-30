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

//graphics/test/interface/test_graphics_shaders_serialize.cpp
//
//Graphics pipelines that only have to CREATE, rather than draw: a pair whose entrypoints aren't named "main",
//and a pipeline dumped to an oiSP, lowered back and rebuilt from it.
//Both build on the same named pair, since it declares neither push constants nor app data and so needs no
//layout beyond the device's default one, which is what a stored pipeline rebuilds against.

//The shared helpers in terms of the handle types. Both C++ headers come BEFORE the block below: a
//standard header included after the C headers landed in oxc::c finds its guard already tripped and
//leaves its symbols in that namespace.

#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/texture_format.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_file.h"
	#include "formats/oiSP/sp_file.h"
	#include "platforms/platform.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/pipeline_serialize.h"
	#include "test_graphics_shared.h"
} }

using namespace oxc;

//The two files every test here loads, by slot in one shared file list.

static const c::C8 *namedShaderPaths[2] = {
	"//OxC3_gtest/test_shaders/test_named_vs.oiSH",
	"//OxC3_gtest/test_shaders/test_named_ps.oiSH"
};

//The name each oiSH is stored under, which is what a stored pipeline resolves its stages by.

static const c::CharString namedShaderNames[2] = {
	c::CharString_createRefCStrConst("test_named_vs.oiSH"), c::CharString_createRefCStrConst("test_named_ps.oiSH")
};

//An SHFile has no handle of its own, so the pair gets one guard that frees on every exit path.

struct NamedFiles {

	c::SHFile files[2] = {};
	c::ListSHFile list = {};
	c::ListCharString names = {};
	const c::Allocator *alloc;

	explicit NamedFiles(const c::Allocator *a) : alloc(a) {}

	~NamedFiles() {
		for(c::U64 i = 0; i < 2; ++i)
			c::SHFile_free(&files[i], alloc);
	}

	//False means the build had no shader compiler, which every module treats as a skip.

	[[nodiscard]] c::Bool load(c::Test *t) {

		c::Bool loadedAll = true;

		for(c::U64 i = 0; i < 2; ++i)
			loadedAll &= gfxtest::loadFile(t, namedShaderPaths[i], files[i]);

		if(!loadedAll)
			return false;

		c::ListSHFile_createRefConst(files, 2, &list, NULL);
		c::ListCharString_createRefConst(namedShaderNames, 2, &names, NULL);
		return true;
	}
};

//The compiler renames a non lib module's sole entrypoint to "main" in the SPIR-V while reflection keeps the
//original name ("mainVS"/"mainPS"), so the oiSH is searched under the HLSL name and the backend binds whatever
//the module actually kept.
//The stage list is built from ids this already resolved, so the C entry point is what takes it: the wrapper's
//graphics factory resolves its own entries by name and has no form that accepts them.

static c::Bool TestShaders_namedPipeline(
	c::Test *t, gfx::Device &dev, const NamedFiles &named, const c::PipelineGraphicsInfo *info, gfx::Pipeline &pipeline
) {

	const c::U32 vertexId = gfxtest::entry(t, dev, named.files[0], "mainVS");
	const c::U32 pixelId = gfxtest::entry(t, dev, named.files[1], "mainPS");

	if(vertexId == c::U32_MAX || pixelId == c::U32_MAX)
		return false;

	c::PipelineStage stages[2] = {
		{ .binaryId = vertexId, .shFileId = 0 },
		{ .binaryId = pixelId, .shFileId = 1 }
	};

	c::ListPipelineStage stageList {};
	c::ListPipelineStage_createRefConst(stages, 2, &stageList, NULL);

	const c::CharString name = c::CharString_createRefCStrConst("Named entrypoint graphics pipeline");
	c::PipelineRef *raw = NULL;

	if(!Test_assert(t, "createNamedEntryPipeline", c::GraphicsDeviceRef_createPipelineGraphics(
		(c::GraphicsDeviceRef*) dev.handle(), &named.list, &stageList, info, &name, c::EPipelineFlags_None,
		NULL, &raw, &t->err
	)))
		return false;

	pipeline = gfx::Pipeline(::oxc::RefPtr<c::Pipeline>::share(raw));
	c::RefPtr_dec(&raw);
	return true;
}

//Whether this device can build the pipelines here at all; the reason is printed so a skip reads as one.

static c::Bool TestShaders_canCreateGraphics(c::Test *t, gfx::Device &dev) {

	if (!dev.hasBindlessTable()) {
		c::Test_print(t, "Device has no bindless descriptor table, skipping graphics pipeline creation tests");
		return false;
	}

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_DirectRendering)) {
		c::Test_print(t, "Device lacks direct rendering, skipping graphics pipeline creation tests");
		return false;
	}

	return true;
}

// -- Named entrypoint graphics pipeline ------------------------------------------

//Only creation is exercised, since that is exactly where a wrong entrypoint name is rejected by validation.

extern "C" void Test_graphicsShaderNamedEntry(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Shaders/namedEntry");

	gfx::Device dev = gfx::Device::share(deviceRef);

	if(!TestShaders_canCreateGraphics(t, dev))
		return;

	NamedFiles named(dev.alloc());

	if (!named.load(t)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping named entrypoint test");
		return;
	}

	const c::PipelineGraphicsInfo info = {
		.attachmentFormatsExt = { c::ETextureFormatId_RGBA8 },
		.attachmentCountExt = 1
	};

	gfx::Pipeline pipeline;
	(void) TestShaders_namedPipeline(t, dev, named, &info, pipeline);
}

// -- Pipeline serialize (oiSP) ---------------------------------------------------

//A live pipeline dumps to an oiSP that's exact, lowers back to the state it was created with, and rebuilds.

extern "C" void Test_graphicsShaderPipelineSerialize(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Shaders/pipeline serialize (oiSP)");

	gfx::Device dev = gfx::Device::share(deviceRef);

	if(!TestShaders_canCreateGraphics(t, dev))
		return;

	const c::Allocator *alloc = dev.alloc();
	NamedFiles named(alloc);

	if (!named.load(t)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping pipeline serialize tests");
		return;
	}

	//State with enough non default values that a lossy round trip would show

	c::PipelineGraphicsInfo info = {
		.rasterizer = { .cullMode = c::ECullMode_None },
		.blendState = {
			.enable = true, .renderTargetMask = 1,
			.writeMask = { c::EWriteMask_All },
			.attachments = { {
				.srcBlend = c::EBlend_One, .dstBlend = c::EBlend_One,
				.srcBlendAlpha = c::EBlend_One, .dstBlendAlpha = c::EBlend_One,
				.blendOp = c::EBlendOp_Add, .blendOpAlpha = c::EBlendOp_Add
			} }
		},
		.attachmentFormatsExt = { c::ETextureFormatId_RGBA8 },
		.attachmentCountExt = 1
	};

	gfx::Pipeline pipeline;

	if(!TestShaders_namedPipeline(t, dev, named, &info, pipeline))
		return;

	//An SPFile is a plain C struct with no handle of its own, so it gets the same guard the files have.

	using OwnedSPFile = gfx::OwnedList<c::SPFile, c::SPFile_free>;
	OwnedSPFile sp(alloc);

	if(!Test_assert(t, "createSP", c::SPFile_create(c::ESPSettingsFlags_None, alloc, &sp.list, &t->err)))
		return;

	c::U32 pipelineId = c::U32_MAX;

	if(!Test_assert(t, "dump", c::Pipeline_toSPFile(
		(c::PipelineRef*) pipeline.handle(), &named.list, &named.names, c::CharString_createRefCStrConst("named"),
		alloc, &sp.list, &pipelineId, &t->err
	)))
		return;

	//Dumped state is exact (nothing assumed) and keeps the one declared target rather than growing to eight

	Test_assert(t, "dumpExact", c::SPFile_isExact(&sp.list, pipelineId));

	const c::SPGraphicsState *state = c::SPFile_graphicsState(&sp.list, pipelineId);

	if (Test_assert(t, "dumpHasGraphicsState", state != NULL)) {
		Test_assert(t, "dumpTargetCount", state->renderTargetCount == 1);
		Test_assert(t, "dumpTargetFormat", state->renderTargetFormats[0] == (c::U8) c::ETextureFormatId_RGBA8);
		Test_assert(t, "dumpBlend", state->blend.enable && state->blend.attachments[0].srcBlend == c::EBlend_One);
	}

	//Lowering gives back what the pipeline was created with

	c::PipelineGraphicsInfo back = {};

	if (Test_assert(t, "lower", c::SPFile_toGraphicsInfo(&sp.list, pipelineId, &back, &t->err))) {

		Test_assert(
			t, "lowerTargets",
			back.attachmentCountExt == 1 && back.attachmentFormatsExt[0] == c::ETextureFormatId_RGBA8
		);

		Test_assert(t, "lowerRasterizer", back.rasterizer.cullMode == c::ECullMode_None);
		Test_assert(t, "lowerBlend", back.blendState.enable && back.blendState.renderTargetMask == 1);

		Test_assert(
			t, "lowerAttachment",
			back.blendState.attachments[0].dstBlendAlpha == c::EBlend_One &&
			back.blendState.attachments[0].blendOp == c::EBlendOp_Add
		);

		Test_assert(t, "lowerWriteMask", back.blendState.writeMask[0] == c::EWriteMask_All);
	}

	//And a stored pipeline rebuilds against the same oiSH files, resolved by name

	c::PipelineRef *raw = NULL;

	if (Test_assert(t, "rebuild", c::GraphicsDeviceRef_createPipelineFromSPFile(
		(c::GraphicsDeviceRef*) dev.handle(), &sp.list, pipelineId, &named.list, &named.names, NULL, alloc, &raw,
		&t->err
	))) {
		Test_assert(t, "rebuilt", raw != NULL);
		c::RefPtr_dec(&raw);
	}
}
