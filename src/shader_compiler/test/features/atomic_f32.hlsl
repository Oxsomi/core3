[[vk::ext_capability(/* AtomicFloat32AddEXT */ 6033)]]
[[vk::ext_extension("SPV_EXT_shader_atomic_float_add")]]
[[vk::ext_instruction(/* OpAtomicFAddEXT */ 6035)]]
float atomicAddF32([[vk::ext_reference]] float mem, uint scope, uint semantics, float value);

globallycoherent RWStructuredBuffer<float> buf;

[[oxc::extension("AtomicF32")]]
[[oxc::model("6.5")]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	buf[id + 1] = atomicAddF32(buf[0], /* Device */ 1, /* None */ 0, (float)(id + 1));
}
