/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "types/time.h"
#include "types/buffer.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/type_cast.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

Error ourAlloc(void *allocator, U64 length, Buffer *output) {

	allocator;

	void *ptr = malloc(length);

	if(!output)
		return Error_nullPointer(2, 0);

	if(!ptr)
		return Error_outOfMemory(0);

	*output = Buffer_createManagedPtr( ptr, length);
	return Error_none();
}

Bool ourFree(void *allocator, Buffer buf) {
	allocator;
	free(buf.ptr);
	return true;
}

//Required to compile

void Error_fillStackTrace(Error *err) {
	if(err)
		err->stackTrace[0] = NULL;
}

String Error_formatPlatformError(Error err) {
	err;
	return String_createNull();
}

//#define _STRICT_VALIDATION

#ifdef _STRICT_VALIDATION
	#define _EXTRA_CHECKS 4
#else
	#define _EXTRA_CHECKS 0
#endif

//

int main() {

	//Fail for big endian systems, because we don't support them.

	U16 v = 1;

	if(!*(U8*)&v) {
		printf("Failed unit test: Unsupported endianness (only supporting little endian architectures)");
		return -1;
	}

	//Test timer

	printf("Testing Time\n");

	Ns now = Time_now();
	TimerFormat nowStr;

	Time_format(now, nowStr);

	Ns now2 = 0;
	EFormatStatus stat = Time_parseFormat(&now2, nowStr);

	if (stat != EFormatStatus_Success || now2 != now) {
		printf("Failed unit test: Time_parseFormat or Time_format is broken\n");
		return 1;
	}

	Allocator alloc = (Allocator) {
		.alloc = ourAlloc,
		.free = ourFree,
		.ptr = NULL
	};

	Buffer emp = Buffer_createNull(), full = Buffer_createNull();
	Buffer outputEncrypted = Buffer_createNull(), outputDecrypted = Buffer_createNull();
	Error err = Error_none();
	String tmp = String_createNull();

	String inputs[19 + _EXTRA_CHECKS] = { 0 };

	//Test Buffer

	printf("Testing Buffer\n");

	_gotoIfError(clean, Buffer_createZeroBits(256, alloc, &emp));
	_gotoIfError(clean, Buffer_createOneBits(256, alloc, &full));

	Bool res = false;

	if (Buffer_eq(emp, full, &res).genericError || res)
		_gotoIfError(clean, Error_invalidOperation(0));

	Buffer_bitwiseNot(emp);

	if (Buffer_eq(emp, full, &res).genericError || !res)
		_gotoIfError(clean, Error_invalidOperation(1));

	Buffer_bitwiseNot(emp);

	Buffer_setBitRange(emp, 9, 240);
	Buffer_unsetBitRange(full, 9, 240);

	Buffer_bitwiseNot(emp);

	if (Buffer_neq(emp, full, &res).genericError || res)
		_gotoIfError(clean, Error_invalidOperation(2));

	Buffer_free(&emp, alloc);
	Buffer_free(&full, alloc);

	//TODO: Test vectors
	//TODO: Test quaternions
	//TODO: Test transform
	//TODO: Test string
	//TODO: Test math
	//TODO: Test file
	//TODO: Test list

	//Test CRC32C function
	//https://stackoverflow.com/questions/20963944/test-vectors-for-crc32c

	printf("Testing Buffer CRC32C\n");

	typedef struct TestCRC32C {
		const C8 *str;
		const C8 v[5];
	} TestCRC32C;

	static const TestCRC32C TEST_CRC32C[] = {
		{ "", "\x00\x00\x00\x00" },
		{ "a", "\x30\x43\xd0\xc1" },
		{ "abc", "\xb7\x3f\x4b\x36" },
		{ "message digest", "\xd0\x79\xbd\x02" },
		{ "abcdefghijklmnopqrstuvwxyz", "\x25\xef\xe6\x9e" },
		{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "\x7d\xd5\x45\xa2" },
		{ "12345678901234567890123456789012345678901234567890123456789012345678901234567890", "\x81\x67\x7a\x47" },
		{ "123456789", "\x83\x92\x06\xe3" }
	};

	for (U64 i = 0; i < sizeof(TEST_CRC32C) / sizeof(TEST_CRC32C[0]); ++i) {

		Buffer buf = Buffer_createConstRef(
			TEST_CRC32C[i].str, String_calcStrLen(TEST_CRC32C[i].str, U64_MAX)
		);

		U32 groundTruth = *(const U32*) TEST_CRC32C[i].v;
		U32 ours = Buffer_crc32c(buf);

		if(groundTruth != ours)
			_gotoIfError(clean, Error_invalidOperation(3));
	}

	//Test SHA256 function
	//https://www.di-mgt.com.au/sha_testvectors.html
	//https://www.dlitz.net/crypto/shad256-test-vectors/
	//https://github.com/amosnier/sha-2/blob/master/test.c

	printf("Testing Buffer SHA256\n");

	static const U32 RESULTS[][8] = {
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

	inputs[1] = String_createConstRefUnsafe("abc");

	_gotoIfError(clean, String_create('a', MEGA, alloc, inputs + 2));

	inputs[3] = String_createConstRefSized("\xde\x18\x89\x41\xa3\x37\x5d\x3a\x8a\x06\x1e\x67\x57\x6e\x92\x6d", 16);

	inputs[4] = String_createConstRefSized(
		"\xDE\x18\x89\x41\xA3\x37\x5D\x3A\x8A\x06\x1E\x67\x57\x6E\x92\x6D\xC7\x1A\x7F\xA3\xF0"
		"\xCC\xEB\x97\x45\x2B\x4D\x32\x27\x96\x5F\x9E\xA8\xCC\x75\x07\x6D\x9F\xB9\xC5\x41\x7A"
		"\xA5\xCB\x30\xFC\x22\x19\x8B\x34\x98\x2D\xBB\x62\x9E", 
		55
	);

	inputs[5] = String_createConstRefUnsafe("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");

	inputs[6] = String_createConstRefUnsafe(
		"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"
	);

	inputs[7] = String_createConstRefUnsafe("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
	inputs[8] = String_createConstRefUnsafe("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde");
	inputs[9] = String_createConstRefUnsafe("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0");

	inputs[10] = String_createConstRefSized("\xBD", 1);
	inputs[11] = String_createConstRefSized("\xC9\x8C\x8E\x55", 4);

	U8 data7[1000] = { 0 }, data8[1000], data9[1005];
	inputs[12] = String_createConstRefSized((const C8*)data7, 55);
	inputs[13] = String_createConstRefSized((const C8*)data7, 56);
	inputs[14] = String_createConstRefSized((const C8*)data7, 57);
	inputs[15] = String_createConstRefSized((const C8*)data7, 64);
	inputs[16] = String_createConstRefSized((const C8*)data7, 1000);
	inputs[17] = String_createConstRefSized((const C8*)data8, 1000);
	inputs[18] = String_createConstRefSized((const C8*)data9, 1005);

	memset(data8, 0x41, sizeof(data8));
	memset(data9, 0x55, sizeof(data9));

	//More extreme checks. Don't wanna run this every time.

	#if _EXTRA_CHECKS

		_gotoIfError(clean, String_create('\x5A', 536'870'912, alloc, inputs + 20));
		_gotoIfError(clean, String_create('\0', 1'090'519'040, alloc, inputs + 21));
		_gotoIfError(clean, String_create('\x42', 1'610'612'798, alloc, inputs + 22));

		inputs[19] = String_createConstRefSized(inputs[21].ptr, 1'000'000);

	#endif

	//Validate inputs

	for(U64 i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {

		U32 result[8];
		Buffer_sha256(String_bufferConst(inputs[i]), result);
	
		Bool b = false;
		Buffer_eq(
			Buffer_createConstRef(result, 32), 
			Buffer_createConstRef(RESULTS[i], 32), 
			&b
		);
	
		if(!b)
			_gotoIfError(clean, Error_invalidOperation(4));
	}

	for(U64 i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i)
		String_free(&inputs[i], alloc);

	//Test big endian conversions

	printf("Testing endianness swapping\n");

	U16 be16 = U16_swapEndianness(0x1234);
	U32 be32 = U32_swapEndianness(0x12345678);
	U64 be64 = U64_swapEndianness(0x123456789ABCDEF0);

	if(be16 != 0x3412)
		_gotoIfError(clean, Error_invalidState(5));

	if(be32 != 0x78563412)
		_gotoIfError(clean, Error_invalidState(6));

	if(be64 != 0xF0DEBC9A78563412)
		_gotoIfError(clean, Error_invalidState(7));

	//Test encryption

	//Test vectors (13-16): https://luca-giuzzi.unibs.it/corsi/Support/papers-cryptography/gcm-spec.pdf
	//						Ignored test vectors 17-18 since we don't allow arbitrary length IVs
	//						Ignored non AES256GCM test vectors (128, 192).
	// 
	//Test vectors	https://www.ieee802.org/1/files/public/docs2011/bn-randall-test-vectors-0511-v1.pdf
	//				2.1.2, 2.2.2, 2.3.2, 2.4.2, 2.5.2, 2.6.2, 2.7.2, 2.8.2

	printf("Testing Buffer encrypt/decrypt\n");

	String testKeys[] = {

		String_createConstRefSized(
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 
			32
		),

		String_createConstRefSized(
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 
			32
		),

		String_createConstRefSized(
			"\xFE\xFF\xE9\x92\x86\x65\x73\x1C\x6D\x6A\x8F\x94\x67\x30\x83\x08"
			"\xFE\xFF\xE9\x92\x86\x65\x73\x1C\x6D\x6A\x8F\x94\x67\x30\x83\x08", 
			32
		),

		String_createConstRefSized(
			"\xFE\xFF\xE9\x92\x86\x65\x73\x1C\x6D\x6A\x8F\x94\x67\x30\x83\x08"
			"\xFE\xFF\xE9\x92\x86\x65\x73\x1C\x6D\x6A\x8F\x94\x67\x30\x83\x08", 
			32
		),

		String_createConstRefSized(
			"\xE3\xC0\x8A\x8F\x06\xC6\xE3\xAD\x95\xA7\x05\x57\xB2\x3F\x75\x48"
			"\x3C\xE3\x30\x21\xA9\xC7\x2B\x70\x25\x66\x62\x04\xC6\x9C\x0B\x72", 
			32
		),

		String_createConstRefSized(
			"\xE3\xC0\x8A\x8F\x06\xC6\xE3\xAD\x95\xA7\x05\x57\xB2\x3F\x75\x48"
			"\x3C\xE3\x30\x21\xA9\xC7\x2B\x70\x25\x66\x62\x04\xC6\x9C\x0B\x72", 
			32
		),

		String_createConstRefSized(
			"\x69\x1D\x3E\xE9\x09\xD7\xF5\x41\x67\xFD\x1C\xA0\xB5\xD7\x69\x08"
			"\x1F\x2B\xDE\x1A\xEE\x65\x5F\xDB\xAB\x80\xBD\x52\x95\xAE\x6B\xE7", 
			32
		),

		String_createConstRefSized(
			"\x69\x1D\x3E\xE9\x09\xD7\xF5\x41\x67\xFD\x1C\xA0\xB5\xD7\x69\x08"
			"\x1F\x2B\xDE\x1A\xEE\x65\x5F\xDB\xAB\x80\xBD\x52\x95\xAE\x6B\xE7", 
			32
		),

		String_createConstRefSized(
			"\x83\xC0\x93\xB5\x8D\xE7\xFF\xE1\xC0\xDA\x92\x6A\xC4\x3F\xB3\x60"
			"\x9A\xC1\xC8\x0F\xEE\x1B\x62\x44\x97\xEF\x94\x2E\x2F\x79\xA8\x23", 
			32
		),

		String_createConstRefSized(
			"\x83\xC0\x93\xB5\x8D\xE7\xFF\xE1\xC0\xDA\x92\x6A\xC4\x3F\xB3\x60"
			"\x9A\xC1\xC8\x0F\xEE\x1B\x62\x44\x97\xEF\x94\x2E\x2F\x79\xA8\x23", 
			32
		),

		String_createConstRefSized(
			"\x4C\x97\x3D\xBC\x73\x64\x62\x16\x74\xF8\xB5\xB8\x9E\x5C\x15\x51"
			"\x1F\xCE\xD9\x21\x64\x90\xFB\x1C\x1A\x2C\xAA\x0F\xFE\x04\x07\xE5", 
			32
		),

		String_createConstRefSized(
			"\x4C\x97\x3D\xBC\x73\x64\x62\x16\x74\xF8\xB5\xB8\x9E\x5C\x15\x51"
			"\x1F\xCE\xD9\x21\x64\x90\xFB\x1C\x1A\x2C\xAA\x0F\xFE\x04\x07\xE5", 
			32
		)
	};

	String testPlainText[] = {

		String_createNull(),

		String_createConstRefSized(
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 
			16
		),

		String_createConstRefSized(
			"\xD9\x31\x32\x25\xF8\x84\x06\xE5\xA5\x59\x09\xC5\xAF\xF5\x26\x9A"
			"\x86\xA7\xA9\x53\x15\x34\xF7\xDA\x2E\x4C\x30\x3D\x8A\x31\x8A\x72"
			"\x1C\x3C\x0C\x95\x95\x68\x09\x53\x2F\xCF\x0E\x24\x49\xA6\xB5\x25"
			"\xB1\x6A\xED\xF5\xAA\x0D\xE6\x57\xBA\x63\x7B\x39\x1A\xAF\xD2\x55", 
			64
		),

		String_createConstRefSized(
			"\xD9\x31\x32\x25\xF8\x84\x06\xE5\xA5\x59\x09\xC5\xAF\xF5\x26\x9A"
			"\x86\xA7\xA9\x53\x15\x34\xF7\xDA\x2E\x4C\x30\x3D\x8A\x31\x8A\x72"
			"\x1C\x3C\x0C\x95\x95\x68\x09\x53\x2F\xCF\x0E\x24\x49\xA6\xB5\x25"
			"\xB1\x6A\xED\xF5\xAA\x0D\xE6\x57\xBA\x63\x7B\x39", 
			60
		),

		String_createNull(),

		String_createConstRefSized(
			"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
			"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
			"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x00\x02", 
			48
		),

		String_createNull(),

		String_createConstRefSized(
			"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
			"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
			"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x00\x04", 
			42
		),

		String_createNull(),

		String_createConstRefSized(
			"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
			"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
			"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x00"
			"\x06", 
			49
		),

		String_createNull(),

		String_createConstRefSized(
			"\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
			"\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C"
			"\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C"
			"\x3D\x3E\x3F\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x00\x08", 
			63
		)
	};

	String additionalData[] = {

		String_createNull(),
		String_createNull(),
		String_createNull(),

		String_createConstRefSized(
			"\xFE\xED\xFA\xCE\xDE\xAD\xBE\xEF\xFE\xED\xFA\xCE\xDE\xAD\xBE\xEF"
			"\xAB\xAD\xDA\xD2", 
			20
		),

		String_createConstRefSized(
			"\xD6\x09\xB1\xF0\x56\x63\x7A\x0D\x46\xDF\x99\x8D\x88\xE5\x22\x2A"
			"\xB2\xC2\x84\x65\x12\x15\x35\x24\xC0\x89\x5E\x81\x08\x00\x0F\x10"
			"\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20"
			"\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30"
			"\x31\x32\x33\x34\x00\x01", 
			70
		),

		String_createConstRefSized(
			"\xD6\x09\xB1\xF0\x56\x63\x7A\x0D\x46\xDF\x99\x8D\x88\xE5\x2E\x00"
			"\xB2\xC2\x84\x65\x12\x15\x35\x24\xC0\x89\x5E\x81",
			28
		),

		String_createConstRefSized(
			"\xE2\x01\x06\xD7\xCD\x0D\xF0\x76\x1E\x8D\xCD\x3D\x88\xE5\x40\x00"
			"\x76\xD4\x57\xED\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18"
			"\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28"
			"\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38"
			"\x39\x3A\x00\x03",
			68
		),

		String_createConstRefSized(
			"\xE2\x01\x06\xD7\xCD\x0D\xF0\x76\x1E\x8D\xCD\x3D\x88\xE5\x4C\x2A"
			"\x76\xD4\x57\xED",
			20
		),

		String_createConstRefSized(
			"\x84\xC5\xD5\x13\xD2\xAA\xF6\xE5\xBB\xD2\x72\x77\x88\xE5\x23\x00"
			"\x89\x32\xD6\x12\x7C\xFD\xE9\xF9\xE3\x37\x24\xC6\x08\x00\x0F\x10"
			"\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20"
			"\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30"
			"\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F\x00"
			"\x05",
			81
		),

		String_createConstRefSized(
			"\x84\xC5\xD5\x13\xD2\xAA\xF6\xE5\xBB\xD2\x72\x77\x88\xE5\x2F\x00"
			"\x89\x32\xD6\x12\x7C\xFD\xE9\xF9\xE3\x37\x24\xC6",
			28
		),

		String_createConstRefSized(
			"\x68\xF2\xE7\x76\x96\xCE\x7A\xE8\xE2\xCA\x4E\xC5\x88\xE5\x41\x00"
			"\x2E\x58\x49\x5C\x08\x00\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18"
			"\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28"
			"\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30\x31\x32\x33\x34\x35\x36\x37\x38"
			"\x39\x3A\x3B\x3C\x3D\x3E\x3F\x40\x41\x42\x43\x44\x45\x46\x47\x48"
			"\x49\x4A\x4B\x4C\x4D\x00\x07",
			87
		),

		String_createConstRefSized(
			"\x68\xF2\xE7\x76\x96\xCE\x7A\xE8\xE2\xCA\x4E\xC5\x88\xE5\x4D\x00"
			"\x2E\x58\x49\x5C",
			20
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

	String results[] = {

		String_createConstRefSized(
			"\x53\x0F\x8A\xFB\xC7\x45\x36\xB9\xA9\x63\xB4\xF1\xC4\xCB\x73\x8B",		//Tag (iv is prepended automatically)
			16
		),

		String_createConstRefSized(
			"\xCE\xA7\x40\x3D\x4D\x60\x6B\x6E\x07\x4E\xC5\xD3\xBA\xF3\x9D\x18"		//Block 0
			"\xD0\xD1\xC8\xA7\x99\x99\x6B\xF0\x26\x5B\x98\xB5\xD4\x8A\xB9\x19",		//Tag
			32
		),

		String_createConstRefSized(
			"\x52\x2D\xC1\xF0\x99\x56\x7D\x07\xF4\x7F\x37\xA3\x2A\x84\x42\x7D"		//Cipher
			"\x64\x3A\x8C\xDC\xBF\xE5\xC0\xC9\x75\x98\xA2\xBD\x25\x55\xD1\xAA"
			"\x8C\xB0\x8E\x48\x59\x0D\xBB\x3D\xA7\xB0\x8B\x10\x56\x82\x88\x38"
			"\xC5\xF6\x1E\x63\x93\xBA\x7A\x0A\xBC\xC9\xF6\x62\x89\x80\x15\xAD"
			"\xB0\x94\xDA\xC5\xD9\x34\x71\xBD\xEC\x1A\x50\x22\x70\xE3\xCC\x6C",		//Tag
			16 + 64
		),

		String_createConstRefSized(
			"\x52\x2D\xC1\xF0\x99\x56\x7D\x07\xF4\x7F\x37\xA3\x2A\x84\x42\x7D"		//Cipher
			"\x64\x3A\x8C\xDC\xBF\xE5\xC0\xC9\x75\x98\xA2\xBD\x25\x55\xD1\xAA"
			"\x8C\xB0\x8E\x48\x59\x0D\xBB\x3D\xA7\xB0\x8B\x10\x56\x82\x88\x38"
			"\xC5\xF6\x1E\x63\x93\xBA\x7A\x0A\xBC\xC9\xF6\x62"
			"\x76\xFC\x6E\xCE\x0F\x4E\x17\x68\xCD\xDF\x88\x53\xBB\x2D\x55\x1B",		//Tag
			16 + 60
		),

		String_createConstRefSized(
			"\x2F\x0B\xC5\xAF\x40\x9E\x06\xD6\x09\xEA\x8B\x7D\x0F\xA5\xEA\x50",		//Tag (iv is prepended automatically)
			16
		),

		String_createConstRefSized(
			"\xE2\x00\x6E\xB4\x2F\x52\x77\x02\x2D\x9B\x19\x92\x5B\xC4\x19\xD7"		//Cipher
			"\xA5\x92\x66\x6C\x92\x5F\xE2\xEF\x71\x8E\xB4\xE3\x08\xEF\xEA\xA7"
			"\xC5\x27\x3B\x39\x41\x18\x86\x0A\x5B\xE2\xA9\x7F\x56\xAB\x78\x36"
			"\x5C\xA5\x97\xCD\xBB\x3E\xDB\x8D\x1A\x11\x51\xEA\x0A\xF7\xB4\x36",		//Tag
			16 + 48
		),

		String_createConstRefSized(
			"\x35\x21\x7C\x77\x4B\xBC\x31\xB6\x31\x66\xBC\xF9\xD4\xAB\xED\x07",		//Tag (iv is prepended automatically)
			16
		),

		String_createConstRefSized(
			"\xC1\x62\x3F\x55\x73\x0C\x93\x53\x30\x97\xAD\xDA\xD2\x56\x64\x96"		//Cipher
			"\x61\x25\x35\x2B\x43\xAD\xAC\xBD\x61\xC5\xEF\x3A\xC9\x0B\x5B\xEE"
			"\x92\x9C\xE4\x63\x0E\xA7\x9F\x6C\xE5\x19"
			"\x12\xAF\x39\xC2\xD1\xFD\xC2\x05\x1F\x8B\x7B\x3C\x9D\x39\x7E\xF2",		//Tag (iv is prepended automatically)
			16 + 42
		),

		String_createConstRefSized(
			"\x6E\xE1\x60\xE8\xFA\xEC\xA4\xB3\x6C\x86\xB2\x34\x92\x0C\xA9\x75",		//Tag (iv is prepended automatically)
			16
		),

		String_createConstRefSized(
			"\x11\x02\x22\xFF\x80\x50\xCB\xEC\xE6\x6A\x81\x3A\xD0\x9A\x73\xED"		//Cipher
			"\x7A\x9A\x08\x9C\x10\x6B\x95\x93\x89\x16\x8E\xD6\xE8\x69\x8E\xA9"
			"\x02\xEB\x12\x77\xDB\xEC\x2E\x68\xE4\x73\x15\x5A\x15\xA7\xDA\xEE"
			"\xD4"
			"\xA1\x0F\x4E\x05\x13\x9C\x23\xDF\x00\xB3\xAA\xDC\x71\xF0\x59\x6A",		//Tag (iv is prepended automatically)
			16 + 49
		),

		String_createConstRefSized(
			"\x00\xBD\xA1\xB7\xE8\x76\x08\xBC\xBF\x47\x0F\x12\x15\x7F\x4C\x07",		//Tag (iv is prepended automatically)
			16
		),

		String_createConstRefSized(
			"\xBA\x8A\xE3\x1B\xC5\x06\x48\x6D\x68\x73\xE4\xFC\xE4\x60\xE7\xDC"
			"\x57\x59\x1F\xF0\x06\x11\xF3\x1C\x38\x34\xFE\x1C\x04\xAD\x80\xB6"
			"\x68\x03\xAF\xCF\x5B\x27\xE6\x33\x3F\xA6\x7C\x99\xDA\x47\xC2\xF0"
			"\xCE\xD6\x8D\x53\x1B\xD7\x41\xA9\x43\xCF\xF7\xA6\x71\x3B\xD0"
			"\x26\x11\xCD\x7D\xAA\x01\xD6\x1C\x5C\x88\x6D\xC1\xA8\x17\x01\x07",		//Tag (iv is prepended automatically)
			16 + 63
		)
	};

	for (U64 i = 0; i < sizeof(ivs) / sizeof(ivs[0]); ++i) {

		//Fetch iv

		I32x4 iv = I32x4_zero();
		Buffer_copy(Buffer_createRef(&iv, 12), Buffer_createConstRef(ivs[i], 12));

		//Copy into tmp variable to be able to modify it instead of using const mem

		_gotoIfError(clean, String_createCopy(testPlainText[i], alloc, &tmp));

		//Encrypt plain text

		I32x4 tag = I32x4_zero();

		_gotoIfError(clean, Buffer_encrypt(
			String_buffer(tmp),
			String_bufferConst(additionalData[i]),
			EBufferEncryptionType_AES256GCM,
			EBufferEncryptionFlags_None,
			(U32*) testKeys[i].ptr,
			&iv,
			&tag
		));

		//Check size

		if(tmp.length + 16 != results[i].length)
			_gotoIfError(clean, Error_invalidState(3));

		//Check tag (intermediate copy because otherwise Release will crash because of unaligned memory)

		I32x4 tmpTag = I32x4_zero();
		Buffer_copy(
			Buffer_createRef(&tmpTag, sizeof(tmpTag)),
			Buffer_createConstRef(results[i].ptr + tmp.length, sizeof(I32x4))
		);

		if(I32x4_any(I32x4_neq(tag, tmpTag)))
			_gotoIfError(clean, Error_invalidState(1));

		//Check result

		Bool b = false;

		_gotoIfError(
			clean, 
			Buffer_eq(
				Buffer_createConstRef(results[i].ptr, testPlainText[i].length),
				String_bufferConst(tmp),
				&b
			)
		);

		if(!b)
			_gotoIfError(clean, Error_invalidState(2));

		//Decrypt the encrypted string and verify if it decrypts to the same thing

		_gotoIfError(clean, Buffer_decrypt(
			String_buffer(tmp),
			String_bufferConst(additionalData[i]),
			EBufferEncryptionType_AES256GCM,
			(const U32*) testKeys[i].ptr,
			tag,
			iv
		));

		//Check result

		b = false;

		_gotoIfError(
			clean, 
			Buffer_eq(
				String_bufferConst(testPlainText[i]),
				String_bufferConst(tmp),
				&b
			)
		);

		if(!b)
			_gotoIfError(clean, Error_invalidState(4));

		String_free(&tmp, alloc);
	}

	//

	F32 dt = (Time_now() - now) / (F32)SECOND;

	printf("Successful unit test! After %fs\n", dt);
	return 0;

clean:

	printf("Failed unit test... Freeing\n");

	String_free(&tmp, alloc);
	
	for(U64 j = 0; j < sizeof(inputs) / sizeof(inputs[0]); ++j)
		String_free(&inputs[j], alloc);

	Buffer_free(&outputEncrypted, alloc);
	Buffer_free(&outputDecrypted, alloc);
	Buffer_free(&emp, alloc);
	Buffer_free(&full, alloc);

	F32 dt2 = (Time_now() - now) / (F32)SECOND;

	printf("Failed unit test... After %fs\n", dt2);

	return (int) err.genericError;
}