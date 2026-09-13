#pragma once

#include <svanes/render/render_system.hpp>

#include <SDL3/SDL.h>

#include <array>
#include <memory>
#include <span>

// https://learnopengl.com/Advanced-Lighting/Bloom
// An aside on...
// ...bloom:
//
// Bloom works as a post processing effect. After we have put the screen
// together into a single image/texture, we might want to have particularly
// bright pixels exude a glow/light around them. This is what bloom does. It
// takes the final image, extracts the bright pixels, blurs them, and then adds
// them back to the original image.
//
// First, what are these images? A texture is a grid of pixels, each of which
// stores a color. We usually draw textures onto the screen, but we can also
// draw into a texture. Such a texture is a render target. This lets us keep
// an intermediate image instead of immediately presenting it to the user.
// A texel is a pixel of a texture, not necessarily a screen pixel.
//
// In the BloomPass:
// scene is the texture containing the original rendered image.
// bright initially contains the bright regions extracted from scene. 
// scratch is temp storage for an intermediate blur result (scratch paper).
//
// I read https://en.wikipedia.org/wiki/Relative_luminance for this next bit:
// How do we decide which pixels are bright? We want a single brightness value
// to compare against a threshold. Merely checking whether any channel is large
// treats saturated blue and saturated green alike, but our eyes are
// much more sensitive to green. Relative luminance accounts for this with a
// weighted sum. For linear RGB with the sRGB primaries, the weighting is:
//
//     Y = 0.2126 R + 0.7152 G + 0.0722 B
//
// Here Y is a scalar brightness measure, and R, G, and B are the pixel's color
// channels. The weights sum to one, so (1, 1, 1) has luminance 1. This assumes
// linear color values: doubling a channel doubles the represented light.
// 
// sRGB image values are encoded nonlinearly. 
// Dividing a byte by 255 gives an encoded value c in [0, 1], not a light
// intensity. We decode each RGB channel with the sRGB transfer function:
//
//     linear(c) = c / 12.92                        if c <= 0.04045
//               ((c + 0.055) / 1.055)^2.4          otherwise
//
// For example, a red val of 128 is ~ 0.502 encoded, but about 0.216 linear.
// If we used 0.502 in our luminance calculation, we would substantially
// overestimate its brightness. Alpha describes opacity and is not decoded.
// https://en.wikipedia.org/wiki/SRGB gives the details.
//
// Here is where this actually happens. scene is an RGBA64_FLOAT render target:
// four 16-bit floating-point channels, with linear sRGB colorspace. SDL's GPU
// renderer converts our draw colors and sprite tints to linear values when
// drawing to that target. It also interpolates the resulting linear vertex
// colors across triangles, including our radial gradients. We must not decode
// those colors again ourselves.
//
// Our ordinary sprite textures contain sRGB bytes. bloom_texture.frag.hlsl
// reads the four texels surrounding a texture coordinate, decodes each texel,
// then interpolates their linear colors. This is bilinear filtering: first
// blend the left/right pair in each row, then blend those two row results.
// The fractional position supplies the blend weights. Decoding before this
// average matters: halfway between black and white is 0.5 linear, whereas
// averaging their encoded values first and then decoding yields about 0.214.
// The shader multiplies the filtered color by the already-linear sprite tint.
// All these values are blended into scene in linear space. The renderer's
// window output remains ordinary sRGB; only the intermediate targets are HDR.
//
// The luminance Y totally determines whether a pixel is bright enough to
// contribute to the glow. If Y exceeds the threshold, we keep the original
// RGB color. If Y is at or below the threshold, we discard the pixel and
// write pure, pitch 0,0,0 black.
//
// Finish runs bloom_extract.frag.hlsl over scene and writes this result into
// bright. A fragment shader is a program the GPU executes to calculate an
// output pixel. Here it reads a scene pixel, computes Y, and chooses either
// that pixel's complete RGB color or black. It does not subtract anything
// from the individual channels. The retained color therefore keeps its RGB
// proportions. A hard threshold can visibly switch a contribution on/off
// when its luminance crosses the threshold; this is the policy we use.
//
// scene, bright, and scratch are all full output resolution, so a small
// source is not averaged away before we test its luminance. All three use
// floating-point storage, which can retain values above 1. For instance,
// two overlapping additive white shapes can contribute a linear value of 2.
// We can consequently set threshold above 1 to select only such HDR regions.
// Our colors are still specified as bytes; additive drawing provides one
// way to create higher intensities without changing that public color type.
//
// Once we have extracted the bright regions, what does it mean to blur them?
// For every destination pixel, look at a neighborhood of source pixels and
// compute a weighted average of their colors. We do this even when the
// destination's original location was black. It can receive a contribution
// from a bright neighbor, which is how the glow extends beyond the source.
//
// First consider just one row. For instance,
// x coordinates:
// 0 1 2 3 4 5 6
// pixels:
// . . . . X . .
//
// Where X is a bright pixel, and . are dark pixels. If our blur radius is 2,
// and we are figuring out the blurred value for pixel 3, we will sample pixels
// 1, 2, 3, 4, and 5. If our blur radius is 3, we will sample pixels 0, 1, 2, 3,
// 4, 5, and 6. A sample means reading a source color. Its offset is its position
// relative to the destination, so pixel 4 has offset +1 from pixel 3.
//
// We could give every sample the same weight. That is a box blur. Instead,
// we want nearby samples to contribute more than distant ones, and we want
// that contribution to decrease smoothly. There is no unique function that
// must be used for this. A useful choice is the Gaussian, a bell-shaped curve:
//
//     g(i) = exp(-i*i / (2*sigma*sigma))
//
// Here i is the signed offset, sigma is a positive spread parameter, and
// exp(a) means e raised to the power a, where e is approximately 2.71828.
// Squaring i makes equal distances to the left and right have equal weight.
// At i = 0, the exponent is zero and g(0) = 1. Farther away, the exponent
// becomes more negative, making the weight smaller but still positive.
//
// Why divide by sigma squared? We are measuring distance in units of sigma:
//
//     g(i) = exp(-0.5 * (i / sigma)^2)
//
// Increasing sigma makes the same offset smaller relative to the spread,
// so farther neighbors retain more influence. At distances sigma, 2*sigma,
// and 3*sigma, the unnormalized weights are approximately 0.607, 0.135,
// and 0.011, respectively. Sigma is called the standard deviation of the
// continuous normalized Gaussian. Its square, the variance, measures the
// average squared distance from the center when weighted by that Gaussian.
// Our finite, sampled kernel only approximates that continuous distribution.
//
// These g values are not yet averaging weights. Their sum is generally greater
// than one, so using them directly would brighten even an image of constant
// color. Let r be the integer sampling radius. We normalize the weights:
//
//     S = sum of g(i), for every integer i from -r through r
//     w(i) = g(i) / S
//
// Now the sum of w(i) is one. If every sampled color equals C, the result is
// C * sum(w(i)) = C. We have spread contributions without arbitrarily scaling
// a constant image's brightness. This table of offsets and weights is the
// blur kernel. We use the same weights separately for R, G, and B.
//
// Black samples contribute zero color, but their weights still belong in S.
// Normalizing using only nonblack samples would remove the dimming we want
// around isolated bright pixels. For instance, with r = 1 and sigma = 1,
// the weights are approximately (0.274, 0.452, 0.274). A single white pixel
// surrounded by black contributes 0.452 at its own location and 0.274 to
// each immediate neighbor in this one-dimensional example.
//
// The Gaussian never reaches exactly zero, so sampling its whole extent
// would require infinitely many samples. Radius specifies where we stop;
// sigma specifies how quickly the weights fall. They are different notions.
// Our Blur connects them by choosing sigma = radius / 3, so the cutoff is
// approximately three standard deviations away, where the weights are small.
// The code keeps sigma at least 0.001 to avoid division by zero. A zero
// radius selects only offset zero, which leaves the image unblurred.
//
// What about a whole two-dimensional image? Denote a destination coordinate
// by (x, y), and a neighboring source coordinate by (x+i, y+j). The distance
// between them is sqrt(i*i + j*j). A circularly symmetric Gaussian uses:
//
//     G(i, j) = exp(-(i*i + j*j) / (2*sigma*sigma))
//
// All offsets at the same distance have the same weight, regardless of their
// direction. Viewed as a surface above the image, this is a smooth mound
// with its highest point at (0, 0), falling away in every direction.
// The fully normalized continuous formula includes 1/(2*pi*sigma*sigma).
// We do not need that factor here: multiplying every sampled weight by the
// same factor cancels when we divide by the sum of the sampled weights.
//
// If we sample a square extending r pixels in each direction, we have
// (2*r+1)*(2*r+1) samples per output pixel. At r = 4, that is 81 samples.
// The square truncates the circularly symmetric function; the finite kernel
// is an approximation, even though the underlying Gaussian is symmetric.
//
// Fortunately, exp(a+b) = exp(a)*exp(b). Applying this to G gives:
//
//     G(i, j) = exp(-i*i / (2*sigma*sigma))
//             * exp(-j*j / (2*sigma*sigma))
//             = g(i) * g(j)
//
// Its sum over the square likewise factors into the horizontal sum times
// the vertical sum. Consequently its normalized weight is w(i)*w(j).
// We can calculate the same weighted average in two stages. Let B(x, y)
// be a color from the extracted image, H the horizontal result, and V the
// finished result. Every sum below runs from -r through r:
//
//     H(x, y) = sum over i of w(i) * B(x+i, y)
//     V(x, y) = sum over j of w(j) * H(x, y+j)
//
// Substitute the first expression into the second:
//
//     V(x, y) = sum over j, then i, of w(j)*w(i) * B(x+i, y+j)
//
// This is exactly the weighted sum over the square we wanted. We call this
// property separability. It reduces 81 samples to 9 horizontal plus 9 vertical
// samples at r = 4. Separating the operations is not itself an approximation;
// truncation, sampling, and finite numerical precision are the approximations.
// Both versions must also use the same rule for samples outside the image.
//
// This explains why we have two intermediate textures. The horizontal pass
// reads bright and writes scratch; the vertical pass reads scratch and writes
// bright. We cannot normally sample a texture while rendering into that same
// texture. Even in a CPU implementation, overwriting samples before neighbors
// have read them would make the result depend on processing order.
//
//     scene -> extraction -> bright -> horizontal blur -> scratch
//                           bright <- vertical blur   <- scratch
//
// Alternating source and destination textures is called ping-pong rendering.
// No new textures are required to repeat this horizontal/vertical pair.
// After the vertical pass, bright contains the complete two-dimensional blur,
// despite its name. scratch no longer contains anything we need to preserve.
//
// How does Blur perform those sums? It selects a source texture, a destination
// texture, and bloom_blur.frag.hlsl, then draws one rectangle covering the
// destination. Each fragment shader invocation computes the weighted average
// for its destination pixel. It accumulates samples in shader variables and
// writes the result once, rather than drawing one image for each sample.
// A pass reads only the source; it overwrites every pixel of its destination.
//
// Texture coordinates are normalized: 0 and 1 describe the texture's edges.
// To move by one texel horizontally, add (1/width, 0) to the coordinate;
// vertically, add (0, 1/height). Blur supplies that vector as step. It supplies
// sigma and the integer sampling radius as well. These values are uniforms:
// parameters held constant across the pixels of a draw, unlike the changing
// texture coordinate of each pixel. settings.radius is in output pixels,
// which are also texels because our targets have the same dimensions.
// The radius is limited to the texture extent and rounded up for sampling;
// sigma uses the bounded floating-point radius before rounding.
//
// The shader starts with the center sample, whose unnormalized weight is 1.
// For each positive offset i it computes distance = i/sigma, then weight =
// exp(-0.5*distance*distance). It reads the neighbors at both +i and -i,
// multiplies their sum by weight, and adds that to the accumulated color.
// total_weight likewise starts at 1 and increases by 2*weight. Dividing the
// accumulated color by total_weight gives the normalized average above.
// At a texture boundary, sampling clamps to the nearest edge texel. This
// repeats the edge color outside the image and keeps a constant image
// constant, including its edges. It does not introduce a black border.
//
// After all blur passes, bloom_composite.frag.hlsl reads both scene and
// bright at the same texture coordinate. It adds their linear colors:
//
//     hdr = scene + strength * bright
//
// We retain the original sharp objects while adding a soft halo around their
// bright regions. strength scales the added light; radius and the iteration
// count determine its spread. The original scene is never blurred.
//
// This sum may exceed the ordinary display range. Clamping every value above
// 1 to white would make intensities 2 and 10 indistinguishable. Instead, we
// use the same exponential tone-mapping curve as the linked bloom example:
//
//     mapped = 1 - exp(-exposure * hdr)
//
// Apply this separately to each RGB channel. At zero input the output is
// zero. As a positive input grows, exp(-exposure*hdr) approaches zero, so
// the output approaches 1 smoothly. With exposure = 1, inputs 1, 2, and 10
// become approximately 0.632, 0.865, and 0.99995. Exposure scales the input
// to this curve; increasing it brightens the result. It does not change
// the luminance threshold, which was evaluated earlier. Tone mapping also
// affects ordinary scene colors, even if strength = 0 or nothing passes
// the threshold. Disabling bloom bypasses this entire HDR processing path.
//
// mapped is still linear light. Before writing to our ordinary sRGB window,
// the composite shader performs the inverse of our initial decoding:
//
//     encoded(L) = 12.92*L                         if L <= 0.0031308
//                  1.055*L^(1/2.4) - 0.055         otherwise
//
// Thus 0.5 linear becomes about 0.735 encoded. Writing 0.5 directly would
// display a darker result. We encode only here, after the linear calculations,
// and the sRGB window receives these encoded values without another encoding.
// The linked example uses a power of 1/2.2 as an approximation; we use the
// piecewise sRGB function for both directions. Tone mapping and encoding
// are separate: one compresses an intensity range, the other represents it
// in the display's expected format. Neither should be repeated between blur
// passes, because doing so would change the light values we are averaging.
//
// Half-float targets still have finite precision and range, but keep much
// more useful precision for faint contributions than 8-bit targets do. Each
// blur sums its samples before writing to the target, so storage rounding
// happens once per pixel per pass. Brightness above 1 is retained until the
// final tone map, rather than being clipped after every additive draw.
//
// What would repeating the blur accomplish? For ideal normalized Gaussians,
// successive blurs add their variances. If their spreads are sigma1 and
// sigma2, the combined spread is sqrt(sigma1^2 + sigma2^2). One way to
// understand this is to imagine spreading a contribution by an offset X,
// then by an independent offset Y. The final offset is X+Y. Its average
// squared distance is the average of X^2 + 2*X*Y + Y^2. Both symmetric
// kernels have average offset zero, so independence makes the average XY
// zero. Only the two average squared offsets, the variances, remain.
// For Gaussian kernels the resulting distribution is itself Gaussian.
//
// Repeating N identical horizontal/vertical pairs therefore gives an ideal
// spread of sqrt(N)*sigma along each axis. Four pairs double the spread,
// rather than quadrupling it. Our finite sampled kernels approximate this;
// at extremely small sigma, almost all weight can land on the center texel,
// so repeating the pass need not produce much visible spreading at all.
//
// num_iterations lets games choose the number of repetitions independently
// of the per-pass radius. Counting a whole horizontal/vertical pair as one
// iteration keeps both axes balanced. Finish extracts the bright regions
// once before the repetitions, and composites once afterward. Repeating
// extraction would keep removing brightness, while compositing each result
// would add several different halos instead of just the final blur.
//
// More iterations are not automatically cheaper than one wider kernel. If
// we halve sigma and the associated radius, we need approximately four pairs
// to recover the same spread. We do fewer samples per pass but more passes,
// with additional target switches and additional intermediate writes. 
// The appropriate choice depends on the desired effect and cost.
//
// The default is five horizontal/vertical pairs. An iteration count of zero
// skips blurring and composites the unblurred extraction; it does not disable
// tone mapping. A zero radius also leaves the extraction unblurred. Set
// enabled to false to bypass bloom altogether.
//
// Unlike the linked example, we extract brightness in a separate full-screen
// pass after drawing the scene, instead of having every scene shader write
// both scene and brightness attachments simultaneously. This keeps our SDL
// drawing commands usable and tests the final blended pixel's luminance.
// The subsequent Gaussian ping-pong blur, HDR addition, and tone mapping
// follow the same sequence. Shader source lives in src/render/shaders, with
// generated SPIR-V, DXIL, and MSL forms for SDL's different GPU backends.

namespace svanes::internal {

class BloomPass final {
  public:
    explicit BloomPass(SDL_Renderer *renderer);
    void Begin(std::int32_t width, std::int32_t height,
               const BloomSettings &settings);
    void Finish(const BloomSettings &settings, const SDL_Rect *clip);
    void SetTextureState(bool textured) const;

  private:
    struct ShaderDeleter {
        SDL_GPUDevice *device = nullptr;
        void operator()(SDL_GPUShader *shader) const;
    };
    struct SamplerDeleter {
        SDL_GPUDevice *device = nullptr;
        void operator()(SDL_GPUSampler *sampler) const;
    };
    using Texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
    using State = std::unique_ptr<SDL_GPURenderState,
                                  decltype(&SDL_DestroyGPURenderState)>;
    struct Effect {
        std::unique_ptr<SDL_GPUShader, ShaderDeleter> shader{nullptr,
                                                             ShaderDeleter{}};
        State state{nullptr, SDL_DestroyGPURenderState};
    };

    Effect CreateEffect(std::span<const std::uint8_t> spirv,
                        std::span<const std::uint8_t> dxil,
                        std::span<const std::uint8_t> msl,
                        std::uint32_t samplers, std::uint32_t uniforms);
    Texture CreateTarget(std::int32_t target_width, std::int32_t target_height);
    void Blur(SDL_Texture *source, SDL_Texture *destination, float radius,
              bool horizontal);
    void Draw(SDL_Texture *source, SDL_Texture *destination,
              SDL_GPURenderState *state);

    SDL_Renderer *renderer;
    SDL_GPUDevice *device = nullptr;
    Texture scene{nullptr, SDL_DestroyTexture};
    Texture bright{nullptr, SDL_DestroyTexture};
    Texture scratch{nullptr, SDL_DestroyTexture};
    std::unique_ptr<SDL_GPUSampler, SamplerDeleter> sampler{nullptr,
                                                            SamplerDeleter{}};
    Effect texture_effect;
    Effect extract_effect;
    Effect blur_effect;
    Effect composite_effect;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

} // namespace svanes::internal
