RWStructuredBuffer<float4> regular;

//Declares full bindless but never touches the heaps; native detection must demote the extension to dormant.

[[oxc::extension("DescriptorHeap")]]
[[oxc::model("6.6")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	regular[id] = float4(id, 0, 0, 1);
}
