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

void Test_DDSRoundTripRGBA8(Test *t);
void Test_DDSRoundTripMipChain(Test *t);
void Test_DDSRoundTripCubemap(Test *t);
void Test_DDSRoundTripBC5Legacy(Test *t);
void Test_DDSRoundTrip3D(Test *t);
void Test_DDSWriteInvalidMipCount(Test *t);
void Test_DDSReadInvalidMagic(Test *t);
void Test_DDSWriteSubresourceMismatch(Test *t);

void Test_DDSWriteSizeConsistencyBC5(Test *t);
void Test_DDSWriteSizeConsistency3D(Test *t);
void Test_DDSWriteSizeConsistencyCubemap(Test *t);
void Test_DDSWriteSizeConsistencyMipChain(Test *t);
void Test_DDSWriteSizeConsistencyRGBA8(Test *t);
