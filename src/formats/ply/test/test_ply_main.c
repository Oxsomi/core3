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

//formats/ply/test/test_ply_main.c

#include "test_ply_shared.h"
#include "types/container/test/basic_alloc.h"

OXC3_TEST_MAIN(formats_ply) {

	const Allocator alloc = BasicAllocator_instance;
	Test t = (Test) { 0 };
	t.alloc = &alloc;

	Test_plyAscii(&t);
	Test_plyBinaryLittleEndian(&t);
	Test_plyBinaryBigEndian(&t);
	Test_plySkipsWhatItDoesNotKnow(&t);
	Test_plyComputeNormals(&t);
	Test_plyQuantizedPositions(&t);
	Test_plyManyProperties(&t);
	Test_plyValidation(&t);
	Test_plyHeader(&t);
	Test_plyHeaderNotFixedStride(&t);
	Test_plyHeaderAsciiNoStride(&t);
	Test_plyRange(&t);
	Test_plyWrite(&t);
	Test_plyWriteByteOrder(&t);
	Test_plyWriteGeometryOnly(&t);

	BasicAllocator_checkLeakedMem(&t);
	return Test_end(&t);
}
