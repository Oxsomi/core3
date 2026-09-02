//SubgroupOperations rides along for the DXIL variant: any wave intrinsic sets the one generic WAVE_OPS
// reflection flag, which maps to SubgroupOperations and has to be declared for processDXIL to accept it.

//A pixel shader is where quad ops need NOTHING but the quad capability: the 2x2 is implicit in the stage,
// so this isolates SubgroupQuad from ComputeDeriv (see subgroup_quad_compute.hlsl for the other half).

[[oxc::extension("SubgroupQuad", "SubgroupOperations")]]
[[oxc::model("6.5")]]
[[oxc::stage("pixel")]]
float4 main(float4 pos : SV_Position) : SV_Target {
	float v = pos.x + pos.y;
	return float4(QuadReadAcrossX(v), QuadReadAcrossY(v), QuadReadAcrossDiagonal(v), 1);   //OpGroupNonUniformQuadSwap
}
