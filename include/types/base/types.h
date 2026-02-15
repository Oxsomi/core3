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

#pragma once
#include <stdint.h>
#include <stdbool.h>

//Null is apparently non-standard

#ifndef NULL
	#define NULL ((void*)0)
#endif

//Version

#define OXC3_MAKE_VERSION(major, minor, patch) (((major) << 22) | ((minor) << 12) | (patch))

#define OXC3_GET_MAJOR(v) (((U32)(v)) >> 22)
#define OXC3_GET_MINOR(v) (((U32)(v)) << 10 >> 22)
#define OXC3_GET_PATCH(v) (((U32)(v)) << 20 >> 20)

#define OXC3_MAJOR 0
#define OXC3_MINOR 2
#define OXC3_PATCH 96
#define OXC3_VERSION OXC3_MAKE_VERSION(OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH)

//A hint to show that something is implementation dependent
//For ease of implementing a new implementation
//These should never be directly called by anyone else than the main library the impl is for.
#define impl

//Types

typedef int8_t  I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

typedef float F32;
typedef double F64;

typedef U64 Ns;		//Time since Unix epoch in Ns
typedef I64 DNs;	//Delta Ns

typedef char C8;

typedef bool Bool;

#ifndef _MSC_VER
	#define MIGHT_BE_UNUSED __attribute__((unused))
	#define __forceinline__ __attribute__((always_inline)) inline
#else
	#define MIGHT_BE_UNUSED
	#define __forceinline__ __forceinline
#endif
