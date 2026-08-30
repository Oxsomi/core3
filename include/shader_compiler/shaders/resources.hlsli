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

//shader_compiler/shaders/resources.hlsli
//
//The bindless binding model: the descriptor arrays every shader sees and the accessors that index
//them, plus the per frame globals. Buffer reads/writes live in their own header, included below.

#pragma once
#include "@types.hlsli"

static const U32 ResourceId_mask = (1 << 17) - 1;
static const U32 U32_MAX = 0xFFFFFFFFu;

//Even though DXIL bindings could use setId == spaceId and default registerId on 0,
//It'd be inefficient, because the root signature can't simplify this to 3 ranges.
//DXIL puts the whole set in OxC3's reserved space rather than space 0, so a custom layout can use the
//spaces people actually reach for without ever colliding with the engine's own bindless arrays.
//SPIRV needs no such move: its sets are separate from the caller's by construction.

#ifdef __spirv__
	#define _binding(bindingId, setId, a, ...) [[vk::binding(bindingId, setId)]] __VA_ARGS__
	#define _vkBinding(a, b) [[vk::binding(a, b)]]
#else
	#define _binding(a, b, registerId, ...) __VA_ARGS__ : register(registerId, OXC3_RESERVED_SPACE)
	#define _vkBinding(a, b)
#endif

_binding( 0, 0, s0, 		SamplerState _samplers[996]);

_binding( 0, 1, t0, 		Texture2D _textures2D[131072]);
_binding( 1, 1, t131072, 	TextureCube _textureCubes[32768]);
_binding( 2, 1, t163840,	Texture3D _textures3D[32768]);

_binding( 3, 1, t196608, 	ByteAddressBuffer _buffer[131072]);

_binding( 4, 1, u0, 		RWByteAddressBuffer _rwBuffer[131072]);

UNKNOWN_FORMAT _binding( 5, 1, u131072, RWTexture3D<F32x4> _rwTextures3D[32768]);
UNKNOWN_FORMAT _binding( 6, 1, u163840, RWTexture3D<I32x4> _rwTextures3Di[8192]);
UNKNOWN_FORMAT _binding( 7, 1, u172032, RWTexture3D<U32x4> _rwTextures3Du[8192]);
UNKNOWN_FORMAT _binding( 8, 1, u180224, RWTexture2D<F32x4> _rwTextures2D[131072]);
UNKNOWN_FORMAT _binding( 9, 1, u311296, RWTexture2D<I32x4> _rwTextures2Di[16384]);
UNKNOWN_FORMAT _binding(10, 1, u327680, RWTexture2D<U32x4> _rwTextures2Du[16384]);

#if defined(__OXC_EXT_RAYQUERY) || defined(__OXC_EXT_RAYTRACING)
	_binding(11, 1, t327680, RaytracingAccelerationStructure _tlasExt[16]);
#endif

//The register is explicit because the default root signature binds globals at a fixed slot, and DXIL linking
// of multi entrypoint files renumbers implicitly assigned registers, which silently moved globals to b1.
//The SPIRV backend ignores register() when vk::binding is present, so no macro split is needed.
//The space is OxC3's reserved one rather than 0: custom layouts let anyone declare their own b0, and space 0
// is exactly what they reach for first, so leaving globals there made a collision a matter of time.

_vkBinding( 0, 2) cbuffer globals : register(b0, OXC3_RESERVED_SPACE) {	//Globals used for the entire frame.

	U32 _frameId;					//Can loop back to 0 after U32_MAX!
	F32 _time;						//Time since launch of app
	F32 _deltaTime;					//deltaTime since last frame.
	U32 _swapchainCount;			//How many swapchains are present

	U32x4 _swapchains[8];			//Descriptors of swapchains: (Read, write)[2][16]
};

//Fetch a swapchain id the application pushed for this frame.
//The offset counts swapchains, and each one carries a read and a write id.

U32 getReadSwapchain(U32 offset) { return offset & 1 ? _swapchains[offset >> 1].z : _swapchains[offset >> 1].x; }
U32 getWriteSwapchain(U32 offset) { return offset & 1 ? _swapchains[offset >> 1].w : _swapchains[offset >> 1].y; }

#define samplerUniform(i) _samplers[i & ResourceId_mask]
#define sampler(i) _samplers[NonUniformResourceIndex(i & ResourceId_mask)]

#define tlasExtUniform(i) _tlasExt[i & ResourceId_mask]
#define tlasExt(i) _tlasExt[NonUniformResourceIndex(i & ResourceId_mask)]

#define rwBufferUniform(i) _rwBuffer[i & ResourceId_mask]
#define bufferUniform(i) _buffer[i & ResourceId_mask]
#define rwBuffer(i) _rwBuffer[NonUniformResourceIndex(i & ResourceId_mask)]
#define buffer(i) _buffer[NonUniformResourceIndex(i & ResourceId_mask)]

#define texture2DUniform(i) _textures2D[i & ResourceId_mask]
#define textureCubeUniform(i) _textureCubes[i & ResourceId_mask]
#define texture3DUniform(i) _textures3D[i & ResourceId_mask]

#define rwTexture3DUniform(i) _rwTextures3D[i & ResourceId_mask]
#define rwTexture3DiUniform(i) _rwTextures3Di[i & ResourceId_mask]
#define rwTexture3DuUniform(i) _rwTextures3Du[i & ResourceId_mask]

#define rwTexture2DUniform(i) _rwTextures2D[i & ResourceId_mask]
#define rwTexture2DiUniform(i) _rwTextures2Di[i & ResourceId_mask]
#define rwTexture2DuUniform(i) _rwTextures2Du[i & ResourceId_mask]

#define texture2D(i) _textures2D[NonUniformResourceIndex(i & ResourceId_mask)]
#define textureCube(i) _textureCubes[NonUniformResourceIndex(i & ResourceId_mask)]
#define texture3D(i) _textures3D[NonUniformResourceIndex(i & ResourceId_mask)]

#define rwTexture3D(i) _rwTextures3D[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture3Di(i) _rwTextures3Di[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture3Du(i) _rwTextures3Du[NonUniformResourceIndex(i & ResourceId_mask)]

#define rwTexture2D(i) _rwTextures2D[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture2Di(i) _rwTextures2Di[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture2Du(i) _rwTextures2Du[NonUniformResourceIndex(i & ResourceId_mask)]

//Split out of this header once it got too broad to read; included here so @resources.hlsli stays
//the one include a shader needs. It is self sufficient if you'd rather include it directly.

#include "@buffer.hlsli"
