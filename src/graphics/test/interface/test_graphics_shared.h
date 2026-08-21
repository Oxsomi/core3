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

//graphics/test/interface/test_graphics_shared.h

#pragma once
#include "types/test/test.h"
#include "graphics/generic/device.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/device_buffer.h"
#include "formats/oiSH/sh_file.h"

//C++ test TUs include this inside oxc::c, so the declarations need C linkage to match the C compiled
//definitions; without it the helpers below mangle as C++ and fail to link.

#ifdef __cplusplus
	extern "C" {
#endif

//Headless modules (pure, no device), called directly from the entry point.

void Test_graphicsFormats(Test *t);
void Test_bindlessDescriptorPacking(Test *t);
void Test_descriptorPacking(Test *t);
void Test_textureRange(Test *t);
void Test_graphicsDefaultBindlessLayout(Test *t);

//Modules that need a live device, called from the device test loop once per adapter.
//They own everything they create, so they can run in any order.

void Test_graphicsCommandList(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsCommandRecording(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsCommandValidation(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsRenderPass(Test *t, GraphicsDeviceRef *deviceRef);

void Test_graphicsDescriptorTable(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindlessDescriptor(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBufferBindless(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindlessInterleave(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindlessEverywhere(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsFrameGlobals(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsDescriptorAlloc(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindful(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulAdvanced(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulSampler(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulDraw(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulLayoutSwitch(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulCbuffer(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulRwTexture(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulArray(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulSpaces(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulIndirect(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulDrawFixed(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulTableUpdate(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulSharedRegister(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulHeapRecycle(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulPushDescriptorBoundary(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulSamplerCmp(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulStructured(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulAppendCounter(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulOmm(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulRayQueryGraphics(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulAtomicFloat(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulPushConstants(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulPushDescriptors(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulReservedSpace(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsBindfulRays(Test *t, GraphicsDeviceRef *deviceRef);

void Test_graphicsTextureRef(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsSamplerAndData(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsPipelineLayout(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsShaderReflection(Test *t, GraphicsDeviceRef *deviceRef);

void Test_graphicsSubmit(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsDeviceMemory(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsGpuExecute(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsAccelerationStructures(Test *t, GraphicsDeviceRef *deviceRef);

void Test_graphicsShaderCompute(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsShaderDraw(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsShaderRays(Test *t, GraphicsDeviceRef *deviceRef);

void Test_graphicsFormatRoundTrip(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsTextureShapes(Test *t, GraphicsDeviceRef *deviceRef);
void Test_graphicsFramesInFlight(Test *t, GraphicsDeviceRef *deviceRef);

void Test_graphicsCapabilities(Test *t, GraphicsDeviceRef *deviceRef);

//Shader execution helpers, defined in test_graphics_shaders.c and shared with the capability execution
//module so the two don't keep their own copies of the same load/dispatch/readback dance.

//One app data layout shared by every test shader, so a single submit can feed mixed pipelines.
//handles[0] = output buffer, handles[1] = base value or TLAS, handles[2] = indirect argument buffer.
//color is what the pixel shaders return, read as F32x4 at U32 offset 4.

typedef struct TestShaderAppData {
	U32 handles[4];
	F32 color[4];
	U32 logicSrc0[4];        //U32 offsets 8..11: what logic op instance 0 writes
	U32 logicSrc1[4];        //U32 offsets 12..15: what instance 1 XORs on top
} TestShaderAppData;

Bool TestShaders_loadFile(Test *t, const C8 *pathStr, SHFile *file);
U32 TestShaders_entry(Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, const C8 *name);
Bool TestShaders_computePipeline(Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, PipelineRef **pipeline);
Bool TestShaders_submitAndWait(
	Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *commandList, const void *appData, U64 appDataLen
);
Bool TestShaders_pullBuffer(Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, DeviceBufferRef *buffer);

//D3D12's GPU based validation instruments raytracing libs into invalid bytecode, so modules that execute
//raytracing pipelines trade the suite's device for a dedicated one with only GPU based validation off.
//Returns false when the workaround was needed but setting it up failed, in which case the module can't run.
//instanceType is caller owned because RefPtr_create keeps a POINTER to the type rather than a copy
// (see ref_ptr.h), so it has to outlive the instance; the end call checks the dedicated instance is
// validation clean and releases both refs.

Bool TestShaders_rtDedicatedDevice(
	Test *t,
	GraphicsDeviceRef **deviceRef,                       //Replaced by the dedicated device when it applies
	GraphicsInstanceRef **ownInstanceRef,
	GraphicsDeviceRef **ownDeviceRef,
	RefPtrType *instanceType
);

void TestShaders_rtDedicatedDeviceEnd(Test *t, GraphicsInstanceRef **ownInstanceRef, GraphicsDeviceRef **ownDeviceRef);

typedef struct TestShaderPixels {
	U32 count;
	U32 padding;
	U64 len;
	U32 pixels[64];
} TestShaderPixels;

Bool TestShaders_pullPixels(
	Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, RefPtr *target, TestShaderPixels *pixels
);

//Pulls the 8x8 render target back and passes when all 64 pixels equal the expected packed RGBA8 value

Bool TestShaders_checkPixels(
	Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, RefPtr *target, U32 expected
);

void Test_graphicsCapabilityExecution(Test *t, GraphicsDeviceRef *deviceRef);

//Config variants build their own devices from the flags under test, so this one takes the instance and the
//adapter rather than a device the suite already created.

void Test_graphicsConfigVariants(Test *t, GraphicsInstanceRef *instRef, const GraphicsDeviceInfo *info);

#ifdef __cplusplus
	}
#endif
