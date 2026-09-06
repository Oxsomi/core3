//The 64 bit integer aliases reach a shader through @types.hlsli, which pulls in @extension.I64.hlsli
#include "@types.hlsli"
RWStructuredBuffer<uint64_t3> buf;

[[oxc::extension("I64")]]
[[oxc::stage("compute")]]
[numthreads(1,1,1)]
void main(U32 id : SV_DispatchThreadID) {
	U64x3 v = buf[id];
	I64x2 delta = (I64x2) v.xy - (I64x2) v.zz;
	buf[id] = v + (U64x3) U64x2(delta).xyy;
}
