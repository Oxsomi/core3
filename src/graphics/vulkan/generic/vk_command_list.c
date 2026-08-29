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

//graphics/vulkan/generic/vk_command_list.c

#include "graphics/vulkan/vk_interface.h"
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/blas.h"
#include "platforms/logx.h"
#include "formats/oiSH/sh_registers.h"
#include "types/base/buffer_base.h"
#include "types/base/error.h"
#include "types/base/constants.h"

static inline Bool addResolveImage(AttachmentInfoInternal attachment, VkRenderingAttachmentInfoKHR *result) {

	VkUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(attachment.resolveImage, Vk, 0);

	switch (attachment.resolveMode) {
		default:                    result->resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;  break;
		case EMSAAResolveMode_Min:  result->resolveMode = VK_RESOLVE_MODE_MIN_BIT;      break;
		case EMSAAResolveMode_Max:  result->resolveMode = VK_RESOLVE_MODE_MAX_BIT;      break;
	}

	U32 viewId = U32_MAX;
	VkImageView view = NULL;

	Descriptor descriptor = (Descriptor) { .resource = attachment.resolveImage };
	ESHRegisterType registerType = ESHRegisterType_Texture2D;        //TODO: Add support for other resources

	if (!VkUnifiedTexture_getView(descriptor, registerType, &view, &viewId, NULL))
		return false;

	result->resolveImageView = view;
	result->resolveImageLayout = imageExt->lastLayout;
	return true;
}

//Bindful: descriptors are emitted lazily at the work op, where the pipeline (and so the layout) is
// known; binds only set state.
//A custom layout emits the bound table's sets against that layout and marks the default sets dirty, since
// binding with an incompatible layout invalidates them; a default layout pipeline then rebinds them once.

Bool GraphicsDevice_rebindDescriptors(GraphicsDevice *device, VkCommandBuffer commandBuffer, Error *e_rr);

static void VkCommandBufferState_bindDescriptors(
	VkCommandBufferState *temp,
	VkGraphicsDevice *deviceExt,
	GraphicsDevice *device,
	PipelineRef *pipelineRef,
	VkPipelineBindPoint bindPoint
) {

	PipelineLayoutRef *layoutRef = PipelineRef_ptr(pipelineRef)->layout;

	const U8 bindPointId =
		bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE ? 0 : (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ? 1 : 2);

	if (layoutRef == device->defaultPipelineLayout) {

		if (!temp->defaultDescriptorsBound) {

			temp->defaultDescriptorsBound = true;
			GraphicsDevice_rebindDescriptors(device, temp->buffer, NULL);

			for(U8 i = 0; i < 3; ++i) {
				temp->lastBoundTable[i] = NULL;
				temp->lastBoundLayout[i] = VK_NULL_HANDLE;
			}
		}

		return;
	}

	const PipelineLayout *layout = PipelineLayoutRef_ptr(layoutRef);
	VkPipelineLayout *layoutExt = PipelineLayout_ext((PipelineLayout*)layout, Vk);

	//Push constants belong to the layout, so a layout switch drops them; this is the one place that knows
	// both the layout and the bind point, which is why they go out from here rather than at the write.
	//Emitted before the table work below, which returns early for a layout that has push constants but no
	// bindings at all.

	if (
		temp->pushConstantSize && layout->info.pushConstants.count &&
		(!temp->pushConstantsEmitted[bindPointId] || temp->lastPushLayout[bindPointId] != *layoutExt)
	) {

		deviceExt->cmdPushConstants(
			temp->buffer,
			*layoutExt,
			vkGetShaderStagesDevice(device, layout->info.pushConstants.visibility),
			0,
			temp->pushConstantSize,
			temp->pushConstantData
		);

		temp->pushConstantsEmitted[bindPointId] = true;
		temp->lastPushLayout[bindPointId] = *layoutExt;
	}

	//A custom layout whose bindings are the device's own bindless set gets that table's sets bound against
	// ITS layout; the sets come from the same DescriptorLayout, so they sit at the same indices.

	if (PipelineLayout_usesRuntimeBindless(layout) && temp->lastBoundLayout[bindPointId] != *layoutExt) {

		VkDescriptorTable *bindlessExt =
			DescriptorTable_ext(DescriptorTableRef_ptr(device->defaultDescriptorTable), Vk);

		for (U64 j = 0, k = 0; j < bindlessExt->bindCommands; ++j) {

			deviceExt->cmdBindDescriptorSets(
				temp->buffer, bindPoint, *layoutExt,
				bindlessExt->offsets[j], bindlessExt->counts[j], &bindlessExt->sets[k],
				0, NULL
			);

			k += bindlessExt->counts[j];
		}

		temp->lastBoundLayout[bindPointId] = *layoutExt;
		temp->lastBoundTable[bindPointId] = NULL;

		//Binding against a custom layout disturbs what the default layout had bound.

		temp->defaultDescriptorsBound = false;
	}

	//A custom layout that declares OxC3's per frame globals gets them pushed here: the default layout's own
	// bind never runs for it, so _frameId/_time would otherwise be whatever the set last held.
	//Tracked with the same pair as the caller's push descriptors, since a layout carries one or the other.

	if (PipelineLayout_hasRuntimeGlobals(layout) && (
		!temp->pushDescriptorsEmitted[bindPointId] || temp->lastPushDescLayout[bindPointId] != *layoutExt
	)) {

		U32 globalsSet = 0;

		if (layout->info.bindings) {

			VkDescriptorLayout *bindExt = DescriptorLayout_ext(DescriptorLayoutRef_ptr(layout->info.bindings), Vk);

			while(globalsSet < 4 && bindExt->layouts[globalsSet])
				++globalsSet;
		}

		DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData[device->fifId]);

		//Without VK_KHR_push_descriptor the device emulates it with one set per frame in flight, already
		// written to that frame's globals buffer, so the emulated path binds it instead of pushing.

		if (deviceExt->cmdPushDescriptorSet) {

			const VkDescriptorBufferInfo bufferInfo = (VkDescriptorBufferInfo) {
				.buffer = DeviceBuffer_ext(frameData, Vk)->buffer,
				.offset = 0,
				.range = frameData->resource.size
			};

			const VkWriteDescriptorSet cbv = (VkWriteDescriptorSet) {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo
			};

			deviceExt->cmdPushDescriptorSet(temp->buffer, bindPoint, *layoutExt, globalsSet, 1, &cbv);
		}

		else deviceExt->cmdBindDescriptorSets(
			temp->buffer, bindPoint, *layoutExt, globalsSet, 1, &deviceExt->cbufferSets[device->fifId], 0, NULL
		);

		temp->pushDescriptorsEmitted[bindPointId] = true;
		temp->lastPushDescLayout[bindPointId] = *layoutExt;
	}

	//Push descriptors go out here for the same reasons, and buffer class only for the same reason D3D12 is:
	// createDescriptorLayout refuses anything else, so the writes below only ever describe buffers and RTASes.

	if (temp->pushDescriptorCount && layout->info.pushDescriptors && (
		!temp->pushDescriptorsEmitted[bindPointId] || temp->lastPushDescLayout[bindPointId] != *layoutExt
	)) {

		const DescriptorLayout *pushLayout = DescriptorLayoutRef_ptr(layout->info.pushDescriptors);

		//The push set sits after whatever sets the ordinary bindings occupy, which is exactly how
		// vk_pipeline_layout.c stacked them

		U32 pushSet = 0;

		if (layout->info.bindings) {

			VkDescriptorLayout *bindExt = DescriptorLayout_ext(DescriptorLayoutRef_ptr(layout->info.bindings), Vk);

			while(pushSet < 4 && bindExt->layouts[pushSet])
				++pushSet;
		}

		VkWriteDescriptorSet writes[OXC3_MAX_PUSH_DESCRIPTORS];
		VkDescriptorBufferInfo buffers[OXC3_MAX_PUSH_DESCRIPTORS];
		VkWriteDescriptorSetAccelerationStructureKHR accelerations[OXC3_MAX_PUSH_DESCRIPTORS];
		VkAccelerationStructureKHR handles[OXC3_MAX_PUSH_DESCRIPTORS];

		U32 writeCount = 0;

		for (U8 i = 0; i < temp->pushDescriptorCount && i < pushLayout->info.bindings.length; ++i) {

			Descriptor d = temp->pushDescriptors[i];
			const DescriptorBinding binding = pushLayout->info.bindings.ptr[i];
			const ESHRegisterType type = (ESHRegisterType)(binding.registerType & ESHRegisterType_TypeMask);

			writes[writeCount] = (VkWriteDescriptorSet) {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstBinding = binding.binding.binding,
				.descriptorCount = 1
			};

			if (type == ESHRegisterType_AccelerationStructure) {

				handles[writeCount] = TLAS_ext(TLASRef_ptr(d.resource), Vk)->as;

				accelerations[writeCount] = (VkWriteDescriptorSetAccelerationStructureKHR) {
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
					.accelerationStructureCount = 1,
					.pAccelerationStructures = &handles[writeCount]
				};

				writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
				writes[writeCount].pNext = &accelerations[writeCount];
			}

			else {

				//An unset end region means the rest of the buffer, the same convention the tables use

				if(!Descriptor_endBuffer(&d)) {
					const U64 len = DeviceBufferRef_ptr(d.resource)->resource.size;
					d.buffer.endRegionAndCounterOffset.region48 |= len - Descriptor_startBuffer(&d);
				}

				buffers[writeCount] = (VkDescriptorBufferInfo) {
					.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(d.resource), Vk)->buffer,
					.offset = Descriptor_startBuffer(&d),
					.range = Descriptor_bufferLength(&d)
				};

				writes[writeCount].descriptorType =
					type == ESHRegisterType_ConstantBuffer ?
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

				writes[writeCount].pBufferInfo = &buffers[writeCount];
			}

			++writeCount;
		}

		if(writeCount)
			deviceExt->cmdPushDescriptorSet(temp->buffer, bindPoint, *layoutExt, pushSet, writeCount, writes);

		temp->pushDescriptorsEmitted[bindPointId] = true;
		temp->lastPushDescLayout[bindPointId] = *layoutExt;
	}

	//A layout without bindings has nothing to emit at all.

	if(!temp->boundDescriptorTable || !layout->info.bindings)
		return;

	const DescriptorTable *table = DescriptorTableRef_ptr(temp->boundDescriptorTable);

	//A table stays bound across a pipeline switch, so it can outlive the layout it was bound for.
	//Emitting it against a layout built from a DIFFERENT DescriptorLayout binds sets the layout never
	// declared, which is what interleaving a bindful dispatch with a bindless one does.
	//The bindless path above already bound the device's own sets for such a layout, so there is nothing
	// left to do here.

	if(table->layout != layout->info.bindings)
		return;

	VkDescriptorTable *tableExt = DescriptorTable_ext((DescriptorTable*)table, Vk);

	if(
		temp->lastBoundTable[bindPointId] == temp->boundDescriptorTable &&
		temp->lastBoundLayout[bindPointId] == *layoutExt
	)
		return;

	temp->lastBoundTable[bindPointId] = temp->boundDescriptorTable;
	temp->lastBoundLayout[bindPointId] = *layoutExt;

	for (U64 j = 0, k = 0; j < tableExt->bindCommands; ++j) {

		deviceExt->cmdBindDescriptorSets(
			temp->buffer,
			bindPoint,
			*layoutExt,
			tableExt->offsets[j], tableExt->counts[j], &tableExt->sets[k],
			0, NULL
		);

		k += tableExt->counts[j];
	}

	temp->defaultDescriptorsBound = false;
}

//A declared stage stands for itself AND every later stage that could reach the resource, because a scope
//names only the FIRST stage that accesses it. Tessellation is a required device feature so those stages are
//always legal to name; geometry is not, and naming a stage the device never enabled is invalid usage.

static VkPipelineStageFlags2 VkPipelineStage_fromMask(U32 stageMask, const GraphicsDevice *device) {

	VkPipelineStageFlags2 stages = 0;

	const VkPipelineStageFlags2 geometry =
		(device->info.capabilities.features & EGraphicsFeatures_GeometryShader) ?
		VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT : 0;

	const VkPipelineStageFlags2 tessEval =
		VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | geometry |
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

	if(stageMask & ((U32)1 << EPipelineStage_Vertex))
		stages |=
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
			VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT | tessEval;

	if(stageMask & ((U32)1 << EPipelineStage_Hull))
		stages |= VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT | tessEval;

	if(stageMask & ((U32)1 << EPipelineStage_Domain))
		stages |= tessEval;

	if(stageMask & ((U32)1 << EPipelineStage_GeometryExt))
		stages |= geometry | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

	if(stageMask & ((U32)1 << EPipelineStage_Pixel))
		stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

	if(stageMask & ((U32)1 << EPipelineStage_Compute))
		stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

	if(stageMask & EPipelineStageMask_RtAny)
		stages |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;

	if (stageMask & EPipelineStageMask_RTASBuild) {

		stages |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;

		//Micromap builds run at their own stage, so on the EXT path the build barrier has to cover both;
		// a stage bit no op in the scope uses is legal and free.

		if(
			(device->info.capabilities.features & EGraphicsFeatures_RayMicromapOpacity) &&
			!(device->info.capabilities.featuresExt & EVkGraphicsFeatures_OpacityMicromapKHR)
		)
			stages |= VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT;
	}

	return stages;
}

//Writes one timestamp to the frame's pool at the current cursor when timing is active, then advances the cursor.
//The cursor advances even where the pool is absent so it stays in lockstep with GraphicsDevice_buildTimings, whose
// slot assignment the resolve indexes by.

static void vkTimestampWrite(VkGraphicsDevice *deviceExt, U8 fifId, VkCommandBuffer buffer) {

	const VkQueryPool pool = deviceExt->timestampPool[fifId];

	if(pool && deviceExt->timestampCursor < deviceExt->timestampCapacity[fifId])
		deviceExt->cmdWriteTimestamp(buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, deviceExt->timestampCursor);

	++deviceExt->timestampCursor;
}

void VK_WRAP_FUNC(CommandList_process)(
	CommandList *commandList,
	GraphicsDeviceRef *deviceRef,
	ECommandOp op,
	const U8 *data,
	void *commandListExt
) {

	//CommandList_process can't fail upward; errors are printed and the op is skipped.

	Bool s_uccess = true;
	Error err = Error_none();
	Error *e_rr = &err;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	VkCommandBufferState *temp = (VkCommandBufferState*) commandListExt;
	VkCommandBuffer buffer = temp->buffer;

	switch (op) {

		case ECommandOp_SetViewport:
		case ECommandOp_SetScissor:
		case ECommandOp_SetViewportAndScissor: {

			I32x2 offset = ((const I32x2*) data)[0];
			I32x2 size = ((const I32x2*) data)[1];

			if((op - ECommandOp_SetViewport + 1) & 1)
				temp->tempViewport = (VkViewport) {
					.x = (F32) I32x2_x(offset),
					.y = (F32) I32x2_y(offset),
					.width = (F32) I32x2_x(size),
					.height = (F32) I32x2_y(size),
					.minDepth = 0,
					.maxDepth = 1
				};

			if ((op - ECommandOp_SetViewport + 1) & 2)
				temp->tempScissor = (VkRect2D) {
					.offset = (VkOffset2D) {
						.x = I32x2_x(offset),
						.y = I32x2_y(offset),
					},
					.extent = (VkExtent2D) {
						.width = I32x2_x(size),
						.height = I32x2_y(size),
					}
				};

			break;
		}

		case ECommandOp_SetStencil:
			temp->tempStencilRef = *(const U8*) data;
			break;

		case ECommandOp_SetBlendConstants:

			Buffer_memcpy(
				Buffer_createRef(&temp->tempBlendConstants, sizeof(F32x4)),
				Buffer_createRefConst(data, sizeof(F32x4))
			);

			break;

		//Clear and copy commands

		case ECommandOp_ClearImages: {

			U64 imageClearCount = *(const U64*) data;

			for(U64 i = 0; i < imageClearCount; ++i) {

				ClearImageCmd image = ((const ClearImageCmd*) (data + sizeof(U64) * 2))[i];
				VkUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(image.image, Vk, 0);

				VkImageSubresourceRange range = (VkImageSubresourceRange) {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1
				};

				deviceExt->cmdClearColorImage(
					buffer,
					imageExt->image,
					imageExt->lastLayout,
					(const VkClearColorValue*) &image.color,
					1,
					&range
				);
			}

			break;
		}

		case ECommandOp_CopyImage: {

			CopyImageCmd copyImage = *(const CopyImageCmd*) data;
			const CopyImageRegion *copyImageRegions = (const CopyImageRegion*) (data + sizeof(copyImage));

			VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

			UnifiedTexture src = TextureRef_getUnifiedTexture(copyImage.src, NULL);

			for(U64 i = 0; i < copyImage.regionCount; ++i) {

				CopyImageRegion image = copyImageRegions[i];

				if(!image.width)
					image.width = src.width - image.srcX;

				if(!image.height)
					image.height = src.height - image.srcY;

				if(!image.length)
					image.length = src.length - image.srcZ;

				VkImageSubresourceLayers subResource = (VkImageSubresourceLayers) {
					.aspectMask = aspectFlags,
					.layerCount = 1
				};

				gotoIfError3(next, ListVkImageCopy_pushBack(&deviceExt->imageCopyRanges, (VkImageCopy) {
					.srcSubresource = subResource,
					.srcOffset = (VkOffset3D) {
						.x = (I32) image.srcX,
						.y = (I32) image.srcY,
						.z = (I32) image.srcZ
					},
					.dstSubresource = subResource,
					.dstOffset = (VkOffset3D) {
						.x = (I32) image.dstX,
						.y = (I32) image.dstY,
						.z = (I32) image.dstZ
					},
					.extent = (VkExtent3D) {
						.width = image.width,
						.height = image.height,
						.depth = image.length
					}
				}, alloc, e_rr));

			next:

				if(!s_uccess) {
					Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
					break;
				}
			}

			VkUnifiedTexture *srcExt = TextureRef_getCurrImgExtT(copyImage.src, Vk, 0);
			VkUnifiedTexture *dstExt = TextureRef_getCurrImgExtT(copyImage.dst, Vk, 0);

			deviceExt->cmdCopyImage(
				buffer,
				srcExt->image,
				srcExt->lastLayout,
				dstExt->image,
				dstExt->lastLayout,
				copyImage.regionCount,
				deviceExt->imageCopyRanges.ptr
			);

			ListVkImageCopy_clear(&deviceExt->imageCopyRanges, e_rr);
			break;
		}

		//Dynamic rendering / direct rendering

		case ECommandOp_StartRenderingExt: {

			const StartRenderCmdExt *startRender = (const StartRenderCmdExt*) data;
			const AttachmentInfoInternal *attachments = (const AttachmentInfoInternal*) (startRender + 1);

			//Prepare attachments

			VkRenderingAttachmentInfoKHR attachmentsExt[8] = { 0 };
			U8 j = 0;

			for (U8 i = 0; i < startRender->colorCount; ++i) {

				if(!((startRender->activeMask >> i) & 1)) {

					attachmentsExt[i] = (VkRenderingAttachmentInfoKHR) {
						.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
						.imageView = NULL,
						.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
						.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
					};

					continue;
				}

				const AttachmentInfoInternal *attachmentsj = &attachments[j++];

				VkUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(attachmentsj->image, Vk, 0);
				VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

				if((startRender->clearMask >> i) & 1)
					loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

				else if((startRender->preserveMask >> i) & 1)
					loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

				//Even though this adds a new view that we don't clean up it should be okay.
				//Likely this view will get accessed again in the same way.
				//If we don't, we still clean it up during destruction of the resource.

				U32 viewId = U32_MAX;
				VkImageView view = NULL;

				Descriptor descriptor = (Descriptor) { .resource = attachmentsj->image };
				ESHRegisterType registerType = ESHRegisterType_Texture2D;        //TODO: Add support for other resources

				if (!VkUnifiedTexture_getView(descriptor, registerType, &view, &viewId, NULL)) {
					Log_errorLnx("VkUnifiedTexture_getView color at ECommandOp_StartRenderingExt, this is problematic!");
					break;
				}

				//VkClearColorValue is a union, and the spec requires the member matching the attachment's numeric
				// format: int32 for SINT, uint32 for UINT, float32 for everything else.
				//Writing float32 unconditionally only appeared to work because ClearColor is a union too, so the
				// bytes aliased through - which relies on a denormal or NaN bit pattern surviving a float
				// assignment, and disagrees with D3D12, whose ClearRenderTargetView takes floats and CONVERTS
				// them to the integer format rather than reinterpreting them.
				//Selecting the member here is what makes an integer clear mean the same thing on both backends.

				const UnifiedTexture colorTex = TextureRef_getUnifiedTexture(attachmentsj->image, NULL);

				const ETexturePrimitive colorPrim =
					ETextureFormat_getPrimitive(ETextureFormatId_unpack[colorTex.textureFormatId]);

				VkClearColorValue clearColor = (VkClearColorValue) { 0 };

				for (U8 c = 0; c < 4; ++c)
					switch (colorPrim) {
						case ETexturePrimitive_SInt:    clearColor.int32[c] = attachmentsj->color.colori[c];    break;
						case ETexturePrimitive_UInt:    clearColor.uint32[c] = attachmentsj->color.coloru[c];   break;
						default:                        clearColor.float32[c] = attachmentsj->color.colorf[c];  break;
					}

				attachmentsExt[i] = (VkRenderingAttachmentInfoKHR) {

					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = view,
					.imageLayout = imageExt->lastLayout,
					.loadOp = loadOp,

					.storeOp =
						!((startRender->unusedAfterRenderMask >> i) & 1) ? VK_ATTACHMENT_STORE_OP_STORE:
						VK_ATTACHMENT_STORE_OP_DONT_CARE,

					.clearValue = (VkClearValue) { .color = clearColor }
				};

				if(attachmentsj->resolveImage && !addResolveImage(*attachmentsj, &attachmentsExt[i])) {
					Log_errorLnx("addResolveImage failed during ECommandOp_StartRenderingExt, this is problematic!");
					break;
				}
			}

			//Send begin render command

			VkRenderingAttachmentInfoKHR depthAttachment;
			VkRenderingAttachmentInfoKHR stencilAttachment;

			if (startRender->flags & EStartRenderFlags_Depth) {

				VkUnifiedTexture *depthExt = TextureRef_getCurrImgExtT(startRender->depthStencil, Vk, 0);

				Bool unusedAfterRender = startRender->flags & EStartRenderFlags_DepthUnusedAfterRender;

				VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

				if(startRender->flags & EStartRenderFlags_ClearDepth)
					loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

				else if(startRender->flags & EStartRenderFlags_PreserveDepth)
					loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

				//See the comment during attachmentsExt[i] setup above about view creation.

				U32 viewId = U32_MAX;
				VkImageView view = NULL;

				//planeId 0xFF = the attachment view spanning every aspect; the stencil attachment below asks for
				// the same, since dynamic rendering requires both slots to carry one identical view.

				Descriptor descriptor = (Descriptor) {
					.resource = startRender->depthStencil,
					.texture = (TextureDescriptorRange) { .planeId = 0xFF }
				};

				ESHRegisterType registerType = ESHRegisterType_Texture2D;        //TODO: Add support for other resources

				if (!VkUnifiedTexture_getView(descriptor, registerType, &view, &viewId, NULL)) {
					Log_errorLnx("VkUnifiedTexture_getView depth at ECommandOp_StartRenderingExt, this is problematic!");
					break;
				}

				depthAttachment = (VkRenderingAttachmentInfoKHR) {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = view,
					.imageLayout = depthExt->lastLayout,
					.loadOp = loadOp,
					.storeOp = unusedAfterRender ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = (VkClearValue) {
						.depthStencil = (VkClearDepthStencilValue) { .depth = startRender->clearDepth }
					}
				};

				if(startRender->resolveDepthStencil) {

					AttachmentInfoInternal tmp = (AttachmentInfoInternal) {
						.resolveImage = startRender->resolveDepthStencil,
						.resolveMode = startRender->resolveDepthStencilMode
					};

					addResolveImage(tmp, &depthAttachment);
				}
			}

			if (startRender->flags & EStartRenderFlags_Stencil) {

				VkUnifiedTexture *stencilExt = TextureRef_getCurrImgExtT(startRender->depthStencil, Vk, 0);

				Bool unusedAfterRender = startRender->flags & EStartRenderFlags_StencilUnusedAfterRender;

				VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

				if(startRender->flags & EStartRenderFlags_ClearStencil)
					loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

				else if(startRender->flags & EStartRenderFlags_PreserveStencil)
					loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

				//See the comment during attachmentsExt[i] setup above about view creation.

				U32 viewId = U32_MAX;
				VkImageView view = NULL;

				Descriptor descriptor = (Descriptor) {
					.resource = startRender->depthStencil,
					.texture = (TextureDescriptorRange) { .planeId = 0xFF }
				};

				ESHRegisterType registerType = ESHRegisterType_Texture2D;        //TODO: Add support for other resources

				if (!VkUnifiedTexture_getView(descriptor, registerType, &view, &viewId, NULL)) {
					Log_errorLnx("VkUnifiedTexture_getView stencil at ECommandOp_StartRenderingExt, this is problematic!");
					break;
				}

				stencilAttachment = (VkRenderingAttachmentInfoKHR) {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = view,
					.imageLayout = stencilExt->lastLayout,
					.loadOp = loadOp,
					.storeOp = unusedAfterRender ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = (VkClearValue) {
						.depthStencil = (VkClearDepthStencilValue) { .stencil = startRender->clearStencil }
					}
				};

				if(startRender->resolveDepthStencil) {

					AttachmentInfoInternal tmp = (AttachmentInfoInternal) {
						.resolveImage = startRender->resolveDepthStencil,
						.resolveMode = startRender->resolveDepthStencilMode
					};

					addResolveImage(tmp, &stencilAttachment);
				}
			}

			VkRenderingInfoKHR renderInfo = (VkRenderingInfoKHR) {
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
				.renderArea = (VkRect2D) {
					.offset = (VkOffset2D) {
						.x = I32x2_x(startRender->offset),
						.y = I32x2_y(startRender->offset)
					},
					.extent = (VkExtent2D) {
						.width = I32x2_x(startRender->size),
						.height = I32x2_y(startRender->size)
					}
				},
				.layerCount = 1,
				.colorAttachmentCount = startRender->colorCount,
				.pColorAttachments = attachmentsExt,
				.pDepthAttachment = startRender->flags & EStartRenderFlags_Depth ? &depthAttachment : NULL,
				.pStencilAttachment = startRender->flags & EStartRenderFlags_Stencil ? &stencilAttachment : NULL
			};

			deviceExt->cmdBeginRendering(buffer, &renderInfo);
			break;
		}

		case ECommandOp_EndRenderingExt:
			deviceExt->cmdEndRendering(buffer);
			break;

		//Draws

		case ECommandOp_SetGraphicsPipeline:
			temp->tempPipelines[EPipelineType_Graphics] = *(PipelineRef* const*) data;
			break;

		case ECommandOp_SetComputePipeline:
			temp->tempPipelines[EPipelineType_Compute] = *(PipelineRef* const*) data;
			break;

		case ECommandOp_SetRaytracingPipelineExt:
			temp->tempPipelines[EPipelineType_RaytracingExt] = *(PipelineRef* const*) data;
			break;

		case ECommandOp_SetPrimitiveBuffers:
			temp->tempBoundBuffers = *(const SetPrimitiveBuffersCmd*) data;
			break;

		case ECommandOp_DrawIndirect:
		case ECommandOp_DrawIndirectCount:
		case ECommandOp_Draw: {

			//Bind viewport and scissor

			Bool eq = Buffer_eq(
				Buffer_createRefConst(&temp->boundViewport, sizeof(VkViewport)),
				Buffer_createRefConst(&temp->tempViewport, sizeof(VkViewport))
			);

			if(!eq) {
				temp->boundViewport = temp->tempViewport;
				deviceExt->cmdSetViewport(buffer, 0, 1, &temp->boundViewport);
			}

			eq = Buffer_eq(
				Buffer_createRefConst(&temp->boundScissor, sizeof(VkRect2D)),
				Buffer_createRefConst(&temp->tempScissor, sizeof(VkRect2D))
			);

			if(!eq) {
				temp->boundScissor = temp->tempScissor;
				deviceExt->cmdSetScissor(buffer, 0, 1, &temp->boundScissor);
			}

			//Bind blend constants and/or stencil ref

			if (F32x4_neqExact4(temp->tempBlendConstants, temp->blendConstants)) {
				temp->blendConstants = temp->tempBlendConstants;
				deviceExt->cmdSetBlendConstants(buffer, (const float*) &temp->blendConstants);
			}

			if (temp->tempStencilRef != temp->stencilRef) {
				temp->stencilRef = temp->tempStencilRef;
				deviceExt->cmdSetStencilReference(buffer, VK_STENCIL_FACE_FRONT_AND_BACK, temp->stencilRef);
			}

			//Bind pipeline

			if(temp->pipelines[EPipelineType_Graphics] != temp->tempPipelines[EPipelineType_Graphics]) {

				temp->pipelines[EPipelineType_Graphics] = temp->tempPipelines[EPipelineType_Graphics];

				deviceExt->cmdBindPipeline(
					temp->buffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					*Pipeline_ext(PipelineRef_ptr(temp->pipelines[EPipelineType_Graphics]), Vk)
				);
			}

			VkCommandBufferState_bindDescriptors(
				temp, deviceExt, device, temp->pipelines[EPipelineType_Graphics], VK_PIPELINE_BIND_POINT_GRAPHICS
			);

			//Bind index buffer

			if (
				temp->tempBoundBuffers.indexBuffer &&
				(
					temp->boundBuffers.indexBuffer != temp->tempBoundBuffers.indexBuffer ||
					temp->boundBuffers.isIndex32Bit != temp->tempBoundBuffers.isIndex32Bit
				)
			) {

				temp->boundBuffers.indexBuffer = temp->tempBoundBuffers.indexBuffer;
				temp->boundBuffers.isIndex32Bit = temp->tempBoundBuffers.isIndex32Bit;

				deviceExt->cmdBindIndexBuffer(
					temp->buffer,
					DeviceBuffer_ext(DeviceBufferRef_ptr(temp->boundBuffers.indexBuffer), Vk)->buffer,
					0,
					temp->boundBuffers.isIndex32Bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16
				);
			}

			//Bind vertex buffers

			{
				VkBuffer vertexBuffers[16] = { 0 };
				VkDeviceSize vertexBufferOffsets[16] = { 0 };

				U32 start = 16, end = 0;

				//Fill vertexBuffers and find start/end range.
				//And ensure bound buffers can't be accidentally transitioned while render hasn't ended yet.

				for(U32 i = 0; i < 16; ++i) {

					DeviceBufferRef *bufferRef = temp->tempBoundBuffers.vertexBuffers[i];

					if (!bufferRef)
						continue;

					if(temp->boundBuffers.vertexBuffers[i] != bufferRef) {

						if(start == 16)
							start = i;

						end = i + 1;

						temp->boundBuffers.vertexBuffers[i] = bufferRef;
					}

					vertexBuffers[i] = DeviceBuffer_ext(DeviceBufferRef_ptr(bufferRef), Vk)->buffer;
				}

				if(end > start)
					deviceExt->cmdBindVertexBuffers(
						temp->buffer,
						start,
						end - start,
						&(vertexBuffers)[start],
						&(vertexBufferOffsets)[start]
					);
			}

			//Direct draws

			if(op == ECommandOp_Draw) {

				DrawCmd draw = *(const DrawCmd*)data;

				if(draw.isIndexed)
					deviceExt->cmdDrawIndexed(
						buffer,
						draw.count, draw.instanceCount,
						draw.indexOffset, draw.vertexOffset,
						draw.instanceOffset
					);

				else deviceExt->cmdDraw(
					buffer,
					draw.count, draw.instanceCount,
					draw.vertexOffset, draw.instanceOffset
				);
			}

			//Indirect draws

			else {

				DrawIndirectCmd drawIndirect = *(const DrawIndirectCmd*)data;
				VkDeviceBuffer *bufferExt = DeviceBuffer_ext(DeviceBufferRef_ptr(drawIndirect.buffer), Vk);

				//Indirect draw count

				if (drawIndirect.countBufferExt) {

					DeviceBuffer *counterBuffer = DeviceBufferRef_ptr(drawIndirect.countBufferExt);
					VkDeviceBuffer *counterExt = DeviceBuffer_ext(counterBuffer, Vk);

					if(drawIndirect.isIndexed)
						deviceExt->cmdDrawIndexedIndirectCount(
							buffer,
							bufferExt->buffer, drawIndirect.bufferOffset,
							counterExt->buffer, drawIndirect.countOffsetExt,
							drawIndirect.drawCalls, sizeof(DrawCallIndexed)
						);

					else deviceExt->cmdDrawIndirectCount(
						buffer,
						bufferExt->buffer, drawIndirect.bufferOffset,
						counterExt->buffer, drawIndirect.countOffsetExt,
						drawIndirect.drawCalls, sizeof(DrawCallUnindexed)
					);
				}

				//Indirect draw (non count)

				else {

					if(drawIndirect.isIndexed)
						deviceExt->cmdDrawIndexedIndirect(
							buffer,
							bufferExt->buffer, drawIndirect.bufferOffset,
							drawIndirect.drawCalls, sizeof(DrawCallIndexed)
						);

					else deviceExt->cmdDrawIndirect(
						buffer, bufferExt->buffer, drawIndirect.bufferOffset,
						drawIndirect.drawCalls, sizeof(DrawCallUnindexed)
					);
				}
			}

			break;
		}

		case ECommandOp_DispatchIndirect:
		case ECommandOp_Dispatch:

			if(temp->pipelines[EPipelineType_Compute] != temp->tempPipelines[EPipelineType_Compute]) {

				temp->pipelines[EPipelineType_Compute] = temp->tempPipelines[EPipelineType_Compute];

				deviceExt->cmdBindPipeline(
					temp->buffer,
					VK_PIPELINE_BIND_POINT_COMPUTE,
					*Pipeline_ext(PipelineRef_ptr(temp->pipelines[EPipelineType_Compute]), Vk)
				);
			}

			VkCommandBufferState_bindDescriptors(
				temp, deviceExt, device, temp->pipelines[EPipelineType_Compute], VK_PIPELINE_BIND_POINT_COMPUTE
			);

			if(op == ECommandOp_Dispatch) {
				DispatchCmd dispatch = *(const DispatchCmd*)data;
				deviceExt->cmdDispatch(
					buffer, dispatch.groups[0], dispatch.groups[1], dispatch.groups[2]
				);
			}

			else {
				DispatchIndirectCmd dispatch = *(const DispatchIndirectCmd*)data;
				VkDeviceBuffer *bufferExt = DeviceBuffer_ext(DeviceBufferRef_ptr(dispatch.buffer), Vk);
				deviceExt->cmdDispatchIndirect(buffer, bufferExt->buffer, dispatch.offset);
			}

			break;

		//JIT RTAS updates in case they are on the GPU (e.g. compute updates)

		case ECommandOp_UpdateBLASExt:

			if(!(VK_WRAP_FUNC(BLASRef_flush))(temp, deviceRef, *(BLASRef**)data, e_rr))
				Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

			break;

		case ECommandOp_CompactBLASExt:

			if(!(VK_WRAP_FUNC(BLASRef_compact))(temp, deviceRef, *(BLASRef**)data, e_rr))
				Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

			break;

		case ECommandOp_UpdateTLASExt:

			if(!(VK_WRAP_FUNC(TLASRef_flush))(temp, deviceRef, *(TLASRef**)data, e_rr))
				Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

			break;

		case ECommandOp_BindDescriptorTable:
			temp->boundDescriptorTable = *(RefPtr* const*) data;
			break;

		//Nothing to emit on Vulkan today (a heap is a pool); recorded for VK_EXT_descriptor_heap later

		case ECommandOp_BindDescriptorHeap:
			temp->boundDescriptorHeap = *(RefPtr* const*) data;
			break;

		//The bytes travel with the command, so nothing here depends on the recorder's buffer still existing

		case ECommandOp_SetPushConstants: {

			const SetPushConstantsCmd *push = (const SetPushConstantsCmd*) data;

			temp->pushConstantSize = (U8) push->size;

			Buffer_memcpy(
				Buffer_createRef(temp->pushConstantData, push->size),
				Buffer_createRefConst(push->data, push->size)
			);

			//A fresh write has to reach every bind point, since each carries its own copy

			for(U8 i = 0; i < 3; ++i)
				temp->pushConstantsEmitted[i] = false;

			break;
		}

		case ECommandOp_SetPushDescriptors: {

			const SetPushDescriptorsCmd *push = (const SetPushDescriptorsCmd*) data;
			const Descriptor *descriptors = (const Descriptor*)(push + 1);

			temp->pushDescriptorCount = (U8) push->count;

			Buffer_memcpy(
				Buffer_createRef(temp->pushDescriptors, push->count * sizeof(Descriptor)),
				Buffer_createRefConst(descriptors, push->count * sizeof(Descriptor))
			);

			for(U8 i = 0; i < 3; ++i)
				temp->pushDescriptorsEmitted[i] = false;

			break;
		}

		case ECommandOp_UpdateOmmExt:

			if(!(VK_WRAP_FUNC(OpacityMicromapRef_flush))(temp, deviceRef, *(OpacityMicromapRef**)data, e_rr))
				Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

			break;

		case ECommandOp_DispatchRaysIndirect:
		case ECommandOp_DispatchRaysExt: {

			Pipeline *raytracingPipeline = PipelineRef_ptr(temp->tempPipelines[EPipelineType_RaytracingExt]);

			if(temp->pipelines[EPipelineType_RaytracingExt] != temp->tempPipelines[EPipelineType_RaytracingExt]) {

				temp->pipelines[EPipelineType_RaytracingExt] = temp->tempPipelines[EPipelineType_RaytracingExt];

				deviceExt->cmdBindPipeline(
					temp->buffer,
					VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
					*Pipeline_ext(raytracingPipeline, Vk)
				);
			}

			VkCommandBufferState_bindDescriptors(
				temp, deviceExt, device, temp->pipelines[EPipelineType_RaytracingExt],
				VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
			);

			PipelineRaytracingInfo info = *Pipeline_info(raytracingPipeline, PipelineRaytracingInfo);

			//The shader binding table is read by the trace itself rather than by anything the scope declared,
			// so no transition ever names it and its tracked state stays where the upload copy left it.
			//Ordering it here is what makes the trace's reads visible to that copy; after the first trace the
			// buffer already sits in this state and the helper adds no barrier.

			if (info.shaderBindingTable) {

				VkDependencyInfo sbtDependency = (VkDependencyInfo) { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
				DeviceBuffer *sbtBuffer = DeviceBufferRef_ptr(info.shaderBindingTable);

				if(!VkDeviceBuffer_transition(
					DeviceBuffer_ext(sbtBuffer, Vk),
					VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
					VK_ACCESS_2_SHADER_READ_BIT,
					deviceExt->queues[EVkCommandQueue_Graphics].queueId,
					0,
					sbtBuffer->resource.size,
					&deviceExt->bufferTransitions,
					&sbtDependency, alloc, e_rr
				))
					Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

				else if(sbtDependency.bufferMemoryBarrierCount)
					deviceExt->cmdPipelineBarrier2(temp->buffer, &sbtDependency);

				ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions, e_rr);
			}

			VkStridedDeviceAddressRegionKHR hit = (VkStridedDeviceAddressRegionKHR) {
				.deviceAddress = getVkDeviceAddress((DeviceData) { .buffer = info.shaderBindingTable }),
				.size = (U64)raytracingShaderAlignment * info.groups.length,
				.stride = raytracingShaderAlignment
			};

			VkStridedDeviceAddressRegionKHR miss = (VkStridedDeviceAddressRegionKHR) {
				.deviceAddress = hit.deviceAddress + hit.size,
				.size = (U64)raytracingShaderAlignment * info.missCount,
				.stride = raytracingShaderAlignment
			};

			VkStridedDeviceAddressRegionKHR raygen = (VkStridedDeviceAddressRegionKHR) {
				.deviceAddress = miss.deviceAddress + miss.size,
				.size = raytracingShaderAlignment,
				.stride = raytracingShaderAlignment
			};

			VkStridedDeviceAddressRegionKHR callable = (VkStridedDeviceAddressRegionKHR) {
				.deviceAddress = raygen.deviceAddress + raygen.size * info.raygenCount,
				.size = (U64)raytracingShaderAlignment * info.callableCount,
				.stride = raytracingShaderAlignment
			};

			if(!info.groups.length)
				hit = (VkStridedDeviceAddressRegionKHR) { 0 };

			if(!info.missCount)
				miss = (VkStridedDeviceAddressRegionKHR) { 0 };

			if(!info.callableCount)
				callable = (VkStridedDeviceAddressRegionKHR) { 0 };

			if(op == ECommandOp_DispatchRaysExt) {
				DispatchRaysExt dispatch = *(const DispatchRaysExt*)data;
				raygen.deviceAddress += raygen.stride * dispatch.raygenId;
				deviceExt->traceRays(
					buffer,
					&raygen, &miss, &hit, &callable,
					dispatch.x, dispatch.y, dispatch.z
				);
			}

			else {
				DispatchRaysIndirectExt dispatch = *(const DispatchRaysIndirectExt*)data;
				raygen.deviceAddress += raygen.stride * dispatch.raygenId;
				deviceExt->traceRaysIndirect(
					buffer,
					&raygen, &miss, &hit, &callable,
					DeviceBufferRef_ptr(dispatch.buffer)->resource.deviceAddress + dispatch.offset
				);
			}

			break;
		}

		case ECommandOp_StartScope: {

			//Stencil and blend constants belong to the scope that set them, so a scope that doesn't set them starts
			// from the default rather than inheriting whatever the previous one left.
			//Recording already forces the pipeline, viewport and scissor to be re-declared per scope; without this
			// these two would be the only state that leaks across, which makes a scope depend on whether an
			// unrelated earlier one happened to be hidden.
			//Only the requested values reset, since stencilRef and blendConstants track what the GPU actually has;
			// the flush before the next draw then sets them again only if they really differ.

			temp->tempStencilRef = 0;
			temp->tempBlendConstants = F32x4_zero();

			VkDependencyInfo dependency = (VkDependencyInfo) {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.dependencyFlags = 0
			};

			U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

			CommandScope scope = commandList->activeScopes.ptr[temp->scopeCounter];
			++temp->scopeCounter;

			//A scope may bracket its work in a timestamp pair and/or a named debug region;
			// the flags carry both decisions to EndScope.
			//A DebugRegion scope has its name in the StartScope payload (data).

			temp->curScopeFlags = (U8) scope.flags;

			if((temp->curScopeFlags & ECommandScopeInternalFlags_DebugRegion) && instanceExt->cmdDebugMarkerBegin) {

				VkDebugUtilsLabelEXT scopeLabel = (VkDebugUtilsLabelEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
					.pLabelName = (const char*) data + sizeof(CommandScopePredicate),
					.color = { 0.4f, 0.6f, 0.9f, 1.0f }
				};

				instanceExt->cmdDebugMarkerBegin(buffer, &scopeLabel);
			}

			if((temp->curScopeFlags & ECommandScopeInternalFlags_Timed))
				vkTimestampWrite(deviceExt, device->fifId, buffer);

			for (U64 i = scope.transitionOffset; i < scope.transitionOffset + scope.transitionCount; ++i) {

				TransitionInternal transition = commandList->transitions.ptr[i];

				if(transition.type == ETransitionType_KeepAlive)        //TODO: Residency management
					continue;

				VkPipelineStageFlags2 pipelineStage = VkPipelineStage_fromMask(transition.stageMask, device);

				//If it's on the GPU then we have to rely on manual RTAS transitions

				Bool isTLAS = transition.resource->refPtrType->typeId == (TypeId)EGraphicsTypeId_TLASExt;
				Bool isOMM = transition.resource->refPtrType->typeId == (TypeId)EGraphicsTypeId_OpacityMicromapExt;

				if (isTLAS || isOMM || transition.resource->refPtrType->typeId == (TypeId)EGraphicsTypeId_BLASExt) {

					RTAS rtas =
						isTLAS ? TLASRef_ptr(transition.resource)->base : (
							isOMM ? OpacityMicromapRef_ptr(transition.resource)->base :
							BLASRef_ptr(transition.resource)->base
						);

					//Read to RTAS is illegal if it's not initialized yet.
					//However, if ShaderWrite is used on an RTAS then it will be written in the current scope.
					//In that case, we want to transition to write of course.
					if (!rtas.isCompleted && transition.type != ETransitionType_ShaderWrite)
						continue;

					//A micromap is written and read with its own access bits on the EXT path; the AS bits stay in
					// the mask so the same barrier is right for a KHR device, where the array IS an AS.
					//A BLAS build consuming a micromap reads it with the micromap bit too.

					VkAccessFlags2 rtasAccess =
						transition.type == ETransitionType_ShaderWrite ? VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR :
						VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;

					if(isOMM && (pipelineStage & VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT))
						rtasAccess |=
							transition.type == ETransitionType_ShaderWrite ?
							VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT : VK_ACCESS_2_MICROMAP_READ_BIT_EXT;

					gotoIfError3(nextTransition, VkDeviceBuffer_transition(

						DeviceBuffer_ext(DeviceBufferRef_ptr(rtas.asBuffer), Vk),
						pipelineStage,

						rtasAccess,

						graphicsQueueId,
						0, 0,
						&deviceExt->bufferTransitions,
						&dependency, alloc, e_rr));

					continue;
				}

				//Grab transition type

				Bool isImage = TextureRef_isTexture(transition.resource);
				Bool isDepthStencil = TextureRef_isDepthStencil(transition.resource);
				Bool isShaderRead = transition.type == ETransitionType_ShaderRead;

				VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;
				VkAccessFlags2 access = 0;

				//A resource in the write state is read as well as written: a UAV is loaded from directly and
				// every atomic on it performs a read before its write. This mask doubles as the NEXT barrier's
				// source scope, so a missing read bit lets a later access race reads still in flight.
				//The read state needs no such pairing: SHADER_READ_ONLY_OPTIMAL doesn't permit storage access
				// at all, so a sampled read is the only thing that can happen there.

				if(isImage) {

					access =
						isShaderRead ? VK_ACCESS_2_SHADER_SAMPLED_READ_BIT :
						VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

					layout = isShaderRead ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
				}

				if (!isImage) {

					access =
						transition.type == ETransitionType_ShaderRead ? VK_ACCESS_2_SHADER_STORAGE_READ_BIT :
						VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

					//A readable uniform buffer is read through UNIFORM_READ rather than storage reads;
					// both can apply when a buffer carries ShaderRead and the uniform usage at once

					if (
						isShaderRead &&
						transition.resource->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer &&
						(DeviceBufferRef_ptr(transition.resource)->usage & EDeviceBufferUsage_Uniform)
					) {

						access |= VK_ACCESS_2_UNIFORM_READ_BIT;

						if(!(DeviceBufferRef_ptr(transition.resource)->resource.flags & EGraphicsResourceFlag_ShaderRead))
							access &=~ VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
					}
				}

				//AS builds read their inputs (vertex/index/aabb/instance data) as SHADER_READ per spec;
				// only the AS itself uses AS read/write, which the RTAS branch above already handled.
				//The write side covers the build's scratch memory, which a build both writes AND reads back
				// while it runs. One scratch buffer is kept alive across an updatable structure's refits, so
				// declaring write only would let the next refit's writes race the previous build's reads.

				if(transition.stageMask & EPipelineStageMask_RTASBuild)
					access =
						isShaderRead ? VK_ACCESS_2_SHADER_READ_BIT :
						VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR |
						VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;

				if(!pipelineStage)
					switch ((ETransitionType) transition.type) {

						case ETransitionType_RenderTargetRead:

							//Same reasoning as the write below: the depth test reads at BOTH fragment test
							// stages, and this stage becomes the next barrier's source, so leaving LATE out
							// lets a later write race reads that hadn't finished.

							//A colour attachment is read by the blend and load machinery at the attachment output
							// stage rather than inside the fragment shader; naming the shader stage left the
							// read outside every later barrier's source scope.
							//The access stays read only because this case is exactly the readOnly attachment.

							pipelineStage =
								isDepthStencil ?
								VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT :
								VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

							access =
								isDepthStencil ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT :
								VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

							layout =
								isDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL :
								VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

							break;

						case ETransitionType_ResolveTargetWrite:

							//A fixed function resolve writes its target at the colour attachment output stage
							// with colour attachment access, even when the attachment being resolved is depth
							// stencil: the validator asks for exactly that scope on a depth resolve.
							//Only the layout stays depth stencil, since that is the layout the resolve
							// attachment itself has to be in.

							pipelineStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

							access =
								VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

							//ATTACHMENT_OPTIMAL rather than the depth specific layout: it is valid for a depth
							// attachment too, and it is the one that agrees with colour attachment access, which
							// the validator requires the pairing to match.

							layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

							break;

						case ETransitionType_RenderTargetWrite:

							//Depth is written by the depth test at both fragment test stages AND by the
							// attachment's storeOp, which the spec places at LATE_FRAGMENT_TESTS. This stage
							// is what the NEXT barrier uses as its source, so leaving LATE out lets a later
							// read of the image race the store (a write after write hazard the validator
							// reports the moment a render pass' depth is sampled by a shader afterwards).

							pipelineStage =
								isDepthStencil ?
								VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT :
								VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

							access =
								isDepthStencil ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
								VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT :
								VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
								VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

							layout =
								isDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
								VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

							break;

						case ETransitionType_Clear:
							pipelineStage = VK_PIPELINE_STAGE_2_CLEAR_BIT;
							access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
							layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							break;

						case ETransitionType_CopyRead:
							pipelineStage =  VK_PIPELINE_STAGE_2_COPY_BIT;
							access = VK_ACCESS_2_TRANSFER_READ_BIT;
							layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
							break;

						case ETransitionType_CopyWrite:
							pipelineStage =  VK_PIPELINE_STAGE_2_COPY_BIT;
							access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
							layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							break;

						case ETransitionType_Indirect:
							pipelineStage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
							access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
							break;

						case ETransitionType_Predicate:
							pipelineStage = VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT;
							access = VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT;
							break;

						case ETransitionType_Index:
							pipelineStage = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
							access = VK_ACCESS_2_INDEX_READ_BIT;
							break;

						case ETransitionType_Vertex:
							pipelineStage = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
							access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
							break;

						default:
							break;
					}

				//Transition resource

				if(isImage) {

					const UnifiedTexture utex = TextureRef_getUnifiedTexture(transition.resource, NULL);
					VkUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(transition.resource, Vk, 0);

					VkImageSubresourceRange range = (VkImageSubresourceRange) {        //TODO:
						.aspectMask = isDepthStencil ? 0 : VK_IMAGE_ASPECT_COLOR_BIT,
						.levelCount = 1,
						.layerCount = 1
					};

					if(isDepthStencil) {

						if(utex.depthFormat >= EDepthStencilFormat_StencilStart)
							range.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

						if(utex.depthFormat != EDepthStencilFormat_S8X24Ext)
							range.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
					}

					gotoIfError3(nextTransition, VkUnifiedTexture_transition(

						imageExt,
						pipelineStage,
						access,
						layout,
						graphicsQueueId,
						&range,

						&deviceExt->imageTransitions,
						&dependency, alloc, e_rr));
				}

				else {

					DeviceBuffer *devBuffer = DeviceBufferRef_ptr(transition.resource);
					VkDeviceBuffer *bufferExt = DeviceBuffer_ext(devBuffer, Vk);

					gotoIfError3(nextTransition, VkDeviceBuffer_transition(
						bufferExt,
						pipelineStage,
						access,
						graphicsQueueId,
						0,                        //TODO: range
						devBuffer->resource.size,
						&deviceExt->bufferTransitions,
						&dependency, alloc, e_rr));
				}

			nextTransition:

				if(!s_uccess)
					Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
			}

			if(dependency.imageMemoryBarrierCount || dependency.bufferMemoryBarrierCount)
				deviceExt->cmdPipelineBarrier2(buffer, &dependency);

			ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions, e_rr);
			ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions, e_rr);

			//AFTER the barriers, so the predicate write this scope waits on is visible to the read; the
			// span closes at EndScope. Without the extension the scope runs unconditionally, as documented.

			if((temp->curScopeFlags & ECommandScopeInternalFlags_Predicated) && deviceExt->cmdBeginConditionalRendering) {

				const CommandScopePredicate pred = *(const CommandScopePredicate*) data;

				VkConditionalRenderingBeginInfoEXT cond = (VkConditionalRenderingBeginInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
					.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(pred.buffer), Vk)->buffer,
					.offset = pred.offset
				};

				deviceExt->cmdBeginConditionalRendering(buffer, &cond);
			}

			break;
		}

		//Debug markers

		case ECommandOp_EndRegionDebugExt:

			if(instanceExt->cmdDebugMarkerEnd)
				instanceExt->cmdDebugMarkerEnd(buffer);

			break;

		case ECommandOp_AddMarkerDebugExt:
		case ECommandOp_StartRegionDebugExt: {

			VkDebugUtilsLabelEXT markerInfo = (VkDebugUtilsLabelEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
				.pLabelName = (const char*) data + sizeof(F32x4),
			};

			Buffer_memcpy(
				Buffer_createRef(&markerInfo.color, sizeof(F32x4)),
				Buffer_createRefConst(data, sizeof(F32x4))
			);

			if(!instanceExt->cmdDebugMarkerInsert)        //No debug markers
				break;

			if(op == ECommandOp_AddMarkerDebugExt)
				instanceExt->cmdDebugMarkerInsert(buffer, &markerInfo);

			else instanceExt->cmdDebugMarkerBegin(buffer, &markerInfo);

			break;
		}

		//Emits nothing itself; the end timestamp write lands here in the measurement pass.

		case ECommandOp_StartTimingRegion:
		case ECommandOp_EndTimingRegion:
		case ECommandOp_InsertTiming:
			vkTimestampWrite(deviceExt, device->fifId, buffer);
			break;

		//The end timestamp and end debug region of a scope; the barriers ran at StartScope.

		case ECommandOp_EndScope:

			if((temp->curScopeFlags & ECommandScopeInternalFlags_Predicated) && deviceExt->cmdEndConditionalRendering)
				deviceExt->cmdEndConditionalRendering(buffer);

			if((temp->curScopeFlags & ECommandScopeInternalFlags_Timed))
				vkTimestampWrite(deviceExt, device->fifId, buffer);

			if((temp->curScopeFlags & ECommandScopeInternalFlags_DebugRegion) && instanceExt->cmdDebugMarkerEnd)
				instanceExt->cmdDebugMarkerEnd(buffer);

			break;

		//Unsupported

		default:
			Log_errorLnx("Unsupported command issued.");
			break;
	}
}
