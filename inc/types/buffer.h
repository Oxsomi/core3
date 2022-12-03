#pragma once
#include "math/vec.h"

typedef struct Buffer Buffer;
typedef struct Allocator Allocator;

typedef struct BitRef {
	U8 *ptr, off;
} BitRef;

Bool BitRef_get(BitRef b);
void BitRef_set(BitRef b);
void BitRef_reset(BitRef b);

void BitRef_setTo(BitRef b, Bool v);

Error Buffer_getBit(Buffer buf, U64 offset, Bool *output);

void Buffer_copy(Buffer dst, Buffer src);
void Buffer_revCopy(Buffer dst, Buffer src);		//Copies bytes from range backwards; useful if ranges overlap

Error Buffer_setBit(Buffer buf, U64 offset);
Error Buffer_resetBit(Buffer buf, U64 offset);

Error Buffer_setBitTo(Buffer buf, U64 offset, Bool value);

Error Buffer_bitwiseOr(Buffer dst, Buffer src);
Error Buffer_bitwiseAnd(Buffer dst, Buffer src);
Error Buffer_bitwiseXor(Buffer dst, Buffer src);
Error Buffer_bitwiseNot(Buffer dst);

Error Buffer_setBitRange(Buffer dst, U64 dstOff, U64 bits);
Error Buffer_unsetBitRange(Buffer dst, U64 dstOff, U64 bits);

Error Buffer_setAllBits(Buffer dst);
Error Buffer_unsetAllBits(Buffer dst);

Error Buffer_setAllBitsTo(Buffer buf, Bool isOn);

//What hash functions are good for:
// 
//argon2id (Unsupported): 
//	Passwords (limit size (not too low) to avoid DDOS and use pepper if applicable)			TODO:
//	https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html
//	https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html
// 
//crc32c: Hashmaps / performance critical hashing / 
//	fast data integrity (encryption checksum / compression) when *NOT* dealing with adversaries
// 
//sha256: data integrity (encryption checksum / compression) when dealing with adversaries
//
//For more info: 
//	https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html

//CRC32 Castagnoli / iSCSI polynomial (0x82f63b78) not for ethernet/zip (0xedb88320)!

U32 Buffer_crc32c(Buffer buf);

void Buffer_sha256(Buffer buf, U32 output[8]);

//Comparison

Error Buffer_eq(Buffer buf0, Buffer buf1, Bool *result);		//Also compares size
Error Buffer_neq(Buffer buf0, Buffer buf1, Bool *result);		//Also compares size

//These should never be Buffer_free-d because Buffer doesn't know if it's allocated

Buffer Buffer_createNull();
Buffer Buffer_createRef(void *v, U64 length);

//All these functions allocate, so Buffer_free them later

Error Buffer_createCopy(Buffer buf, Allocator alloc, Buffer *output);
Error Buffer_createZeroBits(U64 length, Allocator alloc, Buffer *output);
Error Buffer_createOneBits(U64 length, Allocator alloc, Buffer *output);

Error Buffer_createBits(U64 length, Bool value, Allocator alloc, Buffer *result);

Bool Buffer_free(Buffer *buf, Allocator alloc);
Error Buffer_createEmptyBytes(U64 length, Allocator alloc, Buffer *output);

Error Buffer_createUninitializedBytes(U64 length, Allocator alloc, Buffer *output);
Error Buffer_createSubset(Buffer buf, U64 offset, U64 length, Buffer *output);

//Writing data

Error Buffer_offset(Buffer *buf, U64 length);
Error Buffer_append(Buffer *buf, const void *v, U64 length);
Error Buffer_appendBuffer(Buffer *buf, Buffer append);

Error Buffer_combine(Buffer a, Buffer b, Allocator alloc, Buffer *output);

Error Buffer_appendU64(Buffer *buf, U64 v);
Error Buffer_appendU32(Buffer *buf, U32 v);
Error Buffer_appendU16(Buffer *buf, U16 v);
Error Buffer_appendU8(Buffer *buf,  U8 v);
Error Buffer_appendC8(Buffer *buf,  C8 v);

Error Buffer_appendI64(Buffer *buf, I64 v);
Error Buffer_appendI32(Buffer *buf, I32 v);
Error Buffer_appendI16(Buffer *buf, I16 v);
Error Buffer_appendI8(Buffer *buf,  I8 v);

Error Buffer_appendF32(Buffer *buf, F32 v);

Error Buffer_appendF32x4(Buffer *buf, F32x4 v);
Error Buffer_appendF32x2(Buffer *buf, I32x2 v);
Error Buffer_appendI32x4(Buffer *buf, I32x4 v);
Error Buffer_appendI32x2(Buffer *buf, I32x2 v);
