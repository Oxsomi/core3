#include "math/transform.h"

//3D transform

struct PackedTransform Transform_pack(struct Transform t) {

	struct Quat16 q16 = Quat_pack(t.rot);

	return (struct PackedTransform) {

		{ F32x4_x(t.pos), F32x4_y(t.pos), F32x4_z(t.pos) },
		*(const U32*) &q16.arr[0],

		{ F32x4_x(t.scale), F32x4_y(t.scale), F32x4_z(t.scale) },
		*(const U32*) &q16.arr[2]
	};
}

struct Transform PackedTransform_unpack(struct PackedTransform t) {

	struct Quat16 q16;
	*(U32*) &q16.arr[0] = t.quatXy;
	*(U32*) &q16.arr[2] = t.quatZw;

	return (struct Transform) {
		Quat_unpack(q16),
		F32x4_load3(t.pos),
		F32x4_load3(t.scale)
	};
}

struct Transform Transform_create(Quat rot, F32x4 pos, F32x4 scale) {
	return (struct Transform) {
		.rot = rot,
		.pos = pos,
		.scale = scale
	};
}

F32x4 Transform_applyToDirection(struct Transform t, F32x4 dir) {
	return Quat_applyToNormal(t.rot, dir);
}

F32x4 Transform_apply(struct Transform t, F32x4 pos) {
	F32x4 mpos = F32x4_mul(pos, t.scale);		//Scale
	mpos = Quat_applyToNormal(t.rot, mpos);		//Rotate
	return F32x4_add(mpos, t.pos);				//Translate
}

F32x4 Transform_reverse(struct Transform t, F32x4 pos) {
	F32x4 mpos = F32x4_sub(pos, t.pos);
	mpos = Quat_applyToNormal(Quat_inverse(t.rot), mpos);
	return F32x4_div(mpos, t.scale);
}

//2D transform

F32x2 Transform2D_applyToDirection(struct Transform2D t, F32x2 dir);
F32x2 Transform2D_apply(struct Transform2D t, F32x2 pos);
F32x2 Transform2D_reverse(struct Transform2D t, F32x2 pos);

//2D tilemap

F32x2 TilemapTransform_applyToDirection(TilemapTransform t, F32x2 dir);
F32x2 TilemapTransform_apply(TilemapTransform t, F32x2 pos);
F32x2 TilemapTransform_reverse(TilemapTransform t, F32x2 pos);