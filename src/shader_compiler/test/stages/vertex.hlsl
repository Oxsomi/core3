[[oxc::stage("vertex")]]
float4 main(uint id : SV_VertexID) : SV_Position {
	float2 uv = float2((id << 1) & 2, id & 2);
	return float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
}
