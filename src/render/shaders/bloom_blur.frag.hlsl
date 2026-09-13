Texture2D image : register(t0, space2);
SamplerState image_sampler : register(s0, space2);

cbuffer Parameters : register(b0, space3) {
    float2 step;
    float sigma;
    int radius;
};

float4 main(float4 color : COLOR0, float2 uv : TEXCOORD0) : SV_Target {
    float3 sum = image.Sample(image_sampler, uv).rgb;
    float total_weight = 1;
    for (int i = 1; i <= radius; ++i) {
        float distance = float(i) / sigma;
        float weight = exp(-0.5 * distance * distance);
        sum += weight * (image.Sample(image_sampler, uv + step * i).rgb
                       + image.Sample(image_sampler, uv - step * i).rgb);
        total_weight += 2 * weight;
    }
    return float4(sum / total_weight, 1) * color;
}
