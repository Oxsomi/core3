#include "math/transform.h"

struct PackedTransform Transform_Pack(struct Transform t) {
	return (struct PackedTransform) {
		Quat_pack(t.rot),
		{ f32x4_x(t.pos), f32x4_y(t.pos), f32x4_z(t.pos) },
		{ f32x4_x(t.scale), f32x4_y(t.scale), f32x4_z(t.scale) }
	};
}

struct Transform PackedTransform_Unpack(struct PackedTransform t) {
	return (struct Transform) {
		Quat_unpack(t.rot),
		f32x4_load3(t.pos),
		f32x4_load3(t.scale)
	};
}

struct Transform Transform_init(quat rot, f32x4 pos, f32x4 scale) {
	return (struct Transform) {
		.rot = rot,
		.pos = pos,
		.scale = scale
	};
}

f32x4 Transform_applyToDirection(struct Transform t, f32x4 dir) {
	return Quat_applyToNormal(t.rot, dir);
}

f32x4 Transform_apply(struct Transform t, f32x4 pos) {
	f32x4 mpos = f32x4_mul(pos, t.scale);			//Scale
	mpos = Quat_applyToNormal(t.rot, mpos);		//Rotate
	return f32x4_add(mpos, t.pos);				//Translate
}

f32x4 Transform_reverse(struct Transform t, f32x4 pos) {
	f32x4 mpos = f32x4_sub(pos, t.pos);
	mpos = Quat_applyToNormal(Quat_inverse(t.rot), mpos);
	return f32x4_div(mpos, t.scale);
}

struct Transform Transform_mul(struct Transform parent, struct Transform child) {
	return (struct Transform) {
		Quat_mul(parent.rot, child.rot),
		Transform_apply(parent, child.pos),
		f32x4_mul(parent.scale, child.scale)
	};
}