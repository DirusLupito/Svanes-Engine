#include "bloom_pass.hpp"
#include "shaders/bloom_shaders.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace svanes::internal {

static void CheckBloom(bool success) {
    if (!success) {
        throw std::runtime_error("Bloom rendering failed: " +
                                 std::string{SDL_GetError()});
    }
}

BloomPass::BloomPass(SDL_Renderer *renderer) : renderer(renderer) {}

void BloomPass::ShaderDeleter::operator()(SDL_GPUShader *shader) const {
    SDL_ReleaseGPUShader(device, shader);
}

void BloomPass::SamplerDeleter::operator()(SDL_GPUSampler *sampler) const {
    SDL_ReleaseGPUSampler(device, sampler);
}

BloomPass::Effect BloomPass::CreateEffect(std::span<const std::uint8_t> spirv,
                                          std::span<const std::uint8_t> dxil,
                                          std::span<const std::uint8_t> msl,
                                          std::uint32_t samplers,
                                          std::uint32_t uniforms) {
    SDL_GPUShaderCreateInfo info{};
    const auto formats = SDL_GetGPUShaderFormats(device);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = spirv.data();
        info.code_size = spirv.size();
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = dxil.data();
        info.code_size = dxil.size();
    } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        info.format = SDL_GPU_SHADERFORMAT_MSL;
        info.code = msl.data();
        info.code_size = msl.size();
    } else {
        throw std::runtime_error(
            "Bloom requires SPIR-V, DXIL, or MSL shaders.");
    }
    info.entrypoint =
        info.format == SDL_GPU_SHADERFORMAT_MSL ? "main0" : "main";
    info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_samplers = samplers;
    info.num_uniform_buffers = uniforms;
    Effect effect;
    effect.shader = {SDL_CreateGPUShader(device, &info), ShaderDeleter{device}};
    CheckBloom(effect.shader != nullptr);
    SDL_GPURenderStateCreateInfo state_info{};
    state_info.fragment_shader = effect.shader.get();
    effect.state.reset(SDL_CreateGPURenderState(renderer, &state_info));
    CheckBloom(effect.state != nullptr);
    return effect;
}

BloomPass::Texture BloomPass::CreateTarget(std::int32_t target_width,
                                           std::int32_t target_height) {
    Texture texture{SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA64_FLOAT,
                                      SDL_TEXTUREACCESS_TARGET, target_width,
                                      target_height),
                    SDL_DestroyTexture};
    CheckBloom(texture != nullptr);
    CheckBloom(SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_LINEAR));
    CheckBloom(SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_NONE));
    return texture;
}

void BloomPass::Begin(std::int32_t new_width, std::int32_t new_height,
                      const BloomSettings &settings) {
    if (!std::isfinite(settings.threshold) || settings.threshold < 0.0F ||
        !std::isfinite(settings.strength) || settings.strength < 0.0F ||
        !std::isfinite(settings.radius) || settings.radius < 0.0F ||
        !std::isfinite(settings.exposure) || settings.exposure <= 0.0F) {
        throw std::invalid_argument(
            "Bloom requires finite nonnegative threshold, strength, "
            "and radius, and finite positive exposure.");
    }
    if (new_width <= 0 || new_height <= 0) {
        throw std::invalid_argument("Bloom requires a positive output size.");
    }
    if (!device) {
        device = SDL_GetGPURendererDevice(renderer);
        CheckBloom(device != nullptr);
        texture_effect =
            CreateEffect(texture_spirv, texture_dxil, texture_msl, 1, 0);
        extract_effect =
            CreateEffect(extract_spirv, extract_dxil, extract_msl, 1, 1);
        blur_effect = CreateEffect(blur_spirv, blur_dxil, blur_msl, 1, 1);
        composite_effect =
            CreateEffect(composite_spirv, composite_dxil, composite_msl, 2, 1);
        SDL_GPUSamplerCreateInfo info{};
        info.min_filter = SDL_GPU_FILTER_LINEAR;
        info.mag_filter = SDL_GPU_FILTER_LINEAR;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler = {SDL_CreateGPUSampler(device, &info), SamplerDeleter{device}};
        CheckBloom(sampler != nullptr);
    }
    if (width != new_width || height != new_height) {
        CheckBloom(SDL_SetGPURenderState(renderer, nullptr));
        CheckBloom(SDL_SetRenderTarget(renderer, nullptr));
        CheckBloom(SDL_FlushRenderer(renderer));
        auto new_scene = CreateTarget(new_width, new_height);
        auto new_bright = CreateTarget(new_width, new_height);
        auto new_scratch = CreateTarget(new_width, new_height);
        SDL_GPUTextureSamplerBinding binding{};
        binding.texture = static_cast<SDL_GPUTexture *>(SDL_GetPointerProperty(
            SDL_GetTextureProperties(new_bright.get()),
            SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
        CheckBloom(binding.texture != nullptr);
        binding.sampler = sampler.get();
        SDL_GPURenderStateCreateInfo info{};
        info.fragment_shader = composite_effect.shader.get();
        info.num_sampler_bindings = 1;
        info.sampler_bindings = &binding;
        State new_state{SDL_CreateGPURenderState(renderer, &info),
                        SDL_DestroyGPURenderState};
        CheckBloom(new_state != nullptr);
        composite_effect.state = std::move(new_state);
        scene = std::move(new_scene);
        bright = std::move(new_bright);
        scratch = std::move(new_scratch);
        width = new_width;
        height = new_height;
    }
    CheckBloom(SDL_SetGPURenderState(renderer, nullptr));
    CheckBloom(SDL_SetRenderTarget(renderer, scene.get()));
    CheckBloom(SDL_SetRenderClipRect(renderer, nullptr));
    CheckBloom(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255));
    CheckBloom(SDL_RenderClear(renderer));
}

void BloomPass::SetTextureState(bool textured) const {
    CheckBloom(SDL_SetGPURenderState(
        renderer, textured ? texture_effect.state.get() : nullptr));
}

void BloomPass::Draw(SDL_Texture *source, SDL_Texture *destination,
                     SDL_GPURenderState *state) {
    CheckBloom(SDL_SetRenderTarget(renderer, destination));
    CheckBloom(SDL_SetRenderClipRect(renderer, nullptr));
    CheckBloom(SDL_SetGPURenderState(renderer, state));
    CheckBloom(SDL_RenderTexture(renderer, source, nullptr, nullptr));
}

void BloomPass::Blur(SDL_Texture *source, SDL_Texture *destination,
                     float radius, bool horizontal) {
    struct Parameters {
        float step_x;
        float step_y;
        float sigma;
        std::int32_t radius;
    };
    const auto extent = horizontal ? width : height;
    const float bounded_radius = std::min(radius, static_cast<float>(extent));
    const Parameters parameters{
        horizontal ? 1.0F / width : 0.0F,
        horizontal ? 0.0F : 1.0F / height,
        std::max(bounded_radius / 3.0F, 0.001F),
        static_cast<std::int32_t>(std::ceil(bounded_radius)),
    };
    CheckBloom(SDL_SetGPURenderStateFragmentUniforms(
        blur_effect.state.get(), 0, &parameters, sizeof(parameters)));
    Draw(source, destination, blur_effect.state.get());
}

void BloomPass::Finish(const BloomSettings &settings, const SDL_Rect *clip) {
    const std::array<float, 4> extraction{settings.threshold, 0.0F, 0.0F, 0.0F};
    CheckBloom(SDL_SetGPURenderStateFragmentUniforms(
        extract_effect.state.get(), 0, extraction.data(), sizeof(extraction)));
    Draw(scene.get(), bright.get(), extract_effect.state.get());
    for (std::uint32_t i = 0; i < settings.num_iterations; ++i) {
        Blur(bright.get(), scratch.get(), settings.radius, true);
        Blur(scratch.get(), bright.get(), settings.radius, false);
    }
    const std::array<float, 4> composite{settings.strength, settings.exposure,
                                         0.0F, 0.0F};
    CheckBloom(SDL_SetGPURenderStateFragmentUniforms(
        composite_effect.state.get(), 0, composite.data(), sizeof(composite)));
    CheckBloom(SDL_SetRenderTarget(renderer, nullptr));
    CheckBloom(SDL_SetRenderClipRect(renderer, nullptr));
    CheckBloom(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255));
    CheckBloom(SDL_RenderClear(renderer));
    CheckBloom(SDL_SetRenderClipRect(renderer, clip));
    CheckBloom(SDL_SetGPURenderState(renderer, composite_effect.state.get()));
    CheckBloom(SDL_RenderTexture(renderer, scene.get(), nullptr, nullptr));
    CheckBloom(SDL_SetGPURenderState(renderer, nullptr));
    CheckBloom(SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND));
}

} // namespace svanes::internal
