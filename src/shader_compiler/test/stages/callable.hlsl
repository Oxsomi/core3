struct Params { float3 color; };

[shader("callable")]
void main(inout Params p) {
	p.color = p.color * 0.5f;
}
