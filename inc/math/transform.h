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

//2D transform
//16 bytes

struct Transform2D {
	F32 rot, scale;
	F32x2 pos;
};

struct Transform2D Transform2D_create(F32x2 pos, F32 rotDeg, F32 scale);

F32x2 Transform2D_applyToDirection(struct Transform2D t, F32x2 dir);
F32x2 Transform2D_apply(struct Transform2D t, F32x2 pos);
F32x2 Transform2D_reverse(struct Transform2D t, F32x2 pos);

//Transform for pixel art games
//8 bytes
//Contains: 24 bit x, y
//			7 bit layer id
//			4 bit palette id
//			2 bit mirrored
//			2 bit rotated
//			1 bit valid

typedef U64 TilemapTransform;

enum Mirrored {
	Mirrored_None = 0,
	Mirrored_X	 = 1 << 0,
	Mirrored_Y	 = 1 << 1
};

enum Rotated {
	Rotated_None,
	Rotated_90,
	Rotated_180,
	Rotated_270
};


TilemapTransform TilemapTransform_create(U32 x, U32 y, U8 layer, U8 paletteId, enum Mirrored flipped, enum Rotated rotated);

inline U32 TilemapTransform_x(TilemapTransform transform) { return (U32)transform & 0xFFFFFF; }
inline U32 TilemapTransform_y(TilemapTransform transform) { return (U32)(transform >> 24) & 0xFFFFFF; }
inline U8 TilemapTransform_layerId(TilemapTransform transform) { return (U8)(transform >> 48) & 0x7F; }
inline U8 TilemapTransform_paletteId(TilemapTransform transform) { return (U8)(transform >> 55) & 0xF; }
inline enum Mirrored TilemapTransform_mirrored(TilemapTransform transform) { return (enum Mirrored)((U8)(transform >> 59) & 0x3); }
inline enum Rotated TilemapTransform_rotated(TilemapTransform transform) { return (enum Rotated)((U8)(transform >> 61) & 0x3); }
inline Bool TilemapTransform_isValid(TilemapTransform transform) { return transform >> 63;  }

F32x2 TilemapTransform_applyToDirection(TilemapTransform t, F32x2 dir);
F32x2 TilemapTransform_apply(TilemapTransform t, F32x2 pos);
F32x2 TilemapTransform_reverse(TilemapTransform t, F32x2 pos);
