//Mix of used and unused resources across register classes.
//Compiled twice by the test: without keep-all only the used three reflect; with keep-all every declaration must
// keep its register on both backends (DXIL -fhlsl-unused-resource-bindings=keep-all, SPIRV -fspv-preserve-bindings).
//Register indices are unique across ALL classes: SPIRV maps t/s/u/b into one binding namespace per space,
// so overlapping indices would collide in the oiSH register table.

Texture2D<float4> usedTex : register(t0);
SamplerState usedSamp : register(s1);
RWStructuredBuffer<float4> usedOut : register(u2);

Texture2D<float4> unusedTex : register(t3);
Texture2D<float4> unusedTexArr[4] : register(t4);
StructuredBuffer<float4> unusedSB : register(t8);
SamplerState unusedSamp : register(s9);
RWTexture2D<float4> unusedUAV : register(u10);

cbuffer UnusedCB : register(b11) {
	float4 unusedVal;
};

[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	usedOut[id] = usedTex.SampleLevel(usedSamp, float2(0.5, 0.5), 0);
}
