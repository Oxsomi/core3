//Two standalone stages in one file; only the vertex one enables 16BitTypes. Exactly one of the two produced
//binaries may carry that extension bit (per-entrypoint extension isolation).

[[oxc::extension("16BitTypes")]]
[[oxc::model("6.6")]]
[[oxc::stage("vertex")]]
float4 mainA(uint i : SV_VertexID) : SV_Position { half h = (half) i; return h.xxxx; }

[[oxc::stage("pixel")]]
float4 mainB() : SV_Target { return 1; }
