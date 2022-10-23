#pragma once
#include "types/pack.h"

//Transform contains how to go from one space to another
//A transform can also be an inverse transform, which is way faster to apply to go back to the space

//We don't do matrices,
//This is faster and more memory efficient, also easier to implement
//48 bytes

struct Transform {
	Quat rot;
	F32x4 pos;
	F32x4 scale;
};

//More efficient transform, though we have to unpack it
//32 bytes; 2 per cache line

struct PackedTransform {
	F32 pos[3];
	U32 quatXy;
	F32 scale[3];
	U32 quatZw;
};

//Helper functions

struct PackedTransform Transform_pack(struct Transform t);
struct Transform PackedTransform_unpack(struct PackedTransform t);

struct Transform Transform_create(Quat rot, F32x4 pos, F32x4 scale);

F32x4 Transform_applyToDirection(struct Transform t, F32x4 dir);		//Super fast, only need Quat
F32x4 Transform_apply(struct Transform t, F32x4 pos);					//Needs to do scale and translate too
F32x4 Transform_reverse(struct Transform t, F32x4 pos);					//Undo transformation

struct Transform Transform_mul(struct Transform parent, struct Transform child);
