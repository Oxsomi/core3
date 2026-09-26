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

//types/mesh/test/test_mesh_shared.h

#pragma once
#include "types/mesh/test/mesh_test.h"

//What mesh owns and no reader does: the window a file is pulled through, the sink records leave by, and the
// quantization that runs after the bounds are complete.

void Test_meshSourceWindow(Test *t);
void Test_meshSink(Test *t);
void Test_meshPositions(Test *t);

//The attribute codec: one round trip per primitive at every width, since that is what it is written over.

void Test_meshAttribute(Test *t);

//What runs OVER a finished mesh rather than inside a read: normals and the per triangle word.

void Test_meshDerive(Test *t);
