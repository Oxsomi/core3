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

//formats/oiSH/sh_registers.h

#pragma once
#include "formats/oiSB/sb_file.h"
#include "formats/gfx_util/gfx_util.h"
#include "types/container/texture_format.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ESHBufferType {
	ESHBufferType_ConstantBuffer,                   //UBO or CBuffer
	ESHBufferType_PushConstants,                    //Push constants or CBuffer (DXIL)
	ESHBufferType_ByteAddressBuffer,
	ESHBufferType_StructuredBuffer,
	ESHBufferType_StructuredBufferAtomic,           //SBuffer + atomic counter
	ESHBufferType_StorageBuffer,
	ESHBufferType_StorageBufferAtomic,
	ESHBufferType_AccelerationStructure,
	ESHBufferType_Count
} ESHBufferType;

typedef enum ESHTextureType {
	ESHTextureType_Texture1D,
	ESHTextureType_Texture2D,
	ESHTextureType_Texture3D,
	ESHTextureType_TextureCube,
	ESHTextureType_Texture2DMS,
	ESHTextureType_Count
} ESHTextureType;

extern const C8 *EGfxTexturePrimitive_name[EGfxTexturePrimitive_CountAll];

EGfxTexturePrimitive EGfxTexturePrimitive_fromTextureFormat(ETextureFormat format);

typedef U8 SHRegisterType;

typedef struct SHRegister {            //Treated as U64[N + 1]

	GfxBindings bindings;               //Treated as U64[N]

	SHRegisterType registerType;
	U8 isUsedFlag;                     //Per EGfxBinaryType if the register is used

	union {
		U16 padding;                   //Used for samplers, (RW)BAB or AS (should be 0)
		U16 shaderBufferId;            //Used only at serialization (Buffer registers only)
		U16 inputAttachmentId;         //U16_MAX indicates "nothing", otherwise <7, only valid for SubpassInput
		GfxTextureFormat texture;      //Read/write textures
	};

	U16 arrayDimOrId;                  //<= 32767 represents a 1D array with a dimension, else represents an arrayId
	U16 nameId;

} SHRegister;

typedef struct SHRegisterRuntime {
	SHRegister reg;
	CharString name;
	ListU32 arrays;
	SBFile shaderBuffer;
	U64 hash;                          //Only for identical, not for compatible!
	U64 nameHash;                      //Hash of the register name only, for fast name matching (combine)
} SHRegisterRuntime;

TList(SHRegister);
TList(SHRegisterRuntime);

//Whether a register is part of the given binary type at all.
//Normally that is whether it has a binding, since a register the other backend bound on its own reads U64_MAX
// here (a standalone DXIL sampler, a SPIRV subpass input).
//A SPIRV push constant is the exception: Vulkan never binds it to a set, so the used flag is what says it is
// there, which is also why printing a register special cases it.
Bool SHRegister_isPresentIn(const SHRegister *reg, EGfxBinaryType type);

Bool SHRegisterRuntime_hash(
	const SHRegister *reg, const CharString *name, ListU32 *arrays, SBFile *shaderBuffer, U64 *res, Error *e_rr
);

Bool SHRegisterRuntime_createCopy(const SHRegisterRuntime *reg, const Allocator *alloc, SHRegisterRuntime *res, Error *e_rr);

Bool ListSHRegisterRuntime_createCopyUnderlying(
	const ListSHRegisterRuntime *orig,
	const Allocator *alloc,
	ListSHRegisterRuntime *dst,
	Error *e_rr
);

Bool ListSHRegisterRuntime_addBuffer(
	ListSHRegisterRuntime *registers,
	ESHBufferType registerType,
	Bool isWrite,
	U8 isUsedFlag,
	CharString *name,
	ListU32 *arrays,
	SBFile *sbFile,
	GfxBindings bindings,
	const Allocator *alloc,
	Error *e_rr
);

Bool ListSHRegisterRuntime_addTexture(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	Bool isCombinedSampler,
	U8 isUsedFlag,
	EGfxTexturePrimitive textureFormatPrimitive,        //EGfxTexturePrimitive_Count = none
	CharString *name,
	ListU32 *arrays,
	GfxBindings bindings,
	const Allocator *alloc,
	Error *e_rr
);

Bool ListSHRegisterRuntime_addRWTexture(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	U8 isUsedFlag,
	EGfxTexturePrimitive textureFormatPrimitive,        //EGfxTexturePrimitive_Count = auto detect from formatId
	ETextureFormatId textureFormatId,                  //!textureFormatId = only allowed if primitive is set
	CharString *name,
	ListU32 *arrays,
	GfxBindings bindings,
	const Allocator *alloc,
	Error *e_rr
);

Bool ListSHRegisterRuntime_addSubpassInput(
	ListSHRegisterRuntime *registers,
	U8 isUsedFlag,
	CharString *name,
	GfxBindings bindings,
	U16 attachmentId,
	const Allocator *alloc,
	Error *e_rr
);

Bool ListSHRegisterRuntime_addSampler(
	ListSHRegisterRuntime *registers,
	U8 isUsedFlag,
	Bool isSamplerComparisonState,
	CharString *name,
	ListU32 *arrays,
	GfxBindings bindings,
	const Allocator *alloc,
	Error *e_rr
);

//The flattened descriptor count of a register's array dims: 0 for an unbounded array (any 0 dim),
// saturating at U64_MAX so an absurd product can't wrap into a small bounded range.

U64 SHRegister_arrayCount(const ListU32 *arrays);

Bool ListSHRegisterRuntime_addRegister(
	ListSHRegisterRuntime *registers,
	CharString *name,
	ListU32 *arrays,
	SHRegister reg,
	SBFile *sbFile,
	const Allocator *alloc,
	Error *e_rr
);

void SHRegister_print(const SHRegister *reg, U64 indenting, Bool isVerbose, const Allocator *alloc);
void SHRegisterRuntime_print(const SHRegisterRuntime *reg, U64 indenting, Bool isVerbose, const Allocator *alloc);
void ListSHRegisterRuntime_print(const ListSHRegisterRuntime *reg, U64 indenting, Bool isVerbose, const Allocator *alloc);

void SHRegisterRuntime_free(SHRegisterRuntime *reg, const Allocator *alloc);
void ListSHRegisterRuntime_freeUnderlying(ListSHRegisterRuntime *reg, const Allocator *alloc);

#ifdef __cplusplus
	}
#endif
