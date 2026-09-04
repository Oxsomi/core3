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
//Pipelines that only have to CREATE, rather than draw: a pair whose entrypoints aren't named "main", a
//pipeline dumped to an oiSP, lowered back and rebuilt from it, and the layout a derived oiSP describes made
//real and handed to the driver's own introspection.
//The first two build on the same named pair, since it declares neither push constants nor app data and so
//needs no layout beyond the device's default one, which is what a stored pipeline rebuilds against; the
//third needs a shader that declares one of its own, so it brings its own file.

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
	#include "graphics/generic/descriptor_layout.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/pipeline_layout.h"
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

//An SPFile, a name list and an executable list are plain C structs with no handle of their own, so each
//gets the same guard the files have.
//Local rather than gfx::OwnedList: that takes its free through an OwnedListFree<T> specialization, and
//teaching graphics.hpp about three types just for a test isn't worth the coupling.

struct OwnedSPFile {

	c::SPFile list = {};
	const c::Allocator *alloc;

	explicit OwnedSPFile(const c::Allocator *a) : alloc(a) {}
	~OwnedSPFile() { c::SPFile_free(&list, alloc); }

	OwnedSPFile(const OwnedSPFile&) = delete;
	OwnedSPFile &operator=(const OwnedSPFile&) = delete;
};

struct OwnedRuntimeNames {

	c::ListCharString list = {};
	const c::Allocator *alloc;

	explicit OwnedRuntimeNames(const c::Allocator *a) : alloc(a) {}

	//The names are refs into the device's own layouts, so only the list itself is owned here.

	~OwnedRuntimeNames() { c::ListCharString_free(&list, alloc); }

	OwnedRuntimeNames(const OwnedRuntimeNames&) = delete;
	OwnedRuntimeNames &operator=(const OwnedRuntimeNames&) = delete;
};

struct OwnedExecutables {

	c::ListPipelineExecutable list = {};
	const c::Allocator *alloc;

	explicit OwnedExecutables(const c::Allocator *a) : alloc(a) {}
	~OwnedExecutables() { c::ListPipelineExecutable_freeUnderlying(&list, alloc); }

	OwnedExecutables(const OwnedExecutables&) = delete;
	OwnedExecutables &operator=(const OwnedExecutables&) = delete;
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

// -- Live ISA: a derived layout instantiated, then asked what the driver compiled ---

//The path 'OxC3 isa disassemble -asic live' takes, which is the only one that instantiates a DERIVED
//layout: an oiSP is derived from a shader's own reflection and the layout it describes becomes a real one.
//test_frame_globals declares nothing of its own except a push constant, since the bindless set and the per
//frame globals it reads are the runtime's, so derivation leaves a layout holding the range alone and the
//device's two layouts have to come back when it is instantiated. Without them the pipeline uses descriptors
//its layout never declared, which a driver rejects.
//The disassembly itself can't be held against a golden: whether a driver returns ISA text at all is its own
//decision (AMD's own driver and the open source Mesa drivers do, the closed source ones vary), so the text
//only has to BE text wherever one returned some, and the executables only have to arrive.

extern "C" void Test_graphicsShaderPipelineIsa(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Shaders/live ISA");

	gfx::Device dev = gfx::Device::share(deviceRef);

	//The derived layout is completed from the device's own bindless set and globals, which a device without
	//bindless has neither of.

	if (!dev.hasBindlessTable()) {
		c::Test_print(t, "Device has no bindless descriptor table, skipping live ISA tests");
		return;
	}

	const c::Allocator *alloc = dev.alloc();
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile shader(alloc);

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_frame_globals.oiSH", shader.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping live ISA test");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, shader.list, "main");

	if(entryId == c::U32_MAX)
		return;

	c::ListSHFile fileList {};
	c::ListSHFile_createRefConst(&shader.list, 1, &fileList, NULL);

	OwnedSPFile sp(alloc);

	if(!Test_assert(t, "createSP", c::SPFile_create(c::ESPSettingsFlags_None, alloc, &sp.list, e_rr)))
		return;

	//Derived the way the CLI derives it: the low half of a resolved id is the entry, and the registers the
	//device names are the runtime's own, so derivation must not describe them.

	const c::SPStageRef stages[1] = { { .fileId = 0, .entryId = (c::U16) entryId } };
	c::U32 pipelineId = c::U32_MAX;

	{
		OwnedRuntimeNames runtimeNames(alloc);

		if(!Test_assert(t, "runtimeRegisterNames", c::GraphicsDeviceRef_runtimeRegisterNames(
			(c::GraphicsDeviceRef*) dev.handle(), &runtimeNames.list, alloc, e_rr
		)))
			return;

		if(!Test_assert(t, "derive", c::SPFile_derivePipeline(
			&sp.list, &fileList, NULL, c::CharString_createNull(), stages, 1, &runtimeNames.list, alloc,
			&pipelineId, e_rr
		)))
			return;
	}

	//Compute carries no state beyond its shader, so nothing about this pipeline is assumed.

	Test_assert(t, "deriveExact", c::SPFile_isExact(&sp.list, pipelineId));

	//A push constant is the caller's own register, so the file describes a layout and instantiating it has to
	//hand one back rather than leave the pipeline on the device's default.

	c::PipelineLayoutRef *layoutRaw = NULL;

	if(!Test_assert(t, "createLayout", c::SPFile_createPipelineLayout(
		(c::GraphicsDeviceRef*) dev.handle(), &sp.list, pipelineId, alloc, &layoutRaw, e_rr
	)))
		return;

	gfx::PipelineLayout layout(oxc::RefPtr<c::PipelineLayout>::adopt(layoutRaw));

	if(!Test_assert(t, "layoutDescribed", layout.valid()))
		return;

	//What derivation left out has to be back, or the shader's own registers have no layout at all.

	const c::PipelineLayout *layoutPtr = layout.data();

	Test_assert(t, "layoutKeepsBindless", c::PipelineLayout_usesRuntimeBindless(layoutPtr));
	Test_assert(t, "layoutKeepsGlobals", c::PipelineLayout_hasRuntimeGlobals(layoutPtr));
	Test_assert(t, "layoutPushConstant", layoutPtr->info.pushConstants.count == 1);
	Test_assert(t, "layoutPushConstantSize", layoutPtr->info.pushConstants.constantBufferSize == 16);

	//CaptureISA is what makes the driver keep the introspection below, and creating the pipeline at all is
	//what a layout missing the runtime's descriptors fails.

	const c::CharString name = c::CharString_createRefCStrConst("Live ISA pipeline");
	c::PipelineRef *pipelineRaw = NULL;

	if(!Test_assert(t, "createPipeline", c::GraphicsDeviceRef_createPipelineCompute(
		(c::GraphicsDeviceRef*) dev.handle(), &shader.list, &name, entryId, NULL,
		c::EPipelineFlags_CaptureISA, layout.handle(), &pipelineRaw, e_rr
	)))
		return;

	gfx::Pipeline pipeline(oxc::RefPtr<c::Pipeline>::adopt(pipelineRaw));

	if (!(dev.info().capabilities.features2 & c::EGraphicsFeatures2_PipelineExecutableInfo)) {
		c::Test_print(t, "Device exposes no pipeline introspection, so only the pipeline itself was covered");
		return;
	}

	OwnedExecutables execs(alloc);

	if(!Test_assert(t, "getExecutables", c::GraphicsDeviceRef_getPipelineExecutables(
		(c::PipelineRef*) pipeline.handle(), alloc, &execs.list, e_rr
	)))
		return;

	if(!Test_assert(t, "executableCount", execs.list.length != 0))
		return;

	c::U64 withDisassembly = 0;

	for (c::U64 i = 0; i < execs.list.length; ++i) {

		const c::PipelineExecutable exec = execs.list.ptr[i];

		Test_assert(t, "executableNamed", c::CharString_length(exec.name) != 0);

		const c::U64 len = c::CharString_length(exec.disassembly);

		if(!len)
			continue;

		++withDisassembly;

		//Text, rather than whatever a wrong length or a released buffer leaves behind.

		c::Bool printable = true;

		for (c::U64 j = 0; j < len; ++j) {
			const c::C8 c = exec.disassembly.ptr[j];
			printable &= (c >= 0x20 && c < 0x7F) || c == '\t' || c == '\r' || c == '\n';
		}

		Test_assert(t, "disassemblyIsText", printable);
	}

	c::Test_print(
		t,
		withDisassembly ?
		"Driver returned ISA text" : "Driver returned statistics only, no ISA text, which is driver dependent"
	);
}
