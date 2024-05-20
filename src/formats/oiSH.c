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

#include "types/list_impl.h"
#include "formats/oiSH.h"
#include "formats/oiDL.h"
#include "types/math.h"

TListImpl(SHEntry);
TListImpl(SHEntryRuntime);

static const U8 SHHeader_V1_2 = 2;

const C8 *SHEntry_stageNames[] = {

	"vertex",
	"pixel",
	"compute",
	"geometry",
	"hull",
	"domain",

	"mesh",
	"task",

	"node",

	"raygeneration",
	"callable",
	"miss",
	"closesthit",
	"anyhit",
	"intersection",
};

const C8 *SHEntry_stageName(SHEntry entry) {
	return SHEntry_stageNames[entry.stage];
}

const C8 *ESHType_names[] = {
	"", "", "", "",
	"F32",
	"F32x2",
	"F32x3",
	"F32x4",
	"I32",
	"I32x2",
	"I32x3",
	"I32x4",
	"U32",
	"U32x2",
	"U32x3",
	"U32x4"
};

const C8 *ESHType_name(ESHType type) {
	return ESHType_names[type & 0xF];
}

//Helper functions to create it

Error SHFile_create(ESHSettingsFlags flags, ESHExtension extension, Allocator alloc, SHFile *shFile) {

	if(!shFile)
		return Error_nullPointer(0, "SHFile_create()::shFile is required");

	if(shFile->entries.ptr)
		return Error_invalidOperation(0, "SHFile_create()::shFile isn't empty, might indicate memleak");

	if(flags & ESHSettingsFlags_Invalid)
		return Error_invalidParameter(0, 3, "SHFile_create()::flags contained unsupported flag");

	if(extension >> ESHExtension_Count)
		return Error_invalidParameter(0, 1, "SHFile_create()::extension one was unsupported");

	Error err = ListSHEntry_reserve(&shFile->entries, 16, alloc);

	if(err.genericError)
		return err;

	shFile->flags = flags;
	shFile->extensions = extension;
	return Error_none();
}

Bool SHFile_free(SHFile *shFile, Allocator alloc) {

	if(!shFile || !shFile->entries.ptr)
		return true;

	for(U64 i = 0; i < shFile->entries.length; ++i)
		CharString_free(&shFile->entries.ptrNonConst[i].name, alloc);

	for(U64 i = 0; i < ESHBinaryType_Count; ++i)
		Buffer_free(&shFile->binaries[i], alloc);

	ListSHEntry_free(&shFile->entries, alloc);

	*shFile = (SHFile) { 0 };
	return true;
}

//Writing

Error SHFile_addBinary(SHFile *shFile, ESHBinaryType type, Buffer *entry, Allocator alloc) {

	if(!shFile || !entry || !Buffer_length(*entry))
		return Error_nullPointer(!shFile ? 0 : 2, "SHFile_addBinary()::shFile, *entry and entry are required");

	if(type >= ESHBinaryType_Count)
		return Error_invalidEnum(1, (U64)type, (U64)ESHBinaryType_Count, "SHFile_addBinary()::type is invalid");

	if(type == ESHBinaryType_SPIRV && Buffer_length(*entry) & 3)
		return Error_invalidParameter(2, 0, "SHFile_addBinary()::*entry needs to be U32[] for SPIRV");

	Buffer *result = &shFile->binaries[type];
	Error err = Error_none();

	if(result->ptr)
		return Error_alreadyDefined(0, "SHFile_addBinary() can't call on already defined binary type");

	if(Buffer_isRef(*entry))
		gotoIfError(clean, Buffer_createCopy(*entry, alloc, result))

	else *result = *entry;

	*entry = Buffer_createNull();

clean:
	return err;
}

Error SHFile_addEntrypoint(SHFile *shFile, SHEntry *entry, Allocator alloc) {

	if (!shFile || !shFile->entries.ptr || !entry)
		return Error_nullPointer(!shFile ? 0 : 1, "SHFile_addEntrypoint()::shFile and entry are required");

	if(!CharString_length(entry->name))
		return Error_nullPointer(1, "SHFile_addEntrypoint()::entry->name is required");

	if(entry->stage >= ESHPipelineStage_Count)
		return Error_invalidEnum(1, entry->stage, ESHPipelineStage_Count, "SHFile_addEntrypoint()::entry->stage invalid enum");

	ESHPipelineType pipelineType = entry->stage == ESHPipelineStage_Compute ? ESHPipelineType_Compute : (
		entry->stage >= ESHPipelineStage_RtStartExt && entry->stage <= ESHPipelineStage_RtEndExt ? ESHPipelineType_Raytracing :
		ESHPipelineType_Graphics
	);

	U16 groupXYZ = entry->groupX | entry->groupY | entry->groupZ;
	U64 totalGroup = (U64)entry->groupX * entry->groupY * entry->groupZ;

	if(pipelineType != ESHPipelineType_Compute && groupXYZ)
		return Error_invalidOperation(2, "SHFile_addEntrypoint() can't have group size for non compute");

	if(pipelineType == ESHPipelineType_Compute && !groupXYZ)
		return Error_invalidOperation(2, "SHFile_addEntrypoint() needs group size for compute");

	if(totalGroup > 512)
		return Error_invalidOperation(2, "SHFile_addEntrypoint() group count out of bounds (512)");

	if(U16_max(entry->groupX, entry->groupY) > 512)
		return Error_invalidOperation(2, "SHFile_addEntrypoint() group count x or y out of bounds (512)");

	if(entry->groupZ > 64)
		return Error_invalidOperation(2, "SHFile_addEntrypoint() group count z out of bounds (64)");

	if(
		entry->stage == ESHPipelineStage_ClosestHitExt || entry->stage == ESHPipelineStage_AnyHitExt ||
		entry->stage == ESHPipelineStage_IntersectionExt
	) {
		if(!entry->payloadSize)
			return Error_invalidOperation(2, "SHFile_addEntrypoint() payloadSize is required for hit/intersection shaders");
	}

	else if(entry->payloadSize)
		return Error_invalidOperation(2, "SHFile_addEntrypoint() payloadSize is only required for hit/intersection shaders");

	if(!entry->intersectionSize) {

		if(entry->stage == ESHPipelineStage_IntersectionExt)
			return Error_invalidOperation(2, "SHFile_addEntrypoint() intersectionSize is required for intersection shader");
	}

	else if(entry->stage != ESHPipelineStage_IntersectionExt)
		return Error_invalidOperation(2, "SHFile_addEntrypoint() intersectionSize is only required for intersection shader");

	if (pipelineType != ESHPipelineType_Graphics && (entry->outputsU64 | entry->inputsU64))
		return Error_invalidOperation(3, "SHFile_addEntrypoint() outputsU64 and inputsU64 are only for graphics shaders");

	//Check validity of inputs and outputs

	if(entry->outputsU64 | entry->inputsU64)
		for (U64 i = 0; i < 16; ++i) {

			U8 vout = (entry->outputs[i >> 1] >> ((i & 1) << 2)) & 0xF;
			U8 vin = (entry->inputs[i >> 1] >> ((i & 1) << 2)) & 0xF;

			if((vout && vout < ESHType_F32) || (vin && vin < ESHType_F32))
				return Error_invalidOperation(
					3, "SHFile_addEntrypoint() outputs or inputs contains an invalid parameter"
				);
		}

	Error err = Error_none();
	CharString copy = CharString_createNull();
	CharString oldName = entry->name;

	if (CharString_isRef(entry->name)) {
		gotoIfError(clean, CharString_createCopy(entry->name, alloc, &copy))
		entry->name = copy;
	}

	gotoIfError(clean, ListSHEntry_pushBack(&shFile->entries, *entry, alloc))

	if(!Buffer_isAscii(CharString_bufferConst(entry->name)))
		shFile->flags |= ESHSettingsFlags_IsUTF8;

	*entry = (SHEntry) { 0 };

clean:

	if(err.genericError && copy.ptr) {
		CharString_free(&copy, alloc);
		entry->name = oldName;
	}

	return err;
}

Error SHFile_write(SHFile shFile, Allocator alloc, Buffer *result) {

	if(!shFile.entries.ptr)
		return Error_nullPointer(0, "SHFile_write()::shFile is required");

	if(!result)
		return Error_nullPointer(2, "SHFile_write()::result is required");

	if(result->ptr)
		return Error_invalidOperation(0, "SHFile_write()::result wasn't empty, might indicate memleak");

	//Get header size

	U64 headerSize = sizeof(SHHeader);

	if(!(shFile.flags & ESHSettingsFlags_HideMagicNumber))		//Magic number (can be hidden by parent; such as oiSC)
		headerSize += sizeof(U32);

	//Get data size

	Bool isUTF8 = shFile.flags & ESHSettingsFlags_IsUTF8;

	DLSettings settings = (DLSettings) {
		.dataType = isUTF8 ? EDLDataType_UTF8 : EDLDataType_Ascii,
		.flags = EDLSettingsFlags_HideMagicNumber
	};

	DLFile dlFile = (DLFile) { 0 };
	Buffer dlFileBuf = Buffer_createNull();
	Error err = Error_none();
	gotoIfError(clean, DLFile_create(settings, alloc, &dlFile))

	for (U64 i = 0; i < shFile.entries.length; ++i) {

		CharString entry = shFile.entries.ptr[i].name;

		if(isUTF8)
			gotoIfError(clean, DLFile_addEntryUTF8(&dlFile, CharString_bufferConst(entry), alloc))

		else gotoIfError(clean, DLFile_addEntryAscii(&dlFile, entry, alloc))

		switch(shFile.entries.ptr[i].stage) {

			default:
				headerSize += sizeof(U64) * 2;			//input, output
				break;

			case ESHPipelineStage_Compute:
				headerSize += sizeof(U16) * 4;			//group x, y, z, pad
				break;

			case ESHPipelineStage_RaygenExt:
			case ESHPipelineStage_CallableExt:
			case ESHPipelineStage_WorkgraphExt:
				break;

			case ESHPipelineStage_MissExt:
			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:
			case ESHPipelineStage_IntersectionExt:
				headerSize += sizeof(U8) * 2 * shFile.entries.length;
				break;
		}
	}

	gotoIfError(clean, DLFile_write(dlFile, alloc, &dlFileBuf))

	U64 len = headerSize + Buffer_length(dlFileBuf) + shFile.entries.length;

	U8 hasBinary = 0;
	U8 sizes = 0;

	for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

		U64 leni = Buffer_length(shFile.binaries[i]);
		len += leni;

		if(leni) {
			hasBinary |= 1 << i;
			sizes |= EXXDataSizeType_getRequiredType(leni) << (i << 1);
		}
	}

	gotoIfError(clean, Buffer_createUninitializedBytes(len, alloc, result))

	//Prepend header and hash

	U8 *headerIt = (U8*)result->ptr;

	if (!(dlFile.settings.flags & EDLSettingsFlags_HideMagicNumber)) {
		*(U32*)headerIt = SHHeader_MAGIC;
		headerIt += sizeof(U32);
	}

	*((SHHeader*)headerIt) = (SHHeader) {

		.version = SHHeader_V1_2,
		.flags = hasBinary,
		.sizeTypes = sizes,

		.extensions = shFile.extensions
	};

	headerIt += sizeof(DLHeader);

	Buffer_copy(Buffer_createRef(headerIt, Buffer_length(dlFileBuf)), dlFileBuf);
	headerIt += Buffer_length(dlFileBuf);

	for (U64 i = 0; i < shFile.entries.length; ++i)
		headerIt[i] = (U8) shFile.entries.ptr[i].stage;

	headerIt += shFile.entries.length;

	for (U64 i = 0; i < shFile.entries.length; ++i) {

		SHEntry entry = shFile.entries.ptr[i];

		switch(entry.stage) {

			default:

				*(U64*)headerIt = entry.inputsU64;
				headerIt += sizeof(U64);

				*(U64*)headerIt = entry.outputsU64;
				headerIt += sizeof(U64);

				break;

			case ESHPipelineStage_Compute:
				*(U64*)headerIt = entry.groupX | ((U64)entry.groupY << 16) | ((U64)entry.groupZ << 32);
				headerIt += sizeof(U64);
				break;

			case ESHPipelineStage_RaygenExt:
			case ESHPipelineStage_CallableExt:
			case ESHPipelineStage_WorkgraphExt:
				break;

			case ESHPipelineStage_MissExt:
			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:
			case ESHPipelineStage_IntersectionExt:
				*headerIt++ = entry.intersectionSize;
				*headerIt++ = entry.payloadSize;
				break;
		}
	}

	for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

		U64 leni = Buffer_length(shFile.binaries[i]);

		if(leni)
			headerIt += Buffer_forceWriteSizeType(headerIt, (sizes >> (i << 1)) & 3, leni);
	}

	for (U64 i = 0; i < ESHBinaryType_Count; ++i) {
		U64 leni = Buffer_length(shFile.binaries[i]);
		Buffer_copy(Buffer_createRef(headerIt, leni), shFile.binaries[i]);
		headerIt += leni;
	}

clean:
	Buffer_free(&dlFileBuf, alloc);
	DLFile_free(&dlFile, alloc);
	return err;
}

Error SHFile_read(Buffer file, Bool isSubFile, Allocator alloc, SHFile *shFile) {

	if(!shFile)
		return Error_nullPointer(2, "SHFile_read()::shFile is required");

	if(shFile->entries.ptr)
		return Error_invalidOperation(0, "SHFile_read()::shFile isn't empty, might indicate memleak");

	Buffer entireFile = file;

	file = Buffer_createRefFromBuffer(file, true);
	Error err = Error_none();
	DLFile dlFile = (DLFile) { 0 };

	if (!isSubFile) {

		U32 magic;
		gotoIfError(clean, Buffer_consume(&file, &magic, sizeof(magic)))

		if(magic != SHHeader_MAGIC)
			gotoIfError(clean, Error_invalidParameter(0, 0, "SHFile_read() requires magicNumber prefix"))
	}

	//Read from binary

	SHHeader header;
	gotoIfError(clean, Buffer_consume(&file, &header, sizeof(header)))

	//Validate header

	if(header.version != SHHeader_V1_2)
		gotoIfError(clean, Error_invalidParameter(0, 1, "SHFile_read() header.version is invalid"))

	if(header.flags & ESHFlags_Invalid)
		gotoIfError(clean, Error_unsupportedOperation(1, "SHFile_read() unsupported flags"))

	gotoIfError(clean, DLFile_read(file, NULL, true, alloc, &dlFile))
	gotoIfError(clean, Buffer_offset(&file, dlFile.readLength))

	ESHSettingsFlags flags = ESHSettingsFlags_HideMagicNumber;

	if(dlFile.settings.dataType == EDLDataType_UTF8)
		flags |= ESHSettingsFlags_IsUTF8;

	gotoIfError(clean, SHFile_create(flags, header.extensions, alloc, shFile))

	const U8 *mem = file.ptr;
	gotoIfError(clean, Buffer_offset(&file, dlFile.entryStrings.length))

	const U8 *nextMem = file.ptr, *nextMemPrev = nextMem, *nextMemTemp;

	for (U64 i = 0; i < dlFile.entryStrings.length; ++i) {

		SHEntry entry = (SHEntry) { .stage = mem[i], .name = dlFile.entryStrings.ptr[i] };

		switch (entry.stage) {

			case ESHPipelineStage_Compute: {

				nextMemTemp = nextMem;
				nextMem += sizeof(U64);

				if(nextMem > file.ptr + Buffer_length(file))
					gotoIfError(clean, Error_outOfBounds(
						0, nextMem - file.ptr, Buffer_length(file), "SHFile_read() couldn't parse compute stage, out of bounds"
					))

				U64 groups = *(const U64*)nextMemTemp;
				entry.groupX = (U16) groups;
				entry.groupY = (U16) (groups >> 16);
				entry.groupZ = (U16) (groups >> 32);
				break;
			}

			case ESHPipelineStage_RaygenExt:
			case ESHPipelineStage_CallableExt:
			case ESHPipelineStage_WorkgraphExt:
				break;
				
			case ESHPipelineStage_MissExt:
			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:
			case ESHPipelineStage_IntersectionExt:

				nextMemTemp = nextMem;
				nextMem += sizeof(U16);

				if(nextMem > file.ptr + Buffer_length(file))
					gotoIfError(clean, Error_outOfBounds(
						0, nextMem - file.ptr, Buffer_length(file),
						"SHFile_read() couldn't parse raytracing stage, out of bounds"
					))

				entry.intersectionSize = nextMemTemp[i << 1];
				entry.payloadSize = nextMemTemp[(i << 1) | 1];
				break;

			default:

				nextMemTemp = nextMem;
				nextMem += sizeof(U64) * 2;

				if(nextMem > file.ptr + Buffer_length(file))
					gotoIfError(clean, Error_outOfBounds(
						0, nextMem - file.ptr, Buffer_length(file), "SHFile_read() couldn't parse graphics stage, out of bounds"
					))

				entry.inputsU64 = ((const U64*)nextMemTemp)[i << 1];
				entry.outputsU64 = ((const U64*)nextMemTemp)[(i << 1) | 1];
				break;
		}

		gotoIfError(clean, SHFile_addEntrypoint(shFile, &entry, alloc))
	}

	gotoIfError(clean, Buffer_offset(&file, nextMem - nextMemPrev))

	U64 binarySize[ESHBinaryType_Count] = { 0 };

	for (U8 i = 0; i < ESHBinaryType_Count; ++i) {

		if(!(header.flags >> i))
			continue;

		gotoIfError(clean, Buffer_consumeSizeType(&file, (header.sizeTypes >> (i << 1)) & 3, &binarySize[i]))
	}

	for (U8 i = 0; i < ESHBinaryType_Count; ++i) {

		if(!binarySize[i])
			continue;

		Buffer entry = Buffer_createRefConst(file.ptr, binarySize[i]);
		gotoIfError(clean, SHFile_addBinary(shFile, i, &entry, alloc))
		gotoIfError(clean, Buffer_offset(&file, binarySize[i]))
	}

	if(!isSubFile && Buffer_length(file))
		gotoIfError(clean, Error_invalidState(1, "SHFile_read() contained extra data, not allowed if it's not a subfile"))

	if(file.ptr)
		shFile->readLength = file.ptr - entireFile.ptr;

	else shFile->readLength = Buffer_length(entireFile);

clean:

	if(err.genericError)
		SHFile_free(shFile, alloc);

	DLFile_free(&dlFile, alloc);
	return err;
}

//Free SHEntryRuntime, which contains information used only while parsing

void SHEntryRuntime_free(SHEntryRuntime *entry, Allocator alloc) {
	
	if(!entry)
		return;

	CharString_free(&entry->entry.name, alloc);
	ListU16_free(&entry->shaderVersions, alloc);
	ListU8_free(&entry->uniformsPerCompilation, alloc);
	ListCharString_freeUnderlying(&entry->uniforms, alloc);
	ListCharString_freeUnderlying(&entry->uniformValues, alloc);
}

void ListSHEntryRuntime_freeUnderlying(ListSHEntryRuntime *entry, Allocator alloc) {

	if(!entry)
		return;

	for(U64 i = 0; i < entry->length; ++i)
		SHEntryRuntime_free(&entry->ptrNonConst[i], alloc);

	ListSHEntryRuntime_free(entry, alloc);
}
