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

//graphics/generic/texture.h

#pragma once
#include "graphics/generic/resource.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EMSAASamples EMSAASamples;

typedef struct CharString CharString;
typedef RefPtr GraphicsDeviceRef;
typedef RefPtr DescriptorTableRef;

typedef U32 BindlessDescriptor;

typedef struct UnifiedTextureImage {
	BindlessDescriptor readHandle, writeHandle;
	U64 padding;
} UnifiedTextureImage;

//UnifiedTexture (GraphicsResource, etc.), UnifiedTextureImage[N], UnifiedTextureImageExt_size[N]
typedef struct UnifiedTexture {                //Base texture definition, should be at end of struct! (No padding after!)

	GraphicsResource resource;

	U8 textureFormatId;                        //ETextureFormatId, Undefined for DepthStencil
	U8 sampleCount;                            //EMSAASamples
	U8 depthFormat;                            //Only accessible for DepthStencil, otherwise None
	U8 type;                                   //ETextureType

	U16 width, height, length;
	U8 levels, images;

	U8 padding[10];
	U8 maxImages;                              //If 0, uses images, otherwise the amount of images to the next struct
	U8 currentImageId;

	DescriptorTableRef *bindlessDescriptorTable;

} UnifiedTexture;

typedef RefPtr TextureRef;                     //DeviceTexture, RenderTexture, DepthStencil, Swapchain

//TODO: Ability to query allocation size (inc alignment)

//size and formatId can change at resize time in Swapchain, which means it has to be locked before checking.
//This can be ensured by calling getUnifiedTexture with DeviceResourceVersion pointing to an object.
//If version->resource isn't NULL it signifies that the resource is resizable and
// the commandList can't be re-submitted if the versionId has changed.
UnifiedTexture TextureRef_getUnifiedTexture(TextureRef *tex, DeviceResourceVersion *version);

//If the texture ref isn't a swapchain, you can lockfree/copyfree get the info.
const UnifiedTexture *TextureRef_getUnifiedTextureFast(TextureRef *tex);

U32 TextureRef_getReadHandle(TextureRef *tex, U32 subResource, U8 imageId);
U32 TextureRef_getWriteHandle(TextureRef *tex, U32 subResource, U8 imageId);

Bool TextureRef_isRenderTargetWritable(TextureRef *tex);
Bool TextureRef_isDepthStencil(TextureRef *tex);
Bool TextureRef_isTexture(RefPtr *tex);

//Read a region of a render target back from the GPU (RenderTexture, DepthStencil or Swapchain).
//These have no cpuData, so the data arrives through the callback as a tight row buffer it may take over.
//Same timing contract and region semantics as DeviceTextureRef_pullRegion.
//MSAA has to be resolved before pulling.
//plane selects what a multi planar format hands back, one plane per pull: 0 is the first plane (depth, or
// the stencil of a stencil only format), 1 is the stencil of a combined format (1 byte per texel).
//Anything single planar only accepts plane 0.
Bool TextureRef_pullRegion(
	TextureRef *tex,
	U16 x, U16 y, U16 z,
	U16 w, U16 h, U16 l,
	U8 plane,
	TexturePullCallback callback, void *context, Error *e_rr
);

UnifiedTextureImage TextureRef_getImage(TextureRef *tex, U32 subResource, U8 imageId);

U32 TextureRef_getCurrReadHandle(TextureRef *tex, U32 subResource);
U32 TextureRef_getCurrWriteHandle(TextureRef *tex, U32 subResource);

//Only for child classes

void UnifiedTexture_free(TextureRef *textureRef);
Bool UnifiedTexture_create(TextureRef *ref, DescriptorTableRef *bindlessDescriptorTable, const CharString *name, Error *e_rr);

//Internal (only use inside GraphicsDeviceRef_submitCommands)

UnifiedTextureImage TextureRef_getCurrImage(TextureRef *tex, U32 subResource);

void *TextureRef_getImplExt(TextureRef *ref);
#define TextureRef_getImplExtT(T, t) ((T*) TextureRef_getImplExt(t))

void *TextureRef_getImgExt(TextureRef *ref, U32 subResource, U8 imageId);
#define TextureRef_getImgExtT(t, prefix, subResource, imageId) \
	((prefix##UnifiedTexture*) TextureRef_getImgExt(t, subResource, imageId))

void *TextureRef_getCurrImgExt(TextureRef *ref, U32 subResource);
#define TextureRef_getCurrImgExtT(t, prefix, subResource) \
	((prefix##UnifiedTexture*) TextureRef_getCurrImgExt(t, subResource))

#ifdef __cplusplus
	}
#endif
