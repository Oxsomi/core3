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

#include "types/base/time.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/math/type_cast.h"
#include "types/container/buffer_layout.h"
#include "types/math/flp.h"
#include "types/container/big_int.h"
#include "formats/oiBC/chimera.h"
#include "types/container/log.h"

#include <stdlib.h>
#include <string.h>

#include "types/math/quat.h"

Error ourAlloc(void *allocator, U64 length, Buffer *output) {

	(void)allocator;

	if(!output)
		return Error_nullPointer(2, "ourAlloc()::output is required");

	void *ptr = malloc(length);

	if(!ptr)
		return Error_outOfMemory(0, "ourAlloc() malloc failed");

	*output = Buffer_createManagedPtr(ptr, length);
	return Error_none();
}

Bool ourFree(void *allocator, Buffer buf) {
	(void)allocator;
	free((U8*)buf.ptr);
	return true;
}

//Required to compile

void Error_fillStackTrace(Error *err) {
	if(err)
		err->stackTrace[0] = NULL;
}

//#define STRICT_VALIDATION

#ifdef STRICT_VALIDATION
	#define EXTRA_CHECKS 4
#else
	#define EXTRA_CHECKS 0
#endif

//

int main() {

	Allocator alloc = (Allocator) {
		.alloc = ourAlloc,
		.free = ourFree,
		.ptr = NULL
	};

	//Fail for big endian systems, because we don't support them.

	U16 v = 1;

	if(!*(U8*)&v) {
		Log_errorLn(alloc, "Failed unit test: Unsupported endianness (only supporting little endian architectures)");
		return -1;
	}

	//Test timer

	Log_debugLn(alloc, "Testing Time");

	Ns now = Time_now();
	TimeFormat nowStr;

	Time_format(now, nowStr, false);

	Ns now2 = 0;

	if (!(Time_parseFormat(&now2, nowStr, false)) || now2 != now) {
		Log_errorLn(alloc, "Failed unit test: Time_parseFormat or Time_format is broken");
		Log_errorLn(alloc, "%s was parsed but retrieved timestamps %"PRIu64" and %"PRIu64, nowStr, now, now2);
		return 1;
	}

	Time_format(now, nowStr, true);

	if (!(Time_parseFormat(&now2, nowStr, true)) || now2 != now) {
		Log_errorLn(alloc, "Failed unit test: Time_parseFormat or Time_format (local time) is broken");
		return 2;
	}

	Buffer emp = Buffer_createNull(), full = Buffer_createNull();
	Buffer outputEncrypted = Buffer_createNull(), outputDecrypted = Buffer_createNull();
	Error err = Error_none();
	CharString tmp = CharString_createNull();
	BufferLayout bufferLayout = (BufferLayout) { 0 };

	CharString inputs[19 + EXTRA_CHECKS] = { 0 };

	//Test Buffer

	Log_debugLn(alloc, "Testing Buffer");

	gotoIfError(clean, Buffer_createZeroBits(256, alloc, &emp))
	gotoIfError(clean, Buffer_createOneBits(256, alloc, &full))

	if (Buffer_eq(emp, full))
		gotoIfError(clean, Error_invalidOperation(0, "Buffer_eq test failed"))

	Buffer_bitwiseNot(emp);

	if (!Buffer_eq(emp, full))
		gotoIfError(clean, Error_invalidOperation(1, "!Buffer_eq test failed"))

	Buffer_bitwiseNot(emp);

	Buffer_setBitRange(emp, 9, 240);
	Buffer_unsetBitRange(full, 9, 240);

	Buffer_bitwiseNot(emp);

	if (Buffer_neq(emp, full))
		gotoIfError(clean, Error_invalidOperation(2, "Buffer_neq test failed"))

	Buffer_free(&emp, alloc);
	Buffer_free(&full, alloc);

	//TODO: Test vectors
	//TODO: Test quaternions
	//TODO: Test transform
	//TODO: Test string
	//TODO: Test math
	//TODO: Test file
	//TODO: Test list

	//Test string to number functions

	{
		Log_debugLn(alloc, "Testing number to CharString conversions");

		CharString resultsStr[] = {
			CharString_createRefCStrConst("0x1234"),
			CharString_createRefCStrConst("0b10101"),
			CharString_createRefCStrConst("0o707"),
			CharString_createRefCStrConst("0nNiceNumber"),
			CharString_createRefCStrConst("69420")
		};

		U64 resultsU64[] = {

			0x1234,
			0b10101,
			0707,

			((U64)C8_nyto('N') << 54) | ((U64)C8_nyto('i') << 48) | ((U64)C8_nyto('c') << 42) | ((U64)C8_nyto('e') << 36) |
			((U64)C8_nyto('N') << 30) | ((U64)C8_nyto('u') << 24) | ((U64)C8_nyto('m') << 18) | ((U64)C8_nyto('b') << 12) |
			((U64)C8_nyto('e') <<  6) | C8_nyto('r'),

			69420
		};

		//Conversion from number to string
		//TODO: Perhaps make a generic create that can pick any of them

		CharString tmpStr = CharString_createNull();
		gotoIfError(clean, CharString_createHex(resultsU64[0], 0, alloc, &tmpStr))

		if (!CharString_equalsStringSensitive(resultsStr[0], tmpStr)) {
			CharString_free(&tmpStr, alloc);
			gotoIfError(clean, Error_invalidState(0, "CharString_createHex test failed"))
		}

		CharString_free(&tmpStr, alloc);
		gotoIfError(clean, CharString_createBin(resultsU64[1], 0, alloc, &tmpStr))

		if (!CharString_equalsStringSensitive(resultsStr[1], tmpStr)) {
			CharString_free(&tmpStr, alloc);
			gotoIfError(clean, Error_invalidState(1, "CharString_createBin test failed"))
		}

		CharString_free(&tmpStr, alloc);
		gotoIfError(clean, CharString_createOct(resultsU64[2], 0, alloc, &tmpStr))

		if (!CharString_equalsStringSensitive(resultsStr[2], tmpStr)) {
			CharString_free(&tmpStr, alloc);
			gotoIfError(clean, Error_invalidState(2, "CharString_createOct test failed"))
		}

		CharString_free(&tmpStr, alloc);
		gotoIfError(clean, CharString_createNyto(resultsU64[3], 0, alloc, &tmpStr))

		if (!CharString_equalsStringSensitive(resultsStr[3], tmpStr)) {
			CharString_free(&tmpStr, alloc);
			gotoIfError(clean, Error_invalidState(3, "CharString_createNyto test failed"))
		}

		CharString_free(&tmpStr, alloc);
		gotoIfError(clean, CharString_createDec(resultsU64[4], 0, alloc, &tmpStr))

		if (!CharString_equalsStringSensitive(resultsStr[4], tmpStr)) {
			CharString_free(&tmpStr, alloc);
			gotoIfError(clean, Error_invalidState(4, "CharString_createDec test failed"))
		}

		//Conversion from string to number

		U64 tmpRes[5] = { 0 };

		Bool success = true;

		success &= CharString_parseHex(resultsStr[0], tmpRes + 0);
		success &= CharString_parseBin(resultsStr[1], tmpRes + 1);
		success &= CharString_parseOct(resultsStr[2], tmpRes + 2);
		success &= CharString_parseNyto(resultsStr[3], tmpRes + 3);
		success &= CharString_parseDec(resultsStr[4], tmpRes + 4);

		if(!success)
			gotoIfError(clean, Error_invalidState(5, "CharString_parseHex/Bin/Oct/Nyto/Dec returned error(s)"))

		for(U64 i = 0; i < sizeof(resultsStr) / sizeof(resultsStr[0]); ++i)
			success &= tmpRes[i] == resultsU64[i];

		if(!success)
			gotoIfError(clean, Error_invalidState(6, "CharString_parseHex/Bin/Oct/Nyto/Dec test failed"))
	}

	//Test MD5 function
	//A combo between:
	//https://github.com/das-labor/legacy/blob/master/microcontroller-2/arm-crypto-lib/testvectors/Md5-128.unverified.test-vectors
	//https://www.md5hashgenerator.com/
	//https://www.febooti.com/products/filetweak/members/hash-and-crc/test-vectors/
	//https://emn178.github.io/online-tools/md5_checksum.html

	Log_debugLn(alloc, "Testing Buffer MD5");

	{
		CharString md5Strs[] = {
			CharString_createRefCStrConst(""),
			CharString_createRefCStrConst("a"),
			CharString_createRefCStrConst("abc"),
			CharString_createRefCStrConst("Hello world"),
			CharString_createRefCStrConst("abcdefghijklmnopqrstuvwxyz"),
			CharString_createRefCStrConst("1234567890"),
			CharString_createRefCStrConst("The quick brown fox jumps over the lazy dog"),
			CharString_createRefCStrConst("The quick brown fox jumps over the lazy dog.")
		};

		I32x4 md5Values[] = {

			//Constants

			I32x4_createFromU64x2(0xD41D8CD98F00B204, 0xE9800998ECF8427E),
			I32x4_createFromU64x2(0x0CC175B9C0F1B6A8, 0x31C399E269772661),
			I32x4_createFromU64x2(0x900150983CD24FB0, 0xD6963F7D28E17F72),
			I32x4_createFromU64x2(0x3E25960A79DBC69B, 0x674CD4EC67A72C62),
			I32x4_createFromU64x2(0xC3FCD3D76192E400, 0x7DFB496CCA67E13B),
			I32x4_createFromU64x2(0xE807F1FCF82D132F, 0x9BB018CA6738A19F),
			I32x4_createFromU64x2(0x9E107D9D372BB682, 0x6BD81D3542A419D6),
			I32x4_createFromU64x2(0xE4D909C290D0FB1C, 0xA068FFADDF22CBD0),

			//Empty data

			I32x4_createFromU64x2(0x93B885ADFE0DA089, 0xCDF634904FD59F71),	// 1 * '\0'
			I32x4_createFromU64x2(0xC4103F122D27677C, 0x9DB144CAE1394A66),	// 2 * '\0'
			I32x4_createFromU64x2(0x693E9AF84D3DFCC7, 0x1E640E005BDC5E2E),	// 3 * '\0'
			I32x4_createFromU64x2(0xF1D3FF8443297732, 0x862DF21DC4E57262),	// 4 * '\0'
			I32x4_createFromU64x2(0xCA9C491AC66B2C62, 0x500882E93F3719A8),	// 5 * '\0'
			I32x4_createFromU64x2(0x7319468847D7B1AE, 0xE40DBF5DD963C999),	// 6 * '\0'
			I32x4_createFromU64x2(0xD310A40483F9399D, 0xD7ED1712E0FDD702),	// 7 * '\0'
			I32x4_createFromU64x2(0x7DEA362B3FAC8E00, 0x956A4952A3D4F474),	// 8 * '\0'
			I32x4_createFromU64x2(0x3F2829B2FFE8434D, 0x67F98A2A98968652),	// 9 * '\0'
			I32x4_createFromU64x2(0xA63C90CC3684AD8B, 0x0A2176A6A8FE9005),	//10 * '\0'
			I32x4_createFromU64x2(0x74DA4121DC1C0ED2, 0xA8E5B0741F824034),	//11 * '\0'
			I32x4_createFromU64x2(0x8DD6BB7329A71449, 0xB0A1B292B5999164),	//12 * '\0'
			I32x4_createFromU64x2(0x0B867E53C1D233CE, 0x9FE49D54549A2323),	//13 * '\0'
			I32x4_createFromU64x2(0x36DF9540A5EF4996, 0xA9737657E4A8929C),	//14 * '\0'
			I32x4_createFromU64x2(0x3449C9E5E332F1DB, 0xB81505CD739FBF3F),	//15 * '\0'
			I32x4_createFromU64x2(0x4AE71336E44BF9BF, 0x79D2752E234818A5),	//16 * '\0'

			I32x4_createFromU64x2(0xF3C8BDB6B9DF478F, 0x227AF2CE61C8A5A1),	//17 * '\0'
			I32x4_createFromU64x2(0xFF035BFF2DCF972E, 0xE7DFD023455997EF),	//18 * '\0'
			I32x4_createFromU64x2(0x0E6BCE6899FAE841, 0xF79024AFBDF7DB1D),	//19 * '\0'
			I32x4_createFromU64x2(0x4410185252084577, 0x05BF09A8EE3C1093),	//20 * '\0'
			I32x4_createFromU64x2(0x2319AC34F4848755, 0xA639FD524038DFD3),	//21 * '\0'
			I32x4_createFromU64x2(0xDB46E81649D6863B, 0x16BD99AB139C865B),	//22 * '\0'
			I32x4_createFromU64x2(0x6B43B583E2B66272, 0x4B6FBB5189F6AB28),	//23 * '\0'
			I32x4_createFromU64x2(0x1681FFC6E046C7AF, 0x98C9E6C232A3FE0A),	//24 * '\0'
			I32x4_createFromU64x2(0xD28C293E10139D5D, 0x8F6E4592AEAFFC1B),	//25 * '\0'
			I32x4_createFromU64x2(0xA396C59A96AF3B36, 0xD364448C7B687FB1),	//26 * '\0'
			I32x4_createFromU64x2(0x65435A5D117AA6B0, 0x52A5F737D9946A7B),	//27 * '\0'
			I32x4_createFromU64x2(0x1C9E99E48A495FE8, 0x1D388FDB4900E59F),	//28 * '\0'
			I32x4_createFromU64x2(0x4AA476A72347BA44, 0xC9BD20C974D0F181),	//29 * '\0'
			I32x4_createFromU64x2(0x862DEC5C27142824, 0xA394BC6464928F48),	//30 * '\0'
			I32x4_createFromU64x2(0x3861FACEE9EFC127, 0xE340387F1936B8FB),	//31 * '\0'
			I32x4_createFromU64x2(0x70BC8F4B72A86921, 0x468BF8E8441DCE51),	//32 * '\0'

			I32x4_createFromU64x2(0x099A150E83972A43, 0x3492A59C2FBE98E0),	//33 * '\0'
			I32x4_createFromU64x2(0x0B91F1D54F932DC6, 0x382DC69F197900CF),	//34 * '\0'
			I32x4_createFromU64x2(0xC54104D7894A1941, 0xCA710981DA437F9F),	//35 * '\0'
			I32x4_createFromU64x2(0x81684C2E68ADE2CD, 0x4BF9F2E8A67DD4FE),	//36 * '\0'
			I32x4_createFromU64x2(0x21E2E8FE686ED000, 0x3B67D698B1273481),	//37 * '\0'
			I32x4_createFromU64x2(0xF3A534D52E3FE0C7, 0xA85B30CA00CA7424),	//38 * '\0'
			I32x4_createFromU64x2(0x002D5910DE023EDD, 0xCE8358EDF169C07F),	//39 * '\0'
			I32x4_createFromU64x2(0xFD4B38E94292E002, 0x51B9F39C47EE5710),	//40 * '\0'
			I32x4_createFromU64x2(0xF5CFD73023C1EEDB, 0x6B9569736073F1DD),	//41 * '\0'
			I32x4_createFromU64x2(0xC183857770364B05, 0xC2011BDEBB914ED3),	//42 * '\0'
			I32x4_createFromU64x2(0xAEA2FA668453E23C, 0x431649801E5EA548),	//43 * '\0'
			I32x4_createFromU64x2(0x3E5CEB07F51A70D9, 0xD431714F04C0272F),	//44 * '\0'
			I32x4_createFromU64x2(0x7622214B8536AFE7, 0xB89B1C6606069B0D),	//45 * '\0'
			I32x4_createFromU64x2(0xD898504A722BFF15, 0x24134C6AB6A5EAA5),	//46 * '\0'
			I32x4_createFromU64x2(0x0D7DB7FF842F89A3, 0x6B58FA2541DE2A6C),	//47 * '\0'
			I32x4_createFromU64x2(0xB203621A65475445, 0xE6FCDCA717C667B5),	//48 * '\0'

			I32x4_createFromU64x2(0x884BB48A55DA67B4, 0x812805CB8905277D),	//49 * '\0'
			I32x4_createFromU64x2(0x871BDD96B159C14D, 0x15C8D97D9111E9C8),	//50 * '\0'
			I32x4_createFromU64x2(0xE2365BC6A6FBD412, 0x87FAE648437296FA),	//51 * '\0'
			I32x4_createFromU64x2(0x469AA816010C9C86, 0x39A9176F625189AF),	//52 * '\0'
			I32x4_createFromU64x2(0xECA0470178275AC9, 0x4E5DE381969ED232),	//53 * '\0'
			I32x4_createFromU64x2(0x8910E6FC12F07A52, 0xB796EB55FBF3EDDA),	//54 * '\0'
			I32x4_createFromU64x2(0xC9EA3314B91C9FD4, 0xE38F9432064FD1F2),	//55 * '\0'
			I32x4_createFromU64x2(0xE3C4DD21A9171FD3, 0x9D208EFA09BF7883),	//56 * '\0'
			I32x4_createFromU64x2(0xAB9D8EF2FFA9145D, 0x6C325CEFA41D5D4E),	//57 * '\0'
			I32x4_createFromU64x2(0x2C1CF4F76FA1CECC, 0x0C4737CFD8D95118),	//58 * '\0'
			I32x4_createFromU64x2(0x22031453E4C3A1A0, 0xD47B0B97D83D8984),	//59 * '\0'
			I32x4_createFromU64x2(0xA302A771EE0E3127, 0xB8950F0A67D17E49),	//60 * '\0'
			I32x4_createFromU64x2(0xE2A482A389696467, 0x5811DBA0BFDE2F0B),	//61 * '\0'
			I32x4_createFromU64x2(0x8D7D1020185F9B09, 0xCC22E789887BE328),	//62 * '\0'
			I32x4_createFromU64x2(0x65CECFB980D72FDE, 0x57D175D6EC1C3F64),	//63 * '\0'
			I32x4_createFromU64x2(0x3B5D3C7D207E37DC, 0xEEEDD301E35E2E58),	//64 * '\0'

			I32x4_createFromU64x2(0x1EF5E829303A139C, 0xE967440E0CDCA10C),	//65 * '\0'
			I32x4_createFromU64x2(0x402535C9F22FF836, 0xEA91DD12E8B8847B),	//66 * '\0'
			I32x4_createFromU64x2(0x53553242D57214AA, 0xA5726A09B05FE7BC),	//67 * '\0'
			I32x4_createFromU64x2(0x7C909B3E2820C8B4, 0x7ED418753698A6DA),	//68 * '\0'
			I32x4_createFromU64x2(0x3B8151ACFB469AE4, 0x1D3F0449058076E1),	//69 * '\0'
			I32x4_createFromU64x2(0x3287282FA1A1523A, 0x294FB018E3679872),	//70 * '\0'
			I32x4_createFromU64x2(0x2F0F98115F17F286, 0x9C1F59BA804AF077),	//71 * '\0'

			I32x4_createFromU64x2(0xE457FBAE1DD166A0, 0xC89D244AC03F4E93),	//127 * '\0'
			I32x4_createFromU64x2(0xF09F35A563783945, 0x8E462E6350ECBCE4),	//128 * '\0'
			I32x4_createFromU64x2(0x5F54D1240735D469, 0x80B776AF554F44D3),	//129 * '\0'

			I32x4_createFromU64x2(0xAB40B115CE85A866, 0xA20F7E9B7E7A7E0F),	//191 * '\0'
			I32x4_createFromU64x2(0xB7DD5E0194EE0AC0, 0x8A4B802CB73D867F),	//192 * '\0'
			I32x4_createFromU64x2(0xA9320A41AC8208A5, 0x4979900A8FF67DD9)	//193 * '\0'

		};

		U64 md5StrCount = sizeof(md5Strs) / sizeof(md5Strs[0]);

		for(U64 i = 0; i < md5StrCount; ++i) {

			I32x4 hash = Buffer_md5(CharString_bufferConst(md5Strs[i]));
			I32x4 targ = I32x4_yxwz(md5Values[i]);

			/*Log_debugLn(
				alloc,
				"%X %X %X %X and %X %X %X %X",
				I32x4_x(hash), I32x4_y(hash), I32x4_z(hash), I32x4_w(hash),
				I32x4_x(targ), I32x4_y(targ), I32x4_z(targ), I32x4_w(targ)
			);*/

			if(!I32x4_eq4(hash, targ))
				gotoIfError(clean, Error_invalidOperation(3, "MD5 test (strs) failed"))
		}

		const U8 empty[194] = { 0 };

		for(U64 i = md5StrCount; i < sizeof(md5Values) / sizeof(md5Values[0]); ++i) {

			U64 count = i - md5StrCount + 1;

			if(count >= 72)
				count = count < 75 ? (count - 72 + 127) : (count - 75 + 191);

			I32x4 hash = Buffer_md5(Buffer_createRefConst(empty, count));
			I32x4 targ = I32x4_yxwz(md5Values[i]);

			if(!I32x4_eq4(hash, targ))
				gotoIfError(clean, Error_invalidOperation(3, "MD5 test (data) failed"))
		}
	}

	//Test CRC32C function
	//https://stackoverflow.com/questions/20963944/test-vectors-for-crc32c

	Log_debugLn(alloc, "Testing Buffer CRC32C");

	typedef struct TestCRC32C {
		const C8 *str;
		const C8 v[5];
		U8 padding[3];
	} TestCRC32C;

	static const TestCRC32C TEST_CRC32C[] = {
		{ "", "\x00\x00\x00\x00", { 0 } },
		{ "a", "\x30\x43\xd0\xc1", { 0 } },
		{ "abc", "\xb7\x3f\x4b\x36", { 0 } },
		{ "message digest", "\xd0\x79\xbd\x02", { 0 } },
		{ "abcdefghijklmnopqrstuvwxyz", "\x25\xef\xe6\x9e", { 0 } },
		{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "\x7d\xd5\x45\xa2", { 0 } },
		{ "12345678901234567890123456789012345678901234567890123456789012345678901234567890", "\x81\x67\x7a\x47", { 0 } },
		{ "123456789", "\x83\x92\x06\xe3", { 0 } }
	};

	for (U64 i = 0; i < sizeof(TEST_CRC32C) / sizeof(TEST_CRC32C[0]); ++i) {

		Buffer buf = Buffer_createRefConst(
			TEST_CRC32C[i].str, CharString_calcStrLen(TEST_CRC32C[i].str, U64_MAX)
		);

		U32 groundTruth = *(const U32*) TEST_CRC32C[i].v;
		U32 ours = Buffer_crc32c(buf);

		if(groundTruth != ours)
			gotoIfError(clean, Error_invalidOperation(3, "CRC32C test failed"))
	}

	//Test SHA256 function
	//https://www.di-mgt.com.au/sha_testvectors.html
	//https://www.dlitz.net/crypto/shad256-test-vectors/
	//https://github.com/amosnier/sha-2/blob/master/test.c

	Log_debugLn(alloc, "Testing Buffer SHA256");

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

	inputs[1] = CharString_createRefCStrConst("abc");

	gotoIfError(clean, CharString_create('a', MEGA, alloc, inputs + 2))

	inputs[3] = CharString_createRefSizedConst(
		"\xde\x18\x89\x41\xa3\x37\x5d\x3a\x8a\x06\x1e\x67\x57\x6e\x92\x6d", 16, true
	);

	inputs[4] = CharString_createRefSizedConst(
		"\xDE\x18\x89\x41\xA3\x37\x5D\x3A\x8A\x06\x1E\x67\x57\x6E\x92\x6D\xC7\x1A\x7F\xA3\xF0"
		"\xCC\xEB\x97\x45\x2B\x4D\x32\x27\x96\x5F\x9E\xA8\xCC\x75\x07\x6D\x9F\xB9\xC5\x41\x7A"
		"\xA5\xCB\x30\xFC\x22\x19\x8B\x34\x98\x2D\xBB\x62\x9E",
		55,
		true
	);

	inputs[5] = CharString_createRefCStrConst("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");

	inputs[6] = CharString_createRefCStrConst(
		"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"
	);

	inputs[7] = CharString_createRefCStrConst("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
	inputs[8] = CharString_createRefCStrConst("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde");
	inputs[9] = CharString_createRefCStrConst("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0");

	inputs[10] = CharString_createRefSizedConst("\xBD", 1, true);
	inputs[11] = CharString_createRefSizedConst("\xC9\x8C\x8E\x55", 4, true);

	U8 data7[1001] = { 0 }, data8[1000], data9[1005];

	memset(data8, 0x41, sizeof(data8));
	memset(data9, 0x55, sizeof(data9));

	inputs[12] = CharString_createRefSizedConst((const C8*)data7, 55, true);
	inputs[13] = CharString_createRefSizedConst((const C8*)data7, 56, true);
	inputs[14] = CharString_createRefSizedConst((const C8*)data7, 57, true);
	inputs[15] = CharString_createRefSizedConst((const C8*)data7, 64, true);
	inputs[16] = CharString_createRefSizedConst((const C8*)data7, 1000, true);
	inputs[17] = CharString_createRefSizedConst((const C8*)data8, 1000, false);
	inputs[18] = CharString_createRefSizedConst((const C8*)data9, 1005, false);

	//More extreme checks. Don't want to run this every time.

	#if EXTRA_CHECKS

		gotoIfError(clean, CharString_create('\x5A', 536870912, alloc, inputs + 20))
		gotoIfError(clean, CharString_create('\0', 1090519040, alloc, inputs + 21))
		gotoIfError(clean, CharString_create('\x42', 1610612798, alloc, inputs + 22))

		inputs[19] = CharString_createRefSizedConst(inputs[21].ptr, 1000000, true);

	#endif

	//Validate inputs

	for(U64 i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {

		U32 result[8];
		Buffer_sha256(CharString_bufferConst(inputs[i]), result);

		Bool b = Buffer_eq(Buffer_createRefConst(result, 32), Buffer_createRefConst(resultHashes[i], 32));

		if(!b)
			gotoIfError(clean, Error_invalidOperation(4, "Buffer_sha256 test failed"))
	}

	for(U64 i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i)
		CharString_free(&inputs[i], alloc);

	//Test buffer layouts

	//Test 1: Camera[10]

	gotoIfError(clean, BufferLayout_create(alloc, &bufferLayout))

	//Transform

	BufferLayoutStructInfo transformStructInfo = (BufferLayoutStructInfo) {
		.name = CharString_createRefCStrConst("Transform")
	};

	BufferLayoutMemberInfo transformMembers[] = {

		(BufferLayoutMemberInfo) {
			.name = CharString_createRefCStrConst("rot"),
			.typeId = ETypeId_F32x4,
			.stride = sizeof(F32x4),
			.structId = U32_MAX
		},

		(BufferLayoutMemberInfo) {
			.name = CharString_createRefCStrConst("pos"),
			.typeId = ETypeId_F32x4,
			.stride = sizeof(F32x4),
			.offset = sizeof(F32x4),
			.structId = U32_MAX
		},

		(BufferLayoutMemberInfo) {
			.name = CharString_createRefCStrConst("scale"),
			.typeId = ETypeId_F32x4,
			.stride = sizeof(F32x4),
			.offset = sizeof(F32x4) * 2,
			.structId = U32_MAX
		}
	};

	gotoIfError(clean, ListBufferLayoutMemberInfo_createRefConst(
		transformMembers,
		sizeof(transformMembers) / sizeof(transformMembers[0]),
		&transformStructInfo.members
	))

	U32 transformStruct;
	gotoIfError(clean, BufferLayout_createStruct(&bufferLayout, transformStructInfo, alloc, &transformStruct))

	//

	BufferLayoutStructInfo cameraStructInfo = (BufferLayoutStructInfo) {
		.name = CharString_createRefCStrConst("Camera")
	};

	BufferLayoutMemberInfo cameraMembers[] = {

		(BufferLayoutMemberInfo) {
			.name = CharString_createRefCStrConst("transform"),
			.structId = transformStruct,
			.stride = sizeof(F32x4) * 3,
			.typeId = ETypeId_Undefined
		},

		(BufferLayoutMemberInfo) {
			.name = CharString_createRefCStrConst("p0"),
			.typeId = ETypeId_F32x4,
			.stride = sizeof(F32x4),
			.offset = sizeof(F32x4) * 3,
			.structId = U32_MAX
		},

		(BufferLayoutMemberInfo) {
			.name = CharString_createRefCStrConst("right"),
			.typeId = ETypeId_F32x4,
			.stride = sizeof(F32x4),
			.offset = sizeof(F32x4) * 4,
			.structId = U32_MAX
		},

		(BufferLayoutMemberInfo) {
			.name = CharString_createRefCStrConst("up"),
			.typeId = ETypeId_F32x4,
			.stride = sizeof(F32x4),
			.offset = sizeof(F32x4) * 5,
			.structId = U32_MAX
		},

		(BufferLayoutMemberInfo) {
			.name = CharString_createRefCStrConst("near"),
			.typeId = ETypeId_F32,
			.stride = sizeof(F32),
			.offset = sizeof(F32x4) * 6,
			.structId = U32_MAX
		},

		(BufferLayoutMemberInfo) {
			.name = CharString_createRefCStrConst("far"),
			.typeId = ETypeId_F32,
			.stride = sizeof(F32),
			.offset = sizeof(F32x4) * 6 + sizeof(F32),
			.structId = U32_MAX
		},

		(BufferLayoutMemberInfo) {
			.name = CharString_createRefCStrConst("fovRad"),
			.typeId = ETypeId_F32,
			.stride = sizeof(F32),
			.offset = sizeof(F32x4) * 6 + sizeof(F32) * 2,
			.structId = U32_MAX
		}
	};

	gotoIfError(clean, ListBufferLayoutMemberInfo_createRefConst(
		cameraMembers,
		sizeof(cameraMembers) / sizeof(cameraMembers[0]),
		&cameraStructInfo.members
	))

	U32 cameraStruct;
	gotoIfError(clean, BufferLayout_createStruct(&bufferLayout, cameraStructInfo, alloc, &cameraStruct))

	//Camera[10]

	BufferLayoutStructInfo cameraStructArrayInfo = (BufferLayoutStructInfo) { 0 };

	BufferLayoutMemberInfo cameraStructArrayMembers[] = {

		(BufferLayoutMemberInfo) {
			.name = CharString_createRefCStrConst("arr"),
			.structId = cameraStruct,
			.stride = sizeof(F32x4) * 6 + sizeof(F32) * 4 /* 1 float for padding */,
			.typeId = ETypeId_Undefined
		}
	};

	U32 cameraArrayLen = 10;
	gotoIfError(clean, ListU32_createRefConst(&cameraArrayLen, 1, &cameraStructArrayMembers[0].arraySizes))

	gotoIfError(clean, ListBufferLayoutMemberInfo_createRefConst(
		cameraStructArrayMembers,
		sizeof(cameraStructArrayMembers) / sizeof(cameraStructArrayMembers[0]),
		&cameraStructArrayInfo.members
	))

	U32 cameraStructArray;
	gotoIfError(clean, BufferLayout_createStruct(&bufferLayout, cameraStructArrayInfo, alloc, &cameraStructArray))

	gotoIfError(clean, BufferLayout_assignRootStruct(&bufferLayout, cameraStructArray))

	//Instantiate

	gotoIfError(clean, BufferLayout_createInstance(bufferLayout, 1, alloc, &emp))

	typedef struct Transform {
		QuatF32 rot;
		F32x4 pos, scale;
	} Transform;

	typedef struct Camera {

		Transform transform;

		F32x4 p0, right, up;

		F32 near, far, fovRad, padding;

	} Camera;

	if (Buffer_length(emp) != sizeof(Camera) * 10)
		gotoIfError(clean, Error_invalidState(0, "BufferLayout_createInstance was invalid"))

	//Test simple behavior

	F32x4 p0 = F32x4_create4(1, 2, 3, 4);

	gotoIfError(clean, BufferLayout_setF32x4(
		emp, bufferLayout,
		CharString_createRefCStrConst("arr/0/p0"),
		p0,
		alloc
	))

	if(F32x4_neq4(((const Camera*)emp.ptr)[0].p0, p0))
		gotoIfError(clean, Error_invalidState(1, "BufferLayout_setF32x4 arr/0/p0 didn't work"))

	gotoIfError(clean, BufferLayout_setF32x4(
		emp, bufferLayout,
		CharString_createRefCStrConst("arr/1/p0"),
		p0,
		alloc
	))

	if(F32x4_neq4(((const Camera*)emp.ptr)[1].p0, p0))
		gotoIfError(clean, Error_invalidState(2, "BufferLayout_setF32x4 arr/1/p0 didn't work"))

	gotoIfError(clean, BufferLayout_setF32x4(
		emp, bufferLayout,
		CharString_createRefCStrConst("arr/1/transform/scale"),
		p0,
		alloc
	))

	if(F32x4_neq4(((const Camera*)emp.ptr)[1].transform.scale, p0))
		gotoIfError(clean, Error_invalidState(3, "BufferLayout_setF32x4 arr/1/transform/scale didn't work"))

	F32 fovRadTest = 23;

	gotoIfError(clean, BufferLayout_setF32(
		emp, bufferLayout,
		CharString_createRefCStrConst("arr/1/fovRad"),
		fovRadTest,
		alloc
	))

	if(((const Camera*)emp.ptr)[1].fovRad != fovRadTest)
		gotoIfError(clean, Error_invalidState(4, "BufferLayout_setF32 arr/1/fovRad didn't work"))

	//Test big endian conversions

	Log_debugLn(alloc, "Testing endianness swapping");

	U16 be16 = U16_swapEndianness(0x1234);
	U32 be32 = U32_swapEndianness(0x12345678);
	U64 be64 = U64_swapEndianness(0x123456789ABCDEF0);

	if(be16 != 0x3412)
		gotoIfError(clean, Error_invalidState(5, "Little endian to big failed on U16"))

	if(be32 != 0x78563412)
		gotoIfError(clean, Error_invalidState(6, "Little endian to big failed on U32"))

	if(be64 != 0xF0DEBC9A78563412)
		gotoIfError(clean, Error_invalidState(7, "Little endian to big failed on U64"))

	//Test encryption

	//Test vectors (13-16): https://luca-giuzzi.unibs.it/corsi/Support/papers-cryptography/gcm-spec.pdf
	//						Ignored test vectors 17-18 since we don't allow arbitrary length IVs
	//
	//Test vectors	https://www.ieee802.org/1/files/public/docs2011/bn-randall-test-vectors-0511-v1.pdf
	//				2.1.2, 2.2.2, 2.3.2, 2.4.2, 2.5.2, 2.6.2, 2.7.2, 2.8.2

	{
		Log_debugLn(alloc, "Testing Buffer encrypt/decrypt (AES256)");

		CharString testKeys[] = {

			CharString_createRefSizedConst(
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
				32,
				true
			),

			CharString_createRefSizedConst(
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
				32,
				true
			),

			CharString_createRefSizedConst(
				"\xFE\xFF\xE9\x92\x86\x65\x73\x1C\x6D\x6A\x8F\x94\x67\x30\x83\x08"
				"\xFE\xFF\xE9\x92\x86\x65\x73\x1C\x6D\x6A\x8F\x94\x67\x30\x83\x08",
				32,
				true
			),

			CharString_createRefSizedConst(
				"\xFE\xFF\xE9\x92\x86\x65\x73\x1C\x6D\x6A\x8F\x94\x67\x30\x83\x08"
				"\xFE\xFF\xE9\x92\x86\x65\x73\x1C\x6D\x6A\x8F\x94\x67\x30\x83\x08",
				32,
				true
			),

			CharString_createRefSizedConst(
				"\xE3\xC0\x8A\x8F\x06\xC6\xE3\xAD\x95\xA7\x05\x57\xB2\x3F\x75\x48"
				"\x3C\xE3\x30\x21\xA9\xC7\x2B\x70\x25\x66\x62\x04\xC6\x9C\x0B\x72",
				32,
				true
			),

			CharString_createRefSizedConst(
				"\xE3\xC0\x8A\x8F\x06\xC6\xE3\xAD\x95\xA7\x05\x57\xB2\x3F\x75\x48"
				"\x3C\xE3\x30\x21\xA9\xC7\x2B\x70\x25\x66\x62\x04\xC6\x9C\x0B\x72",
				32,
				true
			),

			CharString_createRefSizedConst(
				"\x69\x1D\x3E\xE9\x09\xD7\xF5\x41\x67\xFD\x1C\xA0\xB5\xD7\x69\x08"
				"\x1F\x2B\xDE\x1A\xEE\x65\x5F\xDB\xAB\x80\xBD\x52\x95\xAE\x6B\xE7",
				32,
				true
			),

			CharString_createRefSizedConst(
				"\x69\x1D\x3E\xE9\x09\xD7\xF5\x41\x67\xFD\x1C\xA0\xB5\xD7\x69\x08"
				"\x1F\x2B\xDE\x1A\xEE\x65\x5F\xDB\xAB\x80\xBD\x52\x95\xAE\x6B\xE7",
				32,
				true
			),

			CharString_createRefSizedConst(
				"\x83\xC0\x93\xB5\x8D\xE7\xFF\xE1\xC0\xDA\x92\x6A\xC4\x3F\xB3\x60"
				"\x9A\xC1\xC8\x0F\xEE\x1B\x62\x44\x97\xEF\x94\x2E\x2F\x79\xA8\x23",
				32,
				true
			),

			CharString_createRefSizedConst(
				"\x83\xC0\x93\xB5\x8D\xE7\xFF\xE1\xC0\xDA\x92\x6A\xC4\x3F\xB3\x60"
				"\x9A\xC1\xC8\x0F\xEE\x1B\x62\x44\x97\xEF\x94\x2E\x2F\x79\xA8\x23",
				32,
				true
			),

			CharString_createRefSizedConst(
				"\x4C\x97\x3D\xBC\x73\x64\x62\x16\x74\xF8\xB5\xB8\x9E\x5C\x15\x51"
				"\x1F\xCE\xD9\x21\x64\x90\xFB\x1C\x1A\x2C\xAA\x0F\xFE\x04\x07\xE5",
				32,
				true
			),

			CharString_createRefSizedConst(
				"\x4C\x97\x3D\xBC\x73\x64\x62\x16\x74\xF8\xB5\xB8\x9E\x5C\x15\x51"
				"\x1F\xCE\xD9\x21\x64\x90\xFB\x1C\x1A\x2C\xAA\x0F\xFE\x04\x07\xE5",
				32,
				true
			)
		};

		CharString testPlainText[] = {

			CharString_createNull(),

			CharString_createRefSizedConst(
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
				16,
				true
			),

			CharString_createRefSizedConst(
				"\xD9\x31\x32\x25\xF8\x84\x06\xE5\xA5\x59\x09\xC5\xAF\xF5\x26\x9A"
				"\x86\xA7\xA9\x53\x15\x34\xF7\xDA\x2E\x4C\x30\x3D\x8A\x31\x8A\x72"
				"\x1C\x3C\x0C\x95\x95\x68\x09\x53\x2F\xCF\x0E\x24\x49\xA6\xB5\x25"
				"\xB1\x6A\xED\xF5\xAA\x0D\xE6\x57\xBA\x63\x7B\x39\x1A\xAF\xD2\x55",
				64,
				true
			),

			CharString_createRefSizedConst(
				"\xD9\x31\x32\x25\xF8\x84\x06\xE5\xA5\x59\x09\xC5\xAF\xF5\x26\x9A"
				"\x86\xA7\xA9\x53\x15\x34\xF7\xDA\x2E\x4C\x30\x3D\x8A\x31\x8A\x72"
				"\x1C\x3C\x0C\x95\x95\x68\x09\x53\x2F\xCF\x0E\x24\x49\xA6\xB5\x25"
				"\xB1\x6A\xED\xF5\xAA\x0D\xE6\x57\xBA\x63\x7B\x39",
				60,
				true
			),

			CharString_createNull(),

			CharString_createRefSizedConst(
				"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
				"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
				"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x00\x02",
				48,
				true
			),

			CharString_createNull(),

			CharString_createRefSizedConst(
				"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
				"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
				"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x00\x04",
				42,
				true
			),

			CharString_createNull(),

			CharString_createRefSizedConst(
				"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
				"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
				"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x00"
				"\x06",
				49,
				true
			),

			CharString_createNull(),

			CharString_createRefSizedConst(
				"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
				"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
				"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C"
				"\x3D\x3E\x3F\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x00\x08",
				63,
				true
			)
		};

		CharString additionalData[] = {

			CharString_createNull(),
			CharString_createNull(),
			CharString_createNull(),

			CharString_createRefSizedConst(
				"\xFE\xED\xFA\xCE\xDE\xAD\xBE\xEF\xFE\xED\xFA\xCE\xDE\xAD\xBE\xEF"
				"\xAB\xAD\xDA\xD2",
				20,
				true
			),

			CharString_createRefSizedConst(
				"\xD6\x09\xB1\xF0\x56\x63\x7A\x0D\x46\xDF\x99\x8D\x88\xE5\x22\x2A"
				"\xB2\xC2\x84\x65\x12\x15\x35\x24\xC0\x89\x5E\x81\x08\x00\x0F\x10"
				"\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20"
				"\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30"
				"\x31\x32\x33\x34\x00\x01",
				70,
				true
			),

			CharString_createRefSizedConst(
				"\xD6\x09\xB1\xF0\x56\x63\x7A\x0D\x46\xDF\x99\x8D\x88\xE5\x2E\x00"
				"\xB2\xC2\x84\x65\x12\x15\x35\x24\xC0\x89\x5E\x81",
				28,
				true
			),

			CharString_createRefSizedConst(
				"\xE2\x01\x06\xD7\xCD\x0D\xF0\x76\x1E\x8D\xCD\x3D\x88\xE5\x40\x00"
				"\x76\xD4\x57\xED\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18"
				"\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28"
				"\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38"
				"\x39\x3A\x00\x03",
				68,
				true
			),

			CharString_createRefSizedConst(
				"\xE2\x01\x06\xD7\xCD\x0D\xF0\x76\x1E\x8D\xCD\x3D\x88\xE5\x4C\x2A"
				"\x76\xD4\x57\xED",
				20,
				true
			),

			CharString_createRefSizedConst(
				"\x84\xC5\xD5\x13\xD2\xAA\xF6\xE5\xBB\xD2\x72\x77\x88\xE5\x23\x00"
				"\x89\x32\xD6\x12\x7C\xFD\xE9\xF9\xE3\x37\x24\xC6\x08\x00\x0F\x10"
				"\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20"
				"\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30"
				"\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F\x00"
				"\x05",
				81,
				true
			),

			CharString_createRefSizedConst(
				"\x84\xC5\xD5\x13\xD2\xAA\xF6\xE5\xBB\xD2\x72\x77\x88\xE5\x2F\x00"
				"\x89\x32\xD6\x12\x7C\xFD\xE9\xF9\xE3\x37\x24\xC6",
				28,
				true
			),

			CharString_createRefSizedConst(
				"\x68\xF2\xE7\x76\x96\xCE\x7A\xE8\xE2\xCA\x4E\xC5\x88\xE5\x41\x00"
				"\x2E\x58\x49\x5C\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18"
				"\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28"
				"\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38"
				"\x39\x3A\x3B\x3C\x3D\x3E\x3F\x40\x41\x42\x43\x44\x45\x46\x47\x48"
				"\x49\x4A\x4B\x4C\x4D\x00\x07",
				87,
				true
			),

			CharString_createRefSizedConst(
				"\x68\xF2\xE7\x76\x96\xCE\x7A\xE8\xE2\xCA\x4E\xC5\x88\xE5\x4D\x00"
				"\x2E\x58\x49\x5C",
				20,
				true
			)
		};

		const C8 ivs[][13] = {
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
			"\xCA\xFE\xBA\xBE\xFA\xCE\xDB\xAD\xDE\xCA\xF8\x88",
			"\xCA\xFE\xBA\xBE\xFA\xCE\xDB\xAD\xDE\xCA\xF8\x88",
			"\x12\x15\x35\x24\xC0\x89\x5E\x81\xB2\xC2\x84\x65",
			"\x12\x15\x35\x24\xC0\x89\x5E\x81\xB2\xC2\x84\x65",
			"\xF0\x76\x1E\x8D\xCD\x3D\x00\x01\x76\xD4\x57\xED",
			"\xF0\x76\x1E\x8D\xCD\x3D\x00\x01\x76\xD4\x57\xED",
			"\x7C\xFD\xE9\xF9\xE3\x37\x24\xC6\x89\x32\xD6\x12",
			"\x7C\xFD\xE9\xF9\xE3\x37\x24\xC6\x89\x32\xD6\x12",
			"\x7A\xE8\xE2\xCA\x4E\xC5\x00\x01\x2E\x58\x49\x5C",
			"\x7A\xE8\xE2\xCA\x4E\xC5\x00\x01\x2E\x58\x49\x5C"
		};

		CharString results[] = {

			CharString_createRefSizedConst(
				"\x53\x0F\x8A\xFB\xC7\x45\x36\xB9\xA9\x63\xB4\xF1\xC4\xCB\x73\x8B",		//Tag (iv is prepended automatically)
				16,
				true
			),

			CharString_createRefSizedConst(
				"\xCE\xA7\x40\x3D\x4D\x60\x6B\x6E\x07\x4E\xC5\xD3\xBA\xF3\x9D\x18"		//Block 0
				"\xD0\xD1\xC8\xA7\x99\x99\x6B\xF0\x26\x5B\x98\xB5\xD4\x8A\xB9\x19",		//Tag
				32,
				true
			),

			CharString_createRefSizedConst(
				"\x52\x2D\xC1\xF0\x99\x56\x7D\x07\xF4\x7F\x37\xA3\x2A\x84\x42\x7D"		//Cipher
				"\x64\x3A\x8C\xDC\xBF\xE5\xC0\xC9\x75\x98\xA2\xBD\x25\x55\xD1\xAA"
				"\x8C\xB0\x8E\x48\x59\x0D\xBB\x3D\xA7\xB0\x8B\x10\x56\x82\x88\x38"
				"\xC5\xF6\x1E\x63\x93\xBA\x7A\x0A\xBC\xC9\xF6\x62\x89\x80\x15\xAD"
				"\xB0\x94\xDA\xC5\xD9\x34\x71\xBD\xEC\x1A\x50\x22\x70\xE3\xCC\x6C",		//Tag
				16 + 64,
				true
			),

			CharString_createRefSizedConst(
				"\x52\x2D\xC1\xF0\x99\x56\x7D\x07\xF4\x7F\x37\xA3\x2A\x84\x42\x7D"		//Cipher
				"\x64\x3A\x8C\xDC\xBF\xE5\xC0\xC9\x75\x98\xA2\xBD\x25\x55\xD1\xAA"
				"\x8C\xB0\x8E\x48\x59\x0D\xBB\x3D\xA7\xB0\x8B\x10\x56\x82\x88\x38"
				"\xC5\xF6\x1E\x63\x93\xBA\x7A\x0A\xBC\xC9\xF6\x62"
				"\x76\xFC\x6E\xCE\x0F\x4E\x17\x68\xCD\xDF\x88\x53\xBB\x2D\x55\x1B",		//Tag
				16 + 60,
				true
			),

			CharString_createRefSizedConst(
				"\x2F\x0B\xC5\xAF\x40\x9E\x06\xD6\x09\xEA\x8B\x7D\x0F\xA5\xEA\x50",		//Tag (iv is prepended automatically)
				16,
				true
			),

			CharString_createRefSizedConst(
				"\xE2\x00\x6E\xB4\x2F\x52\x77\x02\x2D\x9B\x19\x92\x5B\xC4\x19\xD7"		//Cipher
				"\xA5\x92\x66\x6C\x92\x5F\xE2\xEF\x71\x8E\xB4\xE3\x08\xEF\xEA\xA7"
				"\xC5\x27\x3B\x39\x41\x18\x86\x0A\x5B\xE2\xA9\x7F\x56\xAB\x78\x36"
				"\x5C\xA5\x97\xCD\xBB\x3E\xDB\x8D\x1A\x11\x51\xEA\x0A\xF7\xB4\x36",		//Tag
				16 + 48,
				true
			),

			CharString_createRefSizedConst(
				"\x35\x21\x7C\x77\x4B\xBC\x31\xB6\x31\x66\xBC\xF9\xD4\xAB\xED\x07",		//Tag (iv is prepended automatically)
				16,
				true
			),

			CharString_createRefSizedConst(
				"\xC1\x62\x3F\x55\x73\x0C\x93\x53\x30\x97\xAD\xDA\xD2\x56\x64\x96"		//Cipher
				"\x61\x25\x35\x2B\x43\xAD\xAC\xBD\x61\xC5\xEF\x3A\xC9\x0B\x5B\xEE"
				"\x92\x9C\xE4\x63\x0E\xA7\x9F\x6C\xE5\x19"
				"\x12\xAF\x39\xC2\xD1\xFD\xC2\x05\x1F\x8B\x7B\x3C\x9D\x39\x7E\xF2",		//Tag (iv is prepended automatically)
				16 + 42,
				true
			),

			CharString_createRefSizedConst(
				"\x6E\xE1\x60\xE8\xFA\xEC\xA4\xB3\x6C\x86\xB2\x34\x92\x0C\xA9\x75",		//Tag (iv is prepended automatically)
				16,
				true
			),

			CharString_createRefSizedConst(
				"\x11\x02\x22\xFF\x80\x50\xCB\xEC\xE6\x6A\x81\x3A\xD0\x9A\x73\xED"		//Cipher
				"\x7A\x9A\x08\x9C\x10\x6B\x95\x93\x89\x16\x8E\xD6\xE8\x69\x8E\xA9"
				"\x02\xEB\x12\x77\xDB\xEC\x2E\x68\xE4\x73\x15\x5A\x15\xA7\xDA\xEE"
				"\xD4"
				"\xA1\x0F\x4E\x05\x13\x9C\x23\xDF\x00\xB3\xAA\xDC\x71\xF0\x59\x6A",		//Tag (iv is prepended automatically)
				16 + 49,
				true
			),

			CharString_createRefSizedConst(
				"\x00\xBD\xA1\xB7\xE8\x76\x08\xBC\xBF\x47\x0F\x12\x15\x7F\x4C\x07",		//Tag (iv is prepended automatically)
				16,
				true
			),

			CharString_createRefSizedConst(
				"\xBA\x8A\xE3\x1B\xC5\x06\x48\x6D\x68\x73\xE4\xFC\xE4\x60\xE7\xDC"
				"\x57\x59\x1F\xF0\x06\x11\xF3\x1C\x38\x34\xFE\x1C\x04\xAD\x80\xB6"
				"\x68\x03\xAF\xCF\x5B\x27\xE6\x33\x3F\xA6\x7C\x99\xDA\x47\xC2\xF0"
				"\xCE\xD6\x8D\x53\x1B\xD7\x41\xA9\x43\xCF\xF7\xA6\x71\x3B\xD0"
				"\x26\x11\xCD\x7D\xAA\x01\xD6\x1C\x5C\x88\x6D\xC1\xA8\x17\x01\x07",		//Tag (iv is prepended automatically)
				16 + 63,
				true
			)
		};

		for (U64 i = 0; i < sizeof(ivs) / sizeof(ivs[0]); ++i) {

			//Fetch iv

			I32x4 iv = I32x4_zero();
			Buffer_copy(Buffer_createRef(&iv, 12), Buffer_createRefConst(ivs[i], 12));

			//Copy into tmp variable to be able to modify it instead of using const mem

			gotoIfError(clean, CharString_createCopy(testPlainText[i], alloc, &tmp))

			//Encrypt plain text

			I32x4 tag = I32x4_zero();

			gotoIfError(clean, Buffer_encrypt(
				CharString_buffer(tmp),
				CharString_bufferConst(additionalData[i]),
				EBufferEncryptionType_AES256GCM,
				EBufferEncryptionFlags_None,
				(U32*) testKeys[i].ptr,
				&iv,
				&tag
			))

			//Check size

			if(CharString_length(tmp) + 16 != CharString_length(results[i]))
				gotoIfError(clean, Error_invalidState(3, "Buffer_encrypt returned invalid size"))

			//Check tag (intermediate copy because otherwise Release will crash because of unaligned memory)

			I32x4 tmpTag = I32x4_zero();
			Buffer_copy(
				Buffer_createRef(&tmpTag, sizeof(tmpTag)),
				Buffer_createRefConst(results[i].ptr + CharString_length(tmp), sizeof(I32x4))
			);

			if(I32x4_any(I32x4_neq(tag, tmpTag)))
				gotoIfError(clean, Error_invalidState(1, "Buffer_encrypt GMAC/Tag was invalid"))

			//Check result

			Bool b = Buffer_eq(
				Buffer_createRefConst(results[i].ptr, CharString_length(testPlainText[i])),
				CharString_bufferConst(tmp)
			);

			if(!b)
				gotoIfError(clean, Error_invalidState(2, "Buffer_encrypt cyphertext was invalid"))

			//Decrypt the encrypted string and verify if it decrypts to the same thing

			gotoIfError(clean, Buffer_decrypt(
				CharString_buffer(tmp),
				CharString_bufferConst(additionalData[i]),
				EBufferEncryptionType_AES256GCM,
				(const U32*) testKeys[i].ptr,
				tag,
				iv
			))

			//Check result

			b = Buffer_eq(
				CharString_bufferConst(testPlainText[i]),
				CharString_bufferConst(tmp)
			);

			if(!b)
				gotoIfError(clean, Error_invalidState(4, "Buffer_decrypt failed"))

			CharString_free(&tmp, alloc);
		}
	}

	{
		Log_debugLn(alloc, "Testing Buffer encrypt/decrypt (AES128)");

		CharString testKeys[] = {
			CharString_createRefSizedConst("\xAD\x7A\x2B\xD0\x3E\xAC\x83\x5A\x6F\x62\x0F\xDC\xB5\x06\xB3\x45", 16, true),
			CharString_createRefSizedConst("\xAD\x7A\x2B\xD0\x3E\xAC\x83\x5A\x6F\x62\x0F\xDC\xB5\x06\xB3\x45", 16, true),
			CharString_createRefSizedConst("\x07\x1B\x11\x3B\x0C\xA7\x43\xFE\xCC\xCF\x3D\x05\x1F\x73\x73\x82", 16, true),
			CharString_createRefSizedConst("\x07\x1B\x11\x3B\x0C\xA7\x43\xFE\xCC\xCF\x3D\x05\x1F\x73\x73\x82", 16, true),
			CharString_createRefSizedConst("\x01\x3F\xE0\x0B\x5F\x11\xBE\x7F\x86\x6D\x0C\xBB\xC5\x5A\x7A\x90", 16, true),
			CharString_createRefSizedConst("\x01\x3F\xE0\x0B\x5F\x11\xBE\x7F\x86\x6D\x0C\xBB\xC5\x5A\x7A\x90", 16, true),
			CharString_createRefSizedConst("\x88\xEE\x08\x7F\xD9\x5D\xA9\xFB\xF6\x72\x5A\xA9\xD7\x57\xB0\xCD", 16, true),
			CharString_createRefSizedConst("\x88\xEE\x08\x7F\xD9\x5D\xA9\xFB\xF6\x72\x5A\xA9\xD7\x57\xB0\xCD", 16, true),
			CharString_createRefSizedConst("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16, true),
			CharString_createRefSizedConst("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16, true),
			CharString_createRefSizedConst("\xFE\xFF\xE9\x92\x86\x65\x73\x1C\x6D\x6A\x8F\x94\x67\x30\x83\x08", 16, true),
			CharString_createRefSizedConst("\xFE\xFF\xE9\x92\x86\x65\x73\x1C\x6D\x6A\x8F\x94\x67\x30\x83\x08", 16, true)
		};

		CharString testPlainText[] = {
			CharString_createNull(),
			CharString_createRefSizedConst(
				"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
				"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
				"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x00\x02",
				48, true
			),
			CharString_createNull(),
			CharString_createRefSizedConst(
				"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
				"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
				"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x00\x04",
				42, true
			),
			CharString_createNull(),
			CharString_createRefSizedConst(
				"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
				"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
				"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x00"
				"\x06",
				49, true
			),
			CharString_createNull(),
			CharString_createRefSizedConst(
				"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
				"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
				"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C"
				"\x3D\x3E\x3F\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x00\x08",
				63,
				true
			),
			CharString_createNull(),
			CharString_createRefSizedConst("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16, true),
			CharString_createRefSizedConst(
				"\xD9\x31\x32\x25\xF8\x84\x06\xE5\xA5\x59\x09\xC5\xAF\xF5\x26\x9A"
				"\x86\xA7\xA9\x53\x15\x34\xF7\xDA\x2E\x4C\x30\x3D\x8A\x31\x8A\x72"
				"\x1C\x3C\x0C\x95\x95\x68\x09\x53\x2F\xCF\x0E\x24\x49\xA6\xB5\x25"
				"\xB1\x6A\xED\xF5\xAA\x0D\xE6\x57\xBA\x63\x7B\x39\x1A\xAF\xD2\x55",
				64,
				true
			),
			CharString_createRefSizedConst(
				"\xD9\x31\x32\x25\xF8\x84\x06\xE5\xA5\x59\x09\xC5\xAF\xF5\x26\x9A"
				"\x86\xA7\xA9\x53\x15\x34\xF7\xDA\x2E\x4C\x30\x3D\x8A\x31\x8A\x72"
				"\x1C\x3C\x0C\x95\x95\x68\x09\x53\x2F\xCF\x0E\x24\x49\xA6\xB5\x25"
				"\xB1\x6A\xED\xF5\xAA\x0D\xE6\x57\xBA\x63\x7B\x39",
				60,
				true
			)
		};

		CharString additionalData[] = {

			CharString_createRefSizedConst(
				"\xD6\x09\xB1\xF0\x56\x63\x7A\x0D\x46\xDF\x99\x8D\x88\xE5\x22\x2A"
				"\xB2\xC2\x84\x65\x12\x15\x35\x24\xC0\x89\x5E\x81\x08\x00\x0F\x10"
				"\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20"
				"\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30"
				"\x31\x32\x33\x34\x00\x01",
				70,
				true
			),

			CharString_createRefSizedConst(
				"\xD6\x09\xB1\xF0\x56\x63\x7A\x0D\x46\xDF\x99\x8D\x88\xE5\x2E\x00"
				"\xB2\xC2\x84\x65\x12\x15\x35\x24\xC0\x89\x5E\x81",
				28,
				true
			),

			CharString_createRefSizedConst(
				"\xE2\x01\x06\xD7\xCD\x0D\xF0\x76\x1E\x8D\xCD\x3D\x88\xE5\x40\x00"
				"\x76\xD4\x57\xED\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18"
				"\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28"
				"\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38"
				"\x39\x3A\x00\x03",
				68,
				true
			),

			CharString_createRefSizedConst(
				"\xE2\x01\x06\xD7\xCD\x0D\xF0\x76\x1E\x8D\xCD\x3D\x88\xE5\x4C\x2A"
				"\x76\xD4\x57\xED",
				20,
				true
			),

			CharString_createRefSizedConst(
				"\x84\xC5\xD5\x13\xD2\xAA\xF6\xE5\xBB\xD2\x72\x77\x88\xE5\x23\x00"
				"\x89\x32\xD6\x12\x7C\xFD\xE9\xF9\xE3\x37\x24\xC6\x08\x00\x0F\x10"
				"\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20"
				"\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30"
				"\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F\x00"
				"\x05",
				81,
				true
			),

			CharString_createRefSizedConst(
				"\x84\xC5\xD5\x13\xD2\xAA\xF6\xE5\xBB\xD2\x72\x77\x88\xE5\x2F\x00"
				"\x89\x32\xD6\x12\x7C\xFD\xE9\xF9\xE3\x37\x24\xC6",
				28,
				true
			),

			CharString_createRefSizedConst(
				"\x68\xF2\xE7\x76\x96\xCE\x7A\xE8\xE2\xCA\x4E\xC5\x88\xE5\x41\x00"
				"\x2E\x58\x49\x5C\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18"
				"\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28"
				"\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38"
				"\x39\x3A\x3B\x3C\x3D\x3E\x3F\x40\x41\x42\x43\x44\x45\x46\x47\x48"
				"\x49\x4A\x4B\x4C\x4D\x00\x07",
				87,
				true
			),

			CharString_createRefSizedConst(
				"\x68\xF2\xE7\x76\x96\xCE\x7A\xE8\xE2\xCA\x4E\xC5\x88\xE5\x4D\x00"
				"\x2E\x58\x49\x5C",
				20,
				true
			),

			CharString_createNull(),
			CharString_createNull(),
			CharString_createNull(),
			CharString_createRefSizedConst(
				"\xFE\xED\xFA\xCE\xDE\xAD\xBE\xEF\xFE\xED\xFA\xCE\xDE\xAD\xBE\xEF"
				"\xAB\xAD\xDA\xD2",
				20,
				true
			)
		};

		const C8 ivs[][13] = {
			"\x12\x15\x35\x24\xC0\x89\x5E\x81\xB2\xC2\x84\x65",
			"\x12\x15\x35\x24\xC0\x89\x5E\x81\xB2\xC2\x84\x65",
			"\xF0\x76\x1E\x8D\xCD\x3D\x00\x01\x76\xD4\x57\xED",
			"\xF0\x76\x1E\x8D\xCD\x3D\x00\x01\x76\xD4\x57\xED",
			"\x7C\xFD\xE9\xF9\xE3\x37\x24\xC6\x89\x32\xD6\x12",
			"\x7C\xFD\xE9\xF9\xE3\x37\x24\xC6\x89\x32\xD6\x12",
			"\x7A\xE8\xE2\xCA\x4E\xC5\x00\x01\x2E\x58\x49\x5C",
			"\x7A\xE8\xE2\xCA\x4E\xC5\x00\x01\x2E\x58\x49\x5C",
			{ 0 },
			{ 0 },
			"\xCA\xFE\xBA\xBE\xFA\xCE\xDB\xAD\xDE\xCA\xF8\x88",
			"\xCA\xFE\xBA\xBE\xFA\xCE\xDB\xAD\xDE\xCA\xF8\x88"
		};

		CharString results[] = {

			CharString_createRefSizedConst("\xF0\x94\x78\xA9\xB0\x90\x07\xD0\x6F\x46\xE9\xB6\xA1\xDA\x25\xDD", 16, true),

			CharString_createRefSizedConst(
				"\x70\x1A\xFA\x1C\xC0\x39\xC0\xD7\x65\x12\x8A\x66\x5D\xAB\x69\x24"
				"\x38\x99\xBF\x73\x18\xCC\xDC\x81\xC9\x93\x1D\xA1\x7F\xBE\x8E\xDD"
				"\x7D\x17\xCB\x8B\x4C\x26\xFC\x81\xE3\x28\x4F\x2B\x7F\xBA\x71\x3D"
				"\x4F\x8D\x55\xE7\xD3\xF0\x6F\xD5\xA1\x3C\x0C\x29\xB9\xD5\xB8\x80",
				64, true
			),

			CharString_createRefSizedConst("\x0C\x01\x7B\xC7\x3B\x22\x7D\xFC\xC9\xBA\xFA\x1C\x41\xAC\xC3\x53", 16, true),

			CharString_createRefSizedConst(
				"\x13\xB4\xC7\x2B\x38\x9D\xC5\x01\x8E\x72\xA1\x71\xDD\x85\xA5\xD3"
				"\x75\x22\x74\xD3\xA0\x19\xFB\xCA\xED\x09\xA4\x25\xCD\x9B\x2E\x1C"
				"\x9B\x72\xEE\xE7\xC9\xDE\x7D\x52\xB3\xF3"
				"\xD6\xA5\x28\x4F\x4A\x6D\x3F\xE2\x2A\x5D\x6C\x2B\x96\x04\x94\xC3",
				58, true
			),

			CharString_createRefSizedConst("\x21\x78\x67\xE5\x0C\x2D\xAD\x74\xC2\x8C\x3B\x50\xAB\xDF\x69\x5A", 16, true),

			CharString_createRefSizedConst(
				"\x3A\x4D\xE6\xFA\x32\x19\x10\x14\xDB\xB3\x03\xD9\x2E\xE3\xA9\xE8"
				"\xA1\xB5\x99\xC1\x4D\x22\xFB\x08\x00\x96\xE1\x38\x11\x81\x6A\x3C"
				"\x9C\x9B\xCF\x7C\x1B\x9B\x96\xDA\x80\x92\x04\xE2\x9D\x0E\x2A\x76"
				"\x42"
				"\xBF\xD3\x10\xA4\x83\x7C\x81\x6C\xCF\xA5\xAC\x23\xAB\x00\x39\x88",
				65, true
			),

			CharString_createRefSizedConst("\x07\x92\x2B\x8E\xBC\xF1\x0B\xB2\x29\x75\x88\xCA\x4C\x61\x45\x23", 16, true),

			CharString_createRefSizedConst(
				"\xC3\x1F\x53\xD9\x9E\x56\x87\xF7\x36\x51\x19\xB8\x32\xD2\xAA\xE7"
				"\x07\x41\xD5\x93\xF1\xF9\xE2\xAB\x34\x55\x77\x9B\x07\x8E\xB8\xFE"
				"\xAC\xDF\xEC\x1F\x8E\x3E\x52\x77\xF8\x18\x0B\x43\x36\x1F\x65\x12"
				"\xAD\xB1\x6D\x2E\x38\x54\x8A\x2C\x71\x9D\xBA\x72\x28\xD8\x40"
				"\x88\xF8\x75\x7A\xDB\x8A\xA7\x88\xD8\xF6\x5A\xD6\x68\xBE\x70\xE7",
				79, true
			),

			CharString_createRefSizedConst("\x58\xE2\xFC\xCE\xFA\x7E\x30\x61\x36\x7F\x1D\x57\xA4\xE7\x45\x5A", 16, true),

			CharString_createRefSizedConst(
				"\x03\x88\xDA\xCE\x60\xB6\xA3\x92\xF3\x28\xC2\xB9\x71\xB2\xFE\x78"
				"\xAB\x6E\x47\xD4\x2C\xEC\x13\xBD\xF5\x3A\x67\xB2\x12\x57\xBD\xDF",
				32, true
			),

			CharString_createRefSizedConst(
				"\x42\x83\x1E\xC2\x21\x77\x74\x24\x4B\x72\x21\xB7\x84\xD0\xD4\x9C"
				"\xE3\xAA\x21\x2F\x2C\x02\xA4\xE0\x35\xC1\x7E\x23\x29\xAC\xA1\x2E"
				"\x21\xD5\x14\xB2\x54\x66\x93\x1C\x7D\x8F\x6A\x5A\xAC\x84\xAA\x05"
				"\x1B\xA3\x0B\x39\x6A\x0A\xAC\x97\x3D\x58\xE0\x91\x47\x3F\x59\x85"
				"\x4D\x5C\x2A\xF3\x27\xCD\x64\xA6\x2C\xF3\x5A\xBD\x2B\xA6\xFA\xB4",
				80, true
			),

			CharString_createRefSizedConst(
				"\x42\x83\x1E\xC2\x21\x77\x74\x24\x4B\x72\x21\xB7\x84\xD0\xD4\x9C"
				"\xE3\xAA\x21\x2F\x2C\x02\xA4\xE0\x35\xC1\x7E\x23\x29\xAC\xA1\x2E"
				"\x21\xD5\x14\xB2\x54\x66\x93\x1C\x7D\x8F\x6A\x5A\xAC\x84\xAA\x05"
				"\x1B\xA3\x0B\x39\x6A\x0A\xAC\x97\x3D\x58\xE0\x91"
				"\x5B\xC9\x4F\xBC\x32\x21\xA5\xDB\x94\xFA\xE9\x5A\xE7\x12\x1A\x47",
				76, true
			)
		};

		for (U64 i = 0; i < sizeof(ivs) / sizeof(ivs[0]); ++i) {

			//Fetch iv

			I32x4 iv = I32x4_zero();
			Buffer_copy(Buffer_createRef(&iv, 12), Buffer_createRefConst(ivs[i], 12));

			//Copy into tmp variable to be able to modify it instead of using const mem

			gotoIfError(clean, CharString_createCopy(testPlainText[i], alloc, &tmp))

			//Encrypt plain text

			I32x4 tag = I32x4_zero();

			gotoIfError(clean, Buffer_encrypt(
				CharString_buffer(tmp),
				CharString_bufferConst(additionalData[i]),
				EBufferEncryptionType_AES128GCM,
				EBufferEncryptionFlags_None,
				(U32*) testKeys[i].ptr,
				&iv,
				&tag
			))

			//Check size

			if(CharString_length(tmp) + 16 != CharString_length(results[i]))
				gotoIfError(clean, Error_invalidState(3, "Buffer_encrypt returned invalid size"))

			//Check tag (intermediate copy because otherwise Release will crash because of unaligned memory)

			I32x4 tmpTag = I32x4_zero();
			Buffer_copy(
				Buffer_createRef(&tmpTag, sizeof(tmpTag)),
				Buffer_createRefConst(results[i].ptr + CharString_length(tmp), sizeof(I32x4))
			);

			if(I32x4_any(I32x4_neq(tag, tmpTag)))
				gotoIfError(clean, Error_invalidState(1, "Buffer_encrypt GMAC/Tag was invalid"))

			//Check result

			Bool b = Buffer_eq(
				Buffer_createRefConst(results[i].ptr, CharString_length(testPlainText[i])),
				CharString_bufferConst(tmp)
			);

			if(!b)
				gotoIfError(clean, Error_invalidState(2, "Buffer_encrypt cyphertext was invalid"))

			//Decrypt the encrypted string and verify if it decrypts to the same thing

			gotoIfError(clean, Buffer_decrypt(
				CharString_buffer(tmp),
				CharString_bufferConst(additionalData[i]),
				EBufferEncryptionType_AES128GCM,
				(const U32*) testKeys[i].ptr,
				tag,
				iv
			))

			//Check result

			b = Buffer_eq(
				CharString_bufferConst(testPlainText[i]),
				CharString_bufferConst(tmp)
			);

			if(!b)
				gotoIfError(clean, Error_invalidState(4, "Buffer_decrypt failed"))

			CharString_free(&tmp, alloc);
		}
	}

	//Check for floating point conversions

	Log_debugLn(alloc, "Testing software floating point expansion casts...");

	{
		static const U32 expansionTests[] = {
			0x7FFFFFFF,		//NaN
			0x00000000,		//0
			0x7F800000,		//Inf
			0x7F800001,		//Another NaN
			0x00000003,		//DeN
			0x00020000,		//DeN that tests bit comparison in expansion function
			0x00000001,		//Smallest DeN
			0x007FFFFF,		//Biggest DeN
			0x00800000,		//Smallest non DeN
			0x7F7FFFFF,		//Float max
			0x3F800000,		//1
			0x3F000000,		//0.5
			0x40000000,		//2
			0x42F60000,		//123
			0x3F9D70A4,		//1.23
			0x3F7FFFFF,		//Almost 1
			0x00000015		//DeN that was failing in the tests
		};

		for (U64 i = 0; i < sizeof(expansionTests) / sizeof(expansionTests[0]); ++i) {

			for(U64 j = 0; j < 2; ++j) {

				F32 fi = ((const F32*)expansionTests)[i];
				void *fiv = &fi;

				if(j)
					*(U32*)fiv |= (U32)1 << 31;

				const F64 doubTarg = fi;
				const void *doubTargv = &doubTarg;
				const U64 doubTarg64 = *(const U64*) doubTargv;

				const F64 doubEmu = F32_castF64(fi);
				const void *doubEmuv = &doubEmu;
				const U64 doubEmu64  = *(const U64*) doubEmuv;

				if (doubEmu64 != doubTarg64)
					gotoIfError(clean, Error_invalidState((U32)((i << 1) | j), "F32_castF64 returned invalid data"))
			}
		}

		//Generate random numbers

		U64 N = 1024;

		Buffer_free(&emp, alloc);
		gotoIfError(clean, Buffer_createEmptyBytes(N * sizeof(U32), alloc, &emp))

		if (!Buffer_csprng(emp))
			gotoIfError(clean, Error_invalidState(0, "Buffer_csprng failed"))

		const U32 *rptr = (const U32*) emp.ptr;

		for (U64 i = 0; i < N; ++i) {

			//Loop normal, NaN, Inf/DeN

			for (U8 j = 0; j < 6; ++j) {

				U32 rv = rptr[i];

				if (j % 3) {		//NaN, Inf and DeN

					rv &= 0x007FFFFF;

					if ((j % 3) > 1)				//NaN or Inf
						rv |= 0x7F800000;
				}

				const void *rvv = &rv;
				F32 fi = *(const F32*)rvv;
				void *fiv = &fi;

				if(j / 3)
					*(U32*)fiv |= (U32)1 << 31;

				F64 doubTarg = fi;
				const void *doubTargv = &doubTarg;
				U64 doubTarg64 = *(const U64*)doubTargv;

				F64 doubEmu = F32_castF64(fi);
				const void *doubEmuv = &doubEmu;
				U64 doubEmu64 = *(const U64*)doubEmuv;

				if (doubEmu64 != doubTarg64)
					gotoIfError(clean, Error_invalidState((U32)i, "F32_castF64 returned invalid data for random data"))
			}
		}

		Buffer_free(&emp, alloc);
	}

	//Halfs

	{
		F16 halfs[] = {
			0x7C00,					//Inf
			0x0000,					//0
			0x7C01,					//NaN #1
			0x7E00,					//NaN (about half)
			0x7E01,					//NaN (about half + 1)
			0x7FFF,					//NaN full
			0x0003,					//DeN
			0x0040,					//DeN that tests bit comparison in expansion function
			0x0001,					//Smallest DeN
			0x03FF,					//Biggest DeN
			0x0400,					//Smallest non DeN
			0x7BFF,					//Float max
			0x3C00,					//1
			0x3800,					//0.5
			0x4000,					//2
			0x57B0,					//123
			0x3CEB,					//1.23
			0x3BFF					//Almost 1
		};

		U32 expectedResultsF32[] = {
			0x7F800000,				//Inf
			0x00000000,				//0
			0x7FC02000,				//NaN #1
			0x7FC00000,				//NaN (about half)
			0x7FC02000,				//NaN (about half + 1)
			0x7FFFE000,				//NaN full
			0x34400000,				//1.7881393e-7
			0x36800000,				//0.0000038146972
			0x33800000,				//5.9604644e-8
			0x387FC000,				//0.00006097555
			0x38800000,				//0.000061035156
			0x477FE000,				//65504
			0x3F800000,				//1
			0x3F000000,				//0.5
			0x40000000,				//2
			0x42F60000,				//123
			0x3F9D6000,				//1.2294922
			0x3F7FE000				//0.9995117
		};

		U64 expectedResultsF64[] = {
			0x7FF0000000000000,		//Inf
			0x0000000000000000,		//0
			0x7FF8040000000000,		//NaN #1
			0x7FF8000000000000,		//NaN (about half)
			0x7FF8040000000000,		//NaN (about half + 1)
			0x7FFFFC0000000000,		//NaN full
			0x3E88000000000000,		//1.7881393432617188e-7
			0x3ED0000000000000,		//0.000003814697265625
			0x3E70000000000000,		//5.960464477539063e-8
			0x3f0ff80000000000,		//0.00006097555160522461
			0x3F10000000000000,		//0.00006103515625
			0x40EFFC0000000000,		//65504
			0x3FF0000000000000,		//1
			0x3FE0000000000000,		//0.5
			0x4000000000000000,		//2
			0x405EC00000000000,		//123
			0x3FF3AC0000000000,		//1.2294921875
			0x3FEFFC0000000000		//0.99951171875
		};

		for (U64 i = 0; i < sizeof(halfs) / sizeof(halfs[0]); ++i) {

			for(U64 j = 0; j < 2; ++j) {

				F16 fh = halfs[i];
				void *fhv = &fh;

				if(j)
					*(U16*)fhv |= 1 << 15;

				const F32 floatEmu = F16_castF32(fh);
				const void *floatEmuv = &floatEmu;
				const U32 floatEmu32  = *(const U32*)floatEmuv;
				U32 floatTarg32 = expectedResultsF32[i];

				if(j)
					floatTarg32 |= 1 << 31;

				if (floatEmu32 != floatTarg32)
					gotoIfError(clean, Error_invalidState((U32)(((i << 1) | j) << 1), "F16_castF32 was invalid"))

				const F64 doubEmu = F16_castF64(fh);
				const void *doubEmuv = &doubEmu;
				const U64 doubEmu64  = *(const U64*)doubEmuv;

				U64 doubTarg64 = expectedResultsF64[i];

				if(j)
					doubTarg64 |= (U64)1 << 63;

				if (doubEmu64 != doubTarg64)
					gotoIfError(clean, Error_invalidState((U32)(((i << 1) | j) << 1) | 1, "F16_castF64 was invalid"))
			}
		}
	}

	Log_debugLn(alloc, "Testing software floating point truncation casts...");

	{
		static const U64 truncTests[] = {

			0x7FFFFFFFFFFFFFFF,		//NaN
			0x0000000000000000,		//0
			0x7FF0000000000000,		//Inf

			0x7FF0000000000001,		//NaN that mostly collapses to 0 mantissa (but top bit 1)
			0x7FF8000000000000,		//NaN with top bit set
			0x7FF1000000000001,		//NaN other
			0x7FF8000000000001,		//NaN with top and bottom bit
			0x7FFC000000000003,		//NaN with two top and bottom bits

			//Normal numbers

			0x3FF0000000000000,		//1
			0x3FE0000000000000,		//0.5
			0x4000000000000000,		//2
			0x405EC00000000000,		//123
			0x3FF3AE147AE147AE,		//1.23
			0x3FEFFFFFFFFFFFFF,		//Almost 1 (rounded to 1)

			0x7FEFFFFFFFFFFFFF,		//Double max

			//Float test numbers

			0x36E4FFFFFFFFEF66,		//float 0x00000015

			0x47EFFFFFE00030B7,		//Float min

			0x36B7FFFFFFFFE40F,		//float 0x00000003#DeN
			0x37B00000000001AF,		//float 0x00020000#DeN that tests bit comparison in expansion function
			0x369FFFFFFFFF870D,		//float 0x00000001#DeN Smallest
			0x368FFFFFFFFF870D,		//float 0x00000000.5#DeN Smallest (Collapsed to 0)
			0x3690000000000000,		//float 0x00000000.5#DeN Nearest (Collapsed to 0)
			0x3690000000000001,		//float 0x00000000.5#DeN Nearest+ (Rounds up)
			0x380FFFFFBFFFB6EF,		//float 0x007FFFFF#DeN Biggest
			0x380FFFFFFFFFBB88,		//float 0x00800000 Smallest non DeN

			//DeNs

			0x0000000000000003,		//DeN
			0x0000000400000003,		//DeN that tests bit comparison in expansion function
			0x0000000000000001,		//Smallest DeN
			0x000FFFFFFFFFFFFF,		//Biggest DeN
			0x0010000000000000,		//Smallest non DeN
			0x37FFFFFFFFFFF765,		//Resulting in a DeN (float)

			//Testing rounding with DeN

			0x0000000020000000,
			0x0000000010000000,
			0x0000000008000000,

			//Collapse to 1 tests (checking correct rounding)

			0x3FF0000020000000,		//1.00000011920928955078125
			0x3FF0000020000001,		//1.00000011920928977282585492503
			0x3FF000003FFFFFFF,		//1.00000023841857887951789507497
			0x3FF0000010000000,		//1.000000059604644775390625,
			0x3FF0000010000001,		//1.00000005960464499743522992503
			0x3FF000001FFFFFFF,		//1.00000011920928932873664507497
			0x3FF0000008000000,		//1.0000000298023223876953125
			0x3FF000000FFFFFFF,		//1.00000005960464455334602007497

			//High and low exponents

			0x47D2CED32A16A1B1,		//1e38
			0x48078287F49C4A1D,		//1e39
			0x483D6329F1C35CA5,		//1e40

			0x366244CE242C5561,		//1e-46
			0x3696D601AD376AB9,		//1e-45
			0x37A16C262777579C,		//1e-40
			0x37D5C72FB1552D83,		//1e-39
			0x380B38FB9DAA78E4,		//1e-38
			0x3841039D428A8B8F,		//1e-37
			0x3E112E0BE826D695,		//1e-9
			0x3BC79CA10C924223,		//1e-20

			//Failing test

			0x369202F02C8DEC27
		};

		for (U64 i = 0; i < sizeof(truncTests) / sizeof(truncTests[0]); ++i) {

			for(U64 j = 0; j < 2; ++j) {

				F64 fd = ((const F64*)truncTests)[i];
				void *fdv = &fd;

				if(j)
					*(U64*)fdv |= (U64)1 << 63;

				const F32 floatTarg = (F32) fd;
				const void *floatTargv = &floatTarg;
				const U32 floatTarg32 = *(const U32*)floatTargv;

				const F32 floatEmu = F64_castF32(fd);
				const void *floatEmuv = &floatEmu;
				const U32 floatEmu32 = *(const U32*)floatEmuv;

				if (floatEmu32 != floatTarg32)
					gotoIfError(clean, Error_invalidState((U32)((i << 1) | j), "F64_castF32 was invalid"))
			}
		}

		//Generate random numbers

		U64 N = 1024;

		Buffer_free(&emp, alloc);
		gotoIfError(clean, Buffer_createEmptyBytes(N * sizeof(U64), alloc, &emp))

		if (!Buffer_csprng(emp))
			gotoIfError(clean, Error_invalidState(0, "Buffer_csprng failed (2)"))

		const U64 *rptr = (const U64*) emp.ptr;

		for (U64 i = 0; i < N; ++i) {

			//Loop normal, NaN, Inf/DeN, Then loop again but with sign set

			for (U8 j = 0; j < 6; ++j) {

				U64 rv = rptr[i] << 1 >> 1;

				if (j % 3) {		//NaN, Inf and DeN

					rv &= 0x000FFFFFFFFFFFFF;

					if (j % 3 > 1)	//NaN or Inf
						rv |= 0x7FF0000000000000;
				}

				if(j / 3)
					rv |= (U64)1 << 63;		//Sign bit

				const void *rvv = &rv;
				F64 fd = *(const F64*)rvv;

				const F32 floatTarg = (F32) fd;
				const void *floatTargv = &floatTarg;
				const U32 floatTarg32 = *(const U32*)floatTargv;

				const F32 floatEmu = F64_castF32(fd);
				const void *floatEmuv = &floatEmu;
				const U32 floatEmu32 = *(const U32*)floatEmuv;

				if (floatEmu32 != floatTarg32)
					gotoIfError(clean, Error_invalidState((U32)i, "F64_castF32 failed with random data"))
			}
		}

		Buffer_free(&emp, alloc);
	}

	//Halfs

	{
		U32 inputFloats[] = {
			0x7F800000,				//Inf
			0x4781E480,				//66505 (collapses to inf)
			0x7E967699,				//1e38 (near float max, collapses to inf)
			0x00000000,				//0
			0x34400000,				//1.7881393e-7
			0x36800000,				//0.0000038146972
			0x33800000,				//5.9604644e-8
			0x387FC000,				//0.00006097555
			0x38800000,				//0.000061035156
			0x477FE000,				//65504
			0x3F800000,				//1
			0x3F000000,				//0.5
			0x40000000,				//2
			0x42F60000,				//123
			0x3F9D6000,				//1.2294922
			0x3F7FE000,				//0.9995117
			0x7FC02000,				//NaN #1
			0x7FC00000,				//NaN (about half)
			0x7FC02000,				//NaN (about half + 1)
			0x7FFFE000,				//NaN full
			0x3F801000,				//Collapse to 1
			0x3F802000,				//Almost collapse to 1
			0x006CE3EE,				//1e-38 collapse to 0
			0x33802000,				//Doesn't collapse to 0
			0x33801FFF,				//Doesn't collapse to 0
			0x33800001,				//Doesn't collapse to 0
			0x33000000				//Collapses to 0
		};

		U64 inputDoubles[] = {
			0x7FF0000000000000,		//Inf
			0x40F03C9000000000,		//65505 (collapses to inf)
			0x7FE1CCF385EBC8A0,		//1e308 (near double max, collapses to inf)
			0x0000000000000000,		//0
			0x3E88000000000000,		//1.7881393432617188e-7
			0x3ED0000000000000,		//0.000003814697265625
			0x3E70000000000000,		//5.960464477539063e-8
			0x3f0ff80000000000,		//0.00006097555160522461
			0x3F10000000000000,		//0.00006103515625
			0x40EFFC0000000000,		//65504
			0x3FF0000000000000,		//1
			0x3FE0000000000000,		//0.5
			0x4000000000000000,		//2
			0x405EC00000000000,		//123
			0x3FF3AC0000000000,		//1.2294921875
			0x3FEFFC0000000000,		//0.99951171875
			0x7FF8040000000000,		//NaN #1
			0x7FF8000000000000,		//NaN (about half)
			0x7FF8040000000000,		//NaN (about half + 1)
			0x7FFFFC0000000000,		//NaN full
			0x3FF0020000000000,		//Collapse to 1
			0x3FF0040000000000,		//Almost collapse to 1
			0x000730D67819E8D2,		//Collapse to 0 (1e-308)
			0x3E70040000000000,		//Doesn't collapse to 0
			0x3E7003FFFFFFFFFF,		//Doesn't collapse to 0
			0x3E70000000000001,		//Doesn't collapse to 0
			0x3E60000000000000		//Collapses to 0
		};

		F16 halfResults[] = {
			0x7C00,					//Inf
			0x7C00,					//Inf
			0x7C00,					//Inf
			0x0000,					//0
			0x0003,					//DeN
			0x0040,					//DeN that tests bit comparison in expansion function
			0x0001,					//Smallest DeN
			0x03FF,					//Biggest DeN
			0x0400,					//Smallest non DeN
			0x7BFF,					//Float max
			0x3C00,					//1
			0x3800,					//0.5
			0x4000,					//2
			0x57B0,					//123
			0x3CEB,					//1.23
			0x3BFF,					//Almost 1
			0x7E01,					//NaN #1
			0x7E00,					//NaN (about half)
			0x7E01,					//NaN (about half + 1)
			0x7FFF,					//NaN full
			0x3C00,					//Collapse to 1
			0x3C01,					//Almost collapse to 1
			0x0000,					//Collapse to 0
			0x0001,					//Doesn't collapse to 0
			0x0001,					//Doesn't collapse to 0
			0x0001,					//Doesn't collapse to 0
			0x0000					//Collapses to 0
		};

		for (U64 i = 0; i < sizeof(halfResults) / sizeof(halfResults[0]); ++i) {

			for(U64 j = 0; j < 2; ++j) {

				F16 fh = halfResults[i];

				if(j)
					fh |= 1 << 15;

				const void *inputFloatsv = inputFloats;
				F32 floatTarg = ((const F32*)inputFloatsv)[i];
				void *floatTargv = &floatTarg;

				if(j)
					*(U32*)floatTargv |= 1 << 31;

				F16 halfEmu = F32_castF16(floatTarg);

				if (halfEmu != fh)
					gotoIfError(clean, Error_invalidState((U32)(((i << 1) | j) << 1), "F32_castF16 failed"))

				const void *inputDoublesv = inputDoubles;
				F64 doubTarg = ((const F64*)inputDoublesv)[i];
				void *doubTargv = &doubTarg;

				if(j)
					*(U64*)doubTargv |= (U64)1 << 63;

				halfEmu = F64_castF16(doubTarg);

				if (halfEmu != fh)
					gotoIfError(clean, Error_invalidState((U32)(((i << 1) | j) << 1) | 1, "F64_castF16 failed"))
			}
		}
	}

	//U128 compare

	{
		Log_debugLn(alloc, "Comparing U128 to U128 as BigInt and U128");

		U128 compares[] = {
			U128_createU64x2(0x0000000000000000, 0x0000000000000000),
			U128_createU64x2(0x0000000000000001, 0x0000000000000000),
			U128_createU64x2(0x000000007FFFFFFF, 0x0000000000000000),
			U128_createU64x2(0x0000000080000000, 0x0000000000000000),
			U128_createU64x2(0x0000000080000001, 0x0000000000000000),
			U128_createU64x2(0x00000000FFFFFFFF, 0x0000000000000000),
			U128_createU64x2(0x00000001FFFFFFFF, 0x0000000000000000),
			U128_createU64x2(0x00000001FFFFFFFF, 0x0000000000000001),
			U128_createU64x2(0x00000001FFFFFFFF, 0x00000000FFFFFFFF),
			U128_createU64x2(0x00000001FFFFFFFF, 0x00000001FFFFFFFF),
			U128_createU64x2(0x00000001FFFFFFFF, 0xFFFFFFFFFFFFFFFF)
		};

		for(U64 i = 1; i < sizeof(compares) / sizeof(compares[0]); ++i) {

			if(
				U128_cmp(compares[i - 1], compares[i]) != ECompareResult_Lt ||
				U128_cmp(compares[i], compares[i - 1]) != ECompareResult_Gt
			)
				gotoIfError(clean, Error_invalidState((U32)i, "U128_cmp failed"))

			BigInt aBig = (BigInt) { 0 }, bBig = (BigInt) { 0 };
			gotoIfError(clean, BigInt_createRefConst((const U64*)&compares[i - 1], 2, &aBig))
			gotoIfError(clean, BigInt_createRefConst((const U64*)&compares[i], 2, &bBig))

			if(
				BigInt_cmp(aBig, bBig) != ECompareResult_Lt ||
				BigInt_cmp(bBig, aBig) != ECompareResult_Gt
			)
				gotoIfError(clean, Error_invalidState((U32)i, "BigInt_cmp failed"))
		}
	}

	//BigInt unit test

	const U64 mulParams[][2] = {
		{ 0x0123456789ABCDEF, 0xFEDCBA9876543210 },
		{ U64_MAX, U64_MAX },
		{ 0x2ABA5DA1FDF6DB0F, 0xE1147CF49D662B78 },
		{ 0x04D7A36BB020207F, 0xAC03EB4D2257AED9 },
		{ 0xC9DB73597154808E, 0xCE841590CDB048A4 }
	};

	const U64 mulResult[][2] = {
		{ 0x2236D88FE5618CF0, 0x0121FA00AD77D742 },
		{ 0x0000000000000001, 0xFFFFFFFFFFFFFFFE },
		{ 0xE9F1BC96FD7C3408, 0x259137B5CA1EDC29 },
		{ 0xAFD5326D0A7ADDA7, 0x340F4C6AD1EF492 },
		{ 0x3191981675EA4AF8, 0xA2D6BCFAA1676325 }
	};

	const U64 addResult[][2] = {
		{ 0x235A1DF76F0D5ADF, 0xFFFEB49923CC0952 },
		{ 0x0000000000000000, 0xFFFFFFFFFFFFFFFE },
		{ 0x14AC1A38FB730F17, 0x6A5B4AA678507A2 },
		{ 0xB4ACD5D8BA9AFE26, 0xAF44E013CF76A36B },
		{ 0xFB6D0B6FE73ECB86, 0x715AD28B6F17ABC9 }
	};

	const C8 *stringified[] = {
		"0xFEDCBA98765432100123456789ABCDEF",
		("0b1111111111111111111111111111111111111111111111111111111111111111"
		  "1111111111111111111111111111111111111111111111111111111111111111"),
		"0o3410507636447263053360252722732077575555417",
		"0n2i0_jD8bUksGJNeskm821$",
		"274506787720133886812119851071477940366"
	};

	BigInt aBig = (BigInt) { 0 }, bBig = (BigInt) { 0 }, cBig = (BigInt) { 0 };

	Log_debugLn(alloc, "Testing big int create from hex/bin/oct/dec");

	for(U64 i = 0; i < sizeof(stringified) / sizeof(stringified[0]); ++i) {

		gotoIfError(clean, BigInt_createFromString(CharString_createRefCStrConst(stringified[i]), 128, alloc, &aBig))
		gotoIfError(clean, BigInt_createRefConst(&mulParams[i][0], 2, &bBig))

		if(BigInt_neq(aBig, bBig))
			gotoIfError(clean, Error_invalidState((U32)i, "BigInt_createFromString failed"))

		BigInt_free(&aBig, alloc);
		bBig = (BigInt) { 0 };
	}

	Log_debugLn(alloc, "Testing big int to hex/bin/oct");

	for(U64 i = 0; i < sizeof(stringified) / sizeof(stringified[0]) && i < EIntegerEncoding_Count; ++i) {

		gotoIfError(clean, BigInt_createRefConst(&mulParams[i][0], 2, &bBig))
		gotoIfError(clean, BigInt_toString(bBig, alloc, &tmp, (EIntegerEncoding)i, false))

		CharString ref = CharString_createRefCStrConst(stringified[i]);

		if(!CharString_equalsStringSensitive(ref, tmp))
			gotoIfError(clean, Error_invalidState((U32)i, "BigInt_toString failed"))

		CharString_free(&tmp, alloc);
		bBig = (BigInt) { 0 };
	}

	Log_debugLn(alloc, "Testing big int mul");

	for(U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {

		aBig = bBig = cBig = (BigInt) { 0 };

		U64 temp[4] = { mulParams[i][0], 0, mulParams[i][1], 0 };
		gotoIfError(clean, BigInt_createRef(&temp[0], 2, &aBig))
		gotoIfError(clean, BigInt_createRefConst(&temp[2], 2, &bBig))
		gotoIfError(clean, BigInt_createRefConst(&mulResult[i][0], 2, &cBig))

		if(!BigInt_mul(&aBig, bBig, alloc) || BigInt_neq(aBig, cBig))
			gotoIfError(clean, Error_invalidOperation((U32)i, "BigInt_mul failed"))
	}

	Log_debugLn(alloc, "Testing big int add");

	for(U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {

		aBig = bBig = cBig = (BigInt) { 0 };

		U64 temp[2] = { mulParams[i][0], mulParams[i][1] };
		gotoIfError(clean, BigInt_createRef(&temp[0], 2, &aBig))
		gotoIfError(clean, BigInt_createRefConst(&mulResult[i][0], 2, &bBig))
		gotoIfError(clean, BigInt_createRefConst(&addResult[i][0], 2, &cBig))

		if(!BigInt_add(&aBig, bBig) || BigInt_neq(aBig, cBig))
			gotoIfError(clean, Error_invalidOperation((U32)i, "BigInt_add failed"))
	}

	Log_debugLn(alloc, "Testing big int sub");

	for(U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {

		aBig = bBig = cBig = (BigInt) { 0 };

		U64 temp[2] = { addResult[i][0], addResult[i][1] };
		gotoIfError(clean, BigInt_createRef(&temp[0], 2, &cBig))
		gotoIfError(clean, BigInt_createRefConst(&mulParams[i][0], 2, &aBig))
		gotoIfError(clean, BigInt_createRefConst(&mulResult[i][0], 2, &bBig))

		if(!BigInt_sub(&cBig, bBig) || BigInt_neq(aBig, cBig))
			gotoIfError(clean, Error_invalidOperation((U32)i, "BigInt_sub failed when solving c - b = a"))

		temp[0] = addResult[i][0];
		temp[1] = addResult[i][1];

		if(!BigInt_sub(&cBig, aBig) || BigInt_neq(bBig, cBig))
			gotoIfError(clean, Error_invalidOperation((U32)i, "BigInt_sub failed when solving c - a = b"))
	}

	const U64 lshResult[][2] = {
		{ 0xFFFFFFFFFFFFFFFE, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFC, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFF8, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFF0, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFE0, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFC0, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFF80, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFF00, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFE00, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFC00, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFF800, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFF000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFE000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFC000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFF8000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFF0000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFE0000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFC0000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFF80000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFF00000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFE00000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFC00000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFF800000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFF000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFE000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFC000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFF8000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFF0000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFE0000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFFC0000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFF80000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFF00000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFFE00000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFFC00000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFF800000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFF000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFFE000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFFC000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFF8000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFF0000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFFE0000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFFC0000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFF80000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFF00000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFFE00000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFFC00000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFF800000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFF000000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFFE000000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFFC000000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFF8000000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFF0000000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFFE0000000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFFC0000000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFF80000000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFF00000000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xFE00000000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xFC00000000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xF800000000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xF000000000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0xE000000000000000, 0xFFFFFFFFFFFFFFFF },	{ 0xC000000000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0x8000000000000000, 0xFFFFFFFFFFFFFFFF },	{ 0x0000000000000000, 0xFFFFFFFFFFFFFFFF },
		{ 0x0000000000000000, 0xFFFFFFFFFFFFFFFE },	{ 0x0000000000000000, 0xFFFFFFFFFFFFFFFC },
		{ 0x0000000000000000, 0xFFFFFFFFFFFFFFF8 },	{ 0x0000000000000000, 0xFFFFFFFFFFFFFFF0 },
		{ 0x0000000000000000, 0xFFFFFFFFFFFFFFE0 },	{ 0x0000000000000000, 0xFFFFFFFFFFFFFFC0 },
		{ 0x0000000000000000, 0xFFFFFFFFFFFFFF80 },	{ 0x0000000000000000, 0xFFFFFFFFFFFFFF00 },
		{ 0x0000000000000000, 0xFFFFFFFFFFFFFE00 },	{ 0x0000000000000000, 0xFFFFFFFFFFFFFC00 },
		{ 0x0000000000000000, 0xFFFFFFFFFFFFF800 },	{ 0x0000000000000000, 0xFFFFFFFFFFFFF000 },
		{ 0x0000000000000000, 0xFFFFFFFFFFFFE000 },	{ 0x0000000000000000, 0xFFFFFFFFFFFFC000 },
		{ 0x0000000000000000, 0xFFFFFFFFFFFF8000 },	{ 0x0000000000000000, 0xFFFFFFFFFFFF0000 },
		{ 0x0000000000000000, 0xFFFFFFFFFFFE0000 },	{ 0x0000000000000000, 0xFFFFFFFFFFFC0000 },
		{ 0x0000000000000000, 0xFFFFFFFFFFF80000 },	{ 0x0000000000000000, 0xFFFFFFFFFFF00000 },
		{ 0x0000000000000000, 0xFFFFFFFFFFE00000 },	{ 0x0000000000000000, 0xFFFFFFFFFFC00000 },
		{ 0x0000000000000000, 0xFFFFFFFFFF800000 },	{ 0x0000000000000000, 0xFFFFFFFFFF000000 },
		{ 0x0000000000000000, 0xFFFFFFFFFE000000 },	{ 0x0000000000000000, 0xFFFFFFFFFC000000 },
		{ 0x0000000000000000, 0xFFFFFFFFF8000000 },	{ 0x0000000000000000, 0xFFFFFFFFF0000000 },
		{ 0x0000000000000000, 0xFFFFFFFFE0000000 },	{ 0x0000000000000000, 0xFFFFFFFFC0000000 },
		{ 0x0000000000000000, 0xFFFFFFFF80000000 },	{ 0x0000000000000000, 0xFFFFFFFF00000000 },
		{ 0x0000000000000000, 0xFFFFFFFE00000000 },	{ 0x0000000000000000, 0xFFFFFFFC00000000 },
		{ 0x0000000000000000, 0xFFFFFFF800000000 },	{ 0x0000000000000000, 0xFFFFFFF000000000 },
		{ 0x0000000000000000, 0xFFFFFFE000000000 },	{ 0x0000000000000000, 0xFFFFFFC000000000 },
		{ 0x0000000000000000, 0xFFFFFF8000000000 },	{ 0x0000000000000000, 0xFFFFFF0000000000 },
		{ 0x0000000000000000, 0xFFFFFE0000000000 },	{ 0x0000000000000000, 0xFFFFFC0000000000 },
		{ 0x0000000000000000, 0xFFFFF80000000000 },	{ 0x0000000000000000, 0xFFFFF00000000000 },
		{ 0x0000000000000000, 0xFFFFE00000000000 },	{ 0x0000000000000000, 0xFFFFC00000000000 },
		{ 0x0000000000000000, 0xFFFF800000000000 },	{ 0x0000000000000000, 0xFFFF000000000000 },
		{ 0x0000000000000000, 0xFFFE000000000000 },	{ 0x0000000000000000, 0xFFFC000000000000 },
		{ 0x0000000000000000, 0xFFF8000000000000 },	{ 0x0000000000000000, 0xFFF0000000000000 },
		{ 0x0000000000000000, 0xFFE0000000000000 },	{ 0x0000000000000000, 0xFFC0000000000000 },
		{ 0x0000000000000000, 0xFF80000000000000 },	{ 0x0000000000000000, 0xFF00000000000000 },
		{ 0x0000000000000000, 0xFE00000000000000 },	{ 0x0000000000000000, 0xFC00000000000000 },
		{ 0x0000000000000000, 0xF800000000000000 },	{ 0x0000000000000000, 0xF000000000000000 },
		{ 0x0000000000000000, 0xE000000000000000 },	{ 0x0000000000000000, 0xC000000000000000 },
		{ 0x0000000000000000, 0x8000000000000000 }
	};

	const U64 rshResult[][2] = {
		{ 0xFFFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x3FFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x1FFFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x0FFFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x07FFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x03FFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x01FFFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x00FFFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x007FFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x003FFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x001FFFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x000FFFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x0007FFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x0003FFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x0001FFFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x0000FFFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x00007FFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x00003FFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x00001FFFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x00000FFFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x000007FFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x000003FFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x000001FFFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x000000FFFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x0000007FFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x0000003FFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x0000001FFFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x0000000FFFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x00000007FFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x00000003FFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x00000001FFFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x00000000FFFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x000000007FFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x000000003FFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x000000001FFFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x000000000FFFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x0000000007FFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x0000000003FFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x0000000001FFFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x0000000000FFFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x00000000007FFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x00000000003FFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x00000000001FFFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x00000000000FFFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x000000000007FFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x000000000003FFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x000000000001FFFF },	{ 0xFFFFFFFFFFFFFFFF, 0x000000000000FFFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x0000000000007FFF },	{ 0xFFFFFFFFFFFFFFFF, 0x0000000000003FFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x0000000000001FFF },	{ 0xFFFFFFFFFFFFFFFF, 0x0000000000000FFF },
		{ 0xFFFFFFFFFFFFFFFF, 0x00000000000007FF },	{ 0xFFFFFFFFFFFFFFFF, 0x00000000000003FF },
		{ 0xFFFFFFFFFFFFFFFF, 0x00000000000001FF },	{ 0xFFFFFFFFFFFFFFFF, 0x00000000000000FF },
		{ 0xFFFFFFFFFFFFFFFF, 0x000000000000007F },	{ 0xFFFFFFFFFFFFFFFF, 0x000000000000003F },
		{ 0xFFFFFFFFFFFFFFFF, 0x000000000000001F },	{ 0xFFFFFFFFFFFFFFFF, 0x000000000000000F },
		{ 0xFFFFFFFFFFFFFFFF, 0x0000000000000007 },	{ 0xFFFFFFFFFFFFFFFF, 0x0000000000000003 },
		{ 0xFFFFFFFFFFFFFFFF, 0x0000000000000001 },	{ 0xFFFFFFFFFFFFFFFF, 0x0000000000000000 },
		{ 0x7FFFFFFFFFFFFFFF, 0x0000000000000000 },	{ 0x3FFFFFFFFFFFFFFF, 0x0000000000000000 },
		{ 0x1FFFFFFFFFFFFFFF, 0x0000000000000000 },	{ 0x0FFFFFFFFFFFFFFF, 0x0000000000000000 },
		{ 0x07FFFFFFFFFFFFFF, 0x0000000000000000 },	{ 0x03FFFFFFFFFFFFFF, 0x0000000000000000 },
		{ 0x01FFFFFFFFFFFFFF, 0x0000000000000000 },	{ 0x00FFFFFFFFFFFFFF, 0x0000000000000000 },
		{ 0x007FFFFFFFFFFFFF, 0x0000000000000000 },	{ 0x003FFFFFFFFFFFFF, 0x0000000000000000 },
		{ 0x001FFFFFFFFFFFFF, 0x0000000000000000 },	{ 0x000FFFFFFFFFFFFF, 0x0000000000000000 },
		{ 0x0007FFFFFFFFFFFF, 0x0000000000000000 },	{ 0x0003FFFFFFFFFFFF, 0x0000000000000000 },
		{ 0x0001FFFFFFFFFFFF, 0x0000000000000000 },	{ 0x0000FFFFFFFFFFFF, 0x0000000000000000 },
		{ 0x00007FFFFFFFFFFF, 0x0000000000000000 },	{ 0x00003FFFFFFFFFFF, 0x0000000000000000 },
		{ 0x00001FFFFFFFFFFF, 0x0000000000000000 },	{ 0x00000FFFFFFFFFFF, 0x0000000000000000 },
		{ 0x000007FFFFFFFFFF, 0x0000000000000000 },	{ 0x000003FFFFFFFFFF, 0x0000000000000000 },
		{ 0x000001FFFFFFFFFF, 0x0000000000000000 },	{ 0x000000FFFFFFFFFF, 0x0000000000000000 },
		{ 0x0000007FFFFFFFFF, 0x0000000000000000 },	{ 0x0000003FFFFFFFFF, 0x0000000000000000 },
		{ 0x0000001FFFFFFFFF, 0x0000000000000000 },	{ 0x0000000FFFFFFFFF, 0x0000000000000000 },
		{ 0x00000007FFFFFFFF, 0x0000000000000000 },	{ 0x00000003FFFFFFFF, 0x0000000000000000 },
		{ 0x00000001FFFFFFFF, 0x0000000000000000 },	{ 0x00000000FFFFFFFF, 0x0000000000000000 },
		{ 0x000000007FFFFFFF, 0x0000000000000000 },	{ 0x000000003FFFFFFF, 0x0000000000000000 },
		{ 0x000000001FFFFFFF, 0x0000000000000000 },	{ 0x000000000FFFFFFF, 0x0000000000000000 },
		{ 0x0000000007FFFFFF, 0x0000000000000000 },	{ 0x0000000003FFFFFF, 0x0000000000000000 },
		{ 0x0000000001FFFFFF, 0x0000000000000000 },	{ 0x0000000000FFFFFF, 0x0000000000000000 },
		{ 0x00000000007FFFFF, 0x0000000000000000 },	{ 0x00000000003FFFFF, 0x0000000000000000 },
		{ 0x00000000001FFFFF, 0x0000000000000000 },	{ 0x00000000000FFFFF, 0x0000000000000000 },
		{ 0x000000000007FFFF, 0x0000000000000000 },	{ 0x000000000003FFFF, 0x0000000000000000 },
		{ 0x000000000001FFFF, 0x0000000000000000 },	{ 0x000000000000FFFF, 0x0000000000000000 },
		{ 0x0000000000007FFF, 0x0000000000000000 },	{ 0x0000000000003FFF, 0x0000000000000000 },
		{ 0x0000000000001FFF, 0x0000000000000000 },	{ 0x0000000000000FFF, 0x0000000000000000 },
		{ 0x00000000000007FF, 0x0000000000000000 },	{ 0x00000000000003FF, 0x0000000000000000 },
		{ 0x00000000000001FF, 0x0000000000000000 },	{ 0x00000000000000FF, 0x0000000000000000 },
		{ 0x000000000000007F, 0x0000000000000000 },	{ 0x000000000000003F, 0x0000000000000000 },
		{ 0x000000000000001F, 0x0000000000000000 },	{ 0x000000000000000F, 0x0000000000000000 },
		{ 0x0000000000000007, 0x0000000000000000 },	{ 0x0000000000000003, 0x0000000000000000 },
		{ 0x0000000000000001, 0x0000000000000000 }
	};

	Log_debugLn(alloc, "Testing big int lsh");

	for (U64 i = 0; i < sizeof(lshResult) / sizeof(lshResult[0]); ++i) {

		aBig = bBig = (BigInt) { 0 };

		U64 temp[2] = { mulParams[1][0], mulParams[1][1] };
		gotoIfError(clean, BigInt_createRef(&temp[0], 2, &aBig))
		gotoIfError(clean, BigInt_createRefConst(&lshResult[i][0], 2, &bBig))

		if(!BigInt_lsh(&aBig, (U16)(i + 1)) || BigInt_neq(aBig, bBig))
			gotoIfError(clean, Error_invalidOperation((U32)i, "BigInt_lsh failed"))
	}

	Log_debugLn(alloc, "Testing big int rsh");

	for (U64 i = 0; i < sizeof(rshResult) / sizeof(rshResult[0]); ++i) {

		aBig = bBig = (BigInt){ 0 };

		U64 temp[2] = { mulParams[1][0], mulParams[1][1] };
		gotoIfError(clean, BigInt_createRef(&temp[0], 2, &aBig))
		gotoIfError(clean, BigInt_createRefConst(&rshResult[i][0], 2, &bBig))

		if(!BigInt_rsh(&aBig, (U16)(i + 1)) || BigInt_neq(aBig, bBig))
			gotoIfError(clean, Error_invalidOperation((U32)i, "BigInt_rsh failed"))
	}

	Log_debugLn(alloc, "Testing big int bitScan");

	for (U64 i = 0; i < sizeof(rshResult) / sizeof(rshResult[0]); ++i) {

		aBig = (BigInt) { 0 };
		gotoIfError(clean, BigInt_createRefConst(&rshResult[i][0], 2, &aBig))

		U16 off = BigInt_bitScan(aBig);

		if(off != (U16)(sizeof(rshResult) / sizeof(rshResult[0]) - 1 - i))
			gotoIfError(clean, Error_invalidOperation((U32)i, "BigInt_bitScan failed"))
	}

	//U128 unit test

	Log_debugLn(alloc, "Testing U64 x U64 = U128 (optimized)");

	for(U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {

		U128 mulRes = U128_create((const U8*) mulResult[i]);
		U128 mulReal = U128_mul64(mulParams[i][0], mulParams[i][1]);

		if (U128_neq(mulRes, mulReal))
			gotoIfError(clean, Error_invalidOperation((U32)i, "U128_mul64 failed"))
	}

	Log_debugLn(alloc, "Testing U128 + U128 = U128 (optimized)");

	for(U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {

		U128 a = U128_create((const U8*) mulParams[i]);
		U128 b = U128_create((const U8*) mulResult[i]);
		U128 c = U128_create((const U8*) addResult[i]);

		if(U128_neq(U128_add(a, b), c))
			gotoIfError(clean, Error_invalidOperation((U32)i, "U128_add failed"))
	}

	Log_debugLn(alloc, "Testing U128 - U128 = U128 (optimized)");

	for(U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {

		U128 c = U128_create((const U8*) addResult[i]);
		U128 a = U128_create((const U8*) mulParams[i]);
		U128 b = U128_create((const U8*) mulResult[i]);

		if(U128_neq(U128_sub(c, b), a) || U128_neq(U128_sub(c, a), b))
			gotoIfError(clean, Error_invalidOperation((U32)i, "U128_sub failed"))
	}

	Log_debugLn(alloc, "Testing U128 lsh (optimized)");

	for (U64 i = 0; i < sizeof(lshResult) / sizeof(lshResult[0]); ++i) {

		U128 a = U128_create((const U8*) mulParams[1]);
		U128 b = U128_create((const U8*) lshResult[i]);
		U128 c = U128_lsh(a, (U8)(1 + i));

		if(U128_neq(c, b))
			gotoIfError(clean, Error_invalidOperation((U32)i, "U128_lsh failed"))
	}

	Log_debugLn(alloc, "Testing U128 rsh (optimized)");

	for (U64 i = 0; i < sizeof(rshResult) / sizeof(rshResult[0]); ++i) {

		U128 a = U128_create((const U8*) mulParams[1]);
		U128 b = U128_create((const U8*) rshResult[i]);
		U128 c = U128_rsh(a, (U8)(1 + i));

		if(U128_neq(c, b))
			gotoIfError(clean, Error_invalidOperation((U32)i, "U128_rsh failed"))
	}

	Log_debugLn(alloc, "Testing U128 bitScan");

	for (U64 i = 0; i < sizeof(rshResult) / sizeof(rshResult[0]); ++i) {

		U128 a = U128_create((const U8*) rshResult[i]);
		U16 off = U128_bitScan(a);

		if(off != (U16)(sizeof(rshResult) / sizeof(rshResult[0]) - 1 - i))
			gotoIfError(clean, Error_invalidOperation((U32)i, "U128_bitScan failed"))
	}

	Log_debugLn(alloc, "Testing U128 create from hex/bin/oct/dec/nyto");

	for(U64 i = 0; i < sizeof(stringified) / sizeof(stringified[0]); ++i) {

		U128 vi = U128_createFromString(CharString_createRefCStrConst(stringified[i]), &err, alloc);
		U128 realVi = U128_create((const U8*) &mulParams[i][0]);

		gotoIfError(clean, err)

		if(U128_neq(vi, realVi))
			gotoIfError(clean, Error_invalidState((U32)i, "U128_createFromString failed"))
	}

	/*Log_debugLn(alloc, "Testing U128 to hex/bin/oct/dec/nyto");

	for(U64 i = 0; i < sizeof(stringified) / sizeof(stringified[0]) && i < EIntegerEncoding_Count; ++i) {

		U128 realVi = U128_create((const U8*) &mulParams[i][0]);
		gotoIfError(clean, U128_toString(realVi, alloc, &tmp, (EIntegerEncoding)i, false))

		CharString ref = CharString_createRefCStrConst(stringified[i]);

		if(!CharString_equalsStringSensitive(ref, tmp))
			gotoIfError(clean, Error_invalidState((U32)i, "U128_toString failed"))

		CharString_free(&tmp, alloc);
	}*/

	//Test some basic chimera operations

	Chimera chimera = (Chimera) {
		.v4f = { F32x4_create4(1, 2, 3, 4) },
		.f = { 2, 1, 3, 4, 1 }
	};

	chimera.v4f[4] = F32x4_create4(1, 2, 3, 4);

	Log_debugLn(alloc, "Testing Chimera Fidi A: 0x00 - 0x1F");

	for(U8 i = 0; i < 4; ++i) {

		//add, sub

		F32 expected = chimera.f[4] + chimera.f[i];
		Chimera_stepFidiA(&chimera, EFidiA_add(i));

		if(chimera.f[4] != expected)
			gotoIfError(clean, Error_invalidState((U32)i, "EFidiA add test failed"))

		expected = chimera.f[4] - chimera.f[i];
		Chimera_stepFidiA(&chimera, EFidiA_sub(i));

		if(chimera.f[4] != expected)
			gotoIfError(clean, Error_invalidState((U32)i, "EFidiA sub test failed"))

		//mul

		expected = chimera.f[4] * chimera.f[i];
		Chimera_stepFidiA(&chimera, EFidiA_mul(i));

		if(chimera.f[4] != expected)
			gotoIfError(clean, Error_invalidState((U32)i, "EFidiA mul test failed"))

		//swap

		expected = chimera.f[i];
		F32 old = chimera.f[4];
		Chimera_stepFidiA(&chimera, EFidiA_swap(i));

		if(chimera.f[4] != expected || chimera.f[i] != old)
			gotoIfError(clean, Error_invalidState((U32)i, "EFidiA swap test failed"))

		//compare

		ECompareResult expectedCmp = chimera.f[4] < chimera.f[i] ? ECompareResult_Lt : (
			chimera.f[4] > chimera.f[i] ? ECompareResult_Gt : ECompareResult_Eq
		);

		Chimera_stepFidiA(&chimera, EFidiA_cmp(i));
		if(expectedCmp != Chimera_getLastCompare(&chimera))
			gotoIfError(clean, Error_invalidState((U32)i, "EFidiA cmp test failed"))

		//load fN

		Chimera_stepFidiA(&chimera, EFidiA_load(i));
		if(chimera.f[i] != chimera.f[4])
			gotoIfError(clean, Error_invalidState((U32)i, "EFidiA load test failed"))
	}

	{
		F32 expected = 0;

		//max

		expected = F32_max(chimera.f[4], chimera.f[0]);
		Chimera_stepFidiA(&chimera, EFidiA_max);
		if(chimera.f[4] != expected)
			gotoIfError(clean, Error_invalidState(0, "EFidiA max test failed"))

		//div

		expected = chimera.f[4] / chimera.f[0];
		Chimera_stepFidiA(&chimera, EFidiA_div);
		if(chimera.f[4] != expected)
			gotoIfError(clean, Error_invalidState(0, "EFidiA div test failed"))

		//mod

		gotoIfError(clean, F32_mod(chimera.f[4], chimera.f[0], &expected))
		Chimera_stepFidiA(&chimera, EFidiA_mod);
		if(chimera.f[4] != expected)
			gotoIfError(clean, Error_invalidState(0, "EFidiA mod test failed"))

		//min

		expected = F32_min(chimera.f[4], chimera.f[0]);
		Chimera_stepFidiA(&chimera, EFidiA_min);
		if(chimera.f[4] != expected)
			gotoIfError(clean, Error_invalidState(0, "EFidiA min test failed"))

		//isfinite

		ECompareResult expectedCmp = F32_isValid(chimera.f[4]) ? ECompareResult_Gt : ECompareResult_Eq;

		Chimera_stepFidiA(&chimera, EFidiA_isfinite);
		if(expectedCmp != Chimera_getLastCompare(&chimera))
			gotoIfError(clean, Error_invalidState(0, "EFidiA isfinite test failed"))

		//isnan

		expectedCmp = F32_isNaN(chimera.f[4]) ? ECompareResult_Gt : ECompareResult_Eq;

		Chimera_stepFidiA(&chimera, EFidiA_isnan);
		if(expectedCmp != Chimera_getLastCompare(&chimera))
			gotoIfError(clean, Error_invalidState(0, "EFidiA isnan test failed"))

		//any

		expectedCmp = F32x4_any(chimera.vf[4]) ? ECompareResult_Gt : ECompareResult_Eq;

		Chimera_stepFidiA(&chimera, EFidiA_anyFv);
		if(expectedCmp != Chimera_getLastCompare(&chimera))
			gotoIfError(clean, Error_invalidState(0, "EFidiA any test failed"))

		//all

		expectedCmp = F32x4_all(chimera.vf[4]) ? ECompareResult_Gt : ECompareResult_Eq;

		Chimera_stepFidiA(&chimera, EFidiA_allFv);
		if(expectedCmp != Chimera_getLastCompare(&chimera))
			gotoIfError(clean, Error_invalidState(0, "EFidiA all test failed"))
	}

	//

	F64 dt = (Time_now() - now) / (F64)SECOND;

	Log_debugLn(alloc, "Successful unit test! After %fs", dt);
	
	goto success;

clean:

	F64 dt2 = (Time_now() - now) / (F64)SECOND;
	Log_errorLn(alloc, "Failed unit test (%s)... Freeing. After %fs", err.errorStr, dt2);

success:
	BufferLayout_free(alloc, &bufferLayout);

	CharString_free(&tmp, alloc);

	for(U64 j = 0; j < sizeof(inputs) / sizeof(inputs[0]); ++j)
		CharString_free(&inputs[j], alloc);

	Buffer_free(&outputEncrypted, alloc);
	Buffer_free(&outputDecrypted, alloc);
	Buffer_free(&emp, alloc);
	Buffer_free(&full, alloc);

	return (int) err.genericError;
}