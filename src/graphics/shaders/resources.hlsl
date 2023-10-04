#pragma once
#pragma dxc diagnostic ignored "-Wignored-attributes"
#include "types.hlsl"

//Overlapping registers ala Darius (https://blog.traverseresearch.nl/bindless-rendering-setup-afeb678d77fc)

[[vk::binding(0,  0)]] SamplerState _samplers[];

[[vk::binding(0,  1)]] Texture2D _textures2D[];
[[vk::binding(0,  2)]] TextureCube _textureCubes[];
[[vk::binding(0,  3)]] Texture3D _textures3D[];
[[vk::binding(0,  4)]] ByteAddressBuffer _buffer[];

[[vk::binding(0,  5)]] RWByteAddressBuffer _rwBuffer[];
[[vk::binding(0,  6)]] RWTexture3D<unorm F32x4> _rwTextures3D[];
[[vk::binding(0,  7)]] RWTexture3D<snorm F32x4> _rwTextures3Ds[];
[[vk::binding(0,  8)]] RWTexture3D<F32x4> _rwTextures3Df[];
[[vk::binding(0,  9)]] RWTexture3D<I32x4> _rwTextures3Di[];
[[vk::binding(0, 10)]] RWTexture3D<U32x4> _rwTextures3Du[];
[[vk::binding(0, 11)]] RWTexture2D<unorm F32x4> _rwTextures2D[];
[[vk::binding(0, 12)]] RWTexture2D<snorm F32x4> _rwTextures2Ds[];
[[vk::binding(0, 13)]] RWTexture2D<F32x4> _rwTextures2Df[];
[[vk::binding(0, 14)]] RWTexture2D<I32x4> _rwTextures2Di[];
[[vk::binding(0, 15)]] RWTexture2D<U32x4> _rwTextures2Du[];

#define rwBufferUniform(i) _rwBuffer[i]
#define bufferUniform(i) _buffer[i]
#define rwBuffer(i) _rwBuffer[NonUniformResourceIndex(i)]
#define buffer(i) _buffer[NonUniformResourceIndex(i)]

#define texture2DUniform(i) _textures2D[i]
#define textureCubeUniform(i) _textureCubes[i]
#define texture3DUniform(i) _textures3D[i]

#define rwTexture3DUniform(i) _rwTextures3D[i]
#define rwTexture3DsUniform(i) _rwTextures3Ds[i]
#define rwTexture3DfUniform(i) _rwTextures3Df[i]
#define rwTexture3DiUniform(i) _rwTextures3Di[i]
#define rwTexture3DuUniform(i) _rwTextures3Du[i]

#define rwTexture2DUniform(i) _rwTextures2D[i]
#define rwTexture2DsUniform(i) _rwTextures2Ds[i]
#define rwTexture2DfUniform(i) _rwTextures2Df[i]
#define rwTexture2DiUniform(i) _rwTextures2Di[i]
#define rwTexture2DuUniform(i) _rwTextures2Du[i]

#define texture2D(i) _textures2D[NonUniformResourceIndex(i)]
#define textureCube(i) _textureCubes[NonUniformResourceIndex(i)]
#define texture3D(i) _textures3D[NonUniformResourceIndex(i)]

#define rwTexture3D(i) _rwTextures3D[NonUniformResourceIndex(i)]
#define rwTexture3Ds(i) _rwTextures3Ds[NonUniformResourceIndex(i)]
#define rwTexture3Df(i) _rwTextures3Df[NonUniformResourceIndex(i)]
#define rwTexture3Di(i) _rwTextures3Di[NonUniformResourceIndex(i)]
#define rwTexture3Du(i) _rwTextures3Du[NonUniformResourceIndex(i)]

#define rwTexture2D(i) _rwTextures2D[NonUniformResourceIndex(i)]
#define rwTexture2Ds(i) _rwTextures2Ds[NonUniformResourceIndex(i)]
#define rwTexture2Df(i) _rwTextures2Df[NonUniformResourceIndex(i)]
#define rwTexture2Di(i) _rwTextures2Di[NonUniformResourceIndex(i)]
#define rwTexture2Du(i) _rwTextures2Du[NonUniformResourceIndex(i)]

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

//Globals used during the entire frame for useful information such as frame id.

/*[[vk::binding(0, 16)]] cbuffer globals {

	U64 _frameId;
	F32 _time;
	U32 _swapchainCount;

	U32x4 _swapchainIds[7];			//Up to 28 swapchains. This points to to where they're at in the global registers

};*/ 
