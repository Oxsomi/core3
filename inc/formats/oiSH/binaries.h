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

#pragma once
#include "formats/oiSH/registers.h"

#ifdef __cplusplus
	extern "C" {
#endif

extern const C8 *ESHBinaryType_names[ESHBinaryType_Count];

typedef enum ESHExtension {

	ESHExtension_None						= 0,

	//These losely map to EGraphicsDataTypes in OxC3 graphics

	ESHExtension_F64						= 1 << 0,
	ESHExtension_I64						= 1 << 1,
	ESHExtension_16BitTypes					= 1 << 2,		//I16, F16

	ESHExtension_AtomicI64					= 1 << 3,
	ESHExtension_AtomicF32					= 1 << 4,
	ESHExtension_AtomicF64					= 1 << 5,

	//Some of them are present in EGraphicsFeatures in OxC3 graphics

	ESHExtension_SubgroupArithmetic			= 1 << 6,
	ESHExtension_SubgroupShuffle			= 1 << 7,

	ESHExtension_RayQuery					= 1 << 8,
	ESHExtension_RayMicromapOpacity			= 1 << 9,
	ESHExtension_RayMicromapDisplacement	= 1 << 10,
	ESHExtension_RayMotionBlur				= 1 << 11,
	ESHExtension_RayReorder					= 1 << 12,

	ESHExtension_Multiview					= 1 << 13,
	ESHExtension_ComputeDeriv				= 1 << 14,

	ESHExtension_PAQ						= 1 << 15,		//Payload access qualifiers

	ESHExtension_MeshTaskTexDeriv			= 1 << 16,

	ESHExtension_WriteMSTexture				= 1 << 17,

	ESHExtension_Bindless					= 1 << 18,
	ESHExtension_UnboundArraySize			= 1 << 19,

	ESHExtension_SubgroupOperations			= 1 << 20,

	ESHExtension_DxilNative =								//Extensions that can be found from DXIL natively
		ESHExtension_RayQuery |
		ESHExtension_16BitTypes |
		ESHExtension_I64 |
		ESHExtension_Multiview |
		ESHExtension_F64 |
		ESHExtension_AtomicI64 |
		ESHExtension_MeshTaskTexDeriv |
		ESHExtension_WriteMSTexture |
		ESHExtension_SubgroupOperations,

	ESHExtension_SpirvNative =								//Extensions that map directly to SPIRV capabilities
		ESHExtension_RayMicromapOpacity |
		ESHExtension_RayQuery |
		ESHExtension_RayMotionBlur |
		ESHExtension_RayReorder |
		ESHExtension_AtomicF32 |
		ESHExtension_AtomicF64 |
		ESHExtension_SubgroupArithmetic |
		ESHExtension_SubgroupShuffle |
		ESHExtension_SubgroupOperations |
		ESHExtension_Multiview |
		ESHExtension_16BitTypes |
		ESHExtension_F64 |
		ESHExtension_I64 |
		ESHExtension_AtomicI64 |
		ESHExtension_ComputeDeriv |
		ESHExtension_WriteMSTexture,

	ESHExtension_Count						= 21,

	ESHExtension_All						= (1 << ESHExtension_Count) - 1

} ESHExtension;

extern const C8 *ESHExtension_defines[ESHExtension_Count];
extern const C8 *ESHExtension_names[ESHExtension_Count];

typedef enum ESHVendor {
	ESHVendor_NV,
	ESHVendor_AMD,
	ESHVendor_ARM,
	ESHVendor_QCOM,
	ESHVendor_INTC,
	ESHVendor_IMGT,
	ESHVendor_MSFT,
	ESHVendor_Count
} ESHVendor;

typedef enum ESHBinaryFlags {

	ESHBinaryFlags_None 					= 0,

	//What type of binaries it includes
	//Must be one at least

	ESHBinaryFlags_HasSPIRV					= 1 << 0,
	ESHBinaryFlags_HasDXIL					= 1 << 1,

	//Reserved
	//ESHBinaryFlags_HasMSL					= 1 << 2,
	//ESHBinaryFlags_HasWGSL				= 1 << 3

	ESHBinaryFlags_HasShaderAnnotation		= 1 << 4,

	ESHBinaryFlags_HasBinary				= ESHBinaryFlags_HasSPIRV | ESHBinaryFlags_HasDXIL,
	//ESHBinaryFlags_HasText				= ESHBinaryFlags_HasMSL | ESHBinaryFlags_HasWGSL
	ESHBinaryFlags_HasSource				= ESHBinaryFlags_HasBinary // | ESHBinaryFlags_HasText

} ESHBinaryFlags;

extern const C8 *ESHVendor_names[ESHVendor_Count];

typedef struct SHBinaryIdentifier {

	CharString entrypoint;		//If it's not a lib, this defines the entrypoint to compile with
	ListCharString uniforms;	//[uniformName, uniformValue][]

	//Don't change order, is used for compare (U64)

	ESHExtension extensions;
	U16 shaderVersion;			//U8 maj, minor
	U16 stageType;				//ESHPipelineStage

} SHBinaryIdentifier;

typedef struct SHBinaryInfo {

	SHBinaryIdentifier identifier;

	ListSHRegisterRuntime registers;

	ESHExtension dormantExtensions;
	U16 vendorMask;
	Bool hasShaderAnnotation;	//If [shader("")] is used rather than [[oxc::stage("")]]
	U8 padding[1];

	Buffer binaries[ESHBinaryType_Count];

} SHBinaryInfo;

TList(SHBinaryIdentifier);
TList(SHBinaryInfo);

Bool SHBinaryIdentifier_equals(SHBinaryIdentifier a, SHBinaryIdentifier b);
void SHBinaryInfo_print(SHBinaryInfo binary, Allocator alloc);
void SHBinaryIdentifier_free(SHBinaryIdentifier *identifier, Allocator alloc);
void SHBinaryInfo_free(SHBinaryInfo *info, Allocator alloc);

#ifndef DISALLOW_SH_OXC3_PLATFORMS
	void SHBinaryInfo_printx(SHBinaryInfo binary);
	void SHBinaryIdentifier_freex(SHBinaryIdentifier *identifier);
	void SHBinaryInfo_freex(SHBinaryInfo *info);
#endif

#ifdef __cplusplus
	}
#endif
