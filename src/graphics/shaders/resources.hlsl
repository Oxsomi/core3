#pragma once
#include "types.hlsl"

enum EResourceLimits {

    EResourceLimits_Samplers = 2'048,

    EResourceLimits_RWTex3Df = 20'000,
    EResourceLimits_RWTex3Di = 5'000,
    EResourceLimits_RWTex3Du = 5'000,
    EResourceLimits_RWTex3D  = 10'000,
    EResourceLimits_RWTex3Ds = 10'000,
    EResourceLimits_RWTex2Df = 100'000,
    EResourceLimits_RWTex2Di = 25'000,
    EResourceLimits_RWTex2Du = 25'000,

    EResourceLimits_RWBuffer = 200'000,

};

#define MAX_READ_BUFFERS 200'000
#define MAX_READ_TEXTURES2D 175'000
#define MAX_READ_TEXTURES3D 20'000
#define MAX_READ_TEXTURECUBES 5'000

SamplerState _samplers[EResourceLimits_Samplers];
RWTexture3D<F32x4> _writeTextures3Df[EResourceLimits_RWTex3Df];
RWTexture3D<I32x4> _writeTextures3Di[EResourceLimits_RWTex3Di];
RWTexture3D<U32x4> _writeTextures3Du[EResourceLimits_RWTex3Du];
RWTexture2D<F32x4> _writeTextures2Df[EResourceLimits_RWTex2Df];
RWTexture2D<I32x4> _writeTextures2Di[EResourceLimits_RWTex2Di];
RWTexture2D<U32x4> _writeTextures2Du[EResourceLimits_RWTex2Du];
RWByteAddressBuffer _writeBuffers[EResourceLimits_RWBuffer];
Texture2D _readTextures2D[MAX_READ_TEXTURES2D];
Texture3D _readTextures3D[MAX_READ_TEXTURES3D];
TextureCube _readTextureCubes[MAX_READ_TEXTURECUBES];
ByteAddressBuffer _readBuffers[MAX_READ_BUFFERS];

enum EResourceType {

    EResourceType_Sampler,          //Sampler is only one where id starts from 1 to allow null handle.
    EResourceType_RWTexture3D,      //Range: float: [0, 30 000>, int: [30 000, 40 000>, uint: [40 000, 50 000>
    EResourceType_RWTexture2D,      //Range: float: [0, 100 000>, int: [100 000, 125 000>, uint: [125 000, 150 000>
    EResourceType_RWBuffer,

    EResourceType_TextureCube,
    EResourceType_Texture3D,
    EResourceType_Texture2D,
    EResourceType_Buffer

} EResourceType;

typedef U32 ResourceId;         //21-bit: 3-bit EResourceType, 18-bit resource id.
typedef U64 ResourceIdx3;       //3x 21-bit resource id.

static const U32 resourceIdMask = (1 << 18) - 1;

//Get dimensions of (read/write) buffers

template<typename T>
U32 getLength(ResourceId resourceId, Bool NURI = false) {
    
    if((resourceId >> 18) != EResourceType_Buffer || id >= MAX_READ_BUFFERS)
        return 0.xxx;

    U32 result = 0;
    _readBuffers[NURI ? NonUniformResourceIndex(id) : id].GetDimensions(result);
    return result / sizeof(T);
}

template<typename T>
U32 getWriteLength(ResourceId resourceId, Bool NURI = false) {
    
    if((resourceId >> 18) != EResourceType_RWBuffer || id >= MAX_WRITE_BUFFERS)
        return 0.xxx;

    U32 result = 0;
    _writeBuffers[NURI ? NonUniformResourceIndex(id) : id].GetDimensions(result);
    return result / sizeof(T);
}

//Get dimensions of (read/write) textures

U32x3 getRes2D(ResourceId resourceId, U32 mipLevel = 0, Bool NURI = false) {
    
    if((resourceId >> 18) != EResourceType_Texture2D || id >= MAX_READ_TEXTURES2D)
        return 0.xxx;

    U32x3 result = 0.xxx;
    _readTextures2D[NURI ? NonUniformResourceIndex(id) : id].GetDimensions(mipLevel, result.x, result.y, result.z);
    return result;
}

U32x2 getWriteRes2D(ResourceId resourceId, Bool NURI = false) {
    
    if((resourceId >> 18) != EResourceType_RWTexture2D || id >= MAX_WRITE_TEXTURES2D)
        return 0.xxx;

    U32x2 result = 0.xxx;
    _writeTextures2D[NURI ? NonUniformResourceIndex(id) : id].GetDimensions(result.x, result.y);
    return result;
}

U32x3 getResCube(ResourceId resourceId, U32 mipLevel = 0, Bool NURI = false) {
    
    if((resourceId >> 18) != EResourceType_TextureCube || id >= MAX_READ_TEXTURECUBES)
        return 0.xxx;

    U32x3 result = 0.xxx;
    _readTexturesCube[NURI ? NonUniformResourceIndex(id) : id].GetDimensions(mipLevel, result.x, result.y, result.z);
    return result;
}

U32x4 getRes3D(ResourceId resourceId, U32 mipLevel = 0, Bool NURI = false) {
    
    if((resourceId >> 18) != EResourceType_Texture3D || id >= MAX_READ_TEXTURES3D)
        return 0.xxxx;

    U32x4 result = 0.xxx;
    _readTextures3D[NURI ? NonUniformResourceIndex(id) : id].GetDimensions(mipLevel, result.x, result.y, result.z, result.w);
    return result;
}

U32x2 getWriteRes3D(ResourceId resourceId, Bool NURI = false) {
    
    if((resourceId >> 18) != EResourceType_RWTexture3D || id >= MAX_WRITE_TEXTURES3D)
        return 0.xxx;

    U32x3 result = 0.xxx;
    _writeTextures3D[NURI ? NonUniformResourceIndex(id) : id].GetDimensions(result.x, result.y, result.z);
    return result;
}

//Writes

Bool write2Df(ResourceId resourceId, I32x2 location, F32x4 value, Bool NURI = false) {

    U32 id = resourceId & resourceIdMask;

    if((resourceId >> 18) != EResourceType_RWTexture2D || id >= MAX_WRITE_TEXTURES2D)
        return false;

    _writeTextures2D[NURI ? NonUniformResourceIndex(id) : id][location] = value;
    return true;
}

Bool write2Du(ResourceId resourceId, I32x2 location, U32x4 value, Bool NURI = false) {
    return write2Df(resourceId, location, asfloat(value), NURI);
}

Bool write2Di(ResourceId resourceId, I32x2 location, I32x4 value, Bool NURI = false) {
    return write2Df(resourceId, location, asfloat(value), NURI);
}

Bool write3Df(ResourceId resourceId, I32x3 location, F32x4 value, Bool NURI = false) {

    U32 id = resourceId & resourceIdMask;

    if((resourceId >> 18) != EResourceType_RWTexture3D || id >= MAX_WRITE_TEXTURES3D)
        return false;

    _writeTextures3D[NURI ? NonUniformResourceIndex(id) : id][location] = value;
    return true;
}

Bool write3Du(ResourceId resourceId, I32x3 location, U32x4 value, Bool NURI = false) {
    return write3Df(resourceId, location, asfloat(value), NURI);
}

Bool write3Di(ResourceId resourceId, I32x3 location, I32x4 value, Bool NURI = false) {
    return write3Df(resourceId, location, asfloat(value), NURI);
}

template<typename T>
Bool writeBuffer(ResourceId resourceId, U32 location, T value, Bool NURI = false) {
    
    U32 id = resourceId & resourceIdMask;

    if((resourceId >> 18) != EResourceType_RWBuffer || id >= MAX_WRITE_BUFFERS)
        return false;

    _writeBuffers[NURI ? NonUniformResourceIndex(id) : id].Write<T>(location, value);
    return true;
}

//Reads

template<typename T>
Bool readBuffer(ResourceId resourceId, U32 location, inout T value, Bool NURI = false) {
    
    U32 id = resourceId & resourceIdMask;

    if((resourceId >> 18) != EResourceType_Buffer || id >= MAX_READ_BUFFERS)
        return false;

    value = _readBuffers[NURI ? NonUniformResourceIndex(id) : id].Read<T>(location);
    return true;
}

F32x4 read2Df(ResourceId resourceId, I32x2 location, F32x4 defaultValue = 0.xxxx) {

    U32 id = resourceId & resourceIdMask;

    if((resourceId >> 18) != EResourceType_Texture2D || id >= MAX_READ_TEXTURES2D)
        return defaultValue;

    return _readTextures2D[NURI ? NonUniformResourceIndex(id) : id][location];
}

I32x4 read2Di(ResourceId resourceId, I32x2 location, I32x4 defaultValue = 0.xxxx) {
    return asint(readTexture2Df(resourceId, location, asfloat(defaultValue)));
}

U32x4 read2Du(ResourceId resourceId, I32x2 location, U32x4 defaultValue = 0.xxxx) {
    return asuint(readTexture2Df(resourceId, location, asfloat(defaultValue)));
}

F32x4 read3Df(ResourceId resourceId, I32x3 location, F32x4 defaultValue = 0.xxxx) {

    U32 id = resourceId & resourceIdMask;

    if((resourceId >> 18) != EResourceType_Texture3D || id >= MAX_READ_TEXTURES3D)
        return defaultValue;

    return _readTextures3D[NURI ? NonUniformResourceIndex(id) : id][location];
}

I32x4 read3Di(ResourceId resourceId, I32x3 location, I32x4 defaultValue = 0.xxxx) {
    return asint(readTexture3Df(resourceId, location, asfloat(defaultValue)));
}

U32x4 read3Du(ResourceId resourceId, I32x3 location, U32x4 defaultValue = 0.xxxx) {
    return asuint(readTexture3Df(resourceId, location, asfloat(defaultValue)));
}

//Sampling, to combine an individual sampler and texture

F32x4 sampleGrad2D(
    ResourceId resourceId, 
    ResourceId samplerId, 
    F32x2 uv,
    F32x2 ddx,
    F32x2 ddy,
    F32x4 defaultValue = 0.xxxx,
    bool NURI = false
) {

    U32 id = resourceId & resourceIdMask;
    U32 sid = samplerId & resourceIdMask;

    if(
        (resourceId >> 18) != EResourceType_Texture2D || id >= MAX_READ_TEXTURES2D ||
        (samplerId >> 18) != EResourceType_Sampler || sid >= MAX_SAMPLERS
    )
        return defaultValue;

    SamplerState samp = _samplers[NURI ? NonUniformResourceIndex(sid) : sid];
    return _readTextures2D[NURI ? NonUniformResourceIndex(id) : id].SampleGrad(samp, uv, ddx, ddy);
}

F32x4 sampleGrad3D(
    ResourceId resourceId, 
    ResourceId samplerId, 
    F32x3 loc,
    F32x2 ddx,
    F32x2 ddy,
    F32x4 defaultValue = 0.xxxx,
    bool NURI = false
) {

    U32 id = resourceId & resourceIdMask;
    U32 sid = samplerId & resourceIdMask;

    if(
        (resourceId >> 18) != EResourceType_Texture3D || id >= MAX_READ_TEXTURES3D ||
        (samplerId >> 18) != EResourceType_Sampler || sid >= MAX_SAMPLERS
    )
        return defaultValue;

    SamplerState samp = _samplers[NURI ? NonUniformResourceIndex(sid) : sid];
    return _readTextures3D[NURI ? NonUniformResourceIndex(id) : id].SampleGrad(samp, loc, ddx, ddy);
}

F32x4 sampleGradCube(
    ResourceId resourceId, 
    ResourceId samplerId, 
    F32x3 dir,
    F32x2 ddx,
    F32x2 ddy,
    F32x4 defaultValue = 0.xxxx,
    bool NURI = false
) {

    U32 id = resourceId & resourceIdMask;
    U32 sid = samplerId & resourceIdMask;

    if(
        (resourceId >> 18) != EResourceType_TextureCube || id >= MAX_READ_TEXTURECUBES ||
        (samplerId >> 18) != EResourceType_Sampler || sid >= MAX_SAMPLERS
    )
        return defaultValue;

    SamplerState samp = _samplers[NURI ? NonUniformResourceIndex(sid) : sid];
    return _readTextures2D[NURI ? NonUniformResourceIndex(id) : id].SampleGrad(samp, dir, ddx, ddy);
}
   
F32x4 sampleLevel2D(ResourceId resourceId, ResourceId samplerId, F32x2 uv, F32 level, F32x4 defaultValue = 0.xxxx, bool NURI = false) {

    U32 id = resourceId & resourceIdMask;
    U32 sid = samplerId & resourceIdMask;

    if(
        (resourceId >> 18) != EResourceType_Texture2D || id >= MAX_READ_TEXTURES2D ||
        (samplerId >> 18) != EResourceType_Sampler || sid >= MAX_SAMPLERS
    )
        return defaultValue;

    SamplerState samp = _samplers[NURI ? NonUniformResourceIndex(sid) : sid];
    return _readTextures2D[NURI ? NonUniformResourceIndex(id) : id].Sample(samp, uv, level);
}

F32x4 sampleLevelCube(ResourceId resourceId, ResourceId samplerId, F32x3 dir, F32 level, F32x4 defaultValue = 0.xxxx, bool NURI = false) {

    U32 id = resourceId & resourceIdMask;
    U32 sid = samplerId & resourceIdMask;

    if(
        (resourceId >> 18) != EResourceType_Texture3D || id >= MAX_READ_TEXTURES3D ||
        (samplerId >> 18) != EResourceType_Sampler || sid >= MAX_SAMPLERS
    )
        return defaultValue;

    SamplerState samp = _samplers[NURI ? NonUniformResourceIndex(sid) : sid];
    return _readTexturesCube[NURI ? NonUniformResourceIndex(id) : id].Sample(samp, dir, level);
}

F32x4 sampleLevel3D(ResourceId resourceId, ResourceId samplerId, F32x3 location, F32 level, F32x4 defaultValue = 0.xxxx, bool NURI = false) {

    U32 id = resourceId & resourceIdMask;
    U32 sid = samplerId & resourceIdMask;

    if(
        (resourceId >> 18) != EResourceType_Texture3D || id >= MAX_READ_TEXTURES3D ||
        (samplerId >> 18) != EResourceType_Sampler || sid >= MAX_SAMPLERS
    )
        return defaultValue;

    SamplerState samp = _samplers[NURI ? NonUniformResourceIndex(sid) : sid];
    return _readTextures3D[NURI ? NonUniformResourceIndex(id) : id].Sample(samp, location, level);
}

#ifdef _GRAPHICS_SHADER

    //Sampling with uvDdx/uvDdy built-in for graphics pipelines

    //TODO: sampleBias
    
    F32x4 sample2D(ResourceId resourceId, ResourceId samplerId, F32x2 uv, F32x4 defaultValue = 0.xxxx, bool NURI = false) {

        U32 id = resourceId & resourceIdMask;
        U32 sid = samplerId & resourceIdMask;

        if(
            (resourceId >> 18) != EResourceType_Texture2D || id >= MAX_READ_TEXTURES2D ||
            (samplerId >> 18) != EResourceType_Sampler || sid >= MAX_SAMPLERS
        )
            return defaultValue;

        SamplerState samp = _samplers[NURI ? NonUniformResourceIndex(sid) : sid];
        return _readTextures2D[NURI ? NonUniformResourceIndex(id) : id].Sample(samp, uv);
    }
    
    F32x4 sampleCube(ResourceId resourceId, ResourceId samplerId, F32x3 dir, F32x4 defaultValue = 0.xxxx, bool NURI = false) {

        U32 id = resourceId & resourceIdMask;
        U32 sid = samplerId & resourceIdMask;

        if(
            (resourceId >> 18) != EResourceType_Texture3D || id >= MAX_READ_TEXTURES3D ||
            (samplerId >> 18) != EResourceType_Sampler || sid >= MAX_SAMPLERS
        )
            return defaultValue;

        SamplerState samp = _samplers[NURI ? NonUniformResourceIndex(sid) : sid];
        return _readTexturesCube[NURI ? NonUniformResourceIndex(id) : id].Sample(samp, dir);
    }
    
    F32x4 sample3D(ResourceId resourceId, ResourceId samplerId, F32x3 location, F32x4 defaultValue = 0.xxxx, bool NURI = false) {

        U32 id = resourceId & resourceIdMask;
        U32 sid = samplerId & resourceIdMask;

        if(
            (resourceId >> 18) != EResourceType_Texture3D || id >= MAX_READ_TEXTURES3D ||
            (samplerId >> 18) != EResourceType_Sampler || sid >= MAX_SAMPLERS
        )
            return defaultValue;

        SamplerState samp = _samplers[NURI ? NonUniformResourceIndex(sid) : sid];
        return _readTextures3D[NURI ? NonUniformResourceIndex(id) : id].Sample(samp, location);
    }

#endif
