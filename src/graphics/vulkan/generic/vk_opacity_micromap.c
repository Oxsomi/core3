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

//graphics/vulkan/generic/vk_opacity_micromap.c

#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_interface.h"
#include "types/container/list_impl.h"
#include "types/container/string.h"
#include "types/base/error.h"

TListNamedImpl(ListVkMicromapUsageEXT);

Bool VK_WRAP_FUNC(OpacityMicromap_init)(OpacityMicromap *micromap, Error *e_rr) {

	Bool s_uccess = true;

	//The usages are translated once here rather than per BLAS, because every BLAS that links this micromap
	// has to hand the same set back to the driver.

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(micromap->base.device);
	VkOpacityMicromap *micromapExt = OpacityMicromap_ext(micromap, Vk);
	CharString tmp = CharString_createNull();

	GraphicsDevice *device = GraphicsDeviceRef_ptr(micromap->base.device);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	//The KHR promotion builds micromap arrays as acceleration structures through entry points this backend
	// has no driver to test against yet, so rather than shipping an untestable build it refuses clearly.
	//Special index OMM keeps working on such a device; only micromap OBJECTS are affected.

	if(device->info.capabilities.featuresExt & EVkGraphicsFeatures_OpacityMicromapKHR)
		retError(clean, Error_unsupportedOperation(
			0,
			"VkOpacityMicromap_init() micromap arrays aren't implemented on the KHR path yet, only special "
			"index OMM works there"
		));

	gotoIfError3(clean, ListVkMicromapUsageEXT_resize(
		&micromapExt->usages, micromap->usages.length, alloc, e_rr
	));

	for (U64 i = 0; i < micromap->usages.length; ++i) {

		const OpacityMicromapUsage usage = micromap->usages.ptr[i];

		//Mapped through a switch rather than cast, so a header renumbering breaks the build instead of the
		// opacity data.

		VkOpacityMicromapFormatEXT format = VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT;

		switch ((EOpacityMicromapFormat) usage.format) {

			case EOpacityMicromapFormat_Opacity2State:
				format = VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT;
				break;

			case EOpacityMicromapFormat_Opacity4State:
				format = VK_OPACITY_MICROMAP_FORMAT_4_STATE_EXT;
				break;

			default:
				retError(clean, Error_unsupportedOperation(0, "VkOpacityMicromap_init() unsupported format"));
		}

		micromapExt->usages.ptrNonConst[i] = (VkMicromapUsageEXT) {
			.count = usage.count,
			.subdivisionLevel = usage.subdivisionLevel,
			.format = (U32) format
		};
	}

	//The build inputs must sit at 256 aligned device addresses (VUID-vkCmdBuildMicromapsEXT-pInfos-07515).
	//OxC3's own ASRead buffers are allocated 256 aligned when micromaps are on, so only a caller supplied
	// OFFSET can break this, which is why it is checked here where the mistake is visible.

	const U64 inputAddr = getVkDeviceAddress(micromap->inputBuffer);
	const U64 entryAddr = getVkDeviceAddress(micromap->entryBuffer);

	if((inputAddr & 255) || (entryAddr & 255))
		retError(clean, Error_invalidParameter(
			1, 0, "VkOpacityMicromap_init() input and entry buffers must sit at 256 byte aligned addresses"
		));

	VkBuildMicromapFlagsEXT buildFlags = 0;

	if(micromap->base.flags & ERTASBuildFlags_AllowCompaction)
		buildFlags |= VK_BUILD_MICROMAP_ALLOW_COMPACTION_BIT_EXT;

	if(micromap->base.flags & ERTASBuildFlags_FastTrace)
		buildFlags |= VK_BUILD_MICROMAP_PREFER_FAST_TRACE_BIT_EXT;

	if(micromap->base.flags & ERTASBuildFlags_FastBuild)
		buildFlags |= VK_BUILD_MICROMAP_PREFER_FAST_BUILD_BIT_EXT;

	micromapExt->build = (VkMicromapBuildInfoEXT) {
		.sType = VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT,
		.type = VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT,
		.flags = buildFlags,
		.mode = VK_BUILD_MICROMAP_MODE_BUILD_EXT,
		.usageCountsCount = (U32) micromapExt->usages.length,
		.pUsageCounts = micromapExt->usages.ptr,
		.data = (VkDeviceOrHostAddressConstKHR) { .deviceAddress = inputAddr },
		.triangleArray = (VkDeviceOrHostAddressConstKHR) { .deviceAddress = entryAddr },
		.triangleArrayStride = micromap->entryStride
	};

	VkMicromapBuildSizesInfoEXT sizes = (VkMicromapBuildSizesInfoEXT) {
		.sType = VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT
	};

	deviceExt->getMicromapBuildSizes(
		deviceExt->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &micromapExt->build, &sizes
	);

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		micromap->base.device,
		EDeviceBufferUsage_ASExt,
		EGraphicsResourceFlag_None,
		NULL,
		&micromap->base.name,
		sizes.micromapSize,
		&micromap->base.asBuffer, e_rr
	));

	//A zero scratch size is legal, and a zero sized buffer is not

	if (sizes.buildScratchSize) {

		gotoIfError3(clean, CharString_format(
			alloc, &tmp, e_rr, "%.*s scratch buffer",
			CharString_length(micromap->base.name), micromap->base.name.ptr
		));

		gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
			micromap->base.device,
			EDeviceBufferUsage_ScratchExt,
			EGraphicsResourceFlag_None,
			NULL,
			&tmp,
			sizes.buildScratchSize,
			&micromap->base.tempScratchBuffer, e_rr
		));
	}

	const VkMicromapCreateInfoEXT createInfo = (VkMicromapCreateInfoEXT) {
		.sType = VK_STRUCTURE_TYPE_MICROMAP_CREATE_INFO_EXT,
		.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(micromap->base.asBuffer), Vk)->buffer,
		.size = sizes.micromapSize,
		.type = VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT
	};

	gotoIfError3(clean, checkVkError(
		deviceExt->createMicromap(deviceExt->device, &createInfo, NULL, &micromapExt->micromap), e_rr
	));

	micromapExt->build.dstMicromap = micromapExt->micromap;

	if(micromap->base.tempScratchBuffer)
		micromapExt->build.scratchData = (VkDeviceOrHostAddressKHR) {
			.deviceAddress = DeviceBufferRef_ptr(micromap->base.tempScratchBuffer)->resource.deviceAddress
		};

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

void VK_WRAP_FUNC(OpacityMicromap_free)(OpacityMicromap *micromap) {

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(micromap->base.device);
	VkOpacityMicromap *micromapExt = OpacityMicromap_ext(micromap, Vk);

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(micromap->base.device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	if(micromapExt->micromap)
		deviceExt->destroyMicromap(deviceExt->device, micromapExt->micromap, NULL);

	ListVkMicromapUsageEXT_free(&micromapExt->usages, alloc);
}

Bool VK_WRAP_FUNC(OpacityMicromapRef_flush)(
	void *commandBufferExt, GraphicsDeviceRef *deviceRef, OpacityMicromapRef *pending, Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	VkCommandBufferState *commandBuffer = (VkCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	OpacityMicromap *micromap = OpacityMicromapRef_ptr(pending);
	VkOpacityMicromap *micromapExt = OpacityMicromap_ext(micromap, Vk);

	//A micromap has no update mode, so a completed one never rebuilds

	if(micromap->base.isCompleted)
		return s_uccess;

	deviceExt->cmdBuildMicromaps(commandBuffer->buffer, 1, &micromapExt->build);

	//Keep the object alive for the frame; the scratch is never needed again after the build, so the flight
	// takes the reference the object held, exactly like a non updatable BLAS.

	if(!ListRefPtr_contains(*currentFlight, pending, 0, NULL)) {
		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, pending, alloc, e_rr));
		RefPtr_inc(pending);
	}

	if(
		micromap->base.tempScratchBuffer &&
		!ListRefPtr_contains(*currentFlight, micromap->base.tempScratchBuffer, 0, NULL)
	) {
		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, micromap->base.tempScratchBuffer, alloc, e_rr));
		micromap->base.tempScratchBuffer = NULL;
	}

	micromap->base.isCompleted = true;

clean:
	return s_uccess;
}
