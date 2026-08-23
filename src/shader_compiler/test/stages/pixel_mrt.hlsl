//A pixel shader writing more than one render target, plus a non-zero default semantic index.
//
//The corpus had neither, which is how a backend divergence stayed invisible: the SPIR-V path used to discard
// the index of a default semantic, recording SV_Target1 as 0 while DXIL recorded 1. The two backends then
// disagreed on outputSemanticNames and could not be combined into one oiSH.
//TEXCOORD1 on the input is here for the same reason and breaks the same way.

struct MRTOutput {
	float4 a : SV_Target0;
	float4 b : SV_Target1;
};

[[oxc::stage("pixel")]]
MRTOutput main(float4 p : SV_Position, float2 uv : TEXCOORD1) {

	MRTOutput o;
	o.a = float4(p.xy, 0.0, 1.0);
	o.b = float4(uv, 1.0, 1.0);
	return o;
}
