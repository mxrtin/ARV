struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : TEXCOORD1;
};

Texture2D Texture : register(t0);
sampler Sampler : register(s0);

float4 main(PixelShaderInput input) : SV_TARGET
{
    return input.color * Texture.Sample(Sampler, input.texcoord);
}
