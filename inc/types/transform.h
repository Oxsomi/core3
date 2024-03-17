/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "types/quat.h"

//Transform contains how to go from one space to another
//A transform can also be an inverse transform, which is way faster to apply to go back to the space

//TODO: Perhaps a F64 transform

//We don't do matrices,
//This is faster and more memory efficient, also easier to implement
//48 bytes

typedef struct Transform {
	QuatF32 rot;
	F32x4 pos;
	F32x4 scale;
} Transform;

//More efficient transform, though we have to unpack it
//32 bytes; 2 per cache line

typedef struct PackedTransform {
	F32 pos[3];
	U32 quatXy;
	F32 scale[3];
	U32 quatZw;
} PackedTransform;

//Helper functions

PackedTransform Transform_pack(Transform t);
Transform PackedTransform_unpack(PackedTransform t);

Transform Transform_create(QuatF32 rot, F32x4 pos, F32x4 scale);

F32x4 Transform_applyToDirection(Transform t, F32x4 dir);		//Super fast, only need Quat
F32x4 Transform_apply(Transform t, F32x4 pos);					//Needs to do scale and translate too
F32x4 Transform_reverse(Transform t, F32x4 pos);				//Undo transformation

//2D transform
//16 bytes

typedef struct Transform2D {
	F32 rot, scale;
	F32x2 pos;
} Transform2D;

/*Transform2D Transform2D_create(F32x2 pos, F32 rotDeg, F32 scale);

F32x2 Transform2D_applyToDirection(Transform2D t, F32x2 dir);
F32x2 Transform2D_apply(Transform2D t, F32x2 pos);
F32x2 Transform2D_reverse(Transform2D t, F32x2 pos);*/

//Transform for pixel art games
//8 bytes
//Contains: 24 bit x, y
//			7 bit layer id
//			4 bit palette id
//			2 bit mirrored
//			1 bit valid

typedef U64 TilemapTransform;

typedef enum EMirrored {
	EMirrored_None = 0,
	EMirrored_X	 = 1 << 0,
	EMirrored_Y	 = 1 << 1
} EMirrored;

//TilemapTransform TilemapTransform_create(U32 x, U32 y, U8 layer, U8 paletteId, EMirrored flipped, ERotated rotated);

U32 TilemapTransform_x(TilemapTransform transform);
U32 TilemapTransform_y(TilemapTransform transform);
U8 TilemapTransform_layerId(TilemapTransform transform);
U8 TilemapTransform_paletteId(TilemapTransform transform);
EMirrored TilemapTransform_mirrored(TilemapTransform transform);
Bool TilemapTransform_isValid(TilemapTransform transform);

/*F32x2 TilemapTransform_applyToDirection(TilemapTransform t, F32x2 dir);
F32x2 TilemapTransform_apply(TilemapTransform t, F32x2 pos);
F32x2 TilemapTransform_reverse(TilemapTransform t, F32x2 pos);*/
