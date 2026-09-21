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

//types/mesh/mesh.c

#include "types/mesh/mesh.h"
#include "types/container/buffer.h"
#include "types/math/flp.h"

//The codec, out of line rather than inline in the header: the format is a RUNTIME value at nearly every call
//site, so a copy at the call specializes nothing and only costs instruction cache. The sites that DO name a
// format literally are still cloned and folded, by LTO rather than by the header.

void MeshAttribute_encode(U8 *dst, ETextureFormatId id, const F32 *src, U8 srcCount) {

	const ETextureFormat f = ETextureFormatId_unpack[id];
	const ETexturePrimitive prim = ETextureFormat_getPrimitive(f);
	const U8 channels = ETextureFormat_getChannels(f);
	const U8 bits = (U8) (ETextureFormat_getBits(f) / (channels ? channels : 1));

	for(U8 i = 0; i < channels; ++i) {

		const F32 v = i < srcCount ? src[i] : 0;
		U8 *at = dst + (U64) i * (bits >> 3);

		//Every arm but the float one converts to an integer, and converting a NaN is undefined rather than
		//wrong, so those arms see a zero where the caller passed one. The float arm stores it as it is,
		// since a float channel can hold a NaN and a caller may mean it.

		const F32 num = F32_isNaN(v) ? 0 : v;

		switch(prim) {

			case ETexturePrimitive_Float: {

				if(bits == 32) {
					const F32 x = v;
					Buffer_memcpy(Buffer_createRef(at, 4), Buffer_createRefConst(&x, 4));
				}

				else {
					const F16 h = F32_castF16(v);
					Buffer_memcpy(Buffer_createRef(at, 2), Buffer_createRefConst(&h, 2));
				}

				break;
			}

			case ETexturePrimitive_SNorm: {

				const F32 lim = (F32) ((1u << (bits - 1)) - 1);
				const I32 q = (I32) F32_round(F32_clamp(num, -1, 1) * lim);

				if(bits == 8) { const I8 x = (I8) q; Buffer_memcpy(Buffer_createRef(at, 1), Buffer_createRefConst(&x, 1)); }
				else { const I16 x = (I16) q; Buffer_memcpy(Buffer_createRef(at, 2), Buffer_createRefConst(&x, 2)); }

				break;
			}

			case ETexturePrimitive_UNorm: {

				const F32 lim = (F32) ((1u << bits) - 1);
				const U32 q = (U32) F32_round(F32_clamp(num, 0, 1) * lim);

				if(bits == 8) { const U8 x = (U8) q; Buffer_memcpy(Buffer_createRef(at, 1), Buffer_createRefConst(&x, 1)); }
				else { const U16 x = (U16) q; Buffer_memcpy(Buffer_createRef(at, 2), Buffer_createRefConst(&x, 2)); }

				break;
			}

			default: {

				//An integer channel stores the value's own bits, so the range is CLAMPED and not wrapped: a
				//conversion past what the type holds is undefined in C, and a silent wrap turns an out of
				// range index into a valid looking one. The bits are two's complement either way, which is
				// what the decode sign extends. F64 because a 32 bit channel's bounds are not exact in F32.

				const Bool isSigned = prim == ETexturePrimitive_SInt;

				const F64 lo = isSigned ? -(F64) (1ull << (bits - 1)) : 0;
				const F64 hi = isSigned ? (F64) ((1ull << (bits - 1)) - 1) : (F64) ((1ull << bits) - 1);

				const I64 q = (I64) F64_round(F64_clamp((F64) num, lo, hi));

				if(bits == 8) {
					const U8 x = (U8) q;
					Buffer_memcpy(Buffer_createRef(at, 1), Buffer_createRefConst(&x, 1));
				}

				else if(bits == 16) {
					const U16 x = (U16) q;
					Buffer_memcpy(Buffer_createRef(at, 2), Buffer_createRefConst(&x, 2));
				}

				else {
					const U32 x = (U32) q;
					Buffer_memcpy(Buffer_createRef(at, 4), Buffer_createRefConst(&x, 4));
				}

				break;
			}
		}
	}
}
void MeshAttribute_decode(const U8 *src, ETextureFormatId id, F32 *dst, U8 dstCount) {

	const ETextureFormat f = ETextureFormatId_unpack[id];
	const ETexturePrimitive prim = ETextureFormat_getPrimitive(f);
	const U8 channels = ETextureFormat_getChannels(f);
	const U8 bits = (U8) (ETextureFormat_getBits(f) / (channels ? channels : 1));

	for(U8 i = 0; i < dstCount; ++i) {

		dst[i] = 0;

		if(i >= channels)
			continue;

		const U8 *at = src + (U64) i * (bits >> 3);

		switch(prim) {

			case ETexturePrimitive_Float:

				if(bits == 32) {
					F32 x = 0;
					Buffer_memcpy(Buffer_createRef(&x, 4), Buffer_createRefConst(at, 4));
					dst[i] = x;
				}

				else {
					F16 h = 0;
					Buffer_memcpy(Buffer_createRef(&h, 2), Buffer_createRefConst(at, 2));
					dst[i] = F16_castF32(h);
				}

				break;

			case ETexturePrimitive_SNorm: {

				const F32 lim = (F32) ((1u << (bits - 1)) - 1);
				I32 q = 0;

				if(bits == 8) { I8 x = 0; Buffer_memcpy(Buffer_createRef(&x, 1), Buffer_createRefConst(at, 1)); q = x; }
				else { I16 x = 0; Buffer_memcpy(Buffer_createRef(&x, 2), Buffer_createRefConst(at, 2)); q = x; }

				dst[i] = F32_max((F32) q / lim, -1);
				break;
			}

			case ETexturePrimitive_UNorm: {

				const F32 lim = (F32) ((1u << bits) - 1);
				U32 q = 0;

				if(bits == 8) { U8 x = 0; Buffer_memcpy(Buffer_createRef(&x, 1), Buffer_createRefConst(at, 1)); q = x; }
				else { U16 x = 0; Buffer_memcpy(Buffer_createRef(&x, 2), Buffer_createRefConst(at, 2)); q = x; }

				dst[i] = (F32) q / lim;
				break;
			}

			default: {

				U32 q = 0;

				switch(bits) {
					case 8:  { U8 x = 0;  Buffer_memcpy(Buffer_createRef(&x, 1), Buffer_createRefConst(at, 1)); q = x; break; }
					case 16: { U16 x = 0; Buffer_memcpy(Buffer_createRef(&x, 2), Buffer_createRefConst(at, 2)); q = x; break; }
					default: { Buffer_memcpy(Buffer_createRef(&q, 4), Buffer_createRefConst(at, 4)); break; }
				}

				//Two's complement, so a signed channel's top bit is its SIGN and reading the bits as
				//unsigned turns every negative value into a large positive one. Flipping that bit and
				// subtracting it back is the sign extension, and it is skipped entirely for UInt.

				if(prim == ETexturePrimitive_SInt) {
					const I64 sign = (I64) 1 << (bits - 1);
					dst[i] = (F32) (((I64) (q ^ (U32) sign)) - sign);
				}

				else dst[i] = (F32) q;

				break;
			}
		}
	}
}
