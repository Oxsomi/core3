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

//graphics/test/interface/test_graphics_shaders_draw.cpp
//
//Draw execution, and the pipeline/render helpers only these draws use.
//Split out of test_graphics_shaders.c, which had grown past 2300 lines.

//The shared helpers in terms of the handle types. Both C++ headers come BEFORE the block below: a
//standard header included after the C headers landed in oxc::c finds its guard already tripped and
//leaves its symbols in that namespace.

#include "test_graphics_shared.hpp"

//Log::debugLn is the C++ front for Log_debugLnx; the x macros name ELogOptions_NewLine unqualified and
//so cannot be reached through the c namespace.

#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/buffer.h"
	#include "types/container/memory_stream.h"
	#include "types/container/texture_format.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_file.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/file.h"
	#include "platforms/logx.h"
	#include "platforms/platform.h"
	#include "graphics/generic/bindless_descriptor.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/depth_stencil.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/opacity_micromap.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/render_texture.h"
	#include "graphics/generic/texture.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

using namespace oxc;

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

//The vertex and pixel shader are separate single entry files, picked out of one shared file list by slot

//The pixel entry is named because SHFile_combine matches entries by name, so two pixel shaders in one
//package can't both be "main" unless they agree on their signature - which a 1 target and a 2 target one
//don't.

//Shared by every pipeline this module builds; they all read the same push constant block.
//Module statics because the pipelines share one layout, so it outlives any single phase - and it has to be
//released before the module returns, or a static handle outlives the device and releases into one that is
//already gone.

static gfx::PipelineLayout drawPushLayout;
static gfx::PipelineLayout argsPushLayout;

//c::TestShaderPushData has exactly the layout the pixel shaders declare, so it is what gets pushed.
//The bytes are captured at record time, so every assignment below precedes the recording that reads it.

static c::TestShaderPushData drawPush = {};

//The draw phases record into a scope that already has a cleared render pass open; that pair is
//gfxtest::DrawPass, since a helper cannot hand back two objects that must outlive it any other way.

using gfxtest::DrawPass;

static c::Bool TestShaders_graphicsPipelineNamed(
	c::Test *t, gfx::Device &dev, const c::ListSHFile *files, c::U16 vertexFile, c::U16 pixelFile,
	const c::C8 *pixelEntry, const c::PipelineGraphicsInfo *info, gfx::Pipeline &pipeline
) {

	const c::U32 vertexId = gfxtest::entry(t, dev, files->ptr[vertexFile], "main");
	const c::U32 pixelId = gfxtest::entry(t, dev, files->ptr[pixelFile], pixelEntry);

	if(vertexId == c::U32_MAX || pixelId == c::U32_MAX)
		return false;

	//The pixel shaders read their color from a push constant, so the layout has to declare the push
	//constants. Detected from the pixel entry, which is the stage that reads them.

	if(!drawPushLayout.valid() && !gfxtest::pushConstantLayout(t, dev, files->ptr[pixelFile], pixelId, drawPushLayout))
		return false;

	//The stage list is built from ids this already resolved, so the C entry point is what takes it: the
	//wrapper's graphics factory resolves its own entries by name and has no form that accepts them.

	c::PipelineStage stages[2] = {
		{ .binaryId = vertexId, .shFileId = vertexFile },
		{ .binaryId = pixelId, .shFileId = pixelFile }
	};

	c::ListPipelineStage stageList {};
	c::ListPipelineStage_createRefConst(stages, 2, &stageList, NULL);

	const c::CharString name = c::CharString_createRefCStrConst("Shader test graphics pipeline");
	c::PipelineRef *raw = NULL;

	if(!Test_assert(t, "createGraphicsPipeline", c::GraphicsDeviceRef_createPipelineGraphics(
		(c::GraphicsDeviceRef*) dev.handle(), files, &stageList, info, &name, c::EPipelineFlags_None,
		(c::PipelineLayoutRef*) drawPushLayout.handle(), &raw, &t->err
	)))
		return false;

	pipeline = gfx::Pipeline(::oxc::RefPtr<c::Pipeline>::share(raw));
	c::RefPtr_dec(&raw);
	return true;
}

static c::Bool TestShaders_graphicsPipeline(
	c::Test *t, gfx::Device &dev, const c::ListSHFile *files, c::U16 vertexFile, c::U16 pixelFile,
	const c::PipelineGraphicsInfo *info, gfx::Pipeline &pipeline
) {
	return TestShaders_graphicsPipelineNamed(t, dev, files, vertexFile, pixelFile, "main", info, pipeline);
}

//Opens a scope, starts a cleared render into the 8x8 target and binds the pipeline with full viewport and scissor.
//The caller already began the command list, so a compute scope can precede the render scope when a phase needs one.

static DrawPass TestShaders_openDraw(
	c::Test *t, gfx::CommandList &commandList, c::U32 scopeId, c::RefPtr *target, const gfx::Pipeline &pipeline
) {

	c::Error *e_rr = &t->err;

	gfx::CommandScope scope = commandList.scope({}, scopeId, {}, e_rr);
	c::Bool ok = Test_assert(t, "scope", (c::Bool) scope);

	const c::AttachmentInfo color = { .image = target, .load = c::ELoadAttachmentType_Clear };

	gfx::CommandRender render = scope.render(c::I32x2_zero, c::I32x2_create2(8, 8), { color }, NULL, e_rr);

	ok &= Test_assert(t, "renderStart", (c::Bool) render);
	ok &= Test_assert(t, "viewportScissor", render.setViewportAndScissor(c::I32x2_zero, c::I32x2_zero, e_rr));
	ok &= Test_assert(t, "bindPipeline", render.setGraphicsPipeline(pipeline, e_rr));

	const c::Bool pushed = Test_assert(t, "pushDraw", render.setPushConstants(drawPush, e_rr));

	return DrawPass{ static_cast<gfx::CommandScope&&>(scope), static_cast<gfx::CommandRender&&>(render), pushed && ok };
}

static c::Bool TestShaders_closeDraw(c::Test *t, DrawPass &pass, gfx::CommandList &commandList) {
	c::Bool ok = Test_assert(t, "renderEnd", pass.render.end(&t->err));
	ok &= Test_assert(t, "scopeEnd", pass.scope.end(&t->err));
	return Test_assert(t, "end", commandList.end(&t->err)) && ok;
}

// -- 32. Draw execution ----------------------------------------------------------

//Every draw renders into an 8x8 target and the resolved pixels are byte compared, colors picked to be exact
// in 8 bit UNORM so rounding can't blur a pass into a flake.
//Covered here: a fullscreen triangle, scissor clipping, additive blending, an indexed and instanced draw
// through real vertex and index buffers, depth test accept and reject, indirect draws from CPU and GPU
// written arguments and an MSAA 4x draw that resolves into the readback target.

extern "C" void Test_graphicsShaderDraw(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Shaders/draw");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if (!dev.hasBindlessTable()) {
		c::Test_print(t, "Device has no bindless descriptor table, skipping draw execution tests");
		return;
	}

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_DirectRendering)) {
		c::Test_print(t, "Device lacks direct rendering, skipping draw execution tests");
		return;
	}

	const c::Allocator *alloc = dev.alloc();

	//One shared file list all graphics pipelines pick their stages from by slot

	static const c::C8 *drawShaderPaths[] = {
		"//OxC3_gtest/test_shaders/test_draw_vs.oiSH",
		"//OxC3_gtest/test_shaders/test_draw_ps.oiSH",
		"//OxC3_gtest/test_shaders/test_depth_vs.oiSH",
		"//OxC3_gtest/test_shaders/test_depth_ps.oiSH",
		"//OxC3_gtest/test_shaders/test_vertex_vs.oiSH",
		"//OxC3_gtest/test_shaders/test_write_args.oiSH",
		"//OxC3_gtest/test_shaders/test_draw_mrt_ps.oiSH",
		"//OxC3_gtest/test_shaders/test_logicop_vs.oiSH",
		"//OxC3_gtest/test_shaders/test_logicop_ps.oiSH",
		"//OxC3_gtest/test_shaders/test_draw_dualsrc_ps.oiSH"
	};

	//An SHFile has no handle of its own, so the whole set gets one guard that frees on every exit path.

	struct OwnedFiles {

		c::SHFile files[10] = {};
		const c::Allocator *alloc;

		explicit OwnedFiles(const c::Allocator *a) : alloc(a) {}

		~OwnedFiles() {
			for(c::U64 i = 0; i < 10; ++i)
				c::SHFile_free(&files[i], alloc);
		}
	} owned(alloc);

	c::SHFile *files = owned.files;
	c::Bool loadedAll = true;

	for(c::U64 i = 0; i < 10; ++i)
		loadedAll &= gfxtest::loadFile(t, drawShaderPaths[i], files[i]);

	if (!loadedAll) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping draw execution tests");
		return;
	}

	c::ListSHFile fileList {};
	c::ListSHFile_createRefConst(files, 10, &fileList, NULL);

	gfx::RenderTexture target, msaaTarget, mrtTarget, logicTarget;
	gfx::DepthStencil depth;
	gfx::DeviceBuffer vertexBuffer, indexBuffer, cpuArgs, gpuArgs;
	gfx::Pipeline flatPipeline, blendPipeline, wirePipeline, mrtPipeline, logicPipeline;
	gfx::Pipeline dualPipeline, vertexPipeline, depthPipeline, msaaPipeline, argsPipeline;
	gfx::CommandList commandList, emptyList;

	Test_assert(t, "createTarget", dev.createRenderTexture(
		8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None,
		"Shader draw target", target, c::EMSAASamples_Off, nullptr, e_rr
	));

	//Designators must appear in declaration order in C++, which is not the order these were written in.

	const c::PipelineGraphicsInfo flatInfo = {
		.attachmentFormatsExt = { c::ETextureFormatId_RGBA8 },
		.attachmentCountExt = 1
	};

	if(!target || !TestShaders_graphicsPipeline(t, dev, &fileList, 0, 1, &flatInfo, flatPipeline))
		return;

	if(!Test_assert(t, "createList", dev.createCommandList(8 * c::KIBI, 128, 32, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "createEmptyList", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmptyList", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmptyList", emptyList.end(e_rr));

	//Fullscreen triangle: every pixel has to hold the pushed color exactly

	Test_assert(t, "beginFlat", commandList.begin(true, e_rr));

	drawPush = c::TestShaderPushData { .color = { 1, 102.f / 255, 51.f / 255, 1 } };

	if (DrawPass pass = TestShaders_openDraw(t, commandList, 1, target.textureRef(), flatPipeline)) {

		Test_assert(t, "draw", pass.render.drawUnindexed(3, 1, &t->err));

		if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList))
			(void) gfxtest::checkPixels(t, dev, emptyList, target.textureRef(), 0xFF3366FFu);
	}

	//Scissor: the draw only lands on the left half, the right half keeps the clear

	Test_assert(t, "beginScissor", commandList.begin(true, e_rr));

	drawPush.color[0] = 0; drawPush.color[1] = 204.f / 255; drawPush.color[2] = 0;

	if (DrawPass pass = TestShaders_openDraw(t, commandList, 1, target.textureRef(), flatPipeline)) {

		Test_assert(t, "scissorHalf", pass.render.setScissor(c::I32x2_zero, c::I32x2_create2(4, 8), e_rr));

		Test_assert(t, "drawScissor", pass.render.drawUnindexed(3, 1, &t->err));

		if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList)) {

			c::TestShaderPixels pixels {};

			if (gfxtest::pullPixels(t, dev, emptyList, target.textureRef(), pixels)) {

				c::U32 matching = 0;

				for(c::U64 i = 0; i < 64; ++i)
					matching += pixels.pixels[i] == ((i & 7) < 4 ? 0xFF00CC00u : 0u);

				Test_assert(t, "scissorPixels", matching == 64);
			}
		}
	}

	//Additive blend: two fullscreen instances of the same color have to sum to exactly twice the bytes

	c::PipelineGraphicsInfo blendInfo = flatInfo;
	blendInfo.blendState = {
		.enable = true,
		.renderTargetMask = 1,
		.writeMask = { c::EWriteMask_All },
		.attachments = { {
			.srcBlend = c::EBlend_One, .dstBlend = c::EBlend_One,
			.srcBlendAlpha = c::EBlend_One, .dstBlendAlpha = c::EBlend_One,
			.blendOp = c::EBlendOp_Add, .blendOpAlpha = c::EBlendOp_Add
		} }
	};

	if (TestShaders_graphicsPipeline(t, dev, &fileList, 0, 1, &blendInfo, blendPipeline)) {

		Test_assert(t, "beginBlend", commandList.begin(true, e_rr));

		drawPush.color[0] = 51.f / 255; drawPush.color[1] = 102.f / 255; drawPush.color[2] = 0; drawPush.color[3] = 51.f / 255;

		if (DrawPass pass = TestShaders_openDraw(t, commandList, 1, target.textureRef(), blendPipeline)) {

			Test_assert(t, "drawBlend", pass.render.drawUnindexed(3, 2, &t->err));

			if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList))
				(void) gfxtest::checkPixels(t, dev, emptyList, target.textureRef(), 0x6600CC66u);
		}
	}

	//Wireframe, when the adapter claims it.
	//The same fullscreen triangle that fills all 64 pixels above covers only its edges in wireframe, so the
	// interior keeps the clear.
	//Counting rather than comparing a fixed picture is deliberate: which pixels an edge touches is the
	// rasterizer's business and differs between implementations, but "some but not all" separates a wireframe
	// that took effect from one that was silently ignored, which would fill all 64 exactly as the flat draw did.

	if(dev.info().capabilities.features & c::EGraphicsFeatures_Wireframe) {

		c::PipelineGraphicsInfo wireInfo = flatInfo;
		wireInfo.rasterizer.flags = (c::U16)(wireInfo.rasterizer.flags | c::ERasterizerFlags_IsWireframeExt);

		if (TestShaders_graphicsPipeline(t, dev, &fileList, 0, 1, &wireInfo, wirePipeline)) {

			Test_assert(t, "beginWire", commandList.begin(true, e_rr));

			drawPush.color[0] = 1; drawPush.color[1] = 1; drawPush.color[2] = 1; drawPush.color[3] = 1;

			if (DrawPass pass = TestShaders_openDraw(t, commandList, 1, target.textureRef(), wirePipeline)) {

				Test_assert(t, "drawWire", pass.render.drawUnindexed(3, 1, &t->err));

				if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList)) {

					c::TestShaderPixels pixels {};

					if (gfxtest::pullPixels(t, dev, emptyList, target.textureRef(), pixels)) {

						c::U32 drawn = 0;

						for(c::U64 i = 0; i < 64; ++i)
							drawn += pixels.pixels[i] == 0xFFFFFFFFu;

						Test_assert(t, "wireDrewSomething", drawn > 0);
						Test_assert(t, "wireLeftInterior", drawn < 64);
					}
				}
			}
		}
	}

	//Multiple render targets: one draw writing two attachments, each getting its own constant.
	//This is the runtime half of the same thing the packaged MRT shader covers at compile time.
	//Nothing else in the suite binds more than one attachment,
	// which is how a backend divergence on the second target's semantic index stayed invisible
	// until packaging refused to merge the two.
	//Both targets are pulled and checked separately, so writing one output to both, or swapping them, fails.

	{
		if(Test_assert(t, "createMrtTarget", dev.createRenderTexture(
			8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None,
			"Shader draw MRT target 1", mrtTarget, c::EMSAASamples_Off, nullptr, e_rr
		))) {

			c::PipelineGraphicsInfo mrtInfo = flatInfo;
			mrtInfo.attachmentCountExt = 2;
			mrtInfo.attachmentFormatsExt[1] = c::ETextureFormatId_RGBA8;

			if (TestShaders_graphicsPipelineNamed(
				t, dev, &fileList, 0, 6, "mainMrt", &mrtInfo, mrtPipeline
			)) {

				Test_assert(t, "beginMrt", commandList.begin(true, e_rr));

				const c::AttachmentInfo mrtColors[2] = {
					{ .image = target.textureRef(),    .load = c::ELoadAttachmentType_Clear },
					{ .image = mrtTarget.textureRef(), .load = c::ELoadAttachmentType_Clear }
				};

				gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
				Test_assert(t, "scopeMrt", (c::Bool) scope);

				gfx::CommandRender render = scope.render(
					c::I32x2_zero, c::I32x2_create2(8, 8), { mrtColors[0], mrtColors[1] }, NULL, e_rr
				);

				Test_assert(t, "renderStartMrt", (c::Bool) render);
				Test_assert(t, "viewportScissorMrt", render.setViewportAndScissor(c::I32x2_zero, c::I32x2_zero, e_rr));
				Test_assert(t, "bindMrt", render.setGraphicsPipeline(mrtPipeline, e_rr));
				Test_assert(t, "pushDraw", render.setPushConstants(drawPush, e_rr));
				Test_assert(t, "drawMrt", render.drawUnindexed(3, 1, e_rr));

				DrawPass pass{
					static_cast<gfx::CommandScope&&>(scope), static_cast<gfx::CommandRender&&>(render), true
				};

				if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList)) {
					(void) gfxtest::checkPixels(t, dev, emptyList, target.textureRef(), 0xFF3366FFu);
					(void) gfxtest::checkPixels(t, dev, emptyList, mrtTarget.textureRef(), 0xFF00CC00u);
				}
			}
		}
	}

	//Logic op, when the adapter claims it: one draw of two instances into a UINT target, the pipeline set to XOR.
	//Instance 0 XORs its value against the zero clear and instance 1 XORs on top,
	// so the readback holds src0 ^ src1 - a value no other op produces from these inputs.
	//Distinct failures land on distinct constants: a dropped logic op (plain overwrite) gives the last value
	// written, OR and AND give their own results, a dead draw leaves the clear.
	//The target is RGBA8u rather than the module's RGBA8, since a logic op is only defined on an integer
	// framebuffer and D3D12 refuses it on UNORM outright.
	//The gate is required: Vulkan reports logicOp false on most mobile GPUs and MoltenVK, and the pipeline
	// create would fail there rather than skip.

	if(!(dev.info().capabilities.features & c::EGraphicsFeatures_LogicOp))
		c::Test_print(t, "Device doesn't support logicOp, skipping logic op test");

	else {

		if(Test_assert(t, "createLogicTarget", dev.createRenderTexture(
			8, 8, c::ETextureFormatId_RGBA8u, c::EGraphicsResourceFlag_None,
			"Shader draw logic op target", logicTarget, c::EMSAASamples_Off, nullptr, e_rr
		))) {

			c::PipelineGraphicsInfo logicInfo = {
				.blendState = {
					.enable = true,                       //Required: D3D12 drops LogicOpEnable otherwise
					.renderTargetMask = 0,                //Required: a logic op excludes blending
					.logicOpExt = c::ELogicOpExt_Xor,
					.writeMask = { c::EWriteMask_All }
				},
				.attachmentFormatsExt = { c::ETextureFormatId_RGBA8u },
				.attachmentCountExt = 1
			};

			if (TestShaders_graphicsPipeline(t, dev, &fileList, 7, 8, &logicInfo, logicPipeline)) {

				Test_assert(t, "beginLogic", commandList.begin(true, e_rr));

				drawPush.logicSrc0[0] = 0xF0; drawPush.logicSrc0[1] = 0x33;
				drawPush.logicSrc0[2] = 0x5A; drawPush.logicSrc0[3] = 0xFF;
				drawPush.logicSrc1[0] = 0x0F; drawPush.logicSrc1[1] = 0x11;
				drawPush.logicSrc1[2] = 0x3C; drawPush.logicSrc1[3] = 0x0F;

				if (DrawPass pass = TestShaders_openDraw(t, commandList, 1, logicTarget.textureRef(), logicPipeline)) {

					Test_assert(t, "drawLogic", pass.render.drawUnindexed(3, 2, &t->err));

					//XOR of the two sources per channel: R 0xF0^0x0F, G 0x33^0x11, B 0x5A^0x3C, A 0xFF^0x0F.
					//R is the low byte of the pulled U32, matching every other expectation in this module.

					if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList))
						(void) gfxtest::checkPixels(t, dev, emptyList, logicTarget.textureRef(), 0xF06622FFu);
				}
			}
		}
	}

	//Dual source blend, when the adapter claims it: the pixel shader emits two colors from one draw and the
	// blend multiplies the first by the second, so the attachment ends up holding the pushed color scaled by
	// exactly a half - a result no single source factor produces from these inputs, so a backend that ignored
	// the second source lands on the unscaled color and fails.
	//The destination factor is Zero, so the cleared attachment contributes nothing.
	//This is also the runtime proof of the dual source reflection path: both outputs sit at LOCATION 0 on
	// SPIR-V, told apart by the Index decoration the DUAL_SRC macros apply.

	if(dev.info().capabilities.features & c::EGraphicsFeatures_DualSrcBlend) {

		c::PipelineGraphicsInfo dualInfo = flatInfo;
		dualInfo.blendState = {
			.enable = true,
			.renderTargetMask = 1,
			.writeMask = { c::EWriteMask_All },
			.attachments = { {
				.srcBlend = c::EBlend_Src1ColorExt, .dstBlend = c::EBlend_Zero,
				.srcBlendAlpha = c::EBlend_Src1AlphaExt, .dstBlendAlpha = c::EBlend_Zero,
				.blendOp = c::EBlendOp_Add, .blendOpAlpha = c::EBlendOp_Add
			} }
		};

		if (TestShaders_graphicsPipelineNamed(
			t, dev, &fileList, 0, 9, "mainDualSrc", &dualInfo, dualPipeline
		)) {

			Test_assert(t, "beginDual", commandList.begin(true, e_rr));

			//All four channels at 0.8, so the halved result is 0.4, which lands on 102 in 8 bit unorm with no
			// rounding ambiguity either before or after the multiply.

			drawPush.color[0] = 204.f / 255; drawPush.color[1] = 204.f / 255;
			drawPush.color[2] = 204.f / 255; drawPush.color[3] = 204.f / 255;

			if (DrawPass pass = TestShaders_openDraw(t, commandList, 1, target.textureRef(), dualPipeline)) {

				Test_assert(t, "drawDual", pass.render.drawUnindexed(3, 1, &t->err));

				if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList))
					(void) gfxtest::checkPixels(t, dev, emptyList, target.textureRef(), 0x66666666u);
			}
		}
	}

	//Indexed and instanced draw through real buffers: instance 0 covers the left half, instance 1 the right,
	// so full coverage proves the index buffer, the vertex fetch and both instances all worked.
	//This is also the only path that replays setPrimitiveBuffers, which used to record 8 bytes of pointer
	// instead of the command payload.

	const c::F32 quad[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
	const c::U16 quadIndices[6] = { 0, 1, 2, 2, 1, 3 };

	c::Buffer dataRef = c::Buffer_createRefConst(quad, sizeof(quad));
	Test_assert(t, "createVertexBuffer", dev.createBufferData(
		c::EDeviceBufferUsage_Vertex, c::EGraphicsResourceFlag_None,
		"Shader draw vertices", &dataRef, vertexBuffer, nullptr, e_rr
	));

	dataRef = c::Buffer_createRefConst(quadIndices, sizeof(quadIndices));
	Test_assert(t, "createIndexBuffer", dev.createBufferData(
		c::EDeviceBufferUsage_Index, c::EGraphicsResourceFlag_None,
		"Shader draw indices", &dataRef, indexBuffer, nullptr, e_rr
	));

	c::PipelineGraphicsInfo vertexInfo = flatInfo;
	vertexInfo.vertexLayout.bufferStrides12_isInstance1[0] = sizeof(c::F32) * 2;
	vertexInfo.vertexLayout.attributes[0] = { .format = c::ETextureFormatId_RG32f };

	if (
		vertexBuffer && indexBuffer &&
		TestShaders_graphicsPipeline(t, dev, &fileList, 4, 1, &vertexInfo, vertexPipeline)
	) {

		Test_assert(t, "beginVertex", commandList.begin(true, e_rr));

		drawPush.color[0] = 204.f / 255; drawPush.color[1] = 0; drawPush.color[2] = 204.f / 255; drawPush.color[3] = 1;

		if (DrawPass pass = TestShaders_openDraw(t, commandList, 1, target.textureRef(), vertexPipeline)) {

			c::SetPrimitiveBuffersCmd primitives {};
			primitives.vertexBuffers[0] = vertexBuffer.handle();
			primitives.indexBuffer = indexBuffer.handle();
			primitives.isIndex32Bit = false;

			Test_assert(t, "setPrimitiveBuffers", pass.render.setPrimitiveBuffers(primitives, e_rr));
			Test_assert(t, "drawIndexed", pass.render.drawIndexed(6, 2, &t->err));

			if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList))
				(void) gfxtest::checkPixels(t, dev, emptyList, target.textureRef(), 0xFFCC00CCu);
		}
	}

	//Depth story in one draw: a far triangle writes, a nearer one passes, the farthest one after it must be
	// rejected, so the survivor's color and its exact depth prove both accept and reject paths

	Test_assert(t, "createDepth", dev.createDepthStencil(
		8, 8, c::EDepthStencilFormat_D32, false, "Shader draw depth", depth, c::EMSAASamples_Off, e_rr
	));

	//Reverse Z is an app convention (fold 1 - z into the projection); the viewport is a plain 0..1 range
	// on every backend, so the test shader outputs reversed z directly: near stores the higher value,
	// the far clear is 0 and the compare is Greater

	c::PipelineGraphicsInfo depthInfo = flatInfo;
	depthInfo.depthStencil = { .flags = c::EDepthStencilFlags_DepthWrite, .depthCompare = c::ECompareOp_Gt };
	depthInfo.depthFormatExt = c::EDepthStencilFormat_D32;

	if (
		depth &&
		TestShaders_graphicsPipeline(t, dev, &fileList, 2, 3, &depthInfo, depthPipeline)
	) {

		Test_assert(t, "beginDepth", commandList.begin(true, e_rr));

		const c::AttachmentInfo color = { .image = target.textureRef(), .load = c::ELoadAttachmentType_Clear };

		const c::DepthStencilAttachmentInfo depthAttach = {
			.image = depth.textureRef(),
			.depthLoad = c::ELoadAttachmentType_Clear,
			.clearDepth = 0
		};

		gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scopeDepth", (c::Bool) scope);

		gfx::CommandRender render = scope.render(
			c::I32x2_zero, c::I32x2_create2(8, 8), { color }, &depthAttach, e_rr
		);

		Test_assert(t, "renderStartDepth", (c::Bool) render);
		Test_assert(t, "viewportScissorDepth", render.setViewportAndScissor(c::I32x2_zero, c::I32x2_zero, e_rr));
		Test_assert(t, "bindDepth", render.setGraphicsPipeline(depthPipeline, e_rr));
		Test_assert(t, "pushDraw", render.setPushConstants(drawPush, e_rr));
		Test_assert(t, "drawDepth", render.drawUnindexed(9, 1, e_rr));

		DrawPass pass{
			static_cast<gfx::CommandScope&&>(scope), static_cast<gfx::CommandRender&&>(render), true
		};

		if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList)) {

			//The middle triangle's green and its 0.7 depth, straight from the shader with no viewport
			// remap in between; a small tolerance stays anyway for the wider GPU test rig

			(void) gfxtest::checkPixels(t, dev, emptyList, target.textureRef(), 0xFF00FF00u);

			c::TestShaderPixels depthPixels {};

			if (gfxtest::pullPixels(t, dev, emptyList, depth.textureRef(), depthPixels)) {

				c::U32 matching = 0;

				for(c::U64 i = 0; i < 64; ++i) {

					c::F32 depthValue = 0;
					c::Buffer_memcpy(
						c::Buffer_createRef(&depthValue, sizeof(depthValue)),
						c::Buffer_createRefConst(&depthPixels.pixels[i], sizeof(c::U32))
					);

					const c::F32 delta = depthValue - 0.7f;
					matching += delta > -1e-6f && delta < 1e-6f;
				}

				Test_assert(t, "depthValues", matching == 64);
			}
		}
	}

	//Indirect draw from CPU written arguments

	const c::U32 drawArgs[4] = { 3, 1, 0, 0 };
	dataRef = c::Buffer_createRefConst(drawArgs, sizeof(drawArgs));
	Test_assert(t, "createDrawArgs", dev.createBufferData(
		c::EDeviceBufferUsage_Indirect, c::EGraphicsResourceFlag_None,
		"Shader draw indirect args", &dataRef, cpuArgs, nullptr, e_rr
	));

	if (cpuArgs) {

		Test_assert(t, "beginIndirect", commandList.begin(true, e_rr));

		drawPush.color[0] = 1; drawPush.color[1] = 1; drawPush.color[2] = 0; drawPush.color[3] = 1;

		if (DrawPass pass = TestShaders_openDraw(t, commandList, 1, target.textureRef(), flatPipeline)) {

			Test_assert(t, "drawIndirect", pass.render.drawIndirect(cpuArgs, 0, 1, false, &t->err));

			if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList))
				(void) gfxtest::checkPixels(t, dev, emptyList, target.textureRef(), 0xFF00FFFFu);
		}
	}

	//Indirect draw from GPU written arguments: a compute scope writes { 3 vertices, 2 instances } and the
	// render scope consumes it in the same submit

	Test_assert(t, "createGpuDrawArgs", dev.createBuffer(
		c::EDeviceBufferUsage_Indirect, c::EGraphicsResourceFlag_ShaderWriteBindless,
		"Shader draw GPU args", 32, gpuArgs, nullptr, e_rr
	));

	if (gpuArgs && gfxtest::computePipelinePush(t, dev, files[5], argsPipeline, argsPushLayout)) {

		const c::Transition argsWrite = {
			.resource = gpuArgs.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
		};

		Test_assert(t, "beginGpuDraw", commandList.begin(true, e_rr));

		//test_write_args declares its own 16 byte block rather than the pixel one, so this pushes that shape
		//instead of drawPush; the work op requires the written size to match what the layout declares.

		const c::U32 argsPushData[4] = { 0, 0, gpuArgs.writeHandle(), 0 };

		{
			gfx::CommandScope scope = commandList.scope({ argsWrite }, 1, {}, e_rr);
			Test_assert(t, "scopeGpuArgs", (c::Bool) scope);
			Test_assert(t, "bindGpuArgs", scope.setComputePipeline(argsPipeline, e_rr));
			Test_assert(t, "pushGpuArgs", scope.setPushConstants(argsPushData, e_rr));
			Test_assert(t, "dispatchGpuArgs", scope.dispatch1D(1, e_rr));
			Test_assert(t, "scopeGpuArgsEnd", scope.end(e_rr));
		}

		drawPush.handles[2] = gpuArgs.writeHandle();
		drawPush.color[0] = 51.f / 255; drawPush.color[1] = 51.f / 255; drawPush.color[2] = 1; drawPush.color[3] = 1;

		if (DrawPass pass = TestShaders_openDraw(t, commandList, 2, target.textureRef(), flatPipeline)) {

			Test_assert(t, "drawGpuIndirect", pass.render.drawIndirect(gpuArgs, 0, 1, false, &t->err));

			if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList))
				(void) gfxtest::checkPixels(t, dev, emptyList, target.textureRef(), 0xFFFF3333u);
		}
	}

	//MSAA: a fully covered pixel resolves to exactly the flat color whatever the sample count, and the result
	// lands in the readback target through the resolve attachment rather than a copy.
	//4x is required of every adapter; 2x and 8x are optional, so those are skipped unless the adapter claims
	// them, and running when it does is what turns the claim into something checked rather than reported.
	//Target and pipeline are rebuilt per count, since both bake the sample count in.

	static const struct {
		c::EMSAASamples samples;
		c::EGraphicsDataTypes dataType;            //0 when the count is required rather than optional
	} msaaCases[] = {
		{ c::EMSAASamples_x2Ext, c::EGraphicsDataTypes_MSAA2x },
		{ c::EMSAASamples_x4,    (c::EGraphicsDataTypes) 0    },
		{ c::EMSAASamples_x8Ext, c::EGraphicsDataTypes_MSAA8x }
	};

	const c::EGraphicsDataTypes msaaTypes = dev.info().capabilities.dataTypes;
	c::U32 msaaRun = 0, msaaSkipped = 0;

	for (c::U64 m = 0; m < sizeof(msaaCases) / sizeof(msaaCases[0]); ++m) {

		if (msaaCases[m].dataType && !(msaaTypes & msaaCases[m].dataType)) {
			++msaaSkipped;
			continue;
		}

		if(!Test_assert(t, "createMsaaTarget", dev.createRenderTexture(
			8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None,
			"Shader draw MSAA target", msaaTarget, msaaCases[m].samples, nullptr, e_rr
		)))
			continue;

		c::PipelineGraphicsInfo msaaInfo = flatInfo;
		msaaInfo.msaa = (c::U8) msaaCases[m].samples;

		if (TestShaders_graphicsPipeline(t, dev, &fileList, 0, 1, &msaaInfo, msaaPipeline)) {

			Test_assert(t, "beginMsaa", commandList.begin(true, e_rr));

			const c::AttachmentInfo msaaColor = {
				.image = msaaTarget.textureRef(),
				.load = c::ELoadAttachmentType_Clear,
				.resolveMode = c::EMSAAResolveMode_Average,
				.resolveImage = target.textureRef()
			};

			drawPush.color[0] = 102.f / 255; drawPush.color[1] = 1; drawPush.color[2] = 51.f / 255; drawPush.color[3] = 1;

			gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
			Test_assert(t, "scopeMsaa", (c::Bool) scope);

			gfx::CommandRender render = scope.render(
				c::I32x2_zero, c::I32x2_create2(8, 8), { msaaColor }, NULL, e_rr
			);

			Test_assert(t, "renderStartMsaa", (c::Bool) render);
			Test_assert(t, "viewportScissorMsaa", render.setViewportAndScissor(c::I32x2_zero, c::I32x2_zero, e_rr));
			Test_assert(t, "bindMsaa", render.setGraphicsPipeline(msaaPipeline, e_rr));
			Test_assert(t, "pushDraw", render.setPushConstants(drawPush, e_rr));
			Test_assert(t, "drawMsaa", render.drawUnindexed(3, 1, e_rr));

			DrawPass pass{
				static_cast<gfx::CommandScope&&>(scope), static_cast<gfx::CommandRender&&>(render), true
			};

			if(TestShaders_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList)) {
				(void) gfxtest::checkPixels(t, dev, emptyList, target.textureRef(), 0xFF33FF66u);
				++msaaRun;
			}
		}

		//Both are rebuilt next iteration, so this iteration's pair goes back first.

		msaaPipeline.release();
		msaaTarget.release();
	}

	Log::debugLn(
		*alloc,
		"-- draw: %" PRIu32 " MSAA sample counts resolved, %" PRIu32 " not claimed by this adapter",
		msaaRun, msaaSkipped
	);

	//These are module statics so every pipeline here can share one layout, which means they outlive the
	//scope that created them AND would otherwise outlive the device, releasing into one that is already gone.

	argsPushLayout.release();
	drawPushLayout.release();
}
