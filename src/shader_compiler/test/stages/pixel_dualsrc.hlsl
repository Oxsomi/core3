#include "@types.hlsli"

//Dual source blending's two outputs: both sit at LOCATION 0 on SPIR-V, told apart by the Index decoration
// the DUAL_SRC_TARGET1 macro applies, while DXIL reads the plain SV_Target1 as src1.
//The reflection has to land both backends on the same slots (0 and 1) or the two binaries can never merge
// into one oiSH, which is exactly what the combine test checks.

struct DualSrcOutput {
	DUAL_SRC_TARGET0 float4 src0 : SV_Target0;
	DUAL_SRC_TARGET1 float4 src1 : SV_Target1;
};

[[oxc::stage("pixel")]]
DualSrcOutput main(float4 p : SV_Position) {

	DualSrcOutput o;
	o.src0 = float4(p.xy, 0.0, 1.0);
	o.src1 = float4(0.5, 0.5, 0.5, 0.5);
	return o;
}
