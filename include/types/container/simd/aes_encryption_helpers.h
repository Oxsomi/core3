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

//types/container/simd/aes_encryption_helpers.h

#pragma once
#include "types/math/vec4i.h"

//Shared helpers between aes implementations

//Refactored from https://www.intel.com/content/dam/develop/external/us/en/documents/clmul-wp-rev-2-02-2014-04-20.pdf
__forceinline__ static I32x4 AESEncryptionContext_ghashReduceClMul(I32x4 clmul00, I32x4 clmulFused, I32x4 clmul11) {

	I32x4 tmp1 = I32x4_lshElements(clmulFused, 2);
	I32x4 tmp3 = I32x4_rshElements(clmulFused, 2);

	I32x4 t0 = I32x4_xor(clmul00, tmp1);
	I32x4 t1 = I32x4_xor(clmul11, tmp3);

	I32x4 tmp0 = I32x4_lsh32(t0, 1);
	I32x4 tmp4 = I32x4_rsh32(t0, 31);
	I32x4 tmp2 = I32x4_lsh32(t1, 1);
	I32x4 tmp6 = I32x4_rsh32(t1, 31);

	I32x4 tmp7 = I32x4_rshElements(tmp4, 3);
	tmp6 = I32x4_lshElements(tmp6, 1);
	I32x4 tmp5 = I32x4_lshElements(tmp4, 1);

	tmp0 = I32x4_or(tmp0, tmp5);
	tmp2 = I32x4_or(tmp2, tmp6);
	tmp5 = I32x4_lsh32(tmp0, 31);
	tmp6 = I32x4_lsh32(tmp0, 30);
	tmp4 = I32x4_or(tmp2, tmp7);
	tmp7 = I32x4_lsh32(tmp0, 25);

	tmp5 = I32x4_xor(tmp5, tmp6);
	tmp5 = I32x4_xor(tmp5, tmp7);

	tmp6 = I32x4_lshElements(tmp5, 3);
	tmp3 = I32x4_rshElements(tmp5, 1);
	tmp5 = I32x4_xor(tmp0, tmp6);

	tmp0 = I32x4_rsh32(tmp5, 1);
	tmp1 = I32x4_rsh32(tmp5, 2);
	tmp2 = I32x4_rsh32(tmp5, 7);

	tmp0 = I32x4_xor(tmp0, tmp5);        //0 ^ 5
	tmp1 = I32x4_xor(tmp1, tmp2);        //1 ^ 2
	tmp3 = I32x4_xor(tmp3, tmp4);        //3 ^ 4
	tmp0 = I32x4_xor(tmp0, tmp1);        //0 ^ 1 ^ 2 ^ 5
	tmp0 = I32x4_xor(tmp0, tmp3);        //0 ^ 1 ^ 2 ^ 3 ^ 4 ^ 5

	return I32x4_swapEndianness(tmp0);
}
