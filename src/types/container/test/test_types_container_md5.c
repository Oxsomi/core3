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

//types/container/test/test_types_container_md5.c

#include "test_types_container_shared.h"
#include "types/math/vec4i_swizzle.h"
#include "types/container/buffer.h"
#include "types/container/string.h"

//MD5
// https://github.com/das-labor/legacy/blob/master/microcontroller-2/arm-crypto-lib/testvectors/Md5-128.unverified.test-vectors
// https://www.md5hashgenerator.com/
// https://www.febooti.com/products/filetweak/members/hash-and-crc/test-vectors/
// https://emn178.github.io/online-tools/md5_checksum.html

void Test_md5(Test *test) {

	Test_setModule(test, "MD5");

	static const struct { const C8 *str; U64 hi; U64 lo; } strVecs[] = {
		{ "",                                             0xD41D8CD98F00B204, 0xE9800998ECF8427E },
		{ "a",                                            0x0CC175B9C0F1B6A8, 0x31C399E269772661 },
		{ "abc",                                          0x900150983CD24FB0, 0xD6963F7D28E17F72 },
		{ "Hello world",                                  0x3E25960A79DBC69B, 0x674CD4EC67A72C62 },
		{ "abcdefghijklmnopqrstuvwxyz",                   0xC3FCD3D76192E400, 0x7DFB496CCA67E13B },
		{ "1234567890",                                   0xE807F1FCF82D132F, 0x9BB018CA6738A19F },
		{ "The quick brown fox jumps over the lazy dog",  0x9E107D9D372BB682, 0x6BD81D3542A419D6 },
		{ "The quick brown fox jumps over the lazy dog.", 0xE4D909C290D0FB1C, 0xA068FFADDF22CBD0 }
	};

	for (U64 i = 0; i < sizeof(strVecs) / sizeof(strVecs[0]); ++i) {
		const Buffer buf = Buffer_createRefConst(strVecs[i].str, CharString_calcStrLen(strVecs[i].str, U64_MAX));
		const I32x4 got  = Buffer_md5(buf);
		const I32x4 want = I32x4_yxwz(I32x4_createFromU64x2(strVecs[i].hi, strVecs[i].lo));
		Test_assert(test, strVecs[i].str, I32x4_eq4(got, want));
	}

	static const U64 nullVecHiLo[][2] = {
		{ 0x93B885ADFE0DA089, 0xCDF634904FD59F71 },   // 1
		{ 0xC4103F122D27677C, 0x9DB144CAE1394A66 },   // 2
		{ 0x693E9AF84D3DFCC7, 0x1E640E005BDC5E2E },   // 3
		{ 0xF1D3FF8443297732, 0x862DF21DC4E57262 },   // 4
		{ 0xCA9C491AC66B2C62, 0x500882E93F3719A8 },   // 5
		{ 0x7319468847D7B1AE, 0xE40DBF5DD963C999 },   // 6
		{ 0xD310A40483F9399D, 0xD7ED1712E0FDD702 },   // 7
		{ 0x7DEA362B3FAC8E00, 0x956A4952A3D4F474 },   // 8
		{ 0x3F2829B2FFE8434D, 0x67F98A2A98968652 },   // 9
		{ 0xA63C90CC3684AD8B, 0x0A2176A6A8FE9005 },   //10
		{ 0x74DA4121DC1C0ED2, 0xA8E5B0741F824034 },   //11
		{ 0x8DD6BB7329A71449, 0xB0A1B292B5999164 },   //12
		{ 0x0B867E53C1D233CE, 0x9FE49D54549A2323 },   //13
		{ 0x36DF9540A5EF4996, 0xA9737657E4A8929C },   //14
		{ 0x3449C9E5E332F1DB, 0xB81505CD739FBF3F },   //15
		{ 0x4AE71336E44BF9BF, 0x79D2752E234818A5 },   //16
		{ 0xF3C8BDB6B9DF478F, 0x227AF2CE61C8A5A1 },   //17
		{ 0xFF035BFF2DCF972E, 0xE7DFD023455997EF },   //18
		{ 0x0E6BCE6899FAE841, 0xF79024AFBDF7DB1D },   //19
		{ 0x4410185252084577, 0x05BF09A8EE3C1093 },   //20
		{ 0x2319AC34F4848755, 0xA639FD524038DFD3 },   //21
		{ 0xDB46E81649D6863B, 0x16BD99AB139C865B },   //22
		{ 0x6B43B583E2B66272, 0x4B6FBB5189F6AB28 },   //23
		{ 0x1681FFC6E046C7AF, 0x98C9E6C232A3FE0A },   //24
		{ 0xD28C293E10139D5D, 0x8F6E4592AEAFFC1B },   //25
		{ 0xA396C59A96AF3B36, 0xD364448C7B687FB1 },   //26
		{ 0x65435A5D117AA6B0, 0x52A5F737D9946A7B },   //27
		{ 0x1C9E99E48A495FE8, 0x1D388FDB4900E59F },   //28
		{ 0x4AA476A72347BA44, 0xC9BD20C974D0F181 },   //29
		{ 0x862DEC5C27142824, 0xA394BC6464928F48 },   //30
		{ 0x3861FACEE9EFC127, 0xE340387F1936B8FB },   //31
		{ 0x70BC8F4B72A86921, 0x468BF8E8441DCE51 },   //32
		{ 0x099A150E83972A43, 0x3492A59C2FBE98E0 },   //33
		{ 0x0B91F1D54F932DC6, 0x382DC69F197900CF },   //34
		{ 0xC54104D7894A1941, 0xCA710981DA437F9F },   //35
		{ 0x81684C2E68ADE2CD, 0x4BF9F2E8A67DD4FE },   //36
		{ 0x21E2E8FE686ED000, 0x3B67D698B1273481 },   //37
		{ 0xF3A534D52E3FE0C7, 0xA85B30CA00CA7424 },   //38
		{ 0x002D5910DE023EDD, 0xCE8358EDF169C07F },   //39
		{ 0xFD4B38E94292E002, 0x51B9F39C47EE5710 },   //40
		{ 0xF5CFD73023C1EEDB, 0x6B9569736073F1DD },   //41
		{ 0xC183857770364B05, 0xC2011BDEBB914ED3 },   //42
		{ 0xAEA2FA668453E23C, 0x431649801E5EA548 },   //43
		{ 0x3E5CEB07F51A70D9, 0xD431714F04C0272F },   //44
		{ 0x7622214B8536AFE7, 0xB89B1C6606069B0D },   //45
		{ 0xD898504A722BFF15, 0x24134C6AB6A5EAA5 },   //46
		{ 0x0D7DB7FF842F89A3, 0x6B58FA2541DE2A6C },   //47
		{ 0xB203621A65475445, 0xE6FCDCA717C667B5 },   //48
		{ 0x884BB48A55DA67B4, 0x812805CB8905277D },   //49
		{ 0x871BDD96B159C14D, 0x15C8D97D9111E9C8 },   //50
		{ 0xE2365BC6A6FBD412, 0x87FAE648437296FA },   //51
		{ 0x469AA816010C9C86, 0x39A9176F625189AF },   //52
		{ 0xECA0470178275AC9, 0x4E5DE381969ED232 },   //53
		{ 0x8910E6FC12F07A52, 0xB796EB55FBF3EDDA },   //54
		{ 0xC9EA3314B91C9FD4, 0xE38F9432064FD1F2 },   //55
		{ 0xE3C4DD21A9171FD3, 0x9D208EFA09BF7883 },   //56
		{ 0xAB9D8EF2FFA9145D, 0x6C325CEFA41D5D4E },   //57
		{ 0x2C1CF4F76FA1CECC, 0x0C4737CFD8D95118 },   //58
		{ 0x22031453E4C3A1A0, 0xD47B0B97D83D8984 },   //59
		{ 0xA302A771EE0E3127, 0xB8950F0A67D17E49 },   //60
		{ 0xE2A482A389696467, 0x5811DBA0BFDE2F0B },   //61
		{ 0x8D7D1020185F9B09, 0xCC22E789887BE328 },   //62
		{ 0x65CECFB980D72FDE, 0x57D175D6EC1C3F64 },   //63
		{ 0x3B5D3C7D207E37DC, 0xEEEDD301E35E2E58 },   //64
		{ 0x1EF5E829303A139C, 0xE967440E0CDCA10C },   //65
		{ 0x402535C9F22FF836, 0xEA91DD12E8B8847B },   //66
		{ 0x53553242D57214AA, 0xA5726A09B05FE7BC },   //67
		{ 0x7C909B3E2820C8B4, 0x7ED418753698A6DA },   //68
		{ 0x3B8151ACFB469AE4, 0x1D3F0449058076E1 },   //69
		{ 0x3287282FA1A1523A, 0x294FB018E3679872 },   //70
		{ 0x2F0F98115F17F286, 0x9C1F59BA804AF077 },   //71
		{ 0xE457FBAE1DD166A0, 0xC89D244AC03F4E93 },   //127
		{ 0xF09F35A563783945, 0x8E462E6350ECBCE4 },   //128
		{ 0x5F54D1240735D469, 0x80B776AF554F44D3 },   //129
		{ 0xAB40B115CE85A866, 0xA20F7E9B7E7A7E0F },   //191
		{ 0xB7DD5E0194EE0AC0, 0x8A4B802CB73D867F },   //192
		{ 0xA9320A41AC8208A5, 0x4979900A8FF67DD9 }    //193
	};

	static const U64 nullVecSizes[] = {
		1,   2,  3,  4,  5,  6,  7,  8,  9, 10,
		11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
		31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
		41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
		51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
		61, 62, 63, 64, 65, 66, 67, 68, 69, 70,
		71,
		127, 128, 129,
		191, 192, 193
	};

	static const U8 empty[194] = { 0 };

	for (U64 i = 0; i < sizeof(nullVecSizes) / sizeof(nullVecSizes[0]); ++i) {
		const I32x4 got  = Buffer_md5(Buffer_createRefConst(empty, nullVecSizes[i]));
		const I32x4 want = I32x4_yxwz(I32x4_createFromU64x2(nullVecHiLo[i][0], nullVecHiLo[i][1]));
		Test_assert(test, "MD5 null bytes", I32x4_eq4(got, want));
	}
}
