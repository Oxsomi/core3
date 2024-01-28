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

#include "types/transform.h"
#include "types/pack.h"

//3D transform

PackedTransform Transform_pack(Transform t) {

	QuatS16 q16 = QuatF32_pack(t.rot);

	return (PackedTransform) {

		{ F32x4_x(t.pos), F32x4_y(t.pos), F32x4_z(t.pos) },
		*(const U32*) &q16.arr[0],

		{ F32x4_x(t.scale), F32x4_y(t.scale), F32x4_z(t.scale) },
		*(const U32*) &q16.arr[2]
	};
}

Transform PackedTransform_unpack(PackedTransform t) {

	QuatS16 q16;
	*(U32*) &q16.arr[0] = t.quatXy;
	*(U32*) &q16.arr[2] = t.quatZw;

	return (Transform) {
		QuatF32_unpack(q16),
		F32x4_load3(t.pos),
		F32x4_load3(t.scale)
	};
}

Transform Transform_create(QuatF32 rot, F32x4 pos, F32x4 scale) {
	return (Transform) {
		.rot = rot,
		.pos = pos,
		.scale = scale
	};
}

F32x4 Transform_applyToDirection(Transform t, F32x4 dir) {
	return QuatF32_applyToNormal(t.rot, dir);
}

F32x4 Transform_apply(Transform t, F32x4 pos) {
	F32x4 mpos = F32x4_mul(pos, t.scale);		//Scale
	mpos = QuatF32_applyToNormal(t.rot, mpos);	//Rotate
	return F32x4_add(mpos, t.pos);				//Translate
}

F32x4 Transform_reverse(Transform t, F32x4 pos) {
	F32x4 mpos = F32x4_sub(pos, t.pos);
	mpos = QuatF32_applyToNormal(QuatF32_inverse(t.rot), mpos);
	return F32x4_div(mpos, t.scale);
}

U32 TilemapTransform_x(TilemapTransform transform) { return (U32)transform & 0xFFFFFF; }
U32 TilemapTransform_y(TilemapTransform transform) { return (U32)(transform >> 24) & 0xFFFFFF; }
U8 TilemapTransform_layerId(TilemapTransform transform) { return (U8)(transform >> 48) & 0x7F; }
U8 TilemapTransform_paletteId(TilemapTransform transform) { return (U8)(transform >> 55) & 0xF; }
EMirrored TilemapTransform_mirrored(TilemapTransform transform) { return (EMirrored)((U8)(transform >> 59) & 0x3); }
ERotated TilemapTransform_rotated(TilemapTransform transform) { return (ERotated)((U8)(transform >> 61) & 0x3); }
Bool TilemapTransform_isValid(TilemapTransform transform) { return transform >> 63; }
