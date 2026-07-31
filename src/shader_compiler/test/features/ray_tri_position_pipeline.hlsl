#include "@extensions.hlsli"

RWStructuredBuffer<float> buf;

struct [raypayload] Payload {
	float3 color : read(caller) : write(caller, closesthit);
};

//Ray-pipeline (closesthit) form of position fetch, reading the current hit triangle's object-space vertices.
[[oxc::extension("RayTriPosition")]]
[[oxc::model("6.10")]]
[shader("closesthit")]
void main(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr) {
	oxc::TrianglePositions p = oxc::HitTrianglePositions();
	payload.color = p.p0 + p.p1 + p.p2 + float3(attr.barycentrics, 0);
	buf[0] = p.p0.x + p.p1.y + p.p2.z;
}
