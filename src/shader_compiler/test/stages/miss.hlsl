struct Payload { float4 color; };

[shader("miss")]
void main(inout Payload p) {
	p.color = float4(0.1, 0.2, 0.3, 1.0);
}
