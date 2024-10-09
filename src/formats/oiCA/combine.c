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

#include "formats/oiCA/ca_file.h"
#include "types/allocator.h"
#include "types/error.h"

Bool CAFile_combine(CAFile a, CAFile b, Allocator alloc, CAFile *combined, Error *e_rr) {

	Bool s_uccess = true;
	Archive archive = (Archive) { 0 };

	const void *aSettingsPtr = &a.settings.compressionType;
	const void *bSettingsPtr = &b.settings.compressionType;

	for(U64 i = 0; i < 5; ++i)
		if(((const U64*)aSettingsPtr)[i] != ((const U64*)bSettingsPtr)[i])
			retError(clean, Error_invalidParameter(1, 0, "CAFile_combine()::a is incompatible with b"))

	if((a.settings.flags & ECASettingsFlags_UseSHA256) != (b.settings.flags & ECASettingsFlags_UseSHA256))
		retError(clean, Error_invalidParameter(1, 0, "CAFile_combine()::a and b have conflicting UseSHA256 flags"))

	//Combine the date flags.
	//If time is missing from b or a but present in the other it'll just use unix epoch.
	//If time is compact in the other, it will use extended time for both

	CASettings settings = a.settings;
	settings.flags |= b.settings.flags & ECASettingsFlags_DateFlags;

	ArchiveCombineSettings archiveCombineSettings = (ArchiveCombineSettings) {
		.mode = EArchiveCombineMode_Rename,
		.flags = EArchiveCombineFlags_ResolveLatestTimestamp
	};

	gotoIfError3(clean, Archive_combine(a.archive, b.archive, archiveCombineSettings, alloc, &archive, e_rr))
	gotoIfError3(clean, CAFile_create(settings, &archive, combined, e_rr))

clean:
	Archive_free(&archive, alloc);
	return s_uccess;
}
