R"(
/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "@types.hlsl"

//We only include nv extensions if DXIL compilation and ray: reorder, motion blur, micromap opacity / displacement is required
//This saves us from parsing, preprocessing and compiling useless stuff.

#if !defined(__spirv__) && (																	\
	defined(__OXC3_EXT_RAYMICROMAPOPACITY)	|| defined(__OXC3_EXT_RAYMICROMAPDISPLACEMENT) ||	\
	defined(__OXC3_EXT_RAYMOTIONBLUR)		|| defined(__OXC3_EXT_RAYREORDER)					\
)

	#define NV_SHADER_EXTN_SLOT u99999
	#define NV_SHADER_EXTN_REGISTER_SPACE space99999

	#include "@nvHLSLExtns.h"

#endif

static const U32 ResourceId_mask = (1 << 17) - 1;
static const U32 ShortResourceId_mask = (1 << 13) - 1;		//Only for Samplers, AS and other short resource ids
static const U32 U32_MAX = 0xFFFFFFFFu;

#ifdef __spirv__
    #define _binding(a, b, ...) [[vk::binding(a, b)]] __VA_ARGS__
	#define _vkBinding(a, b) [[vk::binding(a, b)]]
#else
    #define _binding(a, b, ...) __VA_ARGS__ : register(space##a)
	#define _vkBinding(a, b)
#endif

_binding( 0, 0, SamplerState _samplers[2048]);

_binding( 0, 1, Texture2D _textures2D[131072]);
_binding( 1, 1, TextureCube _textureCubes[32768]);
_binding( 2, 1, Texture3D _textures3D[32768]);

_binding( 3, 1, ByteAddressBuffer _buffer[131072]);
_binding( 4, 1, RWByteAddressBuffer _rwBuffer[131072]);

_binding( 5, 1, RWTexture3D<unorm F32x4> _rwTextures3D[8192]);
_binding( 6, 1, RWTexture3D<snorm F32x4> _rwTextures3Ds[8192]);
_binding( 7, 1, RWTexture3D<F32x4> _rwTextures3Df[32768]);
_binding( 8, 1, RWTexture3D<I32x4> _rwTextures3Di[8192]);
_binding( 9, 1, RWTexture3D<U32x4> _rwTextures3Du[8192]);
_binding(10, 1, RWTexture2D<unorm F32x4> _rwTextures2D[65536]);
_binding(11, 1, RWTexture2D<snorm F32x4> _rwTextures2Ds[8192]);
_binding(12, 1, RWTexture2D<F32x4> _rwTextures2Df[65536]);
_binding(13, 1, RWTexture2D<I32x4> _rwTextures2Di[16384]);
_binding(14, 1, RWTexture2D<U32x4> _rwTextures2Du[16384]);

_binding(15, 1, RaytracingAccelerationStructure _tlasExt[16]);

_vkBinding( 0, 2) cbuffer globals {	//Globals used during the entire frame for useful information such as frame id.

	U32 _frameId;					//Can loop back to 0 after U32_MAX!
	F32 _time;						//Time since launch of app
	F32 _deltaTime;					//deltaTime since last frame.
	U32 _swapchainCount;			//How many swapchains are present (will insert ids into appData)

	U32x4 _swapchains[8];			//Descriptors of swapchains: (Read, write)[2][8]

	//Up to 368 bytes of user data, useful for supplying constant per frame data.
	//Make sure to offset to make sure.

	U32x4 _appData[23];
};

#define samplerUniform(i) _samplers[i & ShortResourceId_mask]
#define sampler(i) _samplers[NonUniformResourceIndex(i & ShortResourceId_mask)]

#define tlasExtUniform(i) _tlasExt[i & ShortResourceId_mask]
#define tlasExt(i) _tlasExt[NonUniformResourceIndex(i & ShortResourceId_mask)]

#define rwBufferUniform(i) _rwBuffer[i & ResourceId_mask]
#define bufferUniform(i) _buffer[i & ResourceId_mask]
#define rwBuffer(i) _rwBuffer[NonUniformResourceIndex(i & ResourceId_mask)]
#define buffer(i) _buffer[NonUniformResourceIndex(i & ResourceId_mask)]

#define texture2DUniform(i) _textures2D[i & ResourceId_mask]
#define textureCubeUniform(i) _textureCubes[i & ResourceId_mask]
#define texture3DUniform(i) _textures3D[i & ResourceId_mask]

#define rwTexture3DUniform(i) _rwTextures3D[i & ResourceId_mask]
#define rwTexture3DsUniform(i) _rwTextures3Ds[i & ResourceId_mask]
#define rwTexture3DfUniform(i) _rwTextures3Df[i & ResourceId_mask]
#define rwTexture3DiUniform(i) _rwTextures3Di[i & ResourceId_mask]
#define rwTexture3DuUniform(i) _rwTextures3Du[i & ResourceId_mask]

#define rwTexture2DUniform(i) _rwTextures2D[i & ResourceId_mask]
#define rwTexture2DsUniform(i) _rwTextures2Ds[i & ResourceId_mask]
#define rwTexture2DfUniform(i) _rwTextures2Df[i & ResourceId_mask]
#define rwTexture2DiUniform(i) _rwTextures2Di[i & ResourceId_mask]
#define rwTexture2DuUniform(i) _rwTextures2Du[i & ResourceId_mask]

#define texture2D(i) _textures2D[NonUniformResourceIndex(i & ResourceId_mask)]
#define textureCube(i) _textureCubes[NonUniformResourceIndex(i & ResourceId_mask)]
#define texture3D(i) _textures3D[NonUniformResourceIndex(i & ResourceId_mask)]

#define rwTexture3D(i) _rwTextures3D[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture3Ds(i) _rwTextures3Ds[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture3Df(i) _rwTextures3Df[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture3Di(i) _rwTextures3Di[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture3Du(i) _rwTextures3Du[NonUniformResourceIndex(i & ResourceId_mask)]

#define rwTexture2D(i) _rwTextures2D[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture2Ds(i) _rwTextures2Ds[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture2Df(i) _rwTextures2Df[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture2Di(i) _rwTextures2Di[NonUniformResourceIndex(i & ResourceId_mask)]
#define rwTexture2Du(i) _rwTextures2Du[NonUniformResourceIndex(i & ResourceId_mask)]

template<typename T>
T getAtUniform(U32 resourceId, U32 id) {
	return bufferUniform(resourceId).Load<T>(id);
}

template<typename T>
T getAt(U32 resourceId, U32 id) {
	return buffer(resourceId).Load<T>(id);
}

template<typename T>
void setAtUniform(U32 resourceId, U32 id, T t) {
	rwBufferUniform(resourceId).Store<T>(id, t);
}

template<typename T>
void setAt(U32 resourceId, U32 id, T t) {
	rwBuffer(resourceId).Store<T>(id, t);
}

//Fetch per frame data from the application.
//If possible, please make sure the offset is const so it's as fast as possible.
//Offset is in uints (4-byte), not in bytes!
//Aligned fetches only return non 0 if all of the vector can be fetched!

U32 getReadSwapchain(uint offset) { return offset & 1 ? _swapchains[offset >> 1].z : _swapchains[offset >> 1].x; }
U32 getWriteSwapchain(uint offset) { return offset & 1 ? _swapchains[offset >> 1].w : _swapchains[offset >> 1].y; }

//Fetch 1 element from user data

U32 getAppData1u(uint offset) { return offset >= 92 ? 0 : _appData[offset >> 2][offset & 3]; }
I32 getAppData1i(uint offset) { return (int) getAppData1u(offset); }
F32 getAppData1f(uint offset) { return asfloat(getAppData1u(offset)); }

//Fetch 2 element vector from user data
//Use unaligned only if necessary, otherwise please align offset to 8-byte!

U32x2 getAppData2u(uint offset) {
	return offset >= 92 ? 0.xx : (
		(offset & 2) == 0 ? _appData[offset >> 2].xy : _appData[offset >> 2].zw
	);
}

I32x2 getAppData2i(uint offset) { return (I32x2) getAppData2u(offset); }
F32x2 getAppData2f(uint offset) { return asfloat(getAppData2u(offset)); }

U32x2 getAppData2uUnaligned(uint offset) { return U32x2(getAppData1u(offset), getAppData1u(offset + 1)); }
I32x2 getAppData2iUnaligned(uint offset) { return (I32x2) getAppData2uUnaligned(offset); }
F32x2 getAppData2fUnaligned(uint offset) { return asfloat(getAppData2uUnaligned(offset)); }

//Fetch 4 element vector from user data
//Use unaligned only if necessary, otherwise please align offset to 8-byte!

U32x4 getAppData4u(uint offset) { return offset >= 92 ? 0 : _appData[offset >> 2]; }
I32x4 getAppData4i(uint offset) { return (I32x4) getAppData4u(offset); }
F32x4 getAppData4f(uint offset) { return asfloat(getAppData4u(offset)); }

U32x4 getAppData4uUnaligned(uint offset) { return U32x4(getAppData2u(offset), getAppData2u(offset + 1)); }
I32x4 getAppData4iUnaligned(uint offset) { return (I32x4) getAppData4uUnaligned(offset); }
F32x4 getAppData4fUnaligned(uint offset) { return asfloat(getAppData4uUnaligned(offset)); }

//Fetch 3 element vector from user data
//Use unaligned only if necessary, otherwise please align offset to 16-byte!

U32x3 getAppData3u(uint offset) { return getAppData4u(offset).xyz; }
I32x3 getAppData3i(uint offset) { return (I32x3) getAppData3u(offset); }
F32x3 getAppData3f(uint offset) { return asfloat(getAppData3u(offset)); }

U32x3 getAppData3uUnaligned(uint offset) { return getAppData4uUnaligned(offset).xyz; }
I32x3 getAppData3iUnaligned(uint offset) { return (I32x3) getAppData3uUnaligned(offset); }
F32x3 getAppData3fUnaligned(uint offset) { return asfloat(getAppData3uUnaligned(offset)); }
)"
