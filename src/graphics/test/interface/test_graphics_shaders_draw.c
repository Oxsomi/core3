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

//graphics/test/interface/test_graphics_shaders_draw.c
//
//Draw execution, and the pipeline/render helpers only these draws use.
//Split out of test_graphics_shaders.c, which had grown past 2300 lines.

#include "graphics/generic/instance.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/file.h"
#include "formats/oiSH/sh_file.h"
#include "types/test/test.h"
#include "types/container/memory_stream.h"
#include "types/container/texture_format.h"
#include "types/container/buffer.h"
#include "types/base/string_base.h"
#include "test_graphics_shared.h"

//The vertex and pixel shader are separate single entry files, picked out of one shared file list by slot

//The pixel entry is named because SHFile_combine matches entries by name, so two pixel shaders in one
//package can't both be "main" unless they agree on their signature - which a 1 target and a 2 target one
//don't.

static Bool TestShaders_graphicsPipelineNamed(
	Test *t, GraphicsDeviceRef *deviceRef, const ListSHFile *files, U16 vertexFile, U16 pixelFile,
	const C8 *pixelEntry, const PipelineGraphicsInfo *info, PipelineRef **pipeline
) {

	const U32 vertexId = TestShaders_entry(t, deviceRef, &files->ptr[vertexFile], "main");
	const U32 pixelId = TestShaders_entry(t, deviceRef, &files->ptr[pixelFile], pixelEntry);

	if(vertexId == U32_MAX || pixelId == U32_MAX)
		return false;

	PipelineStage stages[2] = {
		(PipelineStage) { .binaryId = vertexId, .shFileId = vertexFile },
		(PipelineStage) { .binaryId = pixelId, .shFileId = pixelFile }
	};

	ListPipelineStage stageList = (ListPipelineStage) { 0 };
	ListPipelineStage_createRefConst(stages, 2, &stageList, NULL);

	const CharString name = CharString_createRefCStrConst("Shader test graphics pipeline");

	return Test_assert(t, "createGraphicsPipeline", GraphicsDeviceRef_createPipelineGraphics(
		deviceRef, files, &stageList, info, &name, EPipelineFlags_None, NULL, pipeline, &t->err
	));
}

static Bool TestShaders_graphicsPipeline(
	Test *t, GraphicsDeviceRef *deviceRef, const ListSHFile *files, U16 vertexFile, U16 pixelFile,
	const PipelineGraphicsInfo *info, PipelineRef **pipeline
) {
	return TestShaders_graphicsPipelineNamed(t, deviceRef, files, vertexFile, pixelFile, "main", info, pipeline);
}

//Opens a scope, starts a cleared render into the 8x8 target and binds the pipeline with full viewport and scissor.
//The caller already began the command list, so a compute scope can precede the render scope when a phase needs one.

static Bool TestShaders_openDraw(
	Test *t, CommandListRef *commandList, U32 scopeId, RefPtr *target, PipelineRef *pipeline
) {

	Bool ok = Test_assert(t, "scope", CommandListRef_startScope(commandList, NULL, scopeId, NULL, &t->err));

	const AttachmentInfo color = (AttachmentInfo) { .image = target, .load = ELoadAttachmentType_Clear };
	ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
	ListAttachmentInfo_createRefConst(&color, 1, &colors, NULL);

	ok &= Test_assert(t, "renderStart", CommandListRef_startRenderExt(
		commandList, I32x2_zero, I32x2_create2(8, 8), &colors, NULL, &t->err
	));

	ok &= Test_assert(t, "viewportScissor", CommandListRef_setViewportAndScissor(
		commandList, I32x2_zero, I32x2_zero, &t->err
	));

	return Test_assert(t, "bindPipeline", CommandListRef_setGraphicsPipeline(commandList, pipeline, &t->err)) && ok;
}

static Bool TestShaders_closeDraw(Test *t, CommandListRef *commandList) {
	Bool ok = Test_assert(t, "renderEnd", CommandListRef_endRenderExt(commandList, &t->err));
	ok &= Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	return Test_assert(t, "end", CommandListRef_end(commandList, &t->err)) && ok;
}

// -- 32. Draw execution ----------------------------------------------------------

//Every draw renders into an 8x8 target and the resolved pixels are byte compared, colors picked to be exact
// in 8 bit UNORM so rounding can't blur a pass into a flake.
//Covered here: a fullscreen triangle, scissor clipping, additive blending, an indexed and instanced draw
// through real vertex and index buffers, depth test accept and reject, indirect draws from CPU and GPU
// written arguments and an MSAA 4x draw that resolves into the readback target.

void Test_graphicsShaderDraw(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Shaders/draw");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping draw execution tests");
		return;
	}

	if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {
		Test_print(t, "Device lacks direct rendering, skipping draw execution tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	//One shared file list all graphics pipelines pick their stages from by slot

	static const C8 *drawShaderPaths[] = {
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

	SHFile files[10] = { 0 };
	Bool loadedAll = true;

	for(U64 i = 0; i < 10; ++i)
		loadedAll &= TestShaders_loadFile(t, drawShaderPaths[i], &files[i]);

	if (!loadedAll) {

		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping draw execution tests");

		for(U64 i = 0; i < 10; ++i)
			SHFile_free(&files[i], alloc);

		return;
	}

	ListSHFile fileList = (ListSHFile) { 0 };
	ListSHFile_createRefConst(files, 10, &fileList, NULL);

	RenderTextureRef *target = NULL;
	RenderTextureRef *msaaTarget = NULL;
	DepthStencilRef *depth = NULL;
	DeviceBufferRef *vertexBuffer = NULL;
	DeviceBufferRef *indexBuffer = NULL;
	DeviceBufferRef *cpuArgs = NULL;
	DeviceBufferRef *gpuArgs = NULL;
	PipelineRef *flatPipeline = NULL;
	PipelineRef *blendPipeline = NULL;
	PipelineRef *wirePipeline = NULL;
	PipelineRef *mrtPipeline = NULL;
	RenderTextureRef *mrtTarget = NULL;
	PipelineRef *logicPipeline = NULL;
	RenderTextureRef *logicTarget = NULL;
	PipelineRef *dualPipeline = NULL;
	PipelineRef *vertexPipeline = NULL;
	PipelineRef *depthPipeline = NULL;
	PipelineRef *msaaPipeline = NULL;
	PipelineRef *argsPipeline = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	CharString name = CharString_createRefCStrConst("Shader draw target");

	Test_assert(t, "createTarget", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &name, &target, &t->err
	));

	const PipelineGraphicsInfo flatInfo = (PipelineGraphicsInfo) {
		.attachmentCountExt = 1,
		.attachmentFormatsExt = { ETextureFormatId_RGBA8 }
	};

	if(!target || !TestShaders_graphicsPipeline(t, deviceRef, &fileList, 0, 1, &flatInfo, &flatPipeline))
		goto clean;

	if(!Test_assert(t, "createList", GraphicsDeviceRef_createCommandList(
		deviceRef, 8 * KIBI, 128, 32, true, &commandList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "createEmptyList", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmptyList", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmptyList", CommandListRef_end(emptyList, &t->err));

	//Fullscreen triangle: every pixel has to hold the app data color exactly

	Test_assert(t, "beginFlat", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	TestShaderAppData appData = (TestShaderAppData) { .color = { 1, 102.f / 255, 51.f / 255, 1 } };

	if (TestShaders_openDraw(t, commandList, 1, target, flatPipeline)) {

		Test_assert(t, "draw", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

		if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
			t, deviceRef, commandList, &appData, sizeof(appData)
		))
			TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF3366FFu);
	}

	//Scissor: the draw only lands on the left half, the right half keeps the clear

	Test_assert(t, "beginScissor", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	appData.color[0] = 0; appData.color[1] = 204.f / 255; appData.color[2] = 0;

	if (TestShaders_openDraw(t, commandList, 1, target, flatPipeline)) {

		Test_assert(t, "scissorHalf", CommandListRef_setScissor(
			commandList, I32x2_zero, I32x2_create2(4, 8), &t->err
		));

		Test_assert(t, "drawScissor", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

		if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
			t, deviceRef, commandList, &appData, sizeof(appData)
		)) {

			TestShaderPixels pixels = (TestShaderPixels) { 0 };

			if (TestShaders_pullPixels(t, deviceRef, emptyList, target, &pixels)) {

				U32 matching = 0;

				for(U64 i = 0; i < 64; ++i)
					matching += pixels.pixels[i] == ((i & 7) < 4 ? 0xFF00CC00u : 0u);

				Test_assert(t, "scissorPixels", matching == 64);
			}
		}
	}

	//Additive blend: two fullscreen instances of the same color have to sum to exactly twice the bytes

	PipelineGraphicsInfo blendInfo = flatInfo;
	blendInfo.blendState = (BlendState) {
		.enable = true,
		.renderTargetMask = 1,
		.writeMask = { EWriteMask_All },
		.attachments = { (BlendStateAttachment) {
			.srcBlend = EBlend_One, .dstBlend = EBlend_One,
			.srcBlendAlpha = EBlend_One, .dstBlendAlpha = EBlend_One,
			.blendOp = EBlendOp_Add, .blendOpAlpha = EBlendOp_Add
		} }
	};

	if (TestShaders_graphicsPipeline(t, deviceRef, &fileList, 0, 1, &blendInfo, &blendPipeline)) {

		Test_assert(t, "beginBlend", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		appData.color[0] = 51.f / 255; appData.color[1] = 102.f / 255; appData.color[2] = 0; appData.color[3] = 51.f / 255;

		if (TestShaders_openDraw(t, commandList, 1, target, blendPipeline)) {

			Test_assert(t, "drawBlend", CommandListRef_drawUnindexed(commandList, 3, 2, &t->err));

			if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
				t, deviceRef, commandList, &appData, sizeof(appData)
			))
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0x6600CC66u);
		}
	}

	//Wireframe, when the adapter claims it.
	//The same fullscreen triangle that fills all 64 pixels above covers only its edges in wireframe, so the
	// interior keeps the clear.
	//Counting rather than comparing a fixed picture is deliberate: which pixels an edge touches is the
	// rasterizer's business and differs between implementations, but "some but not all" separates a wireframe
	// that took effect from one that was silently ignored, which would fill all 64 exactly as the flat draw did.

	if(GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features & EGraphicsFeatures_Wireframe) {

		PipelineGraphicsInfo wireInfo = flatInfo;
		wireInfo.rasterizer.flags = (U16)(wireInfo.rasterizer.flags | ERasterizerFlags_IsWireframeExt);

		if (TestShaders_graphicsPipeline(t, deviceRef, &fileList, 0, 1, &wireInfo, &wirePipeline)) {

			Test_assert(t, "beginWire", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

			appData.color[0] = 1; appData.color[1] = 1; appData.color[2] = 1; appData.color[3] = 1;

			if (TestShaders_openDraw(t, commandList, 1, target, wirePipeline)) {

				Test_assert(t, "drawWire", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

				if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
					t, deviceRef, commandList, &appData, sizeof(appData)
				)) {

					TestShaderPixels pixels = (TestShaderPixels) { 0 };

					if (TestShaders_pullPixels(t, deviceRef, emptyList, target, &pixels)) {

						U32 drawn = 0;

						for(U64 i = 0; i < 64; ++i)
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
		name = CharString_createRefCStrConst("Shader draw MRT target 1");

		if(Test_assert(t, "createMrtTarget", GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
			EMSAASamples_Off, NULL, &name, &mrtTarget, &t->err
		))) {

			PipelineGraphicsInfo mrtInfo = flatInfo;
			mrtInfo.attachmentCountExt = 2;
			mrtInfo.attachmentFormatsExt[1] = ETextureFormatId_RGBA8;

			if (TestShaders_graphicsPipelineNamed(
				t, deviceRef, &fileList, 0, 6, "mainMrt", &mrtInfo, &mrtPipeline
			)) {

				Test_assert(t, "beginMrt", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
				Test_assert(t, "scopeMrt", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

				const AttachmentInfo mrtColors[2] = {
					(AttachmentInfo) { .image = target,    .load = ELoadAttachmentType_Clear },
					(AttachmentInfo) { .image = mrtTarget, .load = ELoadAttachmentType_Clear }
				};

				ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
				ListAttachmentInfo_createRefConst(mrtColors, 2, &colors, NULL);

				Test_assert(t, "renderStartMrt", CommandListRef_startRenderExt(
					commandList, I32x2_zero, I32x2_create2(8, 8), &colors, NULL, &t->err
				));

				Test_assert(t, "viewportScissorMrt", CommandListRef_setViewportAndScissor(
					commandList, I32x2_zero, I32x2_zero, &t->err
				));

				Test_assert(t, "bindMrt", CommandListRef_setGraphicsPipeline(commandList, mrtPipeline, &t->err));
				Test_assert(t, "drawMrt", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

				if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
					t, deviceRef, commandList, &appData, sizeof(appData)
				)) {
					TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF3366FFu);
					TestShaders_checkPixels(t, deviceRef, emptyList, mrtTarget, 0xFF00CC00u);
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

	if(!(GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features & EGraphicsFeatures_LogicOp))
		Test_print(t, "Device doesn't support logicOp, skipping logic op test");

	else {

		name = CharString_createRefCStrConst("Shader draw logic op target");

		if(Test_assert(t, "createLogicTarget", GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8u, EGraphicsResourceFlag_None,
			EMSAASamples_Off, NULL, &name, &logicTarget, &t->err
		))) {

			PipelineGraphicsInfo logicInfo = (PipelineGraphicsInfo) {
				.attachmentCountExt = 1,
				.attachmentFormatsExt = { ETextureFormatId_RGBA8u },
				.blendState = (BlendState) {
					.enable = true,                       //Required: D3D12 drops LogicOpEnable otherwise
					.renderTargetMask = 0,                //Required: a logic op excludes blending
					.logicOpExt = ELogicOpExt_Xor,
					.writeMask = { EWriteMask_All }
				}
			};

			if (TestShaders_graphicsPipeline(t, deviceRef, &fileList, 7, 8, &logicInfo, &logicPipeline)) {

				Test_assert(t, "beginLogic", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

				appData.logicSrc0[0] = 0xF0; appData.logicSrc0[1] = 0x33;
				appData.logicSrc0[2] = 0x5A; appData.logicSrc0[3] = 0xFF;
				appData.logicSrc1[0] = 0x0F; appData.logicSrc1[1] = 0x11;
				appData.logicSrc1[2] = 0x3C; appData.logicSrc1[3] = 0x0F;

				if (TestShaders_openDraw(t, commandList, 1, logicTarget, logicPipeline)) {

					Test_assert(t, "drawLogic", CommandListRef_drawUnindexed(commandList, 3, 2, &t->err));

					//XOR of the two sources per channel: R 0xF0^0x0F, G 0x33^0x11, B 0x5A^0x3C, A 0xFF^0x0F.
					//R is the low byte of the pulled U32, matching every other expectation in this module.

					if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
						t, deviceRef, commandList, &appData, sizeof(appData)
					))
						TestShaders_checkPixels(t, deviceRef, emptyList, logicTarget, 0xF06622FFu);
				}
			}
		}
	}

	//Dual source blend, when the adapter claims it: the pixel shader emits two colors from one draw and the
	// blend multiplies the first by the second, so the attachment ends up holding the app data color scaled by
	// exactly a half - a result no single source factor produces from these inputs, so a backend that ignored
	// the second source lands on the unscaled color and fails.
	//The destination factor is Zero, so the cleared attachment contributes nothing.
	//This is also the runtime proof of the dual source reflection path: both outputs sit at LOCATION 0 on
	// SPIR-V, told apart by the Index decoration the DUAL_SRC macros apply.

	if(GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features & EGraphicsFeatures_DualSrcBlend) {

		PipelineGraphicsInfo dualInfo = flatInfo;
		dualInfo.blendState = (BlendState) {
			.enable = true,
			.renderTargetMask = 1,
			.writeMask = { EWriteMask_All },
			.attachments = { (BlendStateAttachment) {
				.srcBlend = EBlend_Src1ColorExt, .dstBlend = EBlend_Zero,
				.srcBlendAlpha = EBlend_Src1AlphaExt, .dstBlendAlpha = EBlend_Zero,
				.blendOp = EBlendOp_Add, .blendOpAlpha = EBlendOp_Add
			} }
		};

		if (TestShaders_graphicsPipelineNamed(
			t, deviceRef, &fileList, 0, 9, "mainDualSrc", &dualInfo, &dualPipeline
		)) {

			Test_assert(t, "beginDual", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

			//All four channels at 0.8, so the halved result is 0.4, which lands on 102 in 8 bit unorm with no
			// rounding ambiguity either before or after the multiply.

			appData.color[0] = 204.f / 255; appData.color[1] = 204.f / 255;
			appData.color[2] = 204.f / 255; appData.color[3] = 204.f / 255;

			if (TestShaders_openDraw(t, commandList, 1, target, dualPipeline)) {

				Test_assert(t, "drawDual", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

				if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
					t, deviceRef, commandList, &appData, sizeof(appData)
				))
					TestShaders_checkPixels(t, deviceRef, emptyList, target, 0x66666666u);
			}
		}
	}

	//Indexed and instanced draw through real buffers: instance 0 covers the left half, instance 1 the right,
	// so full coverage proves the index buffer, the vertex fetch and both instances all worked.
	//This is also the only path that replays setPrimitiveBuffers, which used to record 8 bytes of pointer
	// instead of the command payload.

	const F32 quad[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
	const U16 quadIndices[6] = { 0, 1, 2, 2, 1, 3 };

	Buffer dataRef = Buffer_createRefConst(quad, sizeof(quad));
	name = CharString_createRefCStrConst("Shader draw vertices");

	Test_assert(t, "createVertexBuffer", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Vertex, EGraphicsResourceFlag_None, NULL,
		&name, &dataRef, &vertexBuffer, &t->err
	));

	dataRef = Buffer_createRefConst(quadIndices, sizeof(quadIndices));
	name = CharString_createRefCStrConst("Shader draw indices");

	Test_assert(t, "createIndexBuffer", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Index, EGraphicsResourceFlag_None, NULL,
		&name, &dataRef, &indexBuffer, &t->err
	));

	PipelineGraphicsInfo vertexInfo = flatInfo;
	vertexInfo.vertexLayout.bufferStrides12_isInstance1[0] = sizeof(F32) * 2;
	vertexInfo.vertexLayout.attributes[0] = (VertexAttribute) { .format = ETextureFormatId_RG32f };

	if (
		vertexBuffer && indexBuffer &&
		TestShaders_graphicsPipeline(t, deviceRef, &fileList, 4, 1, &vertexInfo, &vertexPipeline)
	) {

		Test_assert(t, "beginVertex", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		appData.color[0] = 204.f / 255; appData.color[1] = 0; appData.color[2] = 204.f / 255; appData.color[3] = 1;

		if (TestShaders_openDraw(t, commandList, 1, target, vertexPipeline)) {

			SetPrimitiveBuffersCmd primitives = (SetPrimitiveBuffersCmd) { 0 };
			primitives.vertexBuffers[0] = vertexBuffer;
			primitives.indexBuffer = indexBuffer;
			primitives.isIndex32Bit = false;

			Test_assert(t, "setPrimitiveBuffers", CommandListRef_setPrimitiveBuffers(commandList, &primitives, &t->err));
			Test_assert(t, "drawIndexed", CommandListRef_drawIndexed(commandList, 6, 2, &t->err));

			if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
				t, deviceRef, commandList, &appData, sizeof(appData)
			))
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFFCC00CCu);
		}
	}

	//Depth story in one draw: a far triangle writes, a nearer one passes, the farthest one after it must be
	// rejected, so the survivor's color and its exact depth prove both accept and reject paths

	name = CharString_createRefCStrConst("Shader draw depth");

	Test_assert(t, "createDepth", GraphicsDeviceRef_createDepthStencil(
		deviceRef, 8, 8, EDepthStencilFormat_D32, false, EMSAASamples_Off, NULL, &name, &depth, &t->err
	));

	//Reverse Z is an app convention (fold 1 - z into the projection); the viewport is a plain 0..1 range
	// on every backend, so the test shader outputs reversed z directly: near stores the higher value,
	// the far clear is 0 and the compare is Greater

	PipelineGraphicsInfo depthInfo = flatInfo;
	depthInfo.depthStencil = (DepthStencilState) { .flags = EDepthStencilFlags_DepthWrite, .depthCompare = ECompareOp_Gt };
	depthInfo.depthFormatExt = EDepthStencilFormat_D32;

	if (
		depth &&
		TestShaders_graphicsPipeline(t, deviceRef, &fileList, 2, 3, &depthInfo, &depthPipeline)
	) {

		Test_assert(t, "beginDepth", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
		Test_assert(t, "scopeDepth", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

		const AttachmentInfo color = (AttachmentInfo) { .image = target, .load = ELoadAttachmentType_Clear };
		ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
		ListAttachmentInfo_createRefConst(&color, 1, &colors, NULL);

		const DepthStencilAttachmentInfo depthAttach = (DepthStencilAttachmentInfo) {
			.image = depth,
			.depthLoad = ELoadAttachmentType_Clear,
			.clearDepth = 0
		};

		Test_assert(t, "renderStartDepth", CommandListRef_startRenderExt(
			commandList, I32x2_zero, I32x2_create2(8, 8), &colors, &depthAttach, &t->err
		));

		Test_assert(t, "viewportScissorDepth", CommandListRef_setViewportAndScissor(
			commandList, I32x2_zero, I32x2_zero, &t->err
		));

		Test_assert(t, "bindDepth", CommandListRef_setGraphicsPipeline(commandList, depthPipeline, &t->err));
		Test_assert(t, "drawDepth", CommandListRef_drawUnindexed(commandList, 9, 1, &t->err));

		if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0)) {

			//The middle triangle's green and its 0.7 depth, straight from the shader with no viewport
			// remap in between; a small tolerance stays anyway for the wider GPU test rig

			TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF00FF00u);

			TestShaderPixels depthPixels = (TestShaderPixels) { 0 };

			if (TestShaders_pullPixels(t, deviceRef, emptyList, depth, &depthPixels)) {

				U32 matching = 0;

				for(U64 i = 0; i < 64; ++i) {

					F32 depthValue = 0;
					Buffer_memcpy(
						Buffer_createRef(&depthValue, sizeof(depthValue)),
						Buffer_createRefConst(&depthPixels.pixels[i], sizeof(U32))
					);

					const F32 delta = depthValue - 0.7f;
					matching += delta > -1e-6f && delta < 1e-6f;
				}

				Test_assert(t, "depthValues", matching == 64);
			}
		}
	}

	//Indirect draw from CPU written arguments

	const U32 drawArgs[4] = { 3, 1, 0, 0 };
	dataRef = Buffer_createRefConst(drawArgs, sizeof(drawArgs));
	name = CharString_createRefCStrConst("Shader draw indirect args");

	Test_assert(t, "createDrawArgs", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Indirect, EGraphicsResourceFlag_None, NULL,
		&name, &dataRef, &cpuArgs, &t->err
	));

	if (cpuArgs) {

		Test_assert(t, "beginIndirect", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		appData.color[0] = 1; appData.color[1] = 1; appData.color[2] = 0; appData.color[3] = 1;

		if (TestShaders_openDraw(t, commandList, 1, target, flatPipeline)) {

			Test_assert(t, "drawIndirect", CommandListRef_drawIndirect(commandList, cpuArgs, 0, 1, false, &t->err));

			if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
				t, deviceRef, commandList, &appData, sizeof(appData)
			))
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF00FFFFu);
		}
	}

	//Indirect draw from GPU written arguments: a compute scope writes { 3 vertices, 2 instances } and the
	// render scope consumes it in the same submit

	name = CharString_createRefCStrConst("Shader draw GPU args");

	Test_assert(t, "createGpuDrawArgs", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_Indirect,
		EGraphicsResourceFlag_ShaderWriteBindless,
		NULL, &name, 32, &gpuArgs, &t->err
	));

	if (gpuArgs && TestShaders_computePipeline(t, deviceRef, &files[5], &argsPipeline)) {

		const Transition argsWrite = (Transition) {
			.resource = gpuArgs, .stage = EPipelineStage_Compute, .isWrite = true
		};

		ListTransition argsTransition = (ListTransition) { 0 };
		ListTransition_createRefConst(&argsWrite, 1, &argsTransition, NULL);

		Test_assert(t, "beginGpuDraw", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		Test_assert(t, "scopeGpuArgs", CommandListRef_startScope(commandList, &argsTransition, 1, NULL, &t->err));
		Test_assert(t, "bindGpuArgs", CommandListRef_setComputePipeline(commandList, argsPipeline, &t->err));
		Test_assert(t, "dispatchGpuArgs", CommandListRef_dispatch1D(commandList, 1, &t->err));
		Test_assert(t, "scopeGpuArgsEnd", CommandListRef_endScope(commandList, &t->err));

		appData.handles[2] = DeviceBufferRef_ptr(gpuArgs)->writeHandle;
		appData.color[0] = 51.f / 255; appData.color[1] = 51.f / 255; appData.color[2] = 1; appData.color[3] = 1;

		if (TestShaders_openDraw(t, commandList, 2, target, flatPipeline)) {

			Test_assert(t, "drawGpuIndirect", CommandListRef_drawIndirect(commandList, gpuArgs, 0, 1, false, &t->err));

			if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
				t, deviceRef, commandList, &appData, sizeof(appData)
			))
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFFFF3333u);
		}
	}

	//MSAA: a fully covered pixel resolves to exactly the flat color whatever the sample count, and the result
	// lands in the readback target through the resolve attachment rather than a copy.
	//4x is required of every adapter; 2x and 8x are optional, so those are skipped unless the adapter claims
	// them, and running when it does is what turns the claim into something checked rather than reported.
	//Target and pipeline are rebuilt per count, since both bake the sample count in.

	static const struct {
		EMSAASamples samples;
		EGraphicsDataTypes dataType;            //0 when the count is required rather than optional
	} msaaCases[] = {
		{ EMSAASamples_x2Ext, EGraphicsDataTypes_MSAA2x },
		{ EMSAASamples_x4,    (EGraphicsDataTypes) 0    },
		{ EMSAASamples_x8Ext, EGraphicsDataTypes_MSAA8x }
	};

	const EGraphicsDataTypes msaaTypes = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.dataTypes;
	U32 msaaRun = 0, msaaSkipped = 0;

	for (U64 m = 0; m < sizeof(msaaCases) / sizeof(msaaCases[0]); ++m) {

		if (msaaCases[m].dataType && !(msaaTypes & msaaCases[m].dataType)) {
			++msaaSkipped;
			continue;
		}

		name = CharString_createRefCStrConst("Shader draw MSAA target");

		if(!Test_assert(t, "createMsaaTarget", GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
			msaaCases[m].samples, NULL, &name, &msaaTarget, &t->err
		)))
			continue;

		PipelineGraphicsInfo msaaInfo = flatInfo;
		msaaInfo.msaa = msaaCases[m].samples;

		if (TestShaders_graphicsPipeline(t, deviceRef, &fileList, 0, 1, &msaaInfo, &msaaPipeline)) {

			Test_assert(t, "beginMsaa", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
			Test_assert(t, "scopeMsaa", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

			const AttachmentInfo msaaColor = (AttachmentInfo) {
				.image = msaaTarget,
				.load = ELoadAttachmentType_Clear,
				.resolveMode = EMSAAResolveMode_Average,
				.resolveImage = target
			};

			ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
			ListAttachmentInfo_createRefConst(&msaaColor, 1, &colors, NULL);

			Test_assert(t, "renderStartMsaa", CommandListRef_startRenderExt(
				commandList, I32x2_zero, I32x2_create2(8, 8), &colors, NULL, &t->err
			));

			Test_assert(t, "viewportScissorMsaa", CommandListRef_setViewportAndScissor(
				commandList, I32x2_zero, I32x2_zero, &t->err
			));

			Test_assert(t, "bindMsaa", CommandListRef_setGraphicsPipeline(commandList, msaaPipeline, &t->err));
			Test_assert(t, "drawMsaa", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

			appData.color[0] = 102.f / 255; appData.color[1] = 1; appData.color[2] = 51.f / 255; appData.color[3] = 1;

			if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
				t, deviceRef, commandList, &appData, sizeof(appData)
			)) {
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF33FF66u);
				++msaaRun;
			}
		}

		//Both are rebuilt next iteration, and RefPtr_dec nulls them so the clean block below is still safe.

		RefPtr_dec(&msaaPipeline);
		RefPtr_dec(&msaaTarget);
	}

	Log_debugLnx(
		"-- draw: %"PRIu32" MSAA sample counts resolved, %"PRIu32" not claimed by this adapter",
		msaaRun, msaaSkipped
	);

clean:

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&argsPipeline);
	RefPtr_dec(&msaaPipeline);
	RefPtr_dec(&depthPipeline);
	RefPtr_dec(&vertexPipeline);
	RefPtr_dec(&dualPipeline);
	RefPtr_dec(&logicPipeline);
	RefPtr_dec(&logicTarget);
	RefPtr_dec(&mrtPipeline);
	RefPtr_dec(&mrtTarget);
	RefPtr_dec(&wirePipeline);
	RefPtr_dec(&blendPipeline);
	RefPtr_dec(&flatPipeline);
	RefPtr_dec(&gpuArgs);
	RefPtr_dec(&cpuArgs);
	RefPtr_dec(&indexBuffer);
	RefPtr_dec(&vertexBuffer);
	RefPtr_dec(&depth);
	RefPtr_dec(&msaaTarget);
	RefPtr_dec(&target);

	for(U64 i = 0; i < 10; ++i)
		SHFile_free(&files[i], alloc);
}
