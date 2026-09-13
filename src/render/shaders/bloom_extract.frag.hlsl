Texture2D image : register(t0, space2);
SamplerState image_sampler : register(s0, space2);

cbuffer Parameters : register(b0, space3) {
    float threshold;
};

float4 main(float4 color : COLOR0, float2 uv : TEXCOORD0) : SV_Target {
    float3 source = image.Sample(image_sampler, uv).rgb;
    float luminance = dot(source, float3(0.2126, 0.7152, 0.0722));
    return float4(luminance > threshold ? source : float3(0, 0, 0), 1) * color;
}
