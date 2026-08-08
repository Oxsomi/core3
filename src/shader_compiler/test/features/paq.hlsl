struct [raypayload] Payload {
	float3 color : write(caller, closesthit, miss) : read(caller);
	float  hitT  : write(caller, closesthit, miss) : read(caller);
};

RaytracingAccelerationStructure tlas;
RWStructuredBuffer<float4> outBuf;

[[oxc::extension("PAQ")]]
[[oxc::model("6.6")]]
[shader("raygeneration")]
void mainRaygen() {
	RayDesc ray = { float3(0, 0, 0), 0, float3(0, 0, -1), 1e6 };
	Payload payload;
	payload.color = float3(0, 0, 0);
	payload.hitT = -1;
	TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
	outBuf[0] = float4(payload.color, payload.hitT);
}

[[oxc::extension("PAQ")]]
[[oxc::model("6.6")]]
[shader("closesthit")]
void mainClosestHit(inout Payload payload, BuiltInTriangleIntersectionAttributes attr) {
	payload.color = float3(1, 1, 1);
	payload.hitT = RayTCurrent();
}
