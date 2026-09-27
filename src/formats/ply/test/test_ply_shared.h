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

//formats/ply/test/test_ply_shared.h

#pragma once
#include "types/mesh/test/mesh_test.h"
#include "types/base/string_base.h"

void Test_plyAscii(Test *t);
void Test_plyBinaryLittleEndian(Test *t);
void Test_plyBinaryBigEndian(Test *t);
void Test_plySkipsWhatItDoesNotKnow(Test *t);
void Test_plyComputeNormals(Test *t);
void Test_plyQuantizedPositions(Test *t);
void Test_plyManyProperties(Test *t);
void Test_plyValidation(Test *t);

//The header alone, and the body read as spans of it.

void Test_plyHeader(Test *t);
void Test_plyHeaderNotFixedStride(Test *t);
void Test_plyHeaderAsciiNoStride(Test *t);
void Test_plyRange(Test *t);

void Test_plyWrite(Test *t);
void Test_plyWriteByteOrder(Test *t);
void Test_plyWriteGeometryOnly(Test *t);
