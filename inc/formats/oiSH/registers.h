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
#include "formats/oiSB/sb_file.h"
#include "types/container/texture_format.h"

#ifdef ALLOW_SH_OXC3_PLATFORMS
	#include "platforms/ext/listx.h"
#endif

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ESHBinaryType {

	ESHBinaryType_SPIRV,
	ESHBinaryType_DXIL,

	//ESHBinaryType_MSL,
	//ESHBinaryType_WGSL,

	ESHBinaryType_Count

} ESHBinaryType;

typedef enum ESHBufferType {
	ESHBufferType_ConstantBuffer,					//UBO or CBuffer
	ESHBufferType_ByteAddressBuffer,
	ESHBufferType_StructuredBuffer,
	ESHBufferType_StructuredBufferAtomic,			//SBuffer + atomic counter
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

typedef enum ESHTexturePrimitive {

	ESHTexturePrimitive_UInt,
	ESHTexturePrimitive_SInt,
	ESHTexturePrimitive_UNorm,
	ESHTexturePrimitive_SNorm,
	ESHTexturePrimitive_Float,
	ESHTexturePrimitive_Double,
	ESHTexturePrimitive_Count,

	ESHTexturePrimitive_TypeMask		= 0x0F,

	ESHTexturePrimitive_ComponentShift	= 4,

	ESHTexturePrimitive_Component1		= 0x00,		//R
	ESHTexturePrimitive_Component2		= 0x10,		//RG
	ESHTexturePrimitive_Component3		= 0x20,		//RGB
	ESHTexturePrimitive_Component4		= 0x30,		//RGBA

	ESHTexturePrimitive_CountAll		= 0x40,

	ESHTexturePrimitive_Unused			= 0xC0

} ESHTexturePrimitive;

extern const C8 *ESHTexturePrimitive_name[ESHTexturePrimitive_CountAll];

typedef struct SHBinding {
	U32 space;						//Space or set, depending on binary type
	U32 binding;
} SHBinding;

//U32_MAX for both space and binding indicates 'not present'
typedef union SHBindings {
	SHBinding arr[ESHBinaryType_Count];
	U64 arrU64[ESHBinaryType_Count];
} SHBindings;

SHBindings SHBindings_dummy();

typedef struct SHTextureFormat {	//Primitive is set for DXIL always and formatId is only for SPIRV (only when RW)
	U8 primitive;					//Optional for readonly registers: ESHTexturePrimitive must match format approximately
	U8 formatId;					//Optional for write registers: ETextureFormatId Must match formatPrimitive and uncompressed
} SHTextureFormat;

typedef enum ESHRegisterType {

	ESHRegisterType_Sampler,
	ESHRegisterType_SamplerComparisonState,

	ESHRegisterType_ConstantBuffer,					//UBO or CBuffer
	ESHRegisterType_ByteAddressBuffer,
	ESHRegisterType_StructuredBuffer,
	ESHRegisterType_StructuredBufferAtomic,			//SBuffer + atomic counter
	ESHRegisterType_StorageBuffer,
	ESHRegisterType_StorageBufferAtomic,
	ESHRegisterType_AccelerationStructure,

	ESHRegisterType_Texture1D,
	ESHRegisterType_Texture2D,
	ESHRegisterType_Texture3D,
	ESHRegisterType_TextureCube,
	ESHRegisterType_Texture2DMS,
	ESHRegisterType_SubpassInput,

	ESHRegisterType_Count,

	ESHRegisterType_BufferStart			= ESHRegisterType_ConstantBuffer,
	ESHRegisterType_BufferEnd			= ESHRegisterType_AccelerationStructure,

	ESHRegisterType_TextureStart		= ESHRegisterType_Texture1D,
	ESHRegisterType_TextureEnd			= ESHRegisterType_SubpassInput,		//>= to see if real texture, > means 'texture'-like

	ESHRegisterType_TypeMask			= 0xF,
	ESHRegisterType_IsArray				= 1 << 4,	//Only valid on textures
	ESHRegisterType_IsCombinedSampler	= 1 << 5,	//^

	//Invalid on samplers, AS and CBuffer
	//Required on append/consume buffer
	//Valid on everything else (textures and various buffers)
	ESHRegisterType_IsWrite				= 1 << 6,

	ESHRegisterType_Masks				=
	ESHRegisterType_IsArray | ESHRegisterType_IsCombinedSampler | ESHRegisterType_IsWrite

} ESHRegisterType;

typedef struct SHRegister {			//Treated as U64[N + 1]

	SHBindings bindings;			//Treated as U64[N]

	U8 registerType;				//ESHRegisterType
	U8 isUsedFlag;					//Per ESHBinaryType if the register is used

	union {
		U16 padding;				//Used for samplers, (RW)BAB or AS (should be 0)
		U16 shaderBufferId;			//Used only at serialization (Buffer registers only)
		U16 inputAttachmentId;		//U16_MAX indicates "nothing", otherwise <7, only valid for SubpassInput
		SHTextureFormat texture;	//Read/write textures
	};

	U16 arrayId;					//Used at serialization time only, can't be used on subpass inputs
	U16 nameId;

} SHRegister;

typedef struct SHRegisterRuntime {
	SHRegister reg;
	CharString name;
	ListU32 arrays;
	SBFile shaderBuffer;
	U64 hash;						//Only for identical, not for compatible!
} SHRegisterRuntime;

TList(SHRegister);
TList(SHRegisterRuntime);

Bool SHRegisterRuntime_hash(SHRegister reg, CharString name, ListU32 *arrays, SBFile *shaderBuffer, U64 *res, Error *e_rr);
Bool SHRegisterRuntime_createCopy(SHRegisterRuntime reg, Allocator alloc, SHRegisterRuntime *res, Error *e_rr);

Bool ListSHRegisterRuntime_createCopyUnderlying(
	ListSHRegisterRuntime orig,
	Allocator alloc,
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
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
);

Bool ListSHRegisterRuntime_addTexture(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	Bool isCombinedSampler,
	U8 isUsedFlag,
	ESHTexturePrimitive textureFormatPrimitive,		//ESHTexturePrimitive_Count = none
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
);

Bool ListSHRegisterRuntime_addRWTexture(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	U8 isUsedFlag,
	ESHTexturePrimitive textureFormatPrimitive,		//ESHTexturePrimitive_Count = auto detect from formatId
	ETextureFormatId textureFormatId,				//!textureFormatId = only allowed if primitive is set
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
);

Bool ListSHRegisterRuntime_addSubpassInput(
	ListSHRegisterRuntime *registers,
	U8 isUsedFlag,
	CharString *name,
	SHBindings bindings,
	U16 attachmentId,
	Allocator alloc,
	Error *e_rr
);

Bool ListSHRegisterRuntime_addSampler(
	ListSHRegisterRuntime *registers,
	U8 isUsedFlag,
	Bool isSamplerComparisonState,
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
);

Bool ListSHRegisterRuntime_addRegister(
	ListSHRegisterRuntime *registers,
	CharString *name,
	ListU32 *arrays,
	SHRegister reg,
	SBFile *sbFile,
	Allocator alloc,
	Error *e_rr
);

void SHRegister_print(SHRegister reg, U64 indenting, Allocator alloc);
void SHRegisterRuntime_print(SHRegisterRuntime reg, U64 indenting, Allocator alloc);
void ListSHRegisterRuntime_print(ListSHRegisterRuntime reg, U64 indenting, Allocator alloc);
void SHRegisterRuntime_free(SHRegisterRuntime *reg, Allocator alloc);
void ListSHRegisterRuntime_freeUnderlying(ListSHRegisterRuntime *reg, Allocator alloc);

#ifdef ALLOW_SH_OXC3_PLATFORMS

	void SHRegister_printx(SHRegister reg, U64 indenting);
	void SHRegisterRuntime_printx(SHRegisterRuntime reg, U64 indenting);
	void ListSHRegisterRuntime_printx(ListSHRegisterRuntime reg, U64 indenting);

	Bool ListSHRegisterRuntime_createCopyUnderlyingx(ListSHRegisterRuntime orig, ListSHRegisterRuntime *dst, Error *e_rr);

	Bool ListSHRegisterRuntime_addBufferx(
		ListSHRegisterRuntime *registers,
		ESHBufferType registerType,
		Bool isWrite,
		U8 isUsedFlag,
		CharString *name,
		ListU32 *arrays,
		SBFile *sbFile,
		SHBindings bindings,
		Error *e_rr
	);

	Bool ListSHRegisterRuntime_addTexturex(
		ListSHRegisterRuntime *registers,
		ESHTextureType registerType,
		Bool isLayeredTexture,
		Bool isCombinedSampler,
		U8 isUsedFlag,
		ESHTexturePrimitive textureFormatPrimitive,		//ESHTexturePrimitive_Count = none
		CharString *name,
		ListU32 *arrays,
		SHBindings bindings,
		Error *e_rr
	);

	Bool ListSHRegisterRuntime_addRWTexturex(
		ListSHRegisterRuntime *registers,
		ESHTextureType registerType,
		Bool isLayeredTexture,
		U8 isUsedFlag,
		ESHTexturePrimitive textureFormatPrimitive,		//ESHTexturePrimitive_Count = auto detect from formatId
		ETextureFormatId textureFormatId,				//!textureFormatId = only allowed if primitive is set
		CharString *name,
		ListU32 *arrays,
		SHBindings bindings,
		Error *e_rr
	);

	Bool ListSHRegisterRuntime_addSubpassInputx(
		ListSHRegisterRuntime *registers,
		U8 isUsedFlag,
		CharString *name,
		SHBindings bindings,
		U16 attachmentId,
		Error *e_rr
	);

	Bool ListSHRegisterRuntime_addSamplerx(
		ListSHRegisterRuntime *registers,
		U8 isUsedFlag,
		Bool isSamplerComparisonState,
		CharString *name,
		ListU32 *arrays,
		SHBindings bindings,
		Error *e_rr
	);

	Bool ListSHRegisterRuntime_addRegisterx(
		ListSHRegisterRuntime *registers,
		CharString *name,
		ListU32 *arrays,
		SHRegister reg,
		SBFile *sbFile,
		Error *e_rr
	);

#endif

#ifdef __cplusplus
	}
#endif
