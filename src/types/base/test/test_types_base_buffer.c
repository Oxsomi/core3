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

#include "test_types_base_shared.h"
#include "types/base/buffer.h"

void Test_buffer(Test *test) {

	Test_setModule(test, "Buffer");

	U64 dat[256 / 8];
	Buffer buf = Buffer_createRef(dat, sizeof(dat));

	U64 dat1[256 / 8];
	Buffer buf1 = Buffer_createRef(dat1, sizeof(dat1));

	const U64 dat2[256 / 8] = { 0 };
	Buffer buf2 = Buffer_createRefConst(dat2, sizeof(dat2));

	//This is actually not managed, but doesn't matter (we don't free), just for checks
	Buffer fakeBuf = Buffer_createManagedPtr(dat, sizeof(dat));

	//Simple tests

	Test_assert(test, "Buffer_isRef", Buffer_isRef(buf));
	Test_assert(test, "Buffer_isConstRef", !Buffer_isConstRef(buf));

	Test_assert(test, "Buffer_isRef (1)", Buffer_isRef(buf2));
	Test_assert(test, "Buffer_isConstRef (1)", Buffer_isConstRef(buf2));

	Test_assert(test, "Buffer_isRef (1)", !Buffer_isRef(fakeBuf));
	Test_assert(test, "Buffer_isConstRef (1)", !Buffer_isConstRef(fakeBuf));

	Test_assert(test, "Buffer_length", Buffer_length(buf2) == 256);

	//(Un)Setting all bits

	U64 i = 0;

	Test_assert(test, "Buffer_setAllToU8", Buffer_setAllToU8(buf, 0xBA, &test->err));

	for (i = 0; i < 256 / 8; ++i)
		if (dat[i] != 0xBABABABABABABABA)
			break;

	Test_assert(test, "Buffer_setAllToU8", i == 256 / 8);

	Test_assert(test, "Buffer_unsetAllBits", Buffer_unsetAllBits(buf, &test->err));

	for (i = 0; i < 256 / 8; ++i)
		if (dat[i])
			break;

	Test_assert(test, "Buffer_unsetAllBits", i == 256 / 8);

	Test_assert(test, "Buffer_setAllBits", Buffer_setAllBits(buf1, &test->err));

	for (i = 0; i < 256 / 8; ++i)
		if (dat1[i] != (U64)-1)
			break;

	Test_assert(test, "Buffer_setAllBits", i == 256 / 8);

	//Equal

	Test_assert(test, "Buffer_eq", !Buffer_eq(buf, buf1));
	Test_assert(test, "Buffer_neq", Buffer_neq(buf, buf1));
	Test_assert(test, "Buffer_eq (1)", Buffer_eq(buf, buf2));
	Test_assert(test, "Buffer_neq (1)", !Buffer_neq(buf, buf2));

	//Bitwise

	Test_assert(test, "Buffer_bitwiseNot", Buffer_bitwiseNot(buf1, &test->err));
	Test_assert(test, "Buffer_bitwiseNot", Buffer_eq(buf, buf1));

	Test_assert(test, "Buffer_bitwiseOr", Buffer_unsetAllBits(buf, &test->err));
	Test_assert(test, "Buffer_bitwiseOr", Buffer_setAllBits(buf1, &test->err));
	Test_assert(test, "Buffer_bitwiseOr", Buffer_bitwiseOr(buf, buf1, &test->err));

	for (i = 0; i < 256 / 8; ++i)
		if (dat[i] != (U64)-1)
			break;

	Test_assert(test, "Buffer_bitwiseOr", i == 256 / 8);

	Test_assert(test, "Buffer_bitwiseXor", Buffer_bitwiseXor(buf, buf1, &test->err));

	for (i = 0; i < 256 / 8; ++i)
		if (dat[i])
			break;

	Test_assert(test, "Buffer_bitwiseXor", i == 256 / 8);

	Test_assert(test, "Buffer_setAllToU8", Buffer_setAllToU8(buf, 0xAA, &test->err));
	Test_assert(test, "Buffer_setAllToU8", Buffer_setAllToU8(buf1, 0x55, &test->err));

	Test_assert(test, "Buffer_bitwiseAnd", Buffer_bitwiseAnd(buf, buf1, &test->err));    //0xAA & 0x55 = 0x00

	for (i = 0; i < 256 / 8; ++i)
		if (dat[i])
			break;

	Test_assert(test, "Buffer_bitwiseAnd", i == 256 / 8);

	//Memcpy

	Test_assert(test, "Buffer_memcpy", Buffer_setAllToU8(buf, 0xAB, &test->err));
	Test_assert(test, "Buffer_memcpy", Buffer_unsetAllBits(buf1, &test->err));

	Buffer_memcpy(buf1, buf);

	for (i = 0; i < 256 / 8; ++i)
		if (dat1[i] != 0xABABABABABABABAB)
			break;

	Test_assert(test, "Buffer_memcpy", i == 256 / 8);

	Test_assert(test, "Buffer_memcpy", Buffer_unsetAllBits(buf1, &test->err));
	Test_assert(test, "Buffer_memcpy", Buffer_setAllToU8(buf, 0xCD, &test->err));

	Buffer_memcpy(
		Buffer_createRef(dat1 + 4, 16),        //Into dat1[4], dat1[5]
		Buffer_createRefConst(dat + 2, 16)    //Copy dat[2], dat[3]
	);

	for (i = 0; i < 4; ++i)
		if (dat1[i])
			break;

	Test_assert(test, "Buffer_memcpy (partial, pre)", i == 4);

	for (i = 4; i < 6; ++i)
		if (dat1[i] != 0xCDCDCDCDCDCDCDCD)
			break;

	Test_assert(test, "Buffer_memcpy (partial, data)", i == 6);

	for (i = 6; i < 256 / 8; ++i)
		if (dat1[i])
			break;

	Test_assert(test, "Buffer_memcpy (partial, post)", i == 256 / 8);

	//Bit ranges

	Test_assert(test, "Buffer_setBitRange", Buffer_unsetAllBits(buf, &test->err));
	Test_assert(test, "Buffer_setBitRange", Buffer_setAllBits(buf1, &test->err));
	Test_assert(test, "Buffer_setBitRange", Buffer_setBitRange(buf, 9, 240, &test->err));
	Test_assert(test, "Buffer_unsetBitRange", Buffer_unsetBitRange(buf1, 9, 240, &test->err));
	Test_assert(test, "Buffer_unsetBitRange", Buffer_bitwiseNot(buf1, &test->err));
	Test_assert(test, "Buffer_setBitRange, Buffer_unsetBitRange", Buffer_eq(buf, buf1));

	//Individual bits

	Test_assert(test, "Buffer_setBit", Buffer_unsetAllBits(buf, &test->err));

	Test_assert(test, "Buffer_setBit", Buffer_setBit(buf, 0, &test->err));
	Test_assert(test, "Buffer_setBit", Buffer_setBit(buf, 1, &test->err));

	Bool b = false;
	Test_assert(test, "Buffer_getBit", Buffer_getBit(buf, 0, &b, &test->err) && b);
	Test_assert(test, "Buffer_getBit", Buffer_getBit(buf, 1, &b, &test->err) && b);

	Test_assert(test, "Buffer_setBit", buf.ptr[0] == 0b11);

	Test_assert(test, "Buffer_resetBit", Buffer_resetBit(buf, 0, &test->err));
	Test_assert(test, "Buffer_resetBit", Buffer_resetBit(buf, 1, &test->err));

	Test_assert(test, "Buffer_getBit", Buffer_getBit(buf, 0, &b, &test->err) && !b);
	Test_assert(test, "Buffer_getBit", Buffer_getBit(buf, 1, &b, &test->err) && !b);

	Test_assert(test, "Buffer_resetBit", buf.ptr[0] == 0b00);

	//Consume and Append

	for (i = 0; i < 256 / 8; ++i)
		dat[i] = i;

	Buffer consumeBuf = Buffer_createRef(dat, sizeof(dat));
	Buffer appendBuf = Buffer_createRef(dat1, sizeof(dat1));

	//Consume

	U64 val = 0;
	Test_assert(test, "Buffer_consume", Buffer_consume(&consumeBuf, &val, sizeof(val), &test->err));
	Test_assert(test, "Buffer_consume (value)", val == 0);

	Test_assert(test, "Buffer_consume", Buffer_consume(&consumeBuf, &val, sizeof(val), &test->err));
	Test_assert(test, "Buffer_consume (value)", val == 1);

	//Append

	val = 0xDEADBEEFCAFEBABE;
	Test_assert(test, "Buffer_append", Buffer_append(&appendBuf, &val, sizeof(val), &test->err));
	Test_assert(test, "Buffer_append (value)", dat1[0] == 0xDEADBEEFCAFEBABE);

	val = 0x1234567890ABCDEF;
	Test_assert(test, "Buffer_append", Buffer_append(&appendBuf, &val, sizeof(val), &test->err));
	Test_assert(test, "Buffer_append (value)", dat1[1] == 0x1234567890ABCDEF);

	//Consume/Append with smaller types

	U32 val32 = 0;
	Test_assert(test, "Buffer_consume (U32)", Buffer_consume(&consumeBuf, &val32, sizeof(val32), &test->err));
	Test_assert(test, "Buffer_consume (U32 value)", val32 == 2);

	val32 = 0xDEADBEEF;
	Test_assert(test, "Buffer_append (U32)", Buffer_append(&appendBuf, &val32, sizeof(val32), &test->err));

	U16 val16 = 0;
	Test_assert(test, "Buffer_consume (U16)", Buffer_consume(&consumeBuf, &val16, sizeof(val16), &test->err));

	val16 = 0xCAFE;
	Test_assert(test, "Buffer_append (U16)", Buffer_append(&appendBuf, &val16, sizeof(val16), &test->err));

	U8 val8 = 0;
	Test_assert(test, "Buffer_consume (U8)", Buffer_consume(&consumeBuf, &val8, sizeof(val8), &test->err));

	val8 = 0xBA;
	Test_assert(test, "Buffer_append (U8)", Buffer_append(&appendBuf, &val8, sizeof(val8), &test->err));

	//Test consume bounds checking

	Buffer smallBuf = Buffer_createRef(dat, 16);

	for (i = 0; i < 2; ++i)
		Test_assert(test, "Buffer_consume (setup)", Buffer_consume(&smallBuf, &val, sizeof(val), &test->err));

	Test_assert(test, "Buffer_consume (bounds)", !Buffer_consume(&smallBuf, &val, sizeof(val), NULL));

	//Test append bounds checking

	Buffer smallAppendBuf = Buffer_createRef(dat1, 16);

	for (i = 0; i < 2; ++i)
		Test_assert(test, "Buffer_append (setup)", Buffer_append(&smallAppendBuf, &val, sizeof(val), &test->err));

	Test_assert(test, "Buffer_append (bounds)", !Buffer_append(&smallAppendBuf, &val, sizeof(val), NULL));

	//Test Buffer_offset

	Buffer offsetBuf = Buffer_createRef(dat, sizeof(dat));

	Test_assert(test, "Buffer_offset", Buffer_offset(&offsetBuf, 16, &test->err));
	Test_assert(test, "Buffer_offset", Buffer_consume(&offsetBuf, &val, sizeof(val), &test->err));
	Test_assert(test, "Buffer_offset (value)", val == 2);

	Test_assert(test, "Buffer_offset", Buffer_offset(&offsetBuf, 24, &test->err));
	Test_assert(test, "Buffer_offset", Buffer_consume(&offsetBuf, &val, sizeof(val), &test->err));
	Test_assert(test, "Buffer_offset (value)", val == 6);

	Test_assert(test, "Buffer_offset (bounds)", !Buffer_offset(&offsetBuf, 1000, NULL));
	Test_assert(test, "Buffer_offset (owned)", !Buffer_offset(&fakeBuf, 8, NULL));        //Should fail on owned buffer
}
