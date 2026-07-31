struct Payload { uint meshletIndex; };

groupshared Payload payload;

[[oxc::stage("task")]]
[numthreads(1, 1, 1)]
void main(uint gtid : SV_GroupIndex) {
	payload.meshletIndex = gtid;
	DispatchMesh(1, 1, 1, payload);
}
