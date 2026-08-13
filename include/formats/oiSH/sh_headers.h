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

//formats/oiSH/sh_headers.h

#pragma once
#include "formats/oiSH/sh_binaries.h"

#ifdef __cplusplus
	extern "C" {
#endif

//File spec (docs/oiSH.md)

typedef enum ESHVersion {
	ESHVersion_Undefined,
	ESHVersion_V0_1,            //Unsupported
	ESHVersion_V1_2             //Current
} ESHVersion;

typedef struct BinaryInfoFixedSize {

	U8 shaderModel;             //U4 major, minor
	U8 entrypointType;          //ESHPipelineStage: See entrypointType section in oiSH.md
	U16 entrypoint;             //U16_MAX if library, otherwise index into stageNames

	U16 vendorMask;             //Bitset of ESHVendor
	U8 defineCount;
	U8 binaryFlags;             //ESHBinaryFlags

	ESHExtension extensions;    //&~ dormantExt = used extensions, this is what the shader was compiled with

	ESHExtension dormantExt;    //Dormant extensions (not detected in final executable)

	U16 registerCount;
	U8 uniformCount;
	U8 padding;

} BinaryInfoFixedSize;

typedef struct EntryInfoFixedSize {
	U8 pipelineStage;           //ESHPipelineStage
	U8 binaryCount;             //How many binaries this entrypoint references
} EntryInfoFixedSize;

typedef struct SHHeader {

	U32 compilerVersion;

	U32 hash;                   //CRC32C of contents starting at SHHeader::version

	U32 sourceHash;             //CRC32C of source(s), for determining if it's dirty

	U16 uniqueDefines;
	U8 version;                 //major.minor (%10 = minor, /10 = major (+1 to get real major)) at least 1
	U8 sizeTypes;               //Every 2 bits size type of spirv, dxil

	U16 binaryCount;            //How many unique binaries are stored
	U16 stageCount;             //How many stages reference into the binaries

	U16 includeFileCount;       //Number of (recursive) include files
	U16 semanticCount;

	U16 arrayDimCount;
	U16 registerNameCount;

	U16 uniformNameCount;
	U16 flags;                  //Persisted ESHSettingsFlags (currently just ReflectionOnly; HideMagicNumber is derived)

	//ESHExtension_Count of the writer.
	//Extensions added after a file was written can't have been reflected by its compiler, so the reader marks the
	// native ones at or above this count dormant automatically (see SHFile_read).
	U16 extensionCount;

	//ESHVendor_Count of the writer, for the same reason and with the opposite fix up.
	//A vendorMask is a bitset, so "every vendor" is spelled as all bits below the count set.
	//Once a vendor is added that spelling stops matching, and a binary meant for anything would start being
	// refused on the new vendor, so the reader widens a was-everything mask to today's everything.
	U16 vendorCount;

} SHHeader;

typedef struct SHGroups {
	U16 x, y, z, waveSize;
} SHGroups;

#define SHHeader_MAGIC 0x4853696F

#ifdef __cplusplus
	}
#endif
