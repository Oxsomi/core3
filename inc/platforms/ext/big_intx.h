/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/container/big_int.h"

#ifdef __cplusplus
	extern "C" {
#endif

Error BigInt_createx(U16 bitCount, BigInt *big);
Error BigInt_createCopyx(BigInt *a, BigInt *b);

Bool BigInt_freex(BigInt *a);

Error BigInt_createFromHexx(CharString text, U16 bitCount, BigInt *big);
Error BigInt_createFromDecx(CharString text, U16 bitCount, BigInt *big);
Error BigInt_createFromOctx(CharString text, U16 bitCount, BigInt *big);
Error BigInt_createFromBinx(CharString text, U16 bitCount, BigInt *big);
Error BigInt_createFromNytox(CharString text, U16 bitCount, BigInt *big);
Error BigInt_createFromStringx(CharString text, U16 bitCount, BigInt *big);

Bool BigInt_mulx(BigInt *a, BigInt b);

Error BigInt_resizex(BigInt *a, U8 newSize);
Bool BigInt_setx(BigInt *a, BigInt b, Bool allowResize);

Bool BigInt_truncx(BigInt *big);

Error BigInt_isBase2x(BigInt a, Bool *isBase2);
U128 U128_createFromDecx(CharString text, Error *failed);
U128 U128_createFromStringx(CharString text, Error *failed);

Error BigInt_hexx(BigInt b, CharString *result, Bool leadingZeros);
Error BigInt_octx(BigInt b, CharString *result, Bool leadingZeros);
//Error BigInt_decx(BigInt b, CharString *result, Bool leadingZeros);
Error BigInt_binx(BigInt b, CharString *result, Bool leadingZeros);
Error BigInt_nytox(BigInt b, CharString *result, Bool leadingZeros);
Error BigInt_toStringx(BigInt b, CharString *result, EIntegerEncoding encoding, Bool leadingZeros);

Error U128_hexx(U128 b, CharString *result, Bool leadingZeros);
Error U128_octx(U128 b, CharString *result, Bool leadingZeros);
//Error U128_decx(BigInt b, CharString *result, Bool leadingZeros);
Error U128_binx(U128 b, CharString *result, Bool leadingZeros);
Error U128_nytox(U128 b, CharString *result, Bool leadingZeros);
Error U128_toStringx(U128 b, CharString *result, EIntegerEncoding encoding, Bool leadingZeros);

#ifdef __cplusplus
	}
#endif
