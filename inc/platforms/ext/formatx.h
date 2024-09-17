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
#include "types/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;

typedef struct CASettings CASettings;
typedef struct CAFile CAFile;
typedef struct ArchiveEntry ArchiveEntry;

typedef struct DLSettings DLSettings;
typedef struct DLFile DLFile;

typedef enum ESHSettingsFlags ESHSettingsFlags;
typedef enum ESHExtension ESHExtension;
typedef enum ESHBinaryType ESHBinaryType;

typedef struct SHEntry SHEntry;
typedef struct SHBinaryIdentifier SHBinaryIdentifier;
typedef struct SHBinaryInfo SHBinaryInfo;
typedef struct SHEntryRuntime SHEntryRuntime;
typedef struct SHInclude SHInclude;
typedef struct SHFile SHFile;

typedef struct SHRegister SHRegister;
typedef struct SHRegisterRuntime SHRegisterRuntime;
typedef struct ListSHRegisterRuntime ListSHRegisterRuntime;
typedef union SHBindings SHBindings;
typedef enum ESHTextureType ESHTextureType;
typedef enum ESHTexturePrimitive ESHTexturePrimitive;
typedef enum ESHBufferType ESHBufferType;
typedef enum ETextureFormatId ETextureFormatId;

typedef struct SBFile SBFile;
typedef struct SBStruct SBStruct;
typedef enum ESBSettingsFlags ESBSettingsFlags;
typedef enum ESBType ESBType;
typedef enum ESBVarFlag ESBVarFlag;

typedef struct ListCharString ListCharString;
typedef struct ListSHEntryRuntime ListSHEntryRuntime;
typedef struct ListSHInclude ListSHInclude;
typedef struct ListSBFile ListSBFile;

//oiCA

Bool CAFile_freex(CAFile *caFile);

Bool CAFile_writex(CAFile caFile, Buffer *result, Error *e_rr);
Bool CAFile_readx(Buffer file, const U32 encryptionKey[8], CAFile *caFile, Error *e_rr);
Bool CAFile_combinex(CAFile a, CAFile b, CAFile *combined, Error *e_rr);

//oiDL

Bool DLFile_createx(DLSettings settings, DLFile *dlFile, Error *e_rr);
Bool DLFile_freex(DLFile *dlFile);

Bool DLFile_createListx(DLSettings settings, ListBuffer *buffers, DLFile *dlFile, Error *e_rr);
Bool DLFile_createUTF8Listx(DLSettings settings, ListBuffer buffers, DLFile *dlFile, Error *e_rr);
Bool DLFile_createBufferListx(DLSettings settings, ListBuffer buffers, DLFile *dlFile, Error *e_rr);
Bool DLFile_createAsciiListx(DLSettings settings, ListCharString strings, DLFile *dlFile, Error *e_rr);

Bool DLFile_addEntryx(DLFile *dlFile, Buffer entry, Error *e_rr);
Bool DLFile_addEntryAsciix(DLFile *dlFile, CharString entry, Error *e_rr);
Bool DLFile_addEntryUTF8x(DLFile *dlFile, Buffer entry, Error *e_rr);

Bool DLFile_writex(DLFile dlFile, Buffer *result, Error *e_rr);
Bool DLFile_readx(Buffer file, const U32 encryptionKey[8], Bool allowLeftOverData, DLFile *dlFile, Error *e_rr);
Bool DLFile_combinex(DLFile a, DLFile b, DLFile *combined, Error *e_rr);

//oiSH

Bool SHFile_createx(ESHSettingsFlags flags, U32 compilerVersion, U32 sourceHash, SHFile *shFile, Error *e_rr);
void SHFile_freex(SHFile *shFile);

Bool SHFile_addBinaryx(SHFile *shFile, SHBinaryInfo *binaries, Error *e_rr);	//Moves entry
Bool SHFile_addEntrypointx(SHFile *shFile, SHEntry *entry, Error *e_rr);		//Moves entry->name
Bool SHFile_addIncludex(SHFile *shFile, SHInclude *include, Error *e_rr);		//Moves include->relativePath

Bool SHFile_writex(SHFile shFile, Buffer *result, Error *e_rr);
Bool SHFile_readx(Buffer file, Bool isSubFile, SHFile *shFile, Error *e_rr);
Bool SHFile_combinex(SHFile a, SHFile b, SHFile *combined, Error *e_rr);

void SHEntry_printx(SHEntry entry);
void SHEntryRuntime_printx(SHEntryRuntime entry);
void SHBinaryInfo_printx(SHBinaryInfo binary);
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

void SHBinaryIdentifier_freex(SHBinaryIdentifier *identifier);
void SHBinaryInfo_freex(SHBinaryInfo *info);
void SHEntry_freex(SHEntry *entry);

void SHEntryRuntime_freex(SHEntryRuntime *entry);
void ListSHEntryRuntime_freeUnderlyingx(ListSHEntryRuntime *entry);

void ListSHInclude_freeUnderlyingx(ListSHInclude *includes);
void SHInclude_freex(SHInclude *include);

//oiSB

Bool SBFile_createx(
	ESBSettingsFlags flags,
	U32 bufferSize,
	SBFile *sbFile,
	Error *e_rr
);

Bool SBFile_createCopyx(SBFile src, SBFile *dst, Error *e_rr);

void SBFile_freex(SBFile *shFile);

Bool SBFile_addStructx(SBFile *sbFile, CharString *name, SBStruct sbStruct, Error *e_rr);

Bool SBFile_addVariableAsTypex(
	SBFile *sbFile,
	CharString *name,
	U32 offset,
	U16 parentId,		//root = U16_MAX
	ESBType type,
	ESBVarFlag flags,
	ListU32 *arrays,
	Error *e_rr
);

Bool SBFile_addVariableAsStructx(
	SBFile *sbFile,
	CharString *name,
	U32 offset,
	U16 parentId,		//root = U16_MAX
	U16 structId,
	ESBVarFlag flags,
	ListU32 *arrays,
	Error *e_rr
);

Bool SBFile_writex(SBFile sbFile, Buffer *result, Error *e_rr);
Bool SBFile_readx(Buffer file, Bool isSubFile, SBFile *sbFile, Error *e_rr);
void SBFile_printx(SBFile sbFile, U64 indenting, U16 parent, Bool isRecursive);

void ListSBFile_freeUnderlyingx(ListSBFile *files);

//Bool SBFile_combinex(SBFile a, SBFile b, Allocator alloc, SBFile *combined, Error *e_rr);		TODO:

#ifdef __cplusplus
	}
#endif
