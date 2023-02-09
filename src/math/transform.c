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

#include "math/transform.h"
#include "types/pack.h"

//3D transform

PackedTransform Transform_pack(Transform t) {

	Quat16 q16 = Quat_pack(t.rot);

	return (PackedTransform) {

		{ F32x4_x(t.pos), F32x4_y(t.pos), F32x4_z(t.pos) },
		*(const U32*) &q16.arr[0],

		{ F32x4_x(t.scale), F32x4_y(t.scale), F32x4_z(t.scale) },
		*(const U32*) &q16.arr[2]
	};
}

Transform PackedTransform_unpack(PackedTransform t) {

	Quat16 q16;
	*(U32*) &q16.arr[0] = t.quatXy;
	*(U32*) &q16.arr[2] = t.quatZw;

	return (Transform) {
		Quat_unpack(q16),
		F32x4_load3(t.pos),
		F32x4_load3(t.scale)
	};
}

Transform Transform_create(Quat rot, F32x4 pos, F32x4 scale) {
	return (Transform) {
		.rot = rot,
		.pos = pos,
		.scale = scale
	};
}

F32x4 Transform_applyToDirection(Transform t, F32x4 dir) {
	return Quat_applyToNormal(t.rot, dir);
}

F32x4 Transform_apply(Transform t, F32x4 pos) {
	F32x4 mpos = F32x4_mul(pos, t.scale);		//Scale
	mpos = Quat_applyToNormal(t.rot, mpos);		//Rotate
	return F32x4_add(mpos, t.pos);				//Translate
}

F32x4 Transform_reverse(Transform t, F32x4 pos) {
	F32x4 mpos = F32x4_sub(pos, t.pos);
	mpos = Quat_applyToNormal(Quat_inverse(t.rot), mpos);
	return F32x4_div(mpos, t.scale);
}

U32 TilemapTransform_x(TilemapTransform transform) { return (U32)transform & 0xFFFFFF; }
U32 TilemapTransform_y(TilemapTransform transform) { return (U32)(transform >> 24) & 0xFFFFFF; }
U8 TilemapTransform_layerId(TilemapTransform transform) { return (U8)(transform >> 48) & 0x7F; }
U8 TilemapTransform_paletteId(TilemapTransform transform) { return (U8)(transform >> 55) & 0xF; }
EMirrored TilemapTransform_mirrored(TilemapTransform transform) { return (EMirrored)((U8)(transform >> 59) & 0x3); }
ERotated TilemapTransform_rotated(TilemapTransform transform) { return (ERotated)((U8)(transform >> 61) & 0x3); }
Bool TilemapTransform_isValid(TilemapTransform transform) { return transform >> 63; }
