#include "math/transform.h"

struct PackedTransform Transform_pack(struct Transform t) {
	return (struct PackedTransform) {
		Quat_pack(t.rot),
		{ Vec_x(t.pos), Vec_y(t.pos), Vec_z(t.pos) },
		{ Vec_x(t.scale), Vec_y(t.scale), Vec_z(t.scale) }
	};
}

struct Transform PackedTransform_unpack(struct PackedTransform t) {
	return (struct Transform) {
		Quat_unpack(t.rot),
		Vec_load3(t.pos),
		Vec_load3(t.scale)
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
	F32x4 mpos = Vec_mul(pos, t.scale);			//Scale
	mpos = Quat_applyToNormal(t.rot, mpos);		//Rotate
	return Vec_add(mpos, t.pos);				//Translate
}

F32x4 Transform_reverse(struct Transform t, F32x4 pos) {
	F32x4 mpos = Vec_sub(pos, t.pos);
	mpos = Quat_applyToNormal(Quat_inverse(t.rot), mpos);
	return Vec_div(mpos, t.scale);
}

struct Transform Transform_mul(struct Transform parent, struct Transform child) {
	return (struct Transform) {
		Quat_mul(parent.rot, child.rot),
		Transform_apply(parent, child.pos),
		Vec_mul(parent.scale, child.scale)
	};
}