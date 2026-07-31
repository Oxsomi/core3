struct MyAttributes { float2 bary; };

[shader("intersection")]
void main() {
	MyAttributes attr;
	attr.bary = float2(0.5f, 0.5f);
	ReportHit(1.0f, 0u, attr);
}
