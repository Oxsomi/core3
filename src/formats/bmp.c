#include "formats/bmp.h"
#include "types/buffer.h"
#include "types/allocator.h"
#include "types/error.h"

#pragma pack(push, 1)

	typedef struct BMPHeader {
		U16 fileType;
		U32 fileSize;
		U16 r0, r1;
		U32 offsetData;
	} BMPHeader;

	typedef struct BMPInfoHeader {
		U32 size;
		I32 width, height;
		U16 planes, bitCount;
		U32 compression, compressedSize;
		I32 xPixPerM, yPixPerM;
		U32 colorsUsed, colorsImportant;
	} BMPInfoHeader;

	typedef struct BMPColorHeader {
		U32 redMask, greenMask, blueMask, alphaMask;
		U32 colorSpaceType;
		U32 unused[16];
	} BMPColorHeader;

#pragma pack(pop)

const U16 BMP_MAGIC = 0x4D42;
const U32 BMP_SRGB_MAGIC = 0x73524742;

Error BMP_writeRGBA(
	Buffer buf, U16 w, U16 h, Bool isFlipped, 
	Allocator allocator,
	Buffer *result
) {

	if(buf.length > I32_MAX || buf.length != (U64)w * h * 4)
		return Error_invalidParameter(0, 0, 0);

	U32 headersSize = (U32) (
		sizeof(BMPHeader) + 
		sizeof(BMPInfoHeader) + 
		sizeof(BMPColorHeader)
	);

	BMPHeader header = (BMPHeader) {
		.fileType = BMP_MAGIC,
		.fileSize = ((I32) buf.length) * (isFlipped ? -1 : 1),
		.r0 = 0, .r1 = 0, 
		.offsetData = headersSize
	};

	BMPInfoHeader infoHeader = (BMPInfoHeader) {
		.size = sizeof(BMPInfoHeader),
		.width = w,
		.height = h,
		.planes = 1,
		.bitCount = 32,
		.compression = 3		//rgba8
	};

	BMPColorHeader colorHeader = (BMPColorHeader) {

		.redMask	= (U32) 0xFF << 16,
		.greenMask	= (U32) 0xFF << 8,
		.blueMask	= (U32) 0xFF,
		.alphaMask	= (U32) 0xFF << 24,

		.colorSpaceType = BMP_SRGB_MAGIC
	};

	Buffer file = Buffer_createNull();

	Error err = Buffer_createUninitializedBytes(
		headersSize + buf.length,
		allocator,
		&file
	);

	if(err.genericError)
		return err;

	Buffer fileAppend = file;

	_gotoIfError(clean, Buffer_append(&fileAppend, &header, sizeof(header)));
	_gotoIfError(clean, Buffer_append(&fileAppend, &infoHeader, sizeof(infoHeader)));
	_gotoIfError(clean, Buffer_append(&fileAppend, &colorHeader, sizeof(colorHeader)));
	_gotoIfError(clean, Buffer_appendBuffer(&fileAppend, buf));

	*result = file;
	return Error_none();

clean:
	Buffer_free(&file, allocator);
	return err;
}
