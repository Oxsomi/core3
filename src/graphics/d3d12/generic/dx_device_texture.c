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

//graphics/d3d12/generic/dx_device_texture.c

#include "graphics/generic/device_texture.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_buffer.h"
#include "types/container/texture_format.h"
#include "types/container/ref_ptr.h"
#include "types/container/buffer.h"
#include "types/base/string_base.h"

Bool DX_WRAP_FUNC(DeviceTextureRef_flush)(
	void *commandBufferExt,
	GraphicsDeviceRef *deviceRef,
	DeviceTextureRef *pending,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	DxCommandBufferState *commandBuffer = (DxCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DeviceTexture *texture = DeviceTextureRef_ptr(pending);
	DxUnifiedTexture *textureExt = TextureRef_getCurrImgExtT(pending, Dx, 0);

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];
	DeviceBufferRef *tempStagingResource = NULL;

	ETextureFormat format = ETextureFormatId_unpack[texture->base.textureFormatId];

	DXGI_FORMAT dxFormat = ETextureFormatId_toDXFormat(texture->base.textureFormatId);

	//TODO: Copy queue

	U64 allocRange = 0;

	U8 alignmentX = 1, alignmentY = 1;
	ETextureFormat_getAlignment(format, &alignmentX, &alignmentY);

	//Calculate allocation range.
	//We need to align to the row pitch

	for(U64 j = 0; j < texture->pendingChanges.length; ++j) {

		const TextureRange texturej = texture->pendingChanges.ptr[j].texture;

		U64 siz = ETextureFormat_getSize(format, TextureRange_width(texturej), alignmentY, 1);
		siz = (siz + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) &~ (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

		siz *= (TextureRange_height(texturej) / alignmentY) * TextureRange_length(texturej);

		//Every region begins at a placed footprint offset, which D3D12 requires to be 512 byte aligned

		allocRange += (siz + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) &~ (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);
	}

	device->pendingBytes += allocRange;

	D3D12_BARRIER_GROUP bufDep = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };
	D3D12_BARRIER_GROUP imgDep = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_TEXTURE };

	if (allocRange >= DeviceBufferRef_ptr(device->staging)->resource.size / 4) {

		CharString dedicatedStaging = CharString_createRefCStrConst("Dedicated staging buffer");

		gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
			deviceRef,
			EDeviceBufferUsage_None, EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
			NULL,
			&dedicatedStaging,
			allocRange, &tempStagingResource, e_rr
		));

		DeviceBuffer *stagingResource = DeviceBufferRef_ptr(tempStagingResource);
		DxDeviceBuffer *stagingResourceExt = DeviceBuffer_ext(stagingResource, Dx);
		U8 *location = stagingResource->resource.mappedMemoryExt;

		//Copy into our buffer

		allocRange = 0;

		for(U64 m = 0; m < texture->pendingChanges.length; ++m) {

			const TextureRange texturej = texture->pendingChanges.ptr[m].texture;

			const U16 x = texturej.startRange[0];
			const U16 y = texturej.startRange[1];
			const U16 z = texturej.startRange[2];

			const U16 w = TextureRange_width(texturej);
			const U16 h = TextureRange_height(texturej);
			const U16 l = TextureRange_length(texturej);

			U64 rowLen = ETextureFormat_getSize(format, w, alignmentY, 1);
			U64 rowLenUnalign = rowLen;

			rowLen = (rowLen + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) &~ (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

			const U64 rowOff = ETextureFormat_getSize(format, x, alignmentY, 1);

			const U64 h2 = (h + alignmentY - 1) / alignmentY;
			const U64 len = rowLen * h2 * l;

			//cpuData is the tight full texture, so reading it strides by the full row length and slice height;
			// the 256 aligned pitch only applies to the staging destination

			const U64 fullRowLen = ETextureFormat_getSize(format, texture->base.width, alignmentY, 1);
			const U64 fullH2 = ((U64)texture->base.height + alignmentY - 1) / alignmentY;
			const U64 start = fullRowLen * (y / alignmentY + z * fullH2) + rowOff;

			if(w == texture->base.width && h == texture->base.height && rowLenUnalign == rowLen)
				Buffer_memcpy(
					Buffer_createRef(location + allocRange, rowLen * h2 * l),
					Buffer_createRefConst(texture->cpuData.ptr + start, rowLen * h2 * l)
				);

			else for(U64 k = z; k < z + l; ++k) {

				if(w == texture->base.width && rowLenUnalign == rowLen)
					Buffer_memcpy(
						Buffer_createRef(location + allocRange + rowLen * (k - z) * h2, rowLen * h2),
						Buffer_createRefConst(texture->cpuData.ptr + start + fullRowLen * (k - z) * fullH2, rowLen * h2)
					);

				else for (U64 j = y; j < y + h; j += alignmentY) {
					const U64 yOff = (j - y) / alignmentY;
					Buffer_memcpy(
						Buffer_createRef(location + allocRange + rowLen * (yOff + (k - z) * h2), rowLen),
						Buffer_createRefConst(
							texture->cpuData.ptr + start + fullRowLen * (yOff + (k - z) * fullH2), rowLenUnalign
						)
					);
				}
			}

			U64 allocRangeStart = allocRange;
			allocRange += (len + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) &~ (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

			gotoIfError3(clean, DxDeviceBuffer_transition(
				stagingResourceExt,
				D3D12_BARRIER_SYNC_COPY,
				D3D12_BARRIER_ACCESS_COPY_SOURCE,
				&deviceExt->bufferTransitions,
				&bufDep, alloc, e_rr
			));

			D3D12_BARRIER_SUBRESOURCE_RANGE range2 = (D3D12_BARRIER_SUBRESOURCE_RANGE) {
				.NumMipLevels = 1,
				.NumArraySlices = 1,
				.NumPlanes = 1
			};

			gotoIfError3(clean, DxUnifiedTexture_transition(
				textureExt,
				D3D12_BARRIER_SYNC_COPY,
				D3D12_BARRIER_ACCESS_COPY_DEST,
				D3D12_BARRIER_LAYOUT_COPY_DEST,
				&range2,
				&deviceExt->imageTransitions,
				&imgDep, alloc, e_rr
			));

			if(bufDep.NumBarriers || imgDep.NumBarriers) {

				D3D12_BARRIER_GROUP barriers[2];
				barriers[0] = bufDep;
				barriers[!!bufDep.NumBarriers] = imgDep;

				commandBuffer->buffer->lpVtbl->Barrier(
					commandBuffer->buffer,
					!!bufDep.NumBarriers + !!imgDep.NumBarriers,
					barriers
				);

				ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions, e_rr);
				ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);
			}

			//The z coordinate is passed through DstZ; the subresource is always mip 0 of layer 0

			D3D12_TEXTURE_COPY_LOCATION dst = (D3D12_TEXTURE_COPY_LOCATION) {
				.pResource = textureExt->image,
				.SubresourceIndex = 0
			};

			D3D12_TEXTURE_COPY_LOCATION src = (D3D12_TEXTURE_COPY_LOCATION) {
				.pResource = stagingResourceExt->buffer,
				.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
				.PlacedFootprint = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT) {
					.Offset = allocRangeStart,
					.Footprint = (D3D12_SUBRESOURCE_FOOTPRINT) {
						.Format = dxFormat,
						.Width = w,
						.Height = h,
						.Depth = l,
						.RowPitch = (U32) rowLen
					}
				}
			};

			//The box selects from the source footprint, which spans exactly the region, so it starts at zero

			D3D12_BOX srcBox = (D3D12_BOX) {
				.right  = w,
				.bottom = h,
				.back   = l
			};

			commandBuffer->buffer->lpVtbl->CopyTextureRegion(
				commandBuffer->buffer,
				&dst,
				x, y, z,
				&src,
				texture->isPendingFullCopy ? NULL : &srcBox        //Unaligned textures have a GPUBV bug in SDK version 613
			);
		}

		//When staging resource is committed to current in flight then we can relinquish ownership.

		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, tempStagingResource, alloc, e_rr));
		tempStagingResource = NULL;
	}

	//Use staging buffer

	else {

		AllocationBuffer *stagingBuffer = &device->stagingAllocations[device->fifId];
		DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
		DxDeviceBuffer *stagingExt = DeviceBuffer_ext(staging, Dx);

		U8 *defaultLocation = (U8*) 1, *location = defaultLocation;

		AllocationBufferAllocate allocBuffer = (AllocationBufferAllocate) {
			.allocationBuffer = stagingBuffer,
			.alignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT,        //Placed footprint offsets must be 512 aligned
			.alloc = alloc
		};

		Error temp = Error_none();
		Bool allocated = AllocationBuffer_allocateBlock(&allocBuffer, allocRange, (const U8**) &location, &temp);

		if(!allocated && location == defaultLocation) {        //Something major went wrong
			if(e_rr) *e_rr = temp;
			s_uccess = false;
			goto clean;
		}

		//We re-create the staging buffer to fit the new allocation.

		if (!allocated) {

			U64 prevSize = DeviceBufferRef_ptr(device->staging)->resource.size;

			//Allocate new staging buffer.

			U64 newSize = prevSize * 2 + allocRange * 3;
			gotoIfError3(clean, GraphicsDeviceRef_resizeStagingBuffer(deviceRef, newSize, e_rr));

			allocBuffer = (AllocationBufferAllocate) {
				.allocationBuffer = stagingBuffer,
				.alignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT,        //Placed footprint offsets must be 512 aligned
				.alloc = alloc
			};

			gotoIfError3(clean, AllocationBuffer_allocateBlock(&allocBuffer, allocRange, (const U8**) &location, e_rr));

			staging = DeviceBufferRef_ptr(device->staging);
			stagingExt = DeviceBuffer_ext(staging, Dx);
		}

		//Copy into our buffer

		allocRange = 0;

		for(U64 m = 0; m < texture->pendingChanges.length; ++m) {

			const TextureRange texturej = texture->pendingChanges.ptr[m].texture;

			const U16 x = texturej.startRange[0];
			const U16 y = texturej.startRange[1];
			const U16 z = texturej.startRange[2];

			const U16 w = TextureRange_width(texturej);
			const U16 h = TextureRange_height(texturej);
			const U16 l = TextureRange_length(texturej);

			U64 rowLen = ETextureFormat_getSize(format, w, alignmentY, 1);
			const U64 rowLenUnalign = rowLen;

			rowLen = (rowLen + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) &~ (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

			U64 rowOff = ETextureFormat_getSize(format, x, alignmentY, 1);
			const U64 h2 = (h + alignmentY - 1) / alignmentY;
			const U64 len = rowLen * h2 * l;

			//cpuData is the tight full texture, so reading it strides by the full row length and slice height;
			// the 256 aligned pitch only applies to the staging destination

			const U64 fullRowLen = ETextureFormat_getSize(format, texture->base.width, alignmentY, 1);
			const U64 fullH2 = ((U64)texture->base.height + alignmentY - 1) / alignmentY;
			const U64 start = fullRowLen * (y / alignmentY + z * fullH2) + rowOff;

			if(w == texture->base.width && h == texture->base.height && rowLenUnalign == rowLen)
				Buffer_memcpy(
					Buffer_createRef(location + allocRange, rowLen * h2 * l),
					Buffer_createRefConst(texture->cpuData.ptr + start, rowLen * h2 * l)
				);

			else for(U64 k = z; k < z + l; ++k) {

				if(w == texture->base.width && rowLenUnalign == rowLen)
					Buffer_memcpy(
						Buffer_createRef(location + allocRange + rowLen * (k - z) * h2, rowLen * h2),
						Buffer_createRefConst(texture->cpuData.ptr + start + fullRowLen * (k - z) * fullH2, rowLen * h2)
					);

				else for (U64 j = y; j < y + h; j += alignmentY) {
					U64 yOff = (j - y) / alignmentY;
					Buffer_memcpy(
						Buffer_createRef(location + allocRange + rowLen * (yOff + (k - z) * h2), rowLen),
						Buffer_createRefConst(
							texture->cpuData.ptr + start + fullRowLen * (yOff + (k - z) * fullH2), rowLenUnalign
						)
					);
				}
			}

			if(!ListRefPtr_contains(*currentFlight, device->staging, 0, NULL)) {

				gotoIfError3(clean, DxDeviceBuffer_transition(                        //Ensure resource is transitioned
					stagingExt,
					D3D12_BARRIER_SYNC_COPY,
					D3D12_BARRIER_ACCESS_COPY_SOURCE,
					&deviceExt->bufferTransitions,
					&bufDep, alloc, e_rr));

				RefPtr_inc(device->staging);
				gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, device->staging, alloc, e_rr));        //Add to in flight
			}

			D3D12_BARRIER_SUBRESOURCE_RANGE range2 = (D3D12_BARRIER_SUBRESOURCE_RANGE) {
				.NumMipLevels = 1,
				.NumArraySlices = 1,
				.NumPlanes = 1
			};

			gotoIfError3(clean, DxUnifiedTexture_transition(
				textureExt,
				D3D12_BARRIER_SYNC_COPY,
				D3D12_BARRIER_ACCESS_COPY_DEST,
				D3D12_BARRIER_LAYOUT_COPY_DEST,
				&range2,
				&deviceExt->imageTransitions,
				&imgDep, alloc, e_rr));

			if(bufDep.NumBarriers || imgDep.NumBarriers) {

				D3D12_BARRIER_GROUP barriers[2];
				barriers[0] = bufDep;
				barriers[!!bufDep.NumBarriers] = imgDep;

				commandBuffer->buffer->lpVtbl->Barrier(
					commandBuffer->buffer,
					!!bufDep.NumBarriers + !!imgDep.NumBarriers,
					barriers
				);

				ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions, e_rr);
				ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);
			}

			U64 allocRangeStart = allocRange;
			allocRange += (len + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) &~ (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

			//The z coordinate is passed through DstZ; the subresource is always mip 0 of layer 0

			D3D12_TEXTURE_COPY_LOCATION dst = (D3D12_TEXTURE_COPY_LOCATION) {
				.pResource = textureExt->image,
				.SubresourceIndex = 0
			};

			D3D12_TEXTURE_COPY_LOCATION src = (D3D12_TEXTURE_COPY_LOCATION) {
				.pResource = stagingExt->buffer,
				.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
				.PlacedFootprint = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT) {
					//Relative to the staging resource rather than the frame region, since the footprint is resource based
					.Offset = allocRangeStart + (U64)(location - (const U8*)staging->resource.mappedMemoryExt),
					.Footprint = (D3D12_SUBRESOURCE_FOOTPRINT) {
						.Format = dxFormat,
						.Width = w,
						.Height = h,
						.Depth = l,
						.RowPitch = (U32) rowLen
					}
				}
			};

			//The box selects from the source footprint, which spans exactly the region, so it starts at zero

			D3D12_BOX srcBox = (D3D12_BOX) {
				.right  = w,
				.bottom = h,
				.back   = l
			};

			commandBuffer->buffer->lpVtbl->CopyTextureRegion(
				commandBuffer->buffer,
				&dst,
				x, y, z,
				&src,
				texture->isPendingFullCopy ? NULL : &srcBox        //Unaligned textures have a GPUBV bug in SDK version 613
			);
		}
	}

	if(!(texture->base.resource.flags & EGraphicsResourceFlag_CPUBacked))
		Buffer_free(&texture->cpuData, alloc);

	texture->isFirstFrame = texture->isPending = texture->isPendingFullCopy = false;
	gotoIfError3(clean, ListDevicePendingRange_clear(&texture->pendingChanges, e_rr));

	if(RefPtr_inc(pending))
		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, pending, alloc, e_rr));

	if (device->pendingBytes >= device->flushThreshold)
		gotoIfError3(clean, DxGraphicsDevice_flush(deviceRef, commandBuffer, e_rr));

clean:
	RefPtr_dec(&tempStagingResource);
	ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions, e_rr);
	ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);
	return s_uccess;
}

Bool DX_WRAP_FUNC(DeviceTextureRef_pull)(
	void *commandBufferExt,
	GraphicsDeviceRef *deviceRef,
	DeviceTextureRef *resource,
	const TextureRange *range,
	U64 stagingOffset,
	U64 *rowPitch,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	DxCommandBufferState *commandBuffer = (DxCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	//Any texture type can be pulled, so only the unified base is safe to use here

	const UnifiedTexture utex = TextureRef_getUnifiedTexture(resource, NULL);
	DxUnifiedTexture *textureExt = TextureRef_getCurrImgExtT(resource, Dx, 0);
	DxDeviceBuffer *stagingExt = DeviceBuffer_ext(DeviceBufferRef_ptr(device->stagingReadback), Dx);

	D3D12_BARRIER_GROUP bufDep = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };
	D3D12_BARRIER_GROUP imgDep = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_TEXTURE };

	gotoIfError3(clean, DxDeviceBuffer_transition(
		stagingExt, D3D12_BARRIER_SYNC_COPY, D3D12_BARRIER_ACCESS_COPY_DEST,
		&deviceExt->bufferTransitions, &bufDep, alloc, e_rr
	));

	//The range says which plane rides this pull.
	//Subresource indexing puts plane 1 right after plane 0 for a single mip single slice texture,
	// and a PLANE footprint has to use the plane's own format rather than the combined one:
	// R8_TYPELESS for stencil, R32_TYPELESS for a 32 bit depth plane, R24_UNORM_X8_TYPELESS for D24's,
	// matching how the runtime exposes the planes.
	//The stencil only format never reaches D3D12, since EDepthStencilFormat_toDXFormat maps it to 0 and the
	// texture can't be created in the first place.

	const EDepthStencilFormat dsFormat = (EDepthStencilFormat) utex.depthFormat;
	const U8 pullPlane = (U8) range->planeId;
	const Bool pullsStencil = utex.depthFormat && pullPlane == 1;
	const Bool isCombined = utex.depthFormat && EDepthStencilFormat_hasStencil(dsFormat) && EDepthStencilFormat_hasDepth(dsFormat);

	//The barrier covers every plane the image has, matching the engine's single tracked layout: a stencil
	// pull copies plane 1, and a barrier naming only plane 0 would leave it in its old layout, which the
	// runtime rejects at ExecuteCommandLists.

	D3D12_BARRIER_SUBRESOURCE_RANGE subresourceRange = (D3D12_BARRIER_SUBRESOURCE_RANGE) {
		.NumMipLevels = 1,
		.NumArraySlices = 1,
		.NumPlanes = isCombined ? 2 : 1
	};

	gotoIfError3(clean, DxUnifiedTexture_transition(
		textureExt,
		D3D12_BARRIER_SYNC_COPY,
		D3D12_BARRIER_ACCESS_COPY_SOURCE,
		D3D12_BARRIER_LAYOUT_COPY_SOURCE,
		&subresourceRange,
		&deviceExt->imageTransitions,
		&imgDep, alloc, e_rr
	));

	if(bufDep.NumBarriers || imgDep.NumBarriers) {

		D3D12_BARRIER_GROUP barriers[2];
		barriers[0] = bufDep;
		barriers[!!bufDep.NumBarriers] = imgDep;

		commandBuffer->buffer->lpVtbl->Barrier(
			commandBuffer->buffer,
			!!bufDep.NumBarriers + !!imgDep.NumBarriers,
			barriers
		);

		ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions, e_rr);
		ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);
	}

	const DXGI_FORMAT dxFormat =
		pullsStencil ? DXGI_FORMAT_R8_TYPELESS :
		isCombined ? (
			dsFormat == EDepthStencilFormat_D24S8Ext ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_R32_TYPELESS
		) :
		utex.depthFormat ? (DXGI_FORMAT) EDepthStencilFormat_toDXFormat(dsFormat) :
		ETextureFormatId_toDXFormat(utex.textureFormatId);

	U8 alignX = 1, alignY = 1;

	if(!utex.depthFormat) {
		const ETextureFormat format = ETextureFormatId_unpack[utex.textureFormatId];
		ETextureFormat_getAlignment(format, &alignX, &alignY);
	}

	const U16 x = range->startRange[0];
	const U16 y = range->startRange[1];
	const U16 z = range->startRange[2];

	const U16 w = TextureRange_width(*range);
	const U16 h = TextureRange_height(*range);
	const U16 l = TextureRange_length(*range);

	const D3D12_TEXTURE_COPY_LOCATION src = (D3D12_TEXTURE_COPY_LOCATION) {
		.pResource = textureExt->image,
		.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
		.SubresourceIndex = pullPlane
	};

	//The footprint dimensions have to stay block multiples for compressed formats, even at unaligned edges

	const D3D12_TEXTURE_COPY_LOCATION dst = (D3D12_TEXTURE_COPY_LOCATION) {
		.pResource = stagingExt->buffer,
		.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
		.PlacedFootprint = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT) {
			.Offset = stagingOffset,
			.Footprint = (D3D12_SUBRESOURCE_FOOTPRINT) {
				.Format = dxFormat,
				.Width = (w + alignX - 1) / alignX * alignX,
				.Height = (h + alignY - 1) / alignY * alignY,
				.Depth = l,
				.RowPitch = (U32) *rowPitch
			}
		}
	};

	//The box is in texture coordinates on the read side, since the source is the texture itself

	D3D12_BOX srcBox = (D3D12_BOX) {
		.left   = x,
		.top    = y,
		.front  = z,
		.right  = (U32) x + w,
		.bottom = (U32) y + h,
		.back   = (U32) z + l
	};

	commandBuffer->buffer->lpVtbl->CopyTextureRegion(commandBuffer->buffer, &dst, 0, 0, 0, &src, &srcBox);

clean:
	return s_uccess;
}
