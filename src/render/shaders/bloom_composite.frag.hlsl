Texture2D scene : register(t0, space2);
SamplerState scene_sampler : register(s0, space2);
Texture2D blurred : register(t1, space2);
SamplerState blurred_sampler : register(s1, space2);

cbuffer Parameters : register(b0, space3) {
    float strength;
    float exposure;
};

float3 EncodeSRGB(float3 color) {
    return select(color <= 0.0031308, 12.92 * color,
                  1.055 * pow(color, 1.0 / 2.4) - 0.055);
}

float4 main(float4 color : COLOR0, float2 uv : TEXCOORD0) : SV_Target {
    float3 hdr = scene.Sample(scene_sampler, uv).rgb
               + strength * blurred.Sample(blurred_sampler, uv).rgb;
    float3 mapped = 1 - exp(-hdr * exposure);
    return float4(EncodeSRGB(mapped), 1) * color;
}
