/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#pragma once
#include "math/quat.h"

//Transform contains how to go from one space to another
//A transform can also be an inverse transform, which is way faster to apply to go back to the space

//We don't do matrices,
//This is faster and more memory efficient, also easier to implement
//48 bytes

typedef struct Transform {
	Quat rot;
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

Transform Transform_create(Quat rot, F32x4 pos, F32x4 scale);

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
//			2 bit rotated
//			1 bit valid

typedef U64 TilemapTransform;

typedef enum EMirrored {
	EMirrored_None = 0,
	EMirrored_X	 = 1 << 0,
	EMirrored_Y	 = 1 << 1
} EMirrored;

typedef enum ERotated {
	ERotated_None,
	ERotated_90,
	ERotated_180,
	ERotated_270
} ERotated;


//TilemapTransform TilemapTransform_create(U32 x, U32 y, U8 layer, U8 paletteId, EMirrored flipped, ERotated rotated);

U32 TilemapTransform_x(TilemapTransform transform);
U32 TilemapTransform_y(TilemapTransform transform);
U8 TilemapTransform_layerId(TilemapTransform transform);
U8 TilemapTransform_paletteId(TilemapTransform transform);
EMirrored TilemapTransform_mirrored(TilemapTransform transform);
ERotated TilemapTransform_rotated(TilemapTransform transform);
Bool TilemapTransform_isValid(TilemapTransform transform);

/*F32x2 TilemapTransform_applyToDirection(TilemapTransform t, F32x2 dir);
F32x2 TilemapTransform_apply(TilemapTransform t, F32x2 pos);
F32x2 TilemapTransform_reverse(TilemapTransform t, F32x2 pos);*/
