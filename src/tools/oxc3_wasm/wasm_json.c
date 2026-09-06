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

//tools/oxc3_wasm/wasm_json.c

#include "tools/oxc3_wasm/wasm_bridge.h"
#include "types/container/log.h"
#include "platforms/platform.h"
#include "types/base/error.h"
#include <stdarg.h>

//Guard word in front of every allocation the page holds, so a pointer that never came from here (or one freed
// twice) is refused instead of handing a wrong length to the allocator.

#define WASM_ALLOC_MAGIC 0x4F784333574153ULL

typedef struct WasmAllocHeader {
	U64 magic;
	U64 total;              //Bytes of the whole allocation, this header included
} WasmAllocHeader;

void *Wasm_alloc(U64 size) {

	//Every allocation goes through the platform allocator rather than the C one, so a debug build's tracked
	// allocator still accounts for what the page holds.

	if(!Platform_instance)
		return NULL;

	//The header sits in front of the payload, so the page's pointer stays the payload and nothing on that side
	// has to know a header exists.

	U64 total = size + sizeof(WasmAllocHeader);

	if(total < size)        //A size that wraps would allocate less than asked and the payload would run past it
		return NULL;

	Buffer buf = Buffer_createNull();

	if (!Buffer_createUninitializedBytes(total, Platform_instance->alloc, &buf, NULL)) {
		Log_errorLn(Platform_instance->alloc, "Wasm_alloc() failed for %"PRIu64" bytes", total);
		return NULL;
	}

	WasmAllocHeader *header = (WasmAllocHeader*) buf.ptrNonConst;
	header->magic = WASM_ALLOC_MAGIC;
	header->total = total;

	return (void*)(header + 1);
}

void Wasm_free(void *ptr) {

	if(!ptr || !Platform_instance)
		return;

	WasmAllocHeader *header = ((WasmAllocHeader*) ptr) - 1;

	if(header->magic != WASM_ALLOC_MAGIC)
		return;

	U64 total = header->total;
	header->magic = 0;

	Buffer buf = Buffer_createManagedPtr(header, total);
	Buffer_free(&buf, Platform_instance->alloc);
}

void *Wasm_frame(CharString *json, Buffer *blob) {

	CharString jsonStr = json ? *json : CharString_createNull();
	Buffer blobBuf = blob ? *blob : Buffer_createNull();

	U64 jsonLength = CharString_length(jsonStr);
	U64 blobLength = Buffer_length(blobBuf);

	void *payload = Wasm_alloc(sizeof(WasmFrameHeader) + jsonLength + blobLength);

	if (payload) {

		WasmFrameHeader *header = (WasmFrameHeader*) payload;
		header->jsonLength = jsonLength;
		header->blobLength = blobLength;

		U8 *body = (U8*)(header + 1);

		Buffer_memcpy(Buffer_createRef(body, jsonLength), CharString_bufferConst(jsonStr));
		Buffer_memcpy(Buffer_createRef(body + jsonLength, blobLength), blobBuf);
	}

	//Freed whether or not the frame was built: the caller handed both parts over, so a failed allocation must
	// not also leak the document it was supposed to carry.

	if(json && Platform_instance)
		CharString_free(json, Platform_instance->alloc);

	if(blob && Platform_instance)
		Buffer_free(blob, Platform_instance->alloc);

	return payload;
}

void *Wasm_errorFrame(const C8 *message) {

	if(!Platform_instance)
		return NULL;

	CharString json = CharString_createNull();
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	gotoIfError3(clean, Json_raw(&json, "{\"error\":", Platform_instance->alloc, e_rr));
	gotoIfError3(clean, Json_cstr(&json, message ? message : "unknown error", Platform_instance->alloc, e_rr));
	gotoIfError3(clean, Json_raw(&json, "}", Platform_instance->alloc, e_rr));

clean:

	if(!s_uccess) {
		CharString_free(&json, Platform_instance->alloc);
		return NULL;
	}

	return Wasm_frame(&json, NULL);
}

void *Wasm_errorFrameFromError(const Error *err, const C8 *fallback) {

	//Error carries the string the failing call named itself, which is the only part of it a page can act on.

	if(err && err->errorStr)
		return Wasm_errorFrame(err->errorStr);

	return Wasm_errorFrame(fallback);
}

Bool Json_raw(CharString *out, const C8 *v, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!out || !v)
		retError(clean, Error_nullPointer(!out ? 0 : 1, "Json_raw()::out and v are required"));

	CharString ref = CharString_createRefCStrConst(v);
	gotoIfError3(clean, CharString_appendString(out, &ref, alloc, e_rr));

clean:
	return s_uccess;
}

Bool Json_fmt(CharString *out, const Allocator *alloc, Error *e_rr, const C8 *format, ...) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	if(!out || !format)
		retError(clean, Error_nullPointer(!out ? 0 : 3, "Json_fmt()::out and format are required"));

	va_list args;
	va_start(args, format);
	Bool formatted = CharString_formatVariadic(alloc, &tmp, e_rr, format, args);
	va_end(args);

	if(!formatted)
		retError(clean, Error_invalidState(0, "Json_fmt() couldn't format"));

	gotoIfError3(clean, CharString_appendString(out, &tmp, alloc, e_rr));

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

//Escapes what JSON requires and nothing else.
//Everything below 0x20 becomes \u00XX rather than being dropped, since a name or a diagnostic can carry one and
// a raw control byte would make the whole document unparseable on the other side.

Bool Json_str(CharString *out, CharString v, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!out)
		retError(clean, Error_nullPointer(0, "Json_str()::out is required"));

	gotoIfError3(clean, CharString_append(out, '"', alloc, e_rr));

	U64 length = CharString_length(v);

	for (U64 i = 0; i < length; ++i) {

		C8 c = v.ptr[i];

		switch (c) {

			case '"':  gotoIfError3(clean, Json_raw(out, "\\\"", alloc, e_rr));  break;
			case '\\': gotoIfError3(clean, Json_raw(out, "\\\\", alloc, e_rr));  break;
			case '\b': gotoIfError3(clean, Json_raw(out, "\\b", alloc, e_rr));   break;
			case '\f': gotoIfError3(clean, Json_raw(out, "\\f", alloc, e_rr));   break;
			case '\n': gotoIfError3(clean, Json_raw(out, "\\n", alloc, e_rr));   break;
			case '\r': gotoIfError3(clean, Json_raw(out, "\\r", alloc, e_rr));   break;
			case '\t': gotoIfError3(clean, Json_raw(out, "\\t", alloc, e_rr));   break;

			default:

				if((U8) c < 0x20) {
					gotoIfError3(clean, Json_fmt(out, alloc, e_rr, "\\u%04x", (U32)(U8) c));
					break;
				}

				gotoIfError3(clean, CharString_append(out, c, alloc, e_rr));
				break;
		}
	}

	gotoIfError3(clean, CharString_append(out, '"', alloc, e_rr));

clean:
	return s_uccess;
}

Bool Json_cstr(CharString *out, const C8 *v, const Allocator *alloc, Error *e_rr) {
	return Json_str(out, v ? CharString_createRefCStrConst(v) : CharString_createNull(), alloc, e_rr);
}

Bool Json_bool(CharString *out, Bool v, const Allocator *alloc, Error *e_rr) {
	return Json_raw(out, v ? "true" : "false", alloc, e_rr);
}

Bool Json_next(CharString *out, Bool *first, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!first)
		retError(clean, Error_nullPointer(1, "Json_next()::first is required"));

	if(!*first)
		gotoIfError3(clean, CharString_append(out, ',', alloc, e_rr));

	*first = false;

clean:
	return s_uccess;
}

Bool Json_key(CharString *out, const C8 *key, Bool *first, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, Json_next(out, first, alloc, e_rr));
	gotoIfError3(clean, Json_cstr(out, key, alloc, e_rr));
	gotoIfError3(clean, CharString_append(out, ':', alloc, e_rr));

clean:
	return s_uccess;
}
