/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/types.h"

typedef struct CharString CharString;
typedef struct Allocator Allocator;
typedef struct BufferLayout BufferLayout;
typedef struct Error Error;

//JSON has to be converted to and from BufferLayout.
//Because JSON is not strong typed, it will allow you to 
//pass a buffer layout it should align to.

// 
//The following: [ 123, "hi" ] would be turned into a 
//struct with member 0 as F64=123, member 1 as C8[2]="hi".
//While [ 123, 12 ] would turn into a real array F64[].
// 
//If the format is known, it should pass a bufferlayout that should be used.
//For example: U8[] is assumed then [ 123, 12 ] would turn into 2 bytes, 
//while by default it would be 16 otherwise.
//
//To force arrays to be the same size as input buffer l

Error JSON_deserialize(
	CharString string, 
	const BufferLayout *alignToLayout, 
	Allocator alloc, 
	Buffer *output,
	BufferLayout *layout
);

//JSON serialize loses type info, to avoid this, 
//ensure the BufferLayout is used as the alignToLayout in deserialize.
//For example F32x4{ 1, 2, 3, 4 } turns into [ 1, 2, 3, 4 ].
//It'll try to safe cast this simplified layout (F64[]) to our real layout deserialization time (F32x4).
//If any of the F64s turn into NaN or Inf or can't be represented, it will return an error. 
//Since 64-bit numbers can't be declared without breaking support for other json parses, 
//serializing a 52+ bit int will give an out of bounds exception.
//The solution would be to store two ints or not use JSON for that.

Error JSON_serialize(
	Buffer buffer,
	BufferLayout layout,
	CharString path,
	Bool prettify,
	Bool useSpaces,
	U8 indentPerStage,		//For example if you want 2 spaces per tab then set useSpaces and set this to 2.
	Allocator alloc, 
	CharString *serialized
);
