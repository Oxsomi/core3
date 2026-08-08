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
#include "primitive.hlsli"

static const U32 test0	= 0x123;
static const U32 test1	= 0123;
static const U32 test2	= 0b101;
static const U32 test3	= 123;
static const F32 test4	= 123.;
static const F32 test5	= 123.f;
static const F32 test6	= 0.f;
static const F32 test7	= 0.0f;
static const F32 test8	= 0.0;
static const F32 test9	= 0123.f;
static const F32 testA	= 3.4f;
static const F32 testB	= 3.4e-4f;
static const F32 testC	= 3.4e-4;
static const F32 testD	= 3.4e+4;
static const F32 testE	= 3.4e+4f;
static const F32 testF	= .4f;
static const F32 test10	= .4;
static const F32 test11	= .4e+5;
static const F32 test12	= .4e-5;

static const I32 test13	= -123;
static const U32 test14	= +123;
static const F32 test15	= 0e1;
static const U32 test17	= 0;
static const U32 test18	= 1;
static const U32 test19	= 9;
static const U32 test1A	= +9;
static const I32 test1B	= -9;
static const F32 test1C	= 089.f;
static const F32 test1D	= 1e9;
static const U32x3 test1F = 3.xxx;
static const U32x3 test20 = 1.f.xxx;

static const F32 test = 1;

//Generating camera rays using a vInv and vpInv

struct Atmosphere {

	struct ScatteringType {
		F32x3 coefficient;
		F32 scaleHeight;
	};

	F32x3 sunRadianceNits;
	U32 raySamples;

	F32x3 ozoneCoefficient;
	U32 lightSamples;

	ScatteringType rayleigh;

	ScatteringType mie;

	F32x3 sunRadianceLux;
	F32 planetRadius;

	F32x3 sunDir;
	F32 atmosphereRadius;

	//Scattering density and phase functions

	F32 getDensity(F32x3 pos, F32 rayLen, ScatteringType type) {

		F32 dist = length(pos) - planetRadius;

		if(dist <= 0)
			return 0;

		return rayLen * exp(-dist / type.scaleHeight);
	}

	static F32 rayleighPhaseFunction(F32 LoV) {
		return 3.f / (16 * F32_pi) * (1 + pow(LoV, 2));
	}

	static F32 miePhaseFunction(F32 LoV) {
		F32 g = 0.76, g2 = pow(g, 2);     //Anisotropy
		return 3.f / (8 * F32_pi) * (1 - g2) * (1 + pow(LoV, 2)) / ((2 + g2) * pow(1 + g2 - 2 * g * LoV, 1.5));
	}

	//Properties of earth

	static Atmosphere earth(F32x3 sunDir) {

		Atmosphere a;

		a.raySamples = 16;
		a.lightSamples = 8;
		a.planetRadius = 6371000;
		a.atmosphereRadius = a.planetRadius + 80000;

		F32 sunSolidAngle = 0.0000711;                                    //In steradian

		a.sunRadianceLux = F32x3(255, 244, 234) / 255 * 120000;       //5900K at 120k lux
		a.sunRadianceNits = a.sunRadianceLux / sunSolidAngle;

		a.sunDir = sunDir;

		a.ozoneCoefficient = F32x3(3.426, 8.298, 0.356) * 6e-7;

		a.rayleigh.scaleHeight = 8000;
		a.rayleigh.coefficient = F32x3(5.8e-6f, 1.35e-5f, 3.31e-5f);

		a.mie.scaleHeight = 1200;
		a.mie.coefficient = 2.1e-6.xxx;

		return a;
	}

	//Ray marching

	Bool getOpticalDepthLight(
		F32x3 pos,
		ScatteringType rayleigh, ScatteringType mie,
		out F32 rayleighDepth, out F32 mieDepth
	) {

		rayleighDepth = mieDepth = 0;

		//Intersect atmos

		RayDesc ray = createRay(pos, 0, -sunDir, 1e38);
		Sphere atmos = Sphere::create(0.xxx, atmosphereRadius);

		F32x3 intersections; Bool isBackside;
		if(!atmos.intersects(ray, intersections, isBackside))
			return false;

		//Step through only the atmos (nothing before or after)

		F32 start = intersections.y;
		F32 step = (intersections.z - start) / lightSamples;

		U32 i = 0;

		for(; i < lightSamples; ++i) {

			F32x3 pos = posOnRay(ray, start + step * (i + 0.5));

			F32 dist = length(pos) - planetRadius;

			//if(dist <= 0)
			//	break;

			rayleighDepth += getDensity(pos, step, rayleigh);
			mieDepth += getDensity(pos, step, mie);
		}

		return i == lightSamples;
	}

	F32x3 getSunContribution(F32x3 nrm) {
		return saturate(dot(nrm, -sunDir)) * sunRadianceLux / F32_pi;
	}

	F32x3 getContribution(RayDesc ray) {

		//We should remap the relative position to a real position:
		//To do this, we will map y relative to the sphere's surface, while remapping x/z to long/lat.
		//We will remap the pos relative to the center of Iceland.

		/*
		F32x3 relativePos = ray.Origin;

		F32 height = planetRadius + 0.5;

		F32x2 longLat = F32x2(4.897070, 52.377956);
		F32 cosLat = cos(longLat.y);

		F32x3 norm = F32x3(
			cosLat * cos(longLat.x),
			sin(longLat.y),
			cosLat * sin(longLat.x)
		);

		F32x3 pos = norm * height;                                 //TODO: For this, position and camera have to be changed!
		ray.Origin += pos;*/

		ray.Origin += float3(0, planetRadius + 10, 0);

		//Get start and end intersection

		{
			Sphere earth = Sphere::create(0.xxx, planetRadius);

			F32x3 intersections; Bool isBackside;
			if(earth.intersects(ray, intersections, isBackside))
				ray.TMax = intersections.x;
		}

		F32x3 earthNrm = normalize(posOnRay(ray, ray.TMax));
		F32x3 earthShading = getSunContribution(earthNrm);

		Sphere atmos = Sphere::create(0.xxx, atmosphereRadius);

		F32x3 intersections; Bool isBackside;
		if(!atmos.intersects(ray, intersections, isBackside))
			return earthShading;

		F32 start = intersections.y;
		F32 diff = intersections.z - start;

		//Raymarch through the start + end regions
		F32 step = diff / raySamples;

		F32x4 sumRayleigh = 0.xxxx, sumMie = 0.xxxx;

		for(U32 i = 0; i < raySamples; ++i) {

			F32x3 pos = posOnRay(ray, start + step * (0.5 + i));

			F32 densityRayleigh = getDensity(pos, step, rayleigh);
			F32 densityMie = getDensity(pos, step, mie);

			sumRayleigh.w += densityRayleigh;
			sumMie.w += densityMie;

			F32 depthLightRayleigh, depthLightMie;

			if(getOpticalDepthLight(pos, rayleigh, mie, depthLightRayleigh, depthLightMie)) {

				F32x3 tauRayleighOzone = (rayleigh.coefficient + ozoneCoefficient) * (depthLightRayleigh + sumRayleigh.w);
				F32x3 tauMie = mie.coefficient * 1.11 * (depthLightMie + sumMie.w);

				F32x3 atten = exp(-(tauRayleighOzone + tauMie));

				sumRayleigh.xyz += atten * densityRayleigh;
				sumMie.xyz += atten * densityMie;
			}
		}

		float LoV = saturate(dot(ray.Direction, -sunDir));
		F32x3 rayleighContrib = sumRayleigh.xyz * rayleigh.coefficient * rayleighPhaseFunction(LoV);
		F32x3 mieContrib = sumMie.xyz * mie.coefficient * miePhaseFunction(LoV);

		return (rayleighContrib + mieContrib) * sunRadianceLux / F32_pi;
	}
};
