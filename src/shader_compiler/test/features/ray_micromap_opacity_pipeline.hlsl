#include "@extensions.hlsli"

RaytracingAccelerationStructure tlas;
RWStructuredBuffer<float> buf;

struct [raypayload] Payload {
	float t : read(caller) : write(caller, miss);
};

//Library form: a raygen tracing with the OMM ray flag through TraceRay.
//It has no ray query, so it only declares the capabilities and carries no OpacityMicromapIdKHR execution mode.

[[oxc::extension("RayMicromapOpacity")]]
[[oxc::model("6.9")]]
[shader("raygeneration")]
void main() {

	oxc::EnableOpacityMicromap();

	RayDesc r; r.Origin = float3(0, 0, 0); r.Direction = float3(0, 0, 1); r.TMin = 0; r.TMax = 1e30f;
	Payload p; p.t = 1;
	TraceRay(tlas, RAY_FLAG_FORCE_OMM_2_STATE, 0xFF, 0, 1, 0, r, p);
	buf[0] = p.t;
}
