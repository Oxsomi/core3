#include "types/time.h"
#include "types/buffer.h"
#include "types/allocator.h"
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

	*output = (Buffer) { .ptr = ptr, .length = length };
	return Error_none();
}

Error ourFree(void *allocator, Buffer buf) {
	allocator;
	free(buf.ptr);
	return Error_none();
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

#define STRICT_VALIDATION

//

int main() {

	//Fail for big endian systems, because we don't support them.

	U16 v = 1;

	if(!*(U8*)&v) {
		printf("Failed unit test: Unsupported endianness (only supporting little endian architectures)");
		return -1;
	}

	//Test timer

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

	//Test Buffer

	Buffer emp = Buffer_createNull();
	Error err = Buffer_createZeroBits(256, alloc, &emp);

	if(err.genericError) {
		printf("Failed unit test: create zero bits failed with error\n");
		return 4;
	}

	Buffer full = Buffer_createNull(); 
	err = Buffer_createOneBits(256, alloc, &full);

	if(err.genericError) {
		printf("Failed unit test: create one bits failed with error\n");
		Buffer_free(&emp, alloc);
		return 5;
	}

	Bool res = false;

	if (Buffer_eq(emp, full, &res).genericError || res) {
		printf("Failed unit test: empty and full bitset can't be equal!\n");
		Buffer_free(&emp, alloc);
		Buffer_free(&full, alloc);
		return 2;
	}

	Buffer_bitwiseNot(emp);

	if (Buffer_eq(emp, full, &res).genericError || !res) {
		printf("Failed unit test: bitwiseNot is invalid!\n");
		Buffer_free(&emp, alloc);
		Buffer_free(&full, alloc);
		return 2;
	}

	Buffer_bitwiseNot(emp);

	Buffer_setBitRange(emp, 9, 240);
	Buffer_unsetBitRange(full, 9, 240);

	Buffer_bitwiseNot(emp);

	if (Buffer_neq(emp, full, &res).genericError || res) {
		printf("Failed unit test: setBitRange or unsetBitRange is invalid!\n");
		Buffer_free(&emp, alloc);
		Buffer_free(&full, alloc);
		return 3;
	}

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

	typedef struct TestCRC32C {
		const C8 *str;
		const C8 v[5];
	} TestCRC32C;

	static const TestCRC32C testCrc32c[] = {
		{ "", "\x00\x00\x00\x00" },
		{ "a", "\x30\x43\xd0\xc1" },
		{ "abc", "\xb7\x3f\x4b\x36" },
		{ "message digest", "\xd0\x79\xbd\x02" },
		{ "abcdefghijklmnopqrstuvwxyz", "\x25\xef\xe6\x9e" },
		{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "\x7d\xd5\x45\xa2" },
		{ "12345678901234567890123456789012345678901234567890123456789012345678901234567890", "\x81\x67\x7a\x47" },
		{ "123456789", "\x83\x92\x06\xe3" }
	};

	for (U64 i = 0; i < sizeof(testCrc32c) / sizeof(testCrc32c[0]); ++i) {

		Buffer buf = Buffer_createRef((void*)testCrc32c[i].str, String_calcStrLen(testCrc32c[i].str, U64_MAX));
		U32 groundTruth = *(const U32*) testCrc32c[i].v;
		U32 ours = Buffer_crc32c(buf);

		if(groundTruth != ours) {
			printf("Failed unit test: CRC32C test failed! (at offset %llu)\n", i);
			return 5;
		}
	}

	//Test SHA256 function
	//https://www.di-mgt.com.au/sha_testvectors.html
	//https://www.dlitz.net/crypto/shad256-test-vectors/
	//https://github.com/amosnier/sha-2/blob/master/test.c

	#ifdef STRICT_VALIDATION
		#define EXTRA_CHECKS 4
	#else
		#define EXTRA_CHECKS 0
	#endif

	static const U32 results[][8] = {
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

	String inputs[19 + EXTRA_CHECKS] = { 0 };

	inputs[1] = String_createConstRefUnsafe("abc");

	if(String_create('a', Mega, alloc, inputs + 2).genericError) {
		printf("Failed unit test: SHA256 test failed! Couldn't allocate memory for SHA256 test 1!\n");
		return 7;
	}

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

	#ifdef EXTRA_CHECKS

		if(String_create('\x5A', 536'870'912, alloc, inputs + 20).genericError) {

			for(U64 i = 0; i < 20; ++i)
				String_free(&inputs[i], alloc);

			printf("Failed unit test: SHA256 test failed! Couldn't allocate memory for SHA256 test 19!\n");
			return 7;
		}

		if(String_create('\0', 1'090'519'040, alloc, inputs + 21).genericError) {

			for(U64 i = 0; i < 21; ++i)
				String_free(&inputs[i], alloc);

			printf("Failed unit test: SHA256 test failed! Couldn't allocate memory for SHA256 test 20!\n");
			return 8;
		}

		if(String_create('\x42', 1'610'612'798, alloc, inputs + 22).genericError) {

			for(U64 i = 0; i < 22; ++i)
				String_free(&inputs[i], alloc);

			printf("Failed unit test: SHA256 test failed! Couldn't allocate memory for SHA256 test 21!\n");
			return 9;
		}

		inputs[19] = String_createConstRefSized(inputs[21].ptr, 1'000'000);

	#endif

	//Validate inputs

	for(U64 i = 0; i < sizeof(results) / sizeof(results[0]); ++i) {

		U32 result[8];
		Buffer_sha256(String_buffer(inputs[i]), result);
	
		Bool b = false;
		Buffer_eq(Buffer_createRef(result, 32), Buffer_createRef((U32*)results[i], 32), &b);
	
		if(!b) {

			printf("Failed unit test: SHA256 test failed! (at offset %llu)\n", i);

			for(U64 j = 0; j < sizeof(results) / sizeof(results[0]); ++j)
				String_free(&inputs[j], alloc);

			return 6;
		}
	}

	for(U64 i = 0; i < sizeof(results) / sizeof(results[0]); ++i)
		String_free(&inputs[i], alloc);

	//

	F32 dt = (Time_now() - now) / (F32)seconds;

	printf("Successful unit test! After %fs\n", dt);
	return 0;
}