[[oxc::stage("pixel")]]
float4 main(float4 p : SV_Position) : SV_Target {
	return float4(p.xy, 0.0, 1.0);
}
