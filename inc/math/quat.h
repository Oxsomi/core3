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

#pragma once
#include "vec.h"
#include "math.h"

typedef F32x4 Quat;

//Simple quaternion functions

Quat Quat_create(F32 x, F32 y, F32 z, F32 w);

Quat Quat_identity();

Quat Quat_conj(Quat q);
Quat Quat_normalize(Quat q);
Quat Quat_inverse(Quat q);

//Shortcuts

Bool Quat_eq(Quat a, Quat b);
Bool Quat_neq(Quat a, Quat b);

F32 Quat_x(Quat a);
F32 Quat_y(Quat a);
F32 Quat_z(Quat a);
F32 Quat_w(Quat a);
F32 Quat_get(Quat a, U8 i);

void Quat_setX(Quat *a, F32 v);
void Quat_setY(Quat *a, F32 v);
void Quat_setZ(Quat *a, F32 v);
void Quat_setW(Quat *a, F32 v);
void Quat_set(Quat *a, U8 i, F32 v);

Quat Quat_lerp(Quat a, Quat b, F32 perc);

//Helper funcs

Quat Quat_angleAxis(F32x4 axis, F32 angle);
Quat Quat_fromEuler(F32x4 pitchYawRollDeg);
F32x4 Quat_toEuler(Quat q);
Quat Quat_mul(Quat a, Quat b);
Quat Quat_targetDirection(F32x4 origin, F32x4 target);
F32x4 Quat_applyToNormal(Quat R, F32x4 P);

Quat Quat_slerp(Quat a, Quat b, F32 perc);

//inline Quat Quat_fromLookRotation(F32x4 fwd, F32x4 up);
