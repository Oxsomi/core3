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
#include "formats/oiSH/binaries.h"

#ifdef __cplusplus
	extern "C" {
#endif

//File spec (docs/oiSH.md)

typedef enum ESHVersion {
	ESHVersion_Undefined,
	ESHVersion_V0_1,		//Unsupported
	ESHVersion_V1_2			//Current
} ESHVersion;

typedef struct BinaryInfoFixedSize {

	U8 shaderModel;				//U4 major, minor
	U8 entrypointType;			//ESHPipelineStage: See entrypointType section in oiSH.md
	U16 entrypoint;				//U16_MAX if library, otherwise index into stageNames

	U16 vendorMask;				//Bitset of ESHVendor
	U8 uniformCount;
	U8 binaryFlags;				//ESHBinaryFlags

	ESHExtension extensions;	//&~ dormantExt = used extensions, this is what the shader was compiled with

	ESHExtension dormantExt;	//Dormant extensions (not detected in final executable)

	U16 registerCount;
	U16 padding;

} BinaryInfoFixedSize;

typedef struct EntryInfoFixedSize {
	U8 pipelineStage;			//ESHPipelineStage
	U8 binaryCount;				//How many binaries this entrypoint references
} EntryInfoFixedSize;

typedef struct SHHeader {

	U32 compilerVersion;

	U32 hash;					//CRC32C of contents starting at SHHeader::version

	U32 sourceHash;				//CRC32C of source(s), for determining if it's dirty

	U16 uniqueUniforms;
	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major)) at least 1
	U8 sizeTypes;				//Every 2 bits size type of spirv, dxil

	U16 binaryCount;			//How many unique binaries are stored
	U16 stageCount;				//How many stages reference into the binaries

	U16 includeFileCount;		//Number of (recursive) include files
	U16 semanticCount;

	U16 arrayDimCount;
	U16 registerNameCount;

} SHHeader;

#define SHHeader_MAGIC 0x4853696F

#ifdef __cplusplus
	}
#endif
