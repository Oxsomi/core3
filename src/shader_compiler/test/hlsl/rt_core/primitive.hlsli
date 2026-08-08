/* OxC3/RT Core(Oxsomi core 3/RT Core), a general framework for raytracing applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "@resources.hlsli"
#include "ray_basics.hlsli"

//Sphere intersection

struct Sphere {

	F32x3 pos;
	F32 rad;

	static Sphere create(F32x3 pos, F32 rad) {
		Sphere s = { pos, rad };
		return s;
	}

	Bool intersects(RayDesc ray, out F32x3 outT, out Bool isBackface) {

		//Fix to increase sphere precision.
		//Check raytracing gems I: Chapter 7
		// (https://www.realtimerendering.com/raytracinggems/unofficial_RayTracingGems_v1.4.pdf#0004286892.INDD%3AAnchor%2018%3A18)
		//and https://iquilezles.org/articles/intersectors/

		F32x3 dif = ray.Origin - pos;
		F32 b = dot(-dif, ray.Direction);

		F32x3 qc = dif + ray.Direction * b;

		F32 rad2 = rad * rad;
		F32 D = rad2 - dot(qc, qc);

		outT = -1.xxx;
		isBackface = false;

		if(D < 0)
			return false;

		F32 q = b + sign(b) * sqrt(D);
		F32 c = dot(dif, dif) - rad2;

		F32 hitT1 = c / q;
		F32 hitT2 = q;
		isBackface = hitT1 < ray.TMin;

		F32 hitT = isBackface ? hitT2 : hitT1;

		if(hitT < ray.TMin || hitT >= ray.TMax)
			return false;

		outT = F32x3(hitT, isBackface ? ray.TMin : hitT1, hitT2);
		return true;
	}
};

struct Quad {

	F32x3 p0, right, up;
	F32x3 N;

	static Quad create(F32x3 p0, F32x3 right, F32x3 up) {
		Quad q;
		q.p0 = p0;
		q.right = right;
		q.up = up;
		q.N = cross(normalize(up), normalize(right));
		return q;
	}

	Bool intersects(RayDesc ray, inout F32 outT, inout F32x2 outUv, inout F32x3 planeNormal) {

		//Intersect

		F32x4 plane = F32x4(N, -dot(N, p0));
		F32 dif = dot(ray.Direction, -plane.xyz);

		if(dif == 0)
			return false;

		F32 hitT = dot(F32x4(ray.Origin, 1), plane) / dif;

		if(hitT < ray.TMin || hitT >= ray.TMax)
			return false;

		F32x3 pos = posOnRay(ray, hitT);

		F32x2 uv = F32x2(dot(pos, right), dot(pos, up));
		F32x2 leftUv = F32x2(dot(p0, right), dot(p0, up));
		F32x2 delta = uv - leftUv;

		if(any((delta >= 1) | (delta < 0)))
			return false;

		delta.y = 1 - delta.y;

		outT = hitT;
		outUv = delta;
		planeNormal = dif < 0 ? -N : N;

		return true;
	}
};
