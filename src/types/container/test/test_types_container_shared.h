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

//types/container/test/test_types_container_shared.h

#pragma once
#include "types/test/test.h"

void Test_string(Test *test);
void Test_stringOps(Test *test);   //CharString beyond number conversion
void Test_containerBuffer(Test *test);
void Test_filePaths(Test *test);    //File_resolve / File_makeRelative
void Test_list(Test *test);
void Test_jobQueue(Test *test);
void Test_hpp(Test *test);          //Defined in the C++ TU test_types_container_hpp.cpp
void Test_hppWrappers(Test *test);  //Defined in the C++ TU test_types_container_hpp_wrappers.cpp
void Test_bigInt(Test *test);
void Test_u128(Test *test);
void Test_aes128gcm(Test *test);
void Test_aes256gcm(Test *test);
void Test_sha256(Test *test);
void Test_crc32c(Test *test);
void Test_md5(Test *test);
void Test_memoryStream(Test *test);
void Test_encryptionStream(Test *test);
void Test_textureFormat(Test *test);
void Test_allocationBuffer(Test *test);
void Test_logOOM(Test *test);

#if _PLATFORM_TYPE == PLATFORM_WEB
	void Test_hostCrypto(Test *test);   //host routed crypto vs OxC3 kernels
#endif
