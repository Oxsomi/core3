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

#include "formats/oiCA/ca_combine.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_compare.h"
#include "formats/oiCA/ca_edit.h"
#include "types/container/ref_ptr.h"
#include "types/container/log.h"
#include "types/container/file.h"
#include "types/base/string_read_helper.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/mathi.h"

typedef struct CAFileCombine {
	const CAFile         *b;
	CAFile               *combined;
	EArchiveCombineMode  mode;
	EArchiveCombineFlags flags;
	const Allocator	     *alloc;
	Error                *e_rr;
	Bool                 s_uccess;
} CAFileCombine;

static Bool CAFile_copyDataFromB(
	const CAFile *b,
	CAHandle bHandle,
	CAFile *combined,
	CAHandle dstHandle,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;

	U64 streamOff = U64_MAX;
	StreamRef *stream = CAFile_getDataStream(b, bHandle, &streamOff);

	if (stream) {
		gotoIfError3(clean, CAFile_setDataStream(
			combined, dstHandle, alloc, &stream, streamOff,
			CAFile_fileSize(b, bHandle), e_rr
		));
	} else {
		Bool dataValid = false;
		Buffer bData = CAFile_getDataConst(b, bHandle, &dataValid);
		Buffer dataCopy = Buffer_createNull();

		if (dataValid && Buffer_length(bData)) {
			gotoIfError3(clean, Buffer_createCopy(bData, alloc, &dataCopy, e_rr));
			gotoIfError3(clean, CAFile_setData(combined, dstHandle, alloc, &dataCopy, e_rr));
		}
	}

clean:
	RefPtr_dec(&stream);
	return s_uccess;
}

static Bool CAFile_combineCallback(const FileInfo *info, void *object, Error *e_rr) {

	CAFileCombine *ctx = (CAFileCombine*)object;

	Bool s_uccess = true;
	CharString renamed = CharString_createNull();

	Bool isFolder = info->type == EFileType_Folder;

	CAHandle bHandle =
		isFolder ? CAFile_resolveFolder(ctx->b, info->path) :
		CAFile_resolveFile(ctx->b, info->path);

	CAHandle aHandle =
		isFolder ? CAFile_resolveFolder(ctx->combined, info->path) :
		CAFile_resolveFile(ctx->combined, info->path);

	if (aHandle == CAHandle_Invalid) {

		//Not in a, insert into combined

		U64 lastSlash = CharString_findLastSensitive(&info->path, '/', 0, 0);

		CAHandle parent = CAHandle_Root;
		CharString name = info->path;

		if (lastSlash != U64_MAX) {
			CharString parentPath = CharString_createRefSizedConst(info->path.ptr, lastSlash, false);
			parent = CAFile_resolveFolder(ctx->combined, parentPath);
			name = CharString_createRefSizedConst(
				info->path.ptr + lastSlash + 1,
				CharString_length(info->path) - lastSlash - 1,
				false
			);
		}

		if (parent == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "CAFile_combine()::parent folder missing during insert"));

		CharString nameCopy = CharString_createNull();
		gotoIfError3(clean, CharString_createCopy(name, ctx->alloc, &nameCopy, e_rr));

		CAHandle newHandle = CAFile_add(
			ctx->combined,
			parent,
			&nameCopy,
			isFolder ? 0 : info->timestamp,
			!isFolder,
			ctx->alloc,
			e_rr
		);

		if (newHandle == CAHandle_Invalid) {
			CharString_free(&nameCopy, ctx->alloc);
			retError(clean, Error_invalidState(0, "CAFile_combine()::failed to insert entry"));
		}

		if (!isFolder)
			gotoIfError3(clean, CAFile_copyDataFromB(ctx->b, bHandle, ctx->combined, newHandle, ctx->alloc, e_rr));

		goto clean;
	}

	//Entry exists in both, folders just continue (no timestamp stored)

	if (isFolder)
		goto clean;

	//File conflict check

	Ns aTime = CAFile_fileTime(ctx->combined, aHandle);
	Ns bTime = info->timestamp;
	Bool timeSame = aTime == bTime;
	Bool conflict = false;

	ECompareResult dataEq = ECompareResult_Eq;
	gotoIfError3(clean, CAFile_dataEqual(ctx->combined, aHandle, ctx->b, bHandle, ctx->alloc, &dataEq, e_rr));
	Bool dataSame = dataEq == ECompareResult_Eq;

	if (!timeSame) {

		if (ctx->flags & EArchiveCombineFlags_ResolveAcceptLatest) {

			if (!dataSame) {

				if (timeSame)
					conflict = true;

				else if (bTime > aTime)
					gotoIfError3(clean, CAFile_copyDataFromB(
						ctx->b, bHandle, ctx->combined, aHandle, ctx->alloc, e_rr
					));

				//else a is newer, no-op
			}

			if (!conflict)
				gotoIfError3(clean, CAFile_setTime(ctx->combined, aHandle, U64_max(aTime, bTime), e_rr));
		}

		else if (!(ctx->flags & EArchiveCombineFlags_ResolveLatestTimestamp))
			conflict = true;

		else if (!dataSame)
			conflict = true;

		else gotoIfError3(clean, CAFile_setTime(ctx->combined, aHandle, U64_max(aTime, bTime), e_rr));
	}

	else if (!dataSame)
		conflict = true;

	if (!conflict)
		goto clean;

	switch (ctx->mode) {

		default:
			retError(clean, Error_invalidState(0, "CAFile_combine()::settings.mode is invalid"));

		case EArchiveCombineMode_AcceptA:
			break;

		case EArchiveCombineMode_AcceptB:
			gotoIfError3(clean, CAFile_copyDataFromB(ctx->b, bHandle, ctx->combined, aHandle, ctx->alloc, e_rr));
			gotoIfError3(clean, CAFile_setTime(ctx->combined, aHandle, bTime, e_rr));
			break;

		case EArchiveCombineMode_RequireSame:
			retError(clean, Error_invalidState(
				0, "CAFile_combine()::a and b had matching file paths, but mismatching contents"
			));

		case EArchiveCombineMode_Rename: {

			U64 lastSlash = CharString_findLastSensitive(&info->path, '/', 0, 0);

			CharString fileName =
				lastSlash == U64_MAX ? info->path :
				CharString_createRefSizedConst(
					info->path.ptr + lastSlash + 1,
					CharString_length(info->path) - lastSlash - 1,
					false
				);

			CharString baseName = fileName;
			CharString extension = CharString_createRefCStrConst("");

			U64 lastDot = CharString_findLastSensitive(&fileName, '.', 0, 0);

			if (lastDot != U64_MAX) {
				baseName = CharString_createRefSizedConst(fileName.ptr, lastDot, false);
				extension = CharString_createRefSizedConst(
					fileName.ptr + lastDot,
					CharString_length(fileName) - lastDot,
					CharString_isNullTerminated(fileName)
				);
			}

			U64 counter = 0;
			U64 startCounter = CharString_findLastSensitive(&baseName, '-', 0, 0);

			if (startCounter != U64_MAX && C8_isDec(CharString_getAt(baseName, startCounter + 1))) {

				U64 j = startCounter + 2;

				for (; j < CharString_length(baseName); ++j)
					if (!C8_isDec(baseName.ptr[j]))
						break;

				if (j == CharString_length(baseName)) {
					CharString num = CharString_createRefSizedConst(
						baseName.ptr + startCounter + 1, j - startCounter - 1, false
					);

					if (!CharString_parseU64(num, &counter))
						retError(clean, Error_invalidState(0, "CAFile_combine() parse U64 failed"));
				}
			}

			CAHandle parent = CAHandle_Root;

			if (lastSlash != U64_MAX) {

				CharString parentPath = CharString_createRefSizedConst(info->path.ptr, lastSlash, false);
				parent = CAFile_resolveFolder(ctx->combined, parentPath);

				if (parent == CAHandle_Invalid)
					retError(clean, Error_invalidState(0, "CAFile_combine()::rename parent not found"));
			}

			do {
				CharString_free(&renamed, ctx->alloc);
				++counter;

				gotoIfError3(clean, CharString_format(
					ctx->alloc, &renamed, e_rr, "%.*s-%"PRIu64"%.*s",
					(int)CharString_length(baseName), baseName.ptr,
					counter,
					(int)CharString_length(extension), extension.ptr
				));

			} while (CAFile_hasSubObject(ctx->combined, parent, renamed));

			CAHandle newHandle = CAFile_add(
				ctx->combined, parent, &renamed, bTime, true, ctx->alloc, e_rr
			);

			renamed = CharString_createNull();  //ownership transferred

			if (newHandle == CAHandle_Invalid)
				retError(clean, Error_invalidState(0, "CAFile_combine()::rename insert failed"));

			gotoIfError3(clean, CAFile_copyDataFromB(ctx->b, bHandle, ctx->combined, newHandle, ctx->alloc, e_rr));
			break;
		}
	}

clean:
	CharString_free(&renamed, ctx->alloc);

	if (!s_uccess)
		ctx->s_uccess = false;

	return s_uccess;
}

Bool CAFile_combine(
	const CAFile *a,
	const CAFile *b,
	EArchiveCombineMode combineMode,
	EArchiveCombineFlags combineFlags,
	const Allocator *alloc,
	CAFile *combined,
	Error *e_rr
) {
	Bool s_uccess = true;
	Bool allocate = false;

	if (!a || !b || !combined)
		retError(clean, Error_nullPointer(!a ? 0 : (!b ? 1 : 5), "CAFile_combine()::a, b and combined are required"));

	//Validate encryption/compression settings compatibility (compare as U64[5])

	const void *aSettingsPtr = &a->settings.compressionType;
	const void *bSettingsPtr = &b->settings.compressionType;

	for (U64 i = 0; i < 5; ++i)
		if (((const U64*)aSettingsPtr)[i] != ((const U64*)bSettingsPtr)[i])
			retError(clean, Error_invalidParameter(1, 0, "CAFile_combine()::a is incompatible with b"));

	//Merge settings: combine date flags, a leads for everything else

	CASettings settings = a->settings;
	settings.flags |= b->settings.flags & ECASettingsFlags_DateFlags;

	gotoIfError3(clean, CAFile_createCopy(a, alloc, combined, e_rr));
	allocate = true;

	CAFileCombine ctx = {
		.b		 = b,
		.combined  = combined,
		.mode	  = combineMode,
		.flags	 = combineFlags,
		.alloc	 = alloc,
		.e_rr	  = e_rr,
		.s_uccess  = true
	};

	gotoIfError3(clean, CAFile_foreach(b, CAHandle_Root, CAFile_combineCallback, &ctx, true, alloc, e_rr));

	if (!ctx.s_uccess) {
		s_uccess = false;
		goto clean;
	}

	combined->settings = settings;

clean:

	if (allocate && !s_uccess)
		CAFile_free(combined, alloc);

	return s_uccess;
}
