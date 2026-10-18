//Extensions OxC3 doesn't recognize, compiled and never run: a rasterizer ordered view, which DXIL marks with a
// requirement flag and SPIR-V with SPV_EXT_fragment_shader_interlock and its capability, none of which oiSH has an
// extension for.
//unsafe_extension_refused.hlsl is the same shader without the marker, which has to fail on both backends.

RasterizerOrderedTexture2D<float4> rov;

[[oxc::extension("UnsafeExtensionExecution")]]
[[oxc::stage("pixel")]]
float4 main(float4 pos : SV_Position) : SV_Target {
	const uint2 at = uint2(pos.xy);
	const float4 previous = rov[at];
	rov[at] = previous + 1;
	return previous;
}
