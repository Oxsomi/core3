RWStructuredBuffer<uint> buf;

[[oxc::model("6.8")]]
[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1,1,1)]
[numthreads(1,1,1)]
void main(uint3 dtid : SV_DispatchThreadID) {
	buf[dtid.x] = dtid.x + 1;
}
