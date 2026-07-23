RWStructuredBuffer<double> buf;

[[vk::ext_capability(6034)]]                             // AtomicFloat64AddEXT
[[vk::ext_extension("SPV_EXT_shader_atomic_float_add")]]
[[vk::ext_instruction(6035)]]                            // OpAtomicFAddEXT
double atomicAddF64([[vk::ext_reference]] double loc, uint scope, uint semantics, double value);

[[oxc::extension("F64", "AtomicF64")]]
[[oxc::stage("compute")]]
[numthreads(1,1,1)]
void main(uint id : SV_DispatchThreadID) {
	atomicAddF64(buf[0], 1u, 0u, buf[id]);               // scope=1 Device, semantics=0 Relaxed
}
