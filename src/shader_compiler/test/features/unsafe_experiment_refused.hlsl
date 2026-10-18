//unsafe_experiment.hlsl without the UnsafeExperimentExecution marker: the validator refuses the NonSemantic result's
// use, so this has to fail to compile.

RWStructuredBuffer<uint> buf;

[[vk::ext_instruction(1, "NonSemantic.OxC3.Experiment")]]
uint experiment(uint value);

[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	buf[id] = experiment(id) + 1;
}
