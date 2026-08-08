struct Payload { float4 color; };

[shader("anyhit")]
void main(inout Payload p, in BuiltInTriangleIntersectionAttributes a) {
	float2 bary = a.barycentrics;
	p.color += float4(bary, 0, 1);
	if (bary.x > 0.9)
		AcceptHitAndEndSearch();
	else if (bary.y < 0.1)
		IgnoreHit();
}
