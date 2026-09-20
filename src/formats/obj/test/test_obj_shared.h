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

//formats/obj/test/test_obj_shared.h

#pragma once
#include "types/mesh/test/mesh_test.h"
#include "types/base/string_base.h"

void Test_objCubeWithEverything(Test *t);
void Test_objDedup(Test *t);
void Test_objNegativeIndices(Test *t);
void Test_objCornerForms(Test *t);
void Test_objTextTolerance(Test *t);
void Test_objComputeNormals(Test *t);
void Test_objTriangleWords(Test *t);
void Test_objMaterials(Test *t);
void Test_objQuantizedPositions(Test *t);
void Test_objGeometryOnly(Test *t);
void Test_objValidation(Test *t);
void Test_objCoverageFlags(Test *t);
void Test_objUvPrecision(Test *t);

void Test_objWriteRoundTrip(Test *t);
void Test_objWriteQuantized(Test *t);
void Test_objWriteGeometryOnly(Test *t);
