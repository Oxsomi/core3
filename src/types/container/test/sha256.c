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

#include "types/base/string.h"
#include "types/container/string.h"
#include "shared.h"

//https://www.di-mgt.com.au/sha_testvectors.html
//https://www.dlitz.net/crypto/shad256-test-vectors/
//https://github.com/amosnier/sha-2/blob/master/test.c

//#define EXTRA_CHECKS

void Test_sha256(Test *t) {
	
	Test_setModule(t, "SHA256");
	
	static const U32 resultHashes[][8] = {
		{ 0xE3B0C442, 0x98FC1C14, 0x9AFBF4C8, 0x996FB924, 0x27AE41E4, 0x649B934C, 0xA495991B, 0x7852B855 },
		{ 0xBA7816BF, 0x8F01CFEA, 0x414140DE, 0x5DAE2223, 0xB00361A3, 0x96177A9C, 0xB410FF61, 0xF20015AD },
		{ 0xCDC76E5C, 0x9914FB92, 0x81A1C7E2, 0x84D73E67, 0xF1809A48, 0xA497200E, 0x046D39CC, 0xC7112CD0 },
		{ 0x067C5312, 0x69735CA7, 0xF541FDAC, 0xA8F0DC76, 0x305D3CAD, 0xA140F893, 0x72A410FE, 0x5EFF6E4D },
		{ 0x038051E9, 0xC324393B, 0xD1CA1978, 0xDD0952C2, 0xAA3742CA, 0x4F1BD5CD, 0x4611CEA8, 0x3892D382 },
		{ 0x248D6A61, 0xD20638B8, 0xE5C02693, 0x0C3E6039, 0xA33CE459, 0x64FF2167, 0xF6ECEDD4, 0x19DB06C1 },
		{ 0xCF5B16A7, 0x78AF8380, 0x036CE59E, 0x7B049237, 0x0B249B11, 0xE8F07A51, 0xAFAC4503, 0x7AFEE9D1 },
		{ 0xA8AE6E6E, 0xE929ABEA, 0x3AFCFC52, 0x58C8CCD6, 0xF85273E0, 0xD4626D26, 0xC7279F32, 0x50F77C8E },
		{ 0x057EE79E, 0xCE0B9A84, 0x9552AB8D, 0x3C335FE9, 0xA5F1C46E, 0xF5F1D9B1, 0x90C29572, 0x8628299C },
		{ 0x2A6AD82F, 0x3620D3EB, 0xE9D678C8, 0x12AE1231, 0x2699D673, 0x240D5BE8, 0xFAC0910A, 0x70000D93 },
		{ 0x68325720, 0xAABD7C82, 0xF30F554B, 0x313D0570, 0xC95ACCBB, 0x7DC4B5AA, 0xE11204C0, 0x8FFE732B },
		{ 0x7ABC22C0, 0xAE5AF26C, 0xE93DBB94, 0x433A0E0B, 0x2E119D01, 0x4F8E7F65, 0xBD56C61C, 0xCCCD9504 },
		{ 0x02779466, 0xCDEC1638, 0x11D07881, 0x5C633F21, 0x90141308, 0x1449002F, 0x24AA3E80, 0xF0B88EF7 },
		{ 0xD4817AA5, 0x497628E7, 0xC77E6B60, 0x6107042B, 0xBBA31308, 0x88C5F47A, 0x375E6179, 0xBE789FBB },
		{ 0x65A16CB7, 0x861335D5, 0xACE3C607, 0x18B5052E, 0x44660726, 0xDA4CD13B, 0xB745381B, 0x235A1785 },
		{ 0xF5A5FD42, 0xD16A2030, 0x2798EF6E, 0xD309979B, 0x43003D23, 0x20D9F0E8, 0xEA9831A9, 0x2759FB4B },
		{ 0x541B3E9D, 0xAA09B20B, 0xF85FA273, 0xE5CBD3E8, 0x0185AA4E, 0xC298E765, 0xDB87742B, 0x70138A53 },
		{ 0xC2E68682, 0x3489CED2, 0x017F6059, 0xB8B23931, 0x8B6364F6, 0xDCD835D0, 0xA519105A, 0x1EADD6E4 },
		{ 0xF4D62DDE, 0xC0F3DD90, 0xEA1380FA, 0x16A5FF8D, 0xC4C54B21, 0x740650F2, 0x4AFC4120, 0x903552B0 },
		{ 0xD29751F2, 0x649B32FF, 0x572B5E0A, 0x9F541EA6, 0x60A50F94, 0xFF0BEEDF, 0xB0B692B9, 0x24CC8025 },
		{ 0x15A1868C, 0x12CC5395, 0x1E182344, 0x277447CD, 0x0979536B, 0xADCC512A, 0xD24C67E9, 0xB2D4F3DD },
		{ 0x461C19A9, 0x3BD4344F, 0x9215F5EC, 0x64357090, 0x342BC66B, 0x15A14831, 0x7D276E31, 0xCBC20B53 },
		{ 0xC23CE8A7, 0x895F4B21, 0xEC0DAF37, 0x920AC0A2, 0x62A22004, 0x5A03EB2D, 0xFED48EF9, 0xB05AABEA }
	};

	U8 data7[1001];
	U8 data8[1000];
	U8 data9[1005];

	Buffer_unsetAllBits(Buffer_createRef(data7, sizeof(data7)), NULL);
	Buffer_setAllToU8(Buffer_createRef(data8, sizeof(data8)), 0x41, NULL);
	Buffer_setAllToU8(Buffer_createRef(data9, sizeof(data9)), 0x55, NULL);

	CharString inputs[] = {
		{ 0 },
		CharString_createRefCStrConst("abc"),
		{ 0 },
		CharString_createRefSizedConst(
			"\xDE\x18\x89\x41\xA3\x37\x5D\x3A\x8A\x06\x1E\x67\x57\x6E\x92\x6D",
			16, true),
		CharString_createRefSizedConst(
			"\xDE\x18\x89\x41\xA3\x37\x5D\x3A\x8A\x06\x1E\x67\x57\x6E\x92\x6D"
			"\xC7\x1A\x7F\xA3\xF0\xCC\xEB\x97\x45\x2B\x4D\x32\x27\x96\x5F\x9E"
			"\xA8\xCC\x75\x07\x6D\x9F\xB9\xC5\x41\x7A\xA5\xCB\x30\xFC\x22\x19"
			"\x8B\x34\x98\x2D\xBB\x62\x9E",
			55, true),
		CharString_createRefCStrConst("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
		CharString_createRefCStrConst(
			"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
			"hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"),
		CharString_createRefCStrConst("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
		CharString_createRefCStrConst("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde"),
		CharString_createRefCStrConst("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0"),
		CharString_createRefSizedConst("\xBD", 1, true),
		CharString_createRefSizedConst("\xC9\x8C\x8E\x55", 4, true),
		{ 0 },
		{ 0 },
		{ 0 },
		{ 0 },
		{ 0 },
		{ 0 },
		{ 0 },
		#ifdef EXTRA_CHECKS
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
		#endif
	};

	inputs[12] = CharString_createRefSizedConst((const C8*) data7, 55,   true);
	inputs[13] = CharString_createRefSizedConst((const C8*) data7, 56,   true);
	inputs[14] = CharString_createRefSizedConst((const C8*) data7, 57,   true);
	inputs[15] = CharString_createRefSizedConst((const C8*) data7, 64,   true);
	inputs[16] = CharString_createRefSizedConst((const C8*) data7, 1000, true);
	inputs[17] = CharString_createRefSizedConst((const C8*) data8, 1000, false);
	inputs[18] = CharString_createRefSizedConst((const C8*) data9, 1005, false);

	//If any of these fail to allocate, it won't matter, because our test will fail anyways.
	//We don't want this to fail others, but we'll still Test_assert on allocation success

	Bool allocSuccess = CharString_create('a', MEGA, t->alloc, &inputs[2], NULL);

	#ifdef EXTRA_CHECKS
		allocSuccess &= CharString_create('\x5A', 536870912,  t->alloc, &inputs[20], NULL);
		allocSuccess &= CharString_create('\0',   1090519040, t->alloc, &inputs[21], NULL);
		allocSuccess &= CharString_create('\x42', 1610612798, t->alloc, &inputs[22], NULL);
		inputs[19] = CharString_createRefSizedConst(inputs[21].ptr, 1000000, true);
	#endif

	//Validate

	#ifndef EXTRA_CHECKS
		U64 validCount = 19;
	#else
		U64 validCount = sizeof(inputs) / sizeof(inputs[0]);
	#endif

	for (U64 i = 0; i < validCount; ++i) {

		U32 result[8];
		Buffer_sha256(CharString_bufferConst(inputs[i]), result);

		const Bool match = Buffer_eq(
			Buffer_createRefConst(result,          32),
			Buffer_createRefConst(resultHashes[i], 32)
		);

		Test_assert(t, "SHA256", match);
	}

	Test_assert(t, "SHA256: Alloc success", allocSuccess);

	for (U64 i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i)
		CharString_free(&inputs[i], t->alloc);
}
