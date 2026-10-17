//unsafe_extension.hlsl without the UnsafeExtensionExecution marker: oiSH has no extension for a rasterizer ordered
// view on either backend, so this has to fail to compile on both.

RasterizerOrderedTexture2D<float4> rov;

[[oxc::stage("pixel")]]
float4 main(float4 pos : SV_Position) : SV_Target {
	const uint2 at = uint2(pos.xy);
	const float4 previous = rov[at];
	rov[at] = previous + 1;
	return previous;
}
