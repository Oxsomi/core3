//SV_Barycentrics: native semantic on both backends (DXIL SM6.1+; SPIRV BaryCoordKHR via
//SPV_KHR_fragment_shader_barycentric, which the compiler whitelists when the extension is declared).

[[oxc::extension("Barycentrics")]]
[[oxc::model("6.5")]]
[[oxc::stage("pixel")]]
float4 main(float3 bary : SV_Barycentrics) : SV_Target {
	return float4(bary, 1);
}
