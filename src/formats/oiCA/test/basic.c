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

#include "shared.h"
#include "formats/oiCA/ca_file.h"

static const CASettings kCASettings = {
	.flags           = ECASettingsFlags_None,
	.compressionType = EXXCompressionType_None,
	.encryptionType  = EXXEncryptionType_None
};

static const CASettings kCASettingsDate = {
	.flags           = ECASettingsFlags_IncludeDate,
	.compressionType = EXXCompressionType_None,
	.encryptionType  = EXXEncryptionType_None
};

static const CASettings kCASettingsFullDate = {
	.flags           = ECASettingsFlags_IncludeFullDate,
	.compressionType = EXXCompressionType_None,
	.encryptionType  = EXXEncryptionType_None
};

void Test_CACreateFree(Test *t) {
	
	Test_setModule(t, "CAFile_createFree");

	{						//Basic create + free
		CAFile ca = { 0 };
		Test_assert(t, "Create ok",               CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err));

		Test_assert(t, "folders.length 1",        ca.folders.length == 1);		//Root folder
		Test_assert(t, "files.length 0",          ca.files.length   == 0);

		CAFile_free(&ca, t->alloc);
	}

	{						//IncludeDate flag accepted
		CAFile ca = { 0 };
		Test_assert(t, "Create IncludeDate ok",   CAFile_create(&kCASettingsDate, 0, 0, t->alloc, &ca, &t->err));
		CAFile_free(&ca, t->alloc);
	}

	{						//IncludeFullDate flag accepted
		CAFile ca = { 0 };
		Test_assert(t, "Create IncludeFullDate ok", CAFile_create(&kCASettingsFullDate, 0, 0, t->alloc, &ca, &t->err));
		CAFile_free(&ca, t->alloc);
	}

	{						//Double create should fail
		CAFile ca = { 0 };
		Test_assert(t, "First create",            CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err));
		Test_assert(t, "Double-create fails",     !CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, NULL));
		CAFile_free(&ca, t->alloc);
	}

	//Check invalid calls
	
	Test_assert(t, "Null caFile fails",           !CAFile_create(&kCASettings, 0, 0, t->alloc, NULL, NULL));

	{
		CAFile ca = { 0 };
		Test_assert(t, "Null settings fails",     !CAFile_create(NULL, 0, 0, t->alloc, &ca, NULL));
	}

	{						//Invalid flags rejected
		CASettings bad = kCASettings;
		bad.flags = ECASettingsFlags_Invalid;
		CAFile ca = { 0 };
		Test_assert(t, "Invalid flags fails",     !CAFile_create(&bad, 0, 0, t->alloc, &ca, NULL));
	}

	{						//Invalid compression type rejected
		CASettings bad = kCASettings;
		bad.compressionType = (EXXCompressionType)0xFF;
		CAFile ca = { 0 };
		Test_assert(t, "Bad compressionType",     !CAFile_create(&bad, 0, 0, t->alloc, &ca, NULL));
	}

	{						//Invalid encryption type rejected
		CASettings bad = kCASettings;
		bad.encryptionType = (EXXEncryptionType)0xFF;
		CAFile ca = { 0 };
		Test_assert(t, "Bad encryptionType",      !CAFile_create(&bad, 0, 0, t->alloc, &ca, NULL));
	}

	//Check free

	CAFile_free(NULL, t->alloc);				//Free null is safe
	Test_assert(t, "Free NULL safe", true);

	{						//Free of unallocated struct must not crash
		CAFile ca = { 0 };
		CAFile_free(&ca, t->alloc);
		Test_assert(t, "Free unallocated safe", true);
	}

	{						//Free must zero the struct
		CAFile ca = { 0 };
		CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, NULL);
		CAFile_free(&ca, t->alloc);
		CAFile zero = { 0 };
		Test_assert(t, "Free zeroes struct", Buffer_eq(
			Buffer_createRef(&ca, sizeof(ca)), Buffer_createRef(&zero, sizeof(zero))
		));
	}
}
