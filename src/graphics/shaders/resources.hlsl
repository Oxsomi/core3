#pragma once
#include "types.hlsl"

static const uint ResourceId_mask = (1 << 20) - 1;

SamplerState _samplers[]					: register(space0);

Texture2D _textures2D[]						: register(space1);
TextureCube _textureCubes[]					: register(space2);
Texture3D _textures3D[]						: register(space3);
ByteAddressBuffer _buffer[]					: register(space4);

RWByteAddressBuffer _rwBuffer[]				: register(space5);
RWTexture3D<unorm F32x4> _rwTextures3D[]	: register(space6);
RWTexture3D<snorm F32x4> _rwTextures3Ds[]	: register(space7);
RWTexture3D<F32x4> _rwTextures3Df[]			: register(space8);
RWTexture3D<I32x4> _rwTextures3Di[]			: register(space9);
RWTexture3D<U32x4> _rwTextures3Du[]			: register(space10);
RWTexture2D<unorm F32x4> _rwTextures2D[]	: register(space11);
RWTexture2D<snorm F32x4> _rwTextures2Ds[]	: register(space12);
RWTexture2D<F32x4> _rwTextures2Df[]			: register(space13);
RWTexture2D<I32x4> _rwTextures2Di[]			: register(space14);
RWTexture2D<U32x4> _rwTextures2Du[]			: register(space15);

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

//Globals used during the entire frame for useful information such as frame id.

cbuffer globals : register(space16) {

	U32 _frameId;					//Can loop back to 0 after U32_MAX!
	F32 _time;						//Time since launch of app
	F32 _deltaTime;					//deltaTime since last frame.
	U32 _swapchainCount;			//How many swapchains are present (will insert ids into appData)

	//Up to 240 bytes of user data, useful for supplying constant per frame data.
	//However, offset [0, _swapchainCount> are reserved for swapchain handles.
	//Make sure to offset to make sure.

	U32x4 _appData[15];
};

//Fetch per frame data from the application.
//If possible, please make sure the offset is const so it's as fast as possible.
//Offset is in uints (4-byte), not in bytes!
//Aligned fetches only return non 0 if all of the vector can be fetched!

//Fetch 1 element from user data

U32 getAppData1u(uint offset) { return offset >= 60 ? 0 : _appData[offset >> 2][offset & 3]; }
I32 getAppData1i(uint offset) { return (int) getAppData1u(offset); }
F32 getAppData1f(uint offset) { return asfloat(getAppData1u(offset)); }

//Fetch 2 element vector from user data
//Use unaligned only if necessary, otherwise please align offset to 8-byte!

U32x2 getAppData2u(uint offset) { 
	return offset >= 59 ? 0.xx : (
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

U32x4 getAppData4u(uint offset) { return offset >= 56 ? 0 : _appData[offset >> 2]; }
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
