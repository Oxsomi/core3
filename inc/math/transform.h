#pragma once
#include "quat.h"

//Transform contains how to go from one space to another
//A transform can also be an inverse transform, which is way faster to apply to go back to the space

//We don't do matrices,
//This is faster and more memory efficient, also easier to implement
//48 bytes

struct Transform {
	quat rot;
	f32x4 pos;
	f32x4 scale;
};

//More efficient transform, though we have to unpack it
//32 bytes; 2 per cache line

struct PackedTransform {
	struct quat16 rot;
	f32 pos[3];
	f32 scale[3];
};

//Helper functions

struct PackedTransform Transform_Pack(struct Transform t);
struct Transform PackedTransform_Unpack(struct PackedTransform t);

struct Transform Transform_init(quat rot, f32x4 pos, f32x4 scale);

f32x4 Transform_applyToDirection(struct Transform t, f32x4 dir);		//Super fast, only need quat
f32x4 Transform_apply(struct Transform t, f32x4 pos);					//Needs to do scale and translate too
f32x4 Transform_reverse(struct Transform t, f32x4 pos);					//Undo transformation

struct Transform Transform_mul(struct Transform parent, struct Transform child);