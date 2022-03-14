#include "formats/bmp.h"
#include "types/bit.h"
#include "types/assert.h"

#pragma pack(push, 1)

struct BMPHeader {
	u16 fileType;
	u32 fileSize;
	u16 r0, r1;
	u32 offsetData;
};

struct BMPInfoHeader {
	u32 size;
	i32 width, height;
	u16 planes, bitCount;
	u32 compression, compressedSize;
	i32 xPixPerM, yPixPerM;
	u32 colorsUsed, colorsImportant;
};

struct BMPColorHeader {
	u32 redMask, greenMask, blueMask, alphaMask;
	u32 colorSpaceType;
	u32 unused[16];
};

#pragma pack(pop)

const u16 BMP_magic = 0x4D42;
const u32 BMP_srgbMagic = 0x73524742;

struct Buffer BMP_writeRGBA(
	struct Buffer buf, u16 w, u16 h, bool isFlipped, 
	AllocFunc alloc, void *allocator
) {

	ocAssert(
		"BMP can only be up to 2GiB and should match dimensions", 
		buf.siz <= i32_MAX && buf.siz == (usz)w * h * 4
	);

	u32 headersSize = (u32) (
		sizeof(struct BMPHeader) + 
		sizeof(struct BMPInfoHeader) + 
		sizeof(struct BMPColorHeader)
	);

	struct BMPHeader header = (struct BMPHeader) {
		.fileType = BMP_magic,
		.fileSize = ((i32) buf.siz) * (isFlipped ? -1 : 1),
		.r0 = 0, .r1 = 0, 
		.offsetData = headersSize
	};

	struct BMPInfoHeader infoHeader = (struct BMPInfoHeader) {
		.size = sizeof(struct BMPInfoHeader),
		.width = w,
		.height = h,
		.planes = 1,
		.bitCount = 32,
		.compression = 3,		//rgba8
		.compressedSize = 0,
		.xPixPerM = 0, .yPixPerM = 0,
		.colorsUsed = 0, .colorsImportant = 0
	};

	struct BMPColorHeader colorHeader = (struct BMPColorHeader) {

		.redMask	= (u32) 0xFF << 16,
		.greenMask	= (u32) 0xFF << 8,
		.blueMask	= (u32) 0xFF,
		.alphaMask	= (u32) 0xFF << 24,

		.colorSpaceType = BMP_srgbMagic
	};

	struct Buffer file = Bit_bytes(
		headersSize + buf.siz,
		alloc,
		allocator
	);

	struct Buffer fileAppend = file;

	Bit_append(&fileAppend, &header, sizeof(header));
	Bit_append(&fileAppend, &infoHeader, sizeof(infoHeader));
	Bit_append(&fileAppend, &colorHeader, sizeof(colorHeader));
	Bit_appendBuffer(&fileAppend, buf);

	return file;
}