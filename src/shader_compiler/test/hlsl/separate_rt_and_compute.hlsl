#include "@resources.hlsli"

#if defined(__OXC_EXT_RAYTRACING)

	struct Payload {
		F32 t;
	};

	F32x4 test(U32x2 id, U32x2 dim) {

		RayDesc r = (RayDesc) 0;
		r.TMax = 1e6;
		r.Direction = float3(0, 0, -1);

		Payload payload;
		TraceRay(_tlasExt[0], RAY_FLAG_NONE, 0xFF, 0, 0, 0, r, payload);

		return F32x4(payload.t.xxx, 1);
	}

#else

	F32x4 test(U32x2 id, U32x2 dim) {
		return F32x4(F32x2(id) / dim, 0, 1);
	}

#endif

RWTexture2D<F32x4> _test;

#if !defined(__OXC_EXT_RAYTRACING) || defined(__OXC_PREPROCESS)

[shader("compute")]
[numthreads(16, 16, 1)]
void mainCompute(U32x2 id : SV_DispatchThreadID) {

	U32x2 dim;
	_test.GetDimensions(dim.x, dim.y);

	if(any(id >= dim))
		return;

	_test[id] = test(id, dim);
}

#endif

#if defined(__OXC_EXT_RAYTRACING) || defined(__OXC_PREPROCESS)

[shader("raygeneration")]
void mainRaygen() {

	U32x2 id = DispatchRaysIndex().xy;
	U32x2 dim = DispatchRaysDimensions().xy;
	
	if(any(id >= dim))
		return;

	_test[id] = test(id, dim);
}

#endif
