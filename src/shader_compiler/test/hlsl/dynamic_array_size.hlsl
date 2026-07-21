struct PSInput
{
	float4 color : COLOR;
};

static const uint test = 123;

Texture2D t[test];

[[oxc::stage("pixel")]]
float4 PSMain(PSInput input) : SV_TARGET
{
	return input.color * t[0][0.xx];
}