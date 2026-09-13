Texture2D image : register(t0, space2);
SamplerState image_sampler : register(s0, space2);

float3 DecodeSRGB(float3 color) {
    return select(color <= 0.04045, color / 12.92,
                  pow((color + 0.055) / 1.055, 2.4));
}

float4 LoadLinear(int2 position, int2 size) {
    float4 color = image.SampleLevel(image_sampler,
        (float2(clamp(position, int2(0, 0), size - 1)) + 0.5) / size, 0);
    return float4(DecodeSRGB(color.rgb), color.a);
}

float4 main(float4 color : COLOR0, float2 uv : TEXCOORD0) : SV_Target {
    uint width, height;
    image.GetDimensions(width, height);
    int2 size = int2(width, height);
    float2 position = uv * size - 0.5;
    int2 corner = int2(floor(position));
    float2 fraction = frac(position);
    float4 top = lerp(LoadLinear(corner, size),
                      LoadLinear(corner + int2(1, 0), size), fraction.x);
    float4 bottom = lerp(LoadLinear(corner + int2(0, 1), size),
                         LoadLinear(corner + int2(1, 1), size), fraction.x);
    return lerp(top, bottom, fraction.y) * color;
}
