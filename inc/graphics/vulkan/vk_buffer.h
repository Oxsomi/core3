/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "graphics/vulkan/vulkan.h"

typedef struct VkDeviceBuffer {
	VkBuffer buffer;
	VkPipelineStageFlagBits2 lastStage;
	VkAccessFlagBits2 lastAccess;
} VkDeviceBuffer;

Error VkDeviceBuffer_transition(
	VkDeviceBuffer *buffer,
	VkPipelineStageFlags2 stage,
	VkAccessFlagBits2 access,
	U32 graphicsQueueId,
	U64 offset,
	U64 size,
	ListVkBufferMemoryBarrier2 *bufferBarriers,
	VkDependencyInfo *dependency
);