[[oxc::extension("Multiview")]]
[[oxc::model("6.5")]]
[[oxc::stage("vertex")]]
float4 main(float3 pos : POSITION, uint viewId : SV_ViewID) : SV_Position {
	return float4(pos + float(viewId), 1);
}
