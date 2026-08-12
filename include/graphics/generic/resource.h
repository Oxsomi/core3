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

//graphics/generic/resource.h

#pragma once
#include "types/base/types.h"
#include "types/math/vec2.h"
#include "types/container/list.h"
#include "types/container/texture_format.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef RefPtr GraphicsDeviceRef;
typedef RefPtr GraphicsResourceRef;            //UnifiedTexture or DeviceTexture. This isn't a RefPtr<GraphicsResource>

typedef struct GraphicsDevice GraphicsDevice;

typedef enum EGraphicsResourceFlag {

	EGraphicsResourceFlag_None                      = 0,

	EGraphicsResourceFlag_ShaderRead                = 1 << 0,        //Can be read by shaders; excluding constant buffers
	EGraphicsResourceFlag_ShaderWrite               = 1 << 1,        //Can be written by shaders
	EGraphicsResourceFlag_ExposeBindlessRead        = 1 << 2,        //Expose descriptor(s) to bindless DescriptorTable (read)
	EGraphicsResourceFlag_ExposeBindlessWrite       = 1 << 3,        //Expose descriptor(s) to bindless DescriptorTable (RW)

	EGraphicsResourceFlag_InternalWeakDeviceRef     = 1 << 4,        //Only for internal use in device

	EGraphicsResourceFlag_CPUBacked                 = 1 << 5,        //Keep a CPU side copy texture for read/write operations
	EGraphicsResourceFlag_CPUAllocatedBit           = 1 << 6,        //Keep entirely on CPU

	//CPU readable memory (D3D12 readback heap), only used internally for the pullRegion staging buffer.
	//Vulkan reuses the CPU allocated memory type, which is host visible either way.

	EGraphicsResourceFlag_CPUReadBit                = 1 << 7,

	EGraphicsResourceFlag_CPUAllocated              = EGraphicsResourceFlag_CPUBacked | EGraphicsResourceFlag_CPUAllocatedBit,

	EGraphicsResourceFlag_ShaderRWBindful           =
		EGraphicsResourceFlag_ShaderRead | EGraphicsResourceFlag_ShaderWrite,

	EGraphicsResourceFlag_ShaderReadBindless        =
		EGraphicsResourceFlag_ShaderRead | EGraphicsResourceFlag_ExposeBindlessRead,

	EGraphicsResourceFlag_ExposeBindless            =
	EGraphicsResourceFlag_ExposeBindlessRead | EGraphicsResourceFlag_ExposeBindlessWrite,

	EGraphicsResourceFlag_ShaderWriteBindless       =
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_ExposeBindlessWrite,

	EGraphicsResourceFlag_ShaderRWBindless          =
		EGraphicsResourceFlag_ShaderWriteBindless | EGraphicsResourceFlag_ShaderReadBindless,

} EGraphicsResourceFlag;

typedef U16 GraphicsResourceFlag;

typedef enum EResourceType {
	EResourceType_Undefined,
	EResourceType_DeviceTexture,                    //Texture only copyable (no RTV, DSV or UAV)
	EResourceType_RenderTargetOrDepthStencil,       //Also depth stencil
	EResourceType_DeviceBuffer,                     //Any buffer type
	EResourceType_Swapchain,
	EResourceType_Count
} EResourceType;

extern const C8 *EResourceType_names[EResourceType_Count];

typedef struct BufferRange {
	U64 startRange;
	U64 endRange;
} BufferRange;

typedef struct TextureRange {
	U16 startRange[3];
	U16 endRange[3];
	U16 levelId;
	U16 padding;
} TextureRange;

typedef struct ImageRange {
	U32 levelId;        //Set to U32_MAX to indicate all levels, otherwise indicates specific index.
	U32 layerId;        //Set to U32_MAX to indicate all layers, otherwise indicates specific index.
} ImageRange;

typedef union ResourceRange {
	BufferRange buffer;
	ImageRange image;
} ResourceRange;

U16 TextureRange_width(TextureRange r);
U16 TextureRange_height(TextureRange r);
U16 TextureRange_length(TextureRange r);

typedef union DevicePendingRange {
	BufferRange buffer;
	TextureRange texture;
} DevicePendingRange;

//Reading back from the device (pullRegion).
//The callback fires once the frame containing the read completed and the data landed in the resource's cpuData.

typedef void (*DevicePullCallback)(RefPtr *resource, void *context);

typedef struct DevicePendingPull {

	RefPtr *resource;                //Strong ref, released after the callback ran

	DevicePullCallback callback;     //Optional
	void *context;

	DevicePendingRange range;

	U64 stagingOffset;               //Into stagingReadback, only valid once recorded
	U64 rowPitch;                    //Row stride in readback memory for textures, byte count for buffers

	//Strong ref to the readback buffer the pull was recorded into, only set once in flight.
	//A grow (or reserveReadback) swaps the device's ring, so completion has to read the buffer the GPU wrote.

	RefPtr *stagingReadback;

} DevicePendingPull;

TList(DevicePendingPull);

typedef struct DeviceResourceVersion {

	RefPtr *resource;        //SwapchainRef
	U64 version;

	I32x2 size;
	ETextureFormatId format;
	U32 padding;

} DeviceResourceVersion;

TList(DeviceResourceVersion);

//Correct usage of this "base class":
//ChildObject -> GraphicsResource -> NextResource -> ExtStruct
//Example:    DeviceTexture -> GraphicsResource -> UnifiedTexture -> XXManagedImage[N] (e.g. VkManagedImage)
//            DeviceBuffer -> GraphicsResource -> DeviceBufferExt
typedef struct GraphicsResource {

	GraphicsDeviceRef *device;

	U64 size;                               //CPU sized visible memory size

	U64 blockOffset;

	U32 blockId;

	GraphicsResourceFlag flags;
	U8 type;                                //EResourceType
	Bool allocated;

	void *mappedMemoryExt;                  //API specific memory range. Don't write/read from this address

	void *debugExt;                         //Debug information

	U64 deviceAddress;                      //Contains the memory address on the device if available (otherwise 0)

	U64 padding;                            //Padding to 16-byte to allow next element to have vectors

} GraphicsResource;

void GraphicsResource_free(GraphicsResource *resource, RefPtr *resourceRef);

#ifdef __cplusplus
	}
#endif
