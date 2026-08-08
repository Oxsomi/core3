//Two standalone stages requesting different shader models; each binary must keep its own requested model
//rather than collapsing to one (per-entrypoint model isolation).

[[oxc::model("6.7")]]
[[oxc::stage("vertex")]]
float4 mainA() : SV_Position { return 0; }

[[oxc::model("6.5")]]
[[oxc::stage("pixel")]]
float4 mainB() : SV_Target { return 1; }
