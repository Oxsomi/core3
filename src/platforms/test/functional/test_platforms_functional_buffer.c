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

//platforms/test/functional/test_platforms_functional_buffer.c
//
//F1.  CPU buffer -> screen       (draw a colour gradient, verify buffer content)
//F6.  Window_storeCPUBufferToDisk

#include "test_platforms_functional_shared.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "platforms/window_manager.h"
#include "types/test/test.h"
#include "types/container/ref_ptr.h"
#include "formats/dds/dds_file.h"

// -- F1. CPU buffer -> screen ---------------------------------------------------
//
//Gradient is written from onDraw (driven by WM_PAINT / the Wayland frame cycle),
//not by the test body. The test only flips a flag in F1State and waits for the
//callback to have actually run before inspecting the buffer.

typedef struct F1State {
	volatile Bool drawn;
	U8 zxor;
} F1State;

static F1State f1;

static void F1_onDraw(Window *w) {

	U32 W = (U32) I32x2_x(w->size);
	U32 H = (U32) I32x2_y(w->size);

	U8 *px = w->cpuVisibleBuffer.ptrNonConst;

	if (!px)
		return;

	for (U32 y = 0; y < H; ++y)
		for (U32 x = 0; x < W; ++x) {
			U8 *p = px + (y * W + x) * 4;
			p[0] = (U8)x;                  //R gradient
			p[1] = (U8)y;                  //G gradient
			p[2] = 128 ^ f1.zxor;          //B constant baseline
			p[3] = 255;                    //A
		}

	f1.drawn = true;
}

static Bool waitForDraw(volatile Bool *drawn, Ns timeout) {

	*drawn = false;

	Ns waited = 0;
	while (!*drawn && waited < timeout) {
		WindowManager_step(&windowManager, NULL, NULL);
		Thread_sleep(16 * MS);
		waited += 16 * MS;
	}

	return *drawn;
}

static void Test_cpuBuffer(Test *t) {

	Test_setModule(t, "F1/CPUBuffer");

	f1 = (F1State) { 0 };

	WindowCallbacks cbs = (WindowCallbacks){ 0 };
	cbs.onDraw = F1_onDraw;

	I32x2 sz = I32x2_create2(256, 256);

	Window *w = createWindowCallback(t, "F1: CPU Buffer", I32x2_zero, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	sz = w->size;

	U32 W = (U32) I32x2_x(sz);
	U32 H = (U32) I32x2_y(sz);
	U64 expectedBytes = (U64)W * H * 4;

	Test_assert(t, "bufferSize", Buffer_length(w->cpuVisibleBuffer) == expectedBytes);

	present(t, w);

	if (w->type == EWindowType_Physical)
		Test_assert(t, "onDrawFired", waitForDraw(&f1.drawn, 1 * SECOND));

	else F1_onDraw(w);   //Virtual windows have no paint loop to drive onDraw; call directly.

	U8 *px = w->cpuVisibleBuffer.ptrNonConst;
	Test_assert(t, "has px", px != NULL);

	if (!px)
		goto clean;

	U8 *tl = px;
	U8 *tr = px + (W - 1) * 4;
	U8 *bl = px + (H - 1) * W * 4;
	U8 *br = px + ((H - 1) * W + (W - 1)) * 4;

	Test_assert(t, "topLeft_R",  tl[0] == 0);
	Test_assert(t, "topLeft_G",  tl[1] == 0);
	Test_assert(t, "topLeft_B",  tl[2] == 128);
	Test_assert(t, "topRight_R", tr[0] == (U8)(W - 1));
	Test_assert(t, "topRight_G", tr[1] == 0);
	Test_assert(t, "botLeft_R",  bl[0] == 0);
	Test_assert(t, "botLeft_G",  bl[1] == (U8)(H - 1));
	Test_assert(t, "botRight_R", br[0] == (U8)(W - 1));
	Test_assert(t, "botRight_G", br[1] == (U8)(H - 1));

	pump(VISUAL_HOLD_NS);

clean:
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

// -- F6. Window_storeCPUBufferToDisk -------------------------------------------
//
//Fills the buffer with a deterministic pattern from onDraw, then stores it to
//a DDS file and reads it back to verify pixels round-trip correctly.

typedef struct F6State {
	volatile Bool drawn;
} F6State;

static F6State f6;

static void F6_onDraw(Window *w) {

	U32 W = (U32) I32x2_x(w->size);
	U32 H = (U32) I32x2_y(w->size);
	U8 *px = w->cpuVisibleBuffer.ptrNonConst;

	if (!px)
		return;

	//pixel i -> R = (i & 0xFF), G = ((i >> 1) & 0xFF), B = 42, A = 255
	for (U32 i = 0; i < W * H; ++i) {
		px[i * 4 + 0] = (U8)(i & 0xFF);
		px[i * 4 + 1] = (U8)((i >> 1) & 0xFF);
		px[i * 4 + 2] = 42;
		px[i * 4 + 3] = 255;
	}

	f6.drawn = true;
}

static void Test_storeCPUBuffer(Test *t) {

	Test_setModule(t, "F6/StoreCPUBufferToDisk");

	f6 = (F6State){ 0 };

	const Allocator *alloc = Platform_instance->alloc;

	I32x2 sz = I32x2_create2(512, 512);

	WindowCallbacks cbs = (WindowCallbacks){ 0 };
	cbs.onDraw = F6_onDraw;

	Window *w = createWindowCallback(t, "F6: StoreToDisk", I32x2_zero, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs);

	if (w)
		sz = w->size;

	U32 W = (U32)I32x2_x(sz), H = (U32)I32x2_y(sz);

	ListSubResourceData subResources = (ListSubResourceData){ 0 };
	StreamRef *readStream = NULL;

	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType streamType = FileStream_makeType(alloc);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	present(t, w);

	if (w->type == EWindowType_Physical)
		Test_assert(t, "onDrawFired", waitForDraw(&f6.drawn, 1 * SECOND));

	else F6_onDraw(w);

	CharString outPath = CharString_createRefCStrConst("platform_test_cpu_dump.dds");
	File_remove(&outPath, 1 * SECOND, alloc, NULL);

	if (!Test_assert(t, "storeToDisk", Window_storeCPUBufferToDisk(w, outPath, 50 * MS, alloc, &t->err)))
		goto clean;

	pump(VISUAL_HOLD_NS);

	Test_assert(t, "fileExists", File_hasFile(&outPath, alloc));

	//Open the DDS file as a stream and read it back

	if (!Test_assert(t, "openStream", File_openStream(
		&outPath,
		50 * MS,
		EFileOpenType_Read,
		false,
		&fileHandleType,
		&streamType,
		&readStream,
		&t->err
	)))
		goto clean;

	//DDS_read: verify header (DDSInfo)

	DDSInfo info = (DDSInfo) { 0 };
	U64 streamOff = 0;

	if (!Test_assert(t, "ddsRead", DDS_read(readStream, &streamOff, &info, alloc, &subResources, &t->err)))
		goto clean;

	Test_assert(t, "ddsW",      info.w == W);
	Test_assert(t, "ddsH",      info.h == H);
	Test_assert(t, "ddsMips",   info.mips == 1);
	Test_assert(t, "ddsLayers", info.layers == 1);

	//Window_storeCPUBufferToDisk maps RGBA8 -> BGRA8 in the DDSInfo
	//(see the switch in the implementation; RGBA8 falls through to the default BGRA8 case).
	Test_assert(t, "ddsFormat",
		info.textureFormatId == ETextureFormatId_BGRA8 ||
		info.textureFormatId == ETextureFormatId_RGBA8
	);

	//Pixel spot-check via the sub-resource stream
	//DDS_read returns one SubResourceData per mip/layer.
	// For a 512x 1-mip 1-layer image there is exactly one entry.

	Test_assert(t, "oneSubResource", subResources.length == 1);

	if (subResources.length >= 1) {

		const SubResourceData *sr = subResources.ptr;
		U64 pixelBytes = 4;   //BGRA8 / RGBA8, 4 bytes per pixel

		//Read pixel 0 (top-left) from the stream at sr->streamOff
		U32 px0 = 0;
		Buffer px0Buf = Buffer_createRef(&px0, sizeof(px0));
		OxStream *stream = RefPtr_data(sr->stream, OxStream);

		//We wrote R = 0,G = 0,B = 42,A = 255 at pixel 0.
		//storeCPUBufferToDisk maps the window format to BGRA8 by default,
		//so on-disk order is B,G,R,A -> 42,0,0,255.
		//If the format stayed RGBA8 the order is R,G,B,A -> 0,0,42,255.
		//Accept either

		if (Test_assert(t, "readPx0", stream->read(stream, sr->streamOff, sizeof(px0), px0Buf, alloc, &t->err)))
			Test_assert(t, "px0", px0 == 0xFF2A0000 || px0 == 0xFF00002A);

		//Read pixel 1 and verify R is 1 (in either BGR or RGB)

		U32 px1 = 0;
		Buffer px1Buf = Buffer_createRef(&px1, sizeof(px1));
		if (Test_assert(t, "readPx1", stream->read(stream, sr->streamOff + pixelBytes, sizeof(px1), px1Buf, alloc, &t->err)))
			Test_assert(t, "px1", px1 == 0xFF2A0001 || px1 == 0xFF01002A);

		//Verify the reported stream length covers the full image
		U64 expectedBytes = (U64)W * H * pixelBytes;
		Test_assert(t, "streamLen", sr->streamLen == expectedBytes);
	}

	File_remove(&outPath, 1 * SECOND, alloc, NULL);

clean:
	ListSubResourceData_freeUnderlying(&subResources, alloc);
	RefPtr_dec(&readStream);
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

void Test_functionalBuffer(Test *t) {
	Test_cpuBuffer(t);
	Test_storeCPUBuffer(t);
}
