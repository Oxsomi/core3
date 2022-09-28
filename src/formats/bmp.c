#include "formats/bmp.h"
#include "types/bit.h"
#include "types/allocator.h"

#pragma pack(push, 1)

struct BMPHeader {
	U16 fileType;
	U32 fileSize;
	U16 r0, r1;
	U32 offsetData;
};

struct BMPInfoHeader {
	U32 size;
	I32 width, height;
	U16 planes, bitCount;
	U32 compression, compressedSize;
	I32 xPixPerM, yPixPerM;
	U32 colorsUsed, colorsImportant;
};

struct BMPColorHeader {
	U32 redMask, greenMask, blueMask, alphaMask;
	U32 colorSpaceType;
	U32 unused[16];
};

#pragma pack(pop)

const U16 BMP_magic = 0x4D42;
const U32 BMP_srgbMagic = 0x73524742;

struct Error BMP_writeRGBA(
	struct Buffer buf, U16 w, U16 h, Bool isFlipped, 
	struct Allocator allocator,
	struct Buffer *result
) {

	if(buf.siz > I32_MAX || buf.siz != (U64) w * h * 4)
		return (struct Error) { .genericError = GenericError_InvalidParameter };

	U32 headersSize = (U32) (
		sizeof(struct BMPHeader) + 
		sizeof(struct BMPInfoHeader) + 
		sizeof(struct BMPColorHeader)
	);

	struct BMPHeader header = (struct BMPHeader) {
		.fileType = BMP_magic,
		.fileSize = ((I32) buf.siz) * (isFlipped ? -1 : 1),
		.r0 = 0, .r1 = 0, 
		.offsetData = headersSize
	};

	struct BMPInfoHeader infoHeader = (struct BMPInfoHeader) {
		.size = sizeof(struct BMPInfoHeader),
		.width = w,
		.height = h,
		.planes = 1,
		.bitCount = 32,
		.compression = 3		//rgba8
	};

	struct BMPColorHeader colorHeader = (struct BMPColorHeader) {

		.redMask	= (U32) 0xFF << 16,
		.greenMask	= (U32) 0xFF << 8,
		.blueMask	= (U32) 0xFF,
		.alphaMask	= (U32) 0xFF << 24,

		.colorSpaceType = BMP_srgbMagic
	};

	struct Buffer file = (struct Buffer) { 0 }; 

	struct Error err = Bit_createBytes(
		headersSize + buf.siz,
		allocator,
		&file
	);

	if(err.genericError)
		return err;

	struct Buffer fileAppend = file;
	*result = file;

	err = Bit_append(&fileAppend, &header, sizeof(header));

	if(err.genericError)
		return err;

	err = Bit_append(&fileAppend, &infoHeader, sizeof(infoHeader));

	if(err.genericError)
		return err;

	err = Bit_append(&fileAppend, &colorHeader, sizeof(colorHeader));

	if(err.genericError)
		return err;

	err = Bit_appendBuffer(&fileAppend, buf);

	if(err.genericError)
		return err;

	return Error_none();
}
