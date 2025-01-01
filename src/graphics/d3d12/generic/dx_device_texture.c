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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_buffer.h"
#include "types/container/texture_format.h"
#include "types/container/ref_ptr.h"
#include "platforms/ext/bufferx.h"

Error DX_WRAP_FUNC(DeviceTextureRef_flush)(void *commandBufferExt, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending) {

	DxCommandBufferState *commandBuffer = (DxCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DeviceTexture *texture = DeviceTextureRef_ptr(pending);
	DxUnifiedTexture *textureExt = TextureRef_getCurrImgExtT(pending, Dx, 0);

	Error err = Error_none();

	ListRefPtr *currentFlight = &device->resourcesInFlight[(device->submitId - 1) % 3];
	DeviceBufferRef *tempStagingResource = NULL;

	ETextureFormat format = ETextureFormatId_unpack[texture->base.textureFormatId];
	Bool compressed = ETextureFormat_getIsCompressed(format);

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

		allocRange += siz;
	}

	device->pendingBytes += allocRange;

	D3D12_BARRIER_GROUP bufDep = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };
	D3D12_BARRIER_GROUP imgDep = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_TEXTURE };

	if (allocRange >= 16 * MIBI) {		//Resource is too big, allocate dedicated staging resource

		gotoIfError(clean, GraphicsDeviceRef_createBuffer(
			deviceRef,
			EDeviceBufferUsage_None, EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
			CharString_createRefCStrConst("Dedicated staging buffer"),
			allocRange, &tempStagingResource
		))

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

			const U64 len = rowLen * (TextureRange_height(texturej) / alignmentY) * TextureRange_length(texturej);
			const U64 start = (U64) rowLen * (y + z * h) + rowOff;

			if(w == texture->base.width && h == texture->base.height && rowLenUnalign == rowLen)
				Buffer_copy(
					Buffer_createRef(location + allocRange, rowLen * h * l),
					Buffer_createRefConst(texture->cpuData.ptr + start, rowLen * h * l)
				);

			else for(U64 k = z; k < z + l; ++k) {

				if(w == texture->base.width && rowLenUnalign == rowLen)
					Buffer_copy(
						Buffer_createRef(location + allocRange + rowLen * (k - z) * h, rowLen * h),
						Buffer_createRefConst(texture->cpuData.ptr + start + rowLen * (k - z) * h, rowLen * h)
					);

				else for (U64 j = y; j < y + h; j += alignmentY) {
					const U64 yOff = (j - y) / alignmentY;
					Buffer_copy(
						Buffer_createRef(location + allocRange + rowLen * (yOff + (k - z) * h), rowLen),
						Buffer_createRefConst(
							texture->cpuData.ptr + start + rowLenUnalign * (yOff + (k - z) * h), rowLenUnalign
						)
					);
				}
			}

			U64 allocRangeStart = allocRange;
			allocRange += len;

			gotoIfError(clean, DxDeviceBuffer_transition(
				stagingResourceExt,
				D3D12_BARRIER_SYNC_COPY,
				D3D12_BARRIER_ACCESS_COPY_SOURCE,
				&deviceExt->bufferTransitions,
				&bufDep
			))

			D3D12_BARRIER_SUBRESOURCE_RANGE range2 = (D3D12_BARRIER_SUBRESOURCE_RANGE) {
				.NumMipLevels = 1,
				.NumArraySlices = 1,
				.NumPlanes = 1
			};

			gotoIfError(clean, DxUnifiedTexture_transition(
				textureExt,
				D3D12_BARRIER_SYNC_COPY,
				D3D12_BARRIER_ACCESS_COPY_DEST,
				D3D12_BARRIER_LAYOUT_COPY_DEST,
				&range2,
				&deviceExt->imageTransitions,
				&imgDep
			))

			if(bufDep.NumBarriers || imgDep.NumBarriers) {

				D3D12_BARRIER_GROUP barriers[2];
				barriers[0] = bufDep;
				barriers[!!bufDep.NumBarriers] = imgDep;

				commandBuffer->buffer->lpVtbl->Barrier(
					commandBuffer->buffer,
					!!bufDep.NumBarriers + !!imgDep.NumBarriers,
					barriers
				);

				ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions);
				ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions);
			}

			D3D12_TEXTURE_COPY_LOCATION dst = (D3D12_TEXTURE_COPY_LOCATION) {
				.pResource = textureExt->image,
				.SubresourceIndex = z
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

			D3D12_BOX srcBox = (D3D12_BOX) {
				.left		= x,
				.top		= y,
				.front		= z,
				.right		= x + w,
				.bottom		= y + h,
				.back		= z + l
			};

			commandBuffer->buffer->lpVtbl->CopyTextureRegion(
				commandBuffer->buffer,
				&dst,
				x, y, z,
				&src,
				texture->isPendingFullCopy ? NULL : &srcBox		//Unaligned textures have a GPUBV bug in SDK version 613
			);
		}

		//When staging resource is committed to current in flight then we can relinquish ownership.

		gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, tempStagingResource))
		tempStagingResource = NULL;
	}

	//Use staging buffer

	else {

		AllocationBuffer *stagingBuffer = &device->stagingAllocations[(device->submitId - 1) % 3];
		DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
		DxDeviceBuffer *stagingExt = DeviceBuffer_ext(staging, Dx);

		U8 *defaultLocation = (U8*) 1, *location = defaultLocation;
		Error temp = AllocationBuffer_allocateBlockx(
			stagingBuffer, allocRange, compressed ? 16 : 4, (const U8**) &location
		);

		if(temp.genericError && location == defaultLocation)		//Something major went wrong
			gotoIfError(clean, temp)

		//We re-create the staging buffer to fit the new allocation.

		if (temp.genericError) {

			U64 prevSize = DeviceBufferRef_ptr(device->staging)->resource.size;

			//Allocate new staging buffer.

			U64 newSize = prevSize * 2 + allocRange * 3;
			gotoIfError(clean, GraphicsDeviceRef_resizeStagingBuffer(deviceRef, newSize))
			gotoIfError(clean, AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, (const U8**) &location))

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
			const U64 len = rowLen * (TextureRange_height(texturej) / alignmentY) * TextureRange_length(texturej);
			const U64 h2 = h / alignmentY;
			const U64 start = rowLen * (y + z * h2) + rowOff;

			if(w == texture->base.width && h == texture->base.height && rowLenUnalign == rowLen)
				Buffer_copy(
					Buffer_createRef(location + allocRange, rowLen * h2 * l),
					Buffer_createRefConst(texture->cpuData.ptr + start, rowLen * h2 * l)
				);

			else for(U64 k = z; k < z + l; ++k) {

				if(w == texture->base.width && rowLenUnalign == rowLen)
					Buffer_copy(
						Buffer_createRef(location + allocRange + rowLen * (k - z) * h2, rowLen * h2),
						Buffer_createRefConst(texture->cpuData.ptr + start + rowLen * (k - z) * h2, rowLen * h2)
					);

				else for (U64 j = y; j < y + h; j += alignmentY) {
					U64 yOff = (j - y) / alignmentY;
					Buffer_copy(
						Buffer_createRef(location + allocRange + rowLen * (yOff + (k - z) * h2), rowLen),
						Buffer_createRefConst(
							texture->cpuData.ptr + start + rowLenUnalign * (yOff + (k - z) * h2), rowLenUnalign
						)
					);
				}
			}

			if(!ListRefPtr_contains(*currentFlight, device->staging, 0, NULL)) {

				gotoIfError(clean, DxDeviceBuffer_transition(						//Ensure resource is transitioned
					stagingExt,
					D3D12_BARRIER_SYNC_COPY,
					D3D12_BARRIER_ACCESS_COPY_SOURCE,
					&deviceExt->bufferTransitions,
					&bufDep
				))

				RefPtr_inc(device->staging);
				gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, device->staging))		//Add to in flight
			}

			D3D12_BARRIER_SUBRESOURCE_RANGE range2 = (D3D12_BARRIER_SUBRESOURCE_RANGE) {
				.NumMipLevels = 1,
				.NumArraySlices = 1,
				.NumPlanes = 1
			};

			gotoIfError(clean, DxUnifiedTexture_transition(
				textureExt,
				D3D12_BARRIER_SYNC_COPY,
				D3D12_BARRIER_ACCESS_COPY_DEST,
				D3D12_BARRIER_LAYOUT_COPY_DEST,
				&range2,
				&deviceExt->imageTransitions,
				&imgDep
			))

			if(bufDep.NumBarriers || imgDep.NumBarriers) {

				D3D12_BARRIER_GROUP barriers[2];
				barriers[0] = bufDep;
				barriers[!!bufDep.NumBarriers] = imgDep;

				commandBuffer->buffer->lpVtbl->Barrier(
					commandBuffer->buffer,
					!!bufDep.NumBarriers + !!imgDep.NumBarriers,
					barriers
				);

				ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions);
				ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions);
			}

			U64 allocRangeStart = allocRange;
			allocRange += len;

			D3D12_TEXTURE_COPY_LOCATION dst = (D3D12_TEXTURE_COPY_LOCATION) {
				.pResource = textureExt->image,
				.SubresourceIndex = z
			};

			D3D12_TEXTURE_COPY_LOCATION src = (D3D12_TEXTURE_COPY_LOCATION) {
				.pResource = stagingExt->buffer,
				.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
				.PlacedFootprint = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT) {
					.Offset = allocRangeStart + (U64)(location - stagingBuffer->buffer.ptr),
					.Footprint = (D3D12_SUBRESOURCE_FOOTPRINT) {
						.Format = dxFormat,
						.Width = w,
						.Height = h,
						.Depth = l,
						.RowPitch = (U32) rowLen
					}
				}
			};

			D3D12_BOX srcBox = (D3D12_BOX) {
				.left		= x,
				.top		= y,
				.front		= z,
				.right		= x + w,
				.bottom		= y + h,
				.back		= z + l
			};

			commandBuffer->buffer->lpVtbl->CopyTextureRegion(
				commandBuffer->buffer,
				&dst,
				x, y, z,
				&src,
				texture->isPendingFullCopy ? NULL : &srcBox		//Unaligned textures have a GPUBV bug in SDK version 613
			);
		}
	}

	if(!(texture->base.resource.flags & EGraphicsResourceFlag_CPUBacked))
		Buffer_freex(&texture->cpuData);

	texture->isFirstFrame = texture->isPending = texture->isPendingFullCopy = false;
	gotoIfError(clean, ListDevicePendingRange_clear(&texture->pendingChanges))

	if(RefPtr_inc(pending))
		gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, pending))

	if (device->pendingBytes >= device->flushThreshold)
		gotoIfError(clean, DxGraphicsDevice_flush(deviceRef, commandBuffer))

clean:
	DeviceBufferRef_dec(&tempStagingResource);
	ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions);
	ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions);
	return err;
}
