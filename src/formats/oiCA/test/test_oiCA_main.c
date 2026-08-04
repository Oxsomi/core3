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

//formats/oiCA/test/test_oiCA_main.c

#include "types/test/test.h"
#include "test_oiCA_shared.h"
#include "types/container/test/basic_alloc.h"

OXC3_TEST_MAIN(formats_oiCA) {

	const Allocator alloc = BasicAllocator_instance;

	Test t = (Test) { 0 };
	t.alloc = &alloc;

	Test_CACreateFree(&t);
	Test_CACreateCopy(&t);

	Test_CAAddFile(&t);
	Test_CAAddFolder(&t);

	Test_CARemoveFile(&t);
	Test_CARemoveFolder(&t);

	Test_CAResolve(&t);
	Test_CAGetFullName(&t);

	Test_CACounts(&t);
	Test_CAGetInfo(&t);
	Test_CAVersion(&t);
	Test_CACompare(&t);
	Test_CACombine(&t);

	Test_CARename(&t);
	Test_CAMove(&t);

	Test_CAForeach(&t);

	Test_CASetTime(&t);
	Test_CASetData(&t);
	Test_CASetStream(&t);

	Test_CAMixedTree(&t);
	Test_CAStress(&t);

	Test_CAWriteSizeConsistencyEmpty(&t);
	Test_CAWriteSizeConsistencySingleFile(&t);
	Test_CAWriteSizeConsistencyHierarchy(&t);
	Test_CAWriteSizeConsistencyMsDosDate(&t);
	Test_CAWriteSizeConsistencyFullDate(&t);
	Test_CAWriteSizeConsistencyEncrypted(&t);
	Test_CAWriteSizeConsistencyLongDirCount(&t);

	Test_CASerializeEmpty(&t);
	Test_CASerializeSingleFile(&t);
	Test_CASerializeHierarchy(&t);
	Test_CASerializeMsDosDate(&t);
	Test_CASerializeFullDate(&t);
	Test_CASerializeEncrypted(&t);
	Test_CASerializeMultipleFiles(&t);
	Test_CASerializeStreamBacked(&t);

	BasicAllocator_checkLeakedMem(&t);

	return Test_end(&t);
}
