struct Payload { float4 color; };

[shader("closesthit")]
void main(inout Payload p, in BuiltInTriangleIntersectionAttributes a) {
	p.color = float4(a.barycentrics, 0.0f, 1.0f);
}
