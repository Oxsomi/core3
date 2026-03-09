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

#pragma once
#include "types/test/test.h"

void Test_CACreateFree(Test *t);
void Test_CACreateCopy(Test *t);

void Test_CAAddFile(Test *t);
void Test_CAAddFolder(Test *t);

void Test_CARemoveFile(Test *t);
void Test_CARemoveFolder(Test *t);

void Test_CAResolve(Test *t);
void Test_CAGetFullName(Test *t);

void Test_CACounts(Test *t);
void Test_CAGetInfo(Test *t);

void Test_CARename(Test *t);
void Test_CAMove(Test *t);

void Test_CAForeach(Test *t);

void Test_CASetTime(Test *t);
void Test_CASetStream(Test *t);
void Test_CASetData(Test *t);

void Test_CACompare(Test *t);

void Test_CACombine(Test *t);

void Test_CAMixedTree(Test *t);
void Test_CAStress(Test *t);

void Test_CAWriteSizeConsistencyEmpty(Test *t);
void Test_CAWriteSizeConsistencySingleFile(Test *t);
void Test_CAWriteSizeConsistencyHierarchy(Test *t);
void Test_CAWriteSizeConsistencyMsDosDate(Test *t);
void Test_CAWriteSizeConsistencyFullDate(Test *t);
void Test_CAWriteSizeConsistencyEncrypted(Test *t);
void Test_CAWriteSizeConsistencyLongDirCount(Test *t);

void Test_CASerializeEmpty(Test *t);
void Test_CASerializeSingleFile(Test *t);
void Test_CASerializeHierarchy(Test *t);
void Test_CASerializeMsDosDate(Test *t);
void Test_CASerializeFullDate(Test *t);
void Test_CASerializeEncrypted(Test *t);
void Test_CASerializeMultipleFiles(Test *t);
void Test_CASerializeStreamBacked(Test *t);
