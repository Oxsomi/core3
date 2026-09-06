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

//tools/oxc3_wasm/wasm_bridge.h

#pragma once
//The base headers define CharString and Buffer themselves; the container ones are what the .c files that
//actually manipulate them include.
#include "types/base/string_base.h"
#include "types/base/buffer_base.h"
#include "types/base/error.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Only ever a pointer here, so the definition stays in types/base/allocator.h
typedef struct Allocator Allocator;

typedef struct SHFile SHFile;
typedef struct SRFile SRFile;
typedef struct SPFile SPFile;

//Every call across the boundary answers with one allocation, so the page frees exactly one thing per call
// whatever the call produced:
//
//    U64 jsonLen; U64 blobLen; C8 json[jsonLen]; U8 blob[blobLen]
//
//The two lengths are read first and the payloads are sliced out of the heap, which keeps binaries out of the
// JSON entirely; base64 in a field would cost a third more bytes and an encode plus a decode of every SPIR-V
// module the page ever looks at.
//The allocation carries its own size in a header before the pointer, so Wasm_free needs nothing but the
// pointer the page was handed.

typedef struct WasmFrameHeader {
	U64 jsonLength;
	U64 blobLength;
} WasmFrameHeader;

//Payload of `size` bytes, or NULL when there is no platform yet or the allocation failed.
//Both directions use this: an input buffer the page fills before a call, and the frames below.

void *Wasm_alloc(U64 size);
void Wasm_free(void *ptr);

//Packs one frame; both parts are optional and are freed here whether or not packing succeeds, so a caller
// hands over ownership and is done.
//NULL means the frame could not be allocated, which the page reports as a module failure.

void *Wasm_frame(CharString *json, Buffer *blob);

//A frame carrying nothing but {"error": "..."}, for the paths that fail before they have a document.

void *Wasm_errorFrame(const C8 *message);

//A frame carrying {"error": "..."} built from an Error, so the reason the C side gave reaches the page
// rather than a generic failure.

void *Wasm_errorFrameFromError(const Error *err, const C8 *fallback);

//JSON writing.
//`first` tracks whether a separator is due inside the object or array being written; Json_key and Json_next
// consume and clear it.

Bool Json_raw(CharString *out, const C8 *v, const Allocator *alloc, Error *e_rr);
Bool Json_fmt(CharString *out, const Allocator *alloc, Error *e_rr, const C8 *format, ...);
Bool Json_str(CharString *out, CharString v, const Allocator *alloc, Error *e_rr);
Bool Json_cstr(CharString *out, const C8 *v, const Allocator *alloc, Error *e_rr);
Bool Json_bool(CharString *out, Bool v, const Allocator *alloc, Error *e_rr);
Bool Json_key(CharString *out, const C8 *key, Bool *first, const Allocator *alloc, Error *e_rr);
Bool Json_next(CharString *out, Bool *first, const Allocator *alloc, Error *e_rr);

//Serializers onto the document contracts the page consumes (documented at the top of web/js/api.js).
//They mirror the C structs, so each is a walk rather than a translation.

Bool WasmJson_shFile(
	const SHFile *file,
	CharString name,
	CharString sourceName,
	CharString *out,
	const Allocator *alloc,
	Error *e_rr
);

Bool WasmJson_srFile(
	const SRFile *file,
	CharString name,
	CharString sourceName,
	CharString *out,
	const Allocator *alloc,
	Error *e_rr
);

Bool WasmJson_spFile(
	const SPFile *file,
	CharString name,
	CharString sourceName,
	CharString *out,
	const Allocator *alloc,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
