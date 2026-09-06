//The 16 bit aliases reach a shader through @extensions.hlsli, which pulls in @extension.16BitTypes.hlsli.
//Written without a single name from @types.hlsli (hence uint, not U32), since the umbrella does not include it.
#include "@extensions.hlsli"
RWStructuredBuffer<float16_t4> buf;

[[oxc::extension("16BitTypes")]]
[[oxc::stage("compute")]]
[numthreads(1,1,1)]
void main(uint id : SV_DispatchThreadID) {
	F16x4 v = buf[id];
	I16 signedHalf = (I16) v.x;
	U16x2 packedPair = (U16x2) v.yz;
	buf[id] = v * (F16) (signedHalf + packedPair.x + packedPair.y);
}
