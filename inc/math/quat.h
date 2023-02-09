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
