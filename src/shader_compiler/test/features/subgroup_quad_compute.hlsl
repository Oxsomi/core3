//The same quad ops in COMPUTE, which is a strictly larger requirement than the pixel form.
//A quad is implicit in a pixel shader but not in a compute one, so DXC also emits a derivative group
// execution mode to say what the 2x2 is, and the shader needs ComputeDeriv on top of SubgroupQuad.
//Declaring only one of the two is what makes this fail, which is why both extensions exist separately.

//numthreads picks which mode DXC emits: X divisible by 4 with Y and Z of 1 gives the linear rules,
// X and Y both divisible by 2 gives the quad rules. Any other shape is invalid.

RWStructuredBuffer<float> _buf;

[[oxc::extension("SubgroupQuad", "ComputeDeriv", "SubgroupOperations")]]
[[oxc::model("6.6")]]
[[oxc::stage("compute")]]
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	float v = (float)(id.x + id.y);
	_buf[id.x + id.y * 8] = QuadReadAcrossX(v) + QuadReadAcrossY(v);
}
