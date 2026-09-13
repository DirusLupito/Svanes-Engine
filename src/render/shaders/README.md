These HLSL fragment shaders implement the bloom path described in
[bloom_pass.hpp](../bloom_pass.hpp). SDL supplies the vertex shaders and selects
the GPU backend. The ordinary window uses sRGB output; bloom's intermediate
render targets use linear RGBA16F.

- `bloom_texture.frag.hlsl` decodes sprite texels before bilinear filtering.
- `bloom_extract.frag.hlsl` retains colors above the linear luminance threshold.
- `bloom_blur.frag.hlsl` performs one normalized Gaussian blur direction.
- `bloom_composite.frag.hlsl` adds bloom, tone maps, and encodes sRGB output.

`bloom_shaders.hpp` contains generated SPIR-V, DXIL, and MSL versions. It is
included in the engine, so running or building a game needs neither external
shader files nor a shader compiler. Do not edit its byte arrays by hand.

After editing the HLSL, regenerate it with Python and
[SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross):

```text
python src/render/shaders/compile.py /path/to/shadercross
```

The script fails if compilation of any backend fails. SPIR-V and DXIL use the
`main` entry point; shadercross translates the Metal entry point to `main0`.
Keep the fragment inputs `COLOR0` and `TEXCOORD0` in use, since they must match
SDL's vertex shader outputs. Post-processing draws use a white color multiplier.

The overall sequence follows [LearnOpenGL's bloom chapter](https://learnopengl.com/Advanced-Lighting/Bloom).
Brightness extraction is a separate pass over the fully blended scene, rather
than an additional output from every scene shader. The blur radius and number
of complete horizontal/vertical pairs are configurable. Encoding uses the
piecewise sRGB transfer function rather than an approximate gamma exponent.
