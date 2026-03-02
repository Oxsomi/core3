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

#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"
#include "formats/oiCA/ca_compare.h"
#include "formats/oiCA/ca_props.h"

Bool CAFile_dataEqual(
	const CAFile *a, CAHandle aFile,
	const CAFile *b, CAHandle bFile,
	const Allocator *alloc,
	ECompareResult *result,
	Error *e_rr
) {
	Bool s_uccess = true;
	RefPtr *aStream = NULL, *bStream = NULL;
	U64 aOff = 0, bOff = 0;

	if (!a || !b || !result)
		retError(clean, Error_nullPointer(
			!a ? 0 : (!b ? 2 : 5),
			"CAFile_dataEqual()::a, b and result are required"
		));

	if (!CAHandle_isFile(aFile) || !CAHandle_isFile(bFile))
		retError(clean, Error_invalidParameter(
			!CAHandle_isFile(aFile) ? 1 : 3, 0,
			"CAFile_dataEqual()::aFile and bFile must be file handles"
		));

	*result = ECompareResult_Eq;

	U64 aSize = CAFile_fileSize(a, aFile);
	U64 bSize = CAFile_fileSize(b, bFile);

	if (aSize != bSize) {
		*result = aSize < bSize ? ECompareResult_Lt : ECompareResult_Gt;
		goto clean;
	}

	Bool aLoaded = CAFile_isLoaded(a, aFile);
	Bool bLoaded = CAFile_isLoaded(b, bFile);

	if (aLoaded && bLoaded) {

		Bool aValid = false, bValid = false;
		Buffer aData = CAFile_getDataConst(a, aFile, &aValid);
		Buffer bData = CAFile_getDataConst(b, bFile, &bValid);

		if (!aValid || !bValid)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::failed to get buffer data"));

		U64 aLen = Buffer_length(aData);
		U64 bLen = Buffer_length(bData);

		if (aLen != bLen) {
			*result = aLen < bLen ? ECompareResult_Lt : ECompareResult_Gt;
			goto clean;
		}

		*result = Buffer_cmp(aData, bData);
		goto clean;
	}

	//At least one side is a stream, normalize both to StreamRef via MemoryStream wrapper if needed

	RefPtrType memType = MemoryStream_makeType(alloc);

	if (aLoaded) {

		Bool aValid = false;
		Buffer aData = CAFile_getDataConst(a, aFile, &aValid);

		if (!aValid)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::failed to get buffer for a"));

		gotoIfError3(clean, MemoryStream_createFromBufferRegion(
			aData, 0, 0, EMemoryStreamFlags_None, &memType, &aStream, e_rr
		));

		aOff = 0;

		bStream = CAFile_getDataStream(b, bFile, &bOff);

		if (!bStream)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::expected stream for b"));

	} else if (bLoaded) {

		Bool bValid = false;
		Buffer bData = CAFile_getDataConst(b, bFile, &bValid);

		if (!bValid)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::failed to get buffer for b"));

		gotoIfError3(clean, MemoryStream_createFromBufferRegion(
			bData, 0, 0, EMemoryStreamFlags_None, &memType, &bStream, e_rr
		));

		bOff = 0;
		aStream = CAFile_getDataStream(a, aFile, &aOff);

		if (!aStream)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::expected stream for a"));

	} else {

		aStream = CAFile_getDataStream(a, aFile, &aOff);
		bStream = CAFile_getDataStream(b, bFile, &bOff);

		if (!aStream || !bStream)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::expected streams on both sides"));
	}

	gotoIfError3(clean, Stream_compare(
		aStream, bStream,
		aOff, bOff,
		CAFile_fileSize(a, aFile),
		0,
		alloc,
		result,
		e_rr
	));

clean:
	RefPtr_dec(&aStream);
	RefPtr_dec(&bStream);
	return s_uccess;
}
