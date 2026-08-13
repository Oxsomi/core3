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

//formats/oiSR/test/test_oiSR_shared.h

#pragma once
#include "types/test/test.h"

//test_oiSR_validation.c: read-side rejection paths and structural round-trips beyond the happy path.

void Test_SRReadHeaderTamper(Test *t);              //magic/version/unsupported flags/unknown features/feature-flag mismatch
void Test_SRReadNodeFieldBounds(Test *t);           //bad type/interp/flags, OOB name/semantic/parent/child/fwd, annot range
void Test_SRReadSymbolAndAnnotationBounds(Test *t); //OOB symbol.fileNameId and annotation.nameId
void Test_SRReadFwdDeclare(Test *t);                //valid pair round-trip + self-loop/type-mismatch/wrong-dir/non-decl-flag
void Test_SRReadStreamShape(Test *t);               //truncated stream, trailing data, misaligned offset
void Test_SRFileStructuralRoundTrips(Test *t);      //empty, anonymous node, 1-node, large-N, interpolation + fwd field
void Test_SRFileHashAndName(Test *t);               //hash determinism + content sensitivity, ESRNodeType_name coverage
void Test_SRFileCreateAndWriteGuards(Test *t);      //create flag/feature/memleak guards, write/finalize symbol-parallel guards
