#pragma once
#include "types/string.h"

typedef enum EXXCompressionType {
	EXXCompressionType_None,							//--uncompressed
	EXXCompressionType_Brotli11,						//(default)
	EXXCompressionType_Brotli1,							//--fast-compress
	EXXCompressionType_Count
} EXXCompressionType;

typedef enum EXXEncryptionType {
	EXXEncryptionType_None,								//(default)
	EXXEncryptionType_AES256GCM,						//--aes <32-byte key (in hex or nyto)>
	EXXEncryptionType_Count
} EXXEncryptionType;

typedef enum EXXDataSizeType {		//Can be represented as a 2-bit array for example
	EXXDataSizeType_U8,
	EXXDataSizeType_U16,
	EXXDataSizeType_U32,
	EXXDataSizeType_U64
} EXXDataSizeType;

static const U8 SIZE_BYTE_TYPE[4] = { 1, 2, 4, 8 };
