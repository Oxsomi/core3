//An unsafe experiment, compiled and never run: an instruction of a NonSemantic extended instruction set whose result is
// used, which the validator refuses.
//unsafe_experiment_refused.hlsl is the same shader without the marker, which has to fail.

RWStructuredBuffer<uint> buf;

[[vk::ext_instruction(1, "NonSemantic.OxC3.Experiment")]]
uint experiment(uint value);

[[oxc::extension("UnsafeExperimentExecution")]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	buf[id] = experiment(id) + 1;
}
