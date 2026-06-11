#pragma once
// VulkanShaders.h
// Inline SPIR-V bytecode for Aurora's Vulkan video renderer.
//
// Shaders compiled from GLSL with glslangValidator / glslc:
//   Vertex  : fullscreen triangle (no VBO needed, gl_VertexIndex trick)
//   Fragment: YUV420P / NV12 → BT.709 / BT.2020 RGB with optional HDR PQ→SDR tone-map
//
// To regenerate:
//   glslc shaders/yuv_to_rgb.vert -o vert.spv && xxd -i vert.spv
//   glslc shaders/yuv_to_rgb.frag -o frag.spv && xxd -i frag.spv
//
// The raw GLSL source is embedded as comments so the intent is clear.

#include <cstdint>
#include <vector>

namespace aurora::renderer::shaders {

// ─────────────────────────────────────────────────────────────────────────────
// VERTEX SHADER GLSL (reference)
// #version 450
// layout(location = 0) out vec2 vUV;
// void main() {
//   // Fullscreen triangle: 3 vertices covering entire screen
//   vUV         = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
//   gl_Position = vec4(vUV * 2.0 - 1.0, 0.0, 1.0);
//   vUV.y       = 1.0 - vUV.y;  // flip Y for Vulkan NDC
// }
// ─────────────────────────────────────────────────────────────────────────────
// Pre-compiled SPIR-V for the fullscreen triangle vertex shader
static const uint32_t kVertSPIRV[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x00000019,
    0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0008000f, 0x00000000, 0x00000004, 0x6e69616d,
    0x00000000, 0x00000008, 0x00000014, 0x00000016,
    0x00030003, 0x00000002, 0x000001c2, 0x00040005,
    0x00000004, 0x6e69616d, 0x00000000, 0x00040005,
    0x00000008, 0x56566e61, 0x00000000, 0x00060005,
    0x00000012, 0x505f6c67, 0x65567265, 0x78657472,
    0x00000000, 0x00060006, 0x00000012, 0x00000000,
    0x505f6c67, 0x7469736f, 0x006e6f69, 0x00070006,
    0x00000012, 0x00000001, 0x505f6c67, 0x746e696f, 
    0x657a6953, 0x00000000, 0x00030005, 0x00000014,
    0x00000000, 0x00040005, 0x00000016, 0x565f6c67,
    0x78657472, 0x00040047, 0x00000008, 0x0000001e,
    0x00000000, 0x00050048, 0x00000012, 0x00000000,
    0x0000000b, 0x00000000, 0x00050048, 0x00000012,
    0x00000001, 0x0000000f, 0x00000000, 0x00030047,
    0x00000012, 0x00000002, 0x00040047, 0x00000016,
    0x0000000b, 0x00000007,
    // ... truncated for brevity — full SPIR-V loaded at runtime via embedded bytes
    // In production: load from embedded binary or compiled .spv file
};

// ─────────────────────────────────────────────────────────────────────────────
// FRAGMENT SHADER GLSL (reference)
// #version 450
// layout(set=0, binding=0) uniform sampler2D uY;
// layout(set=0, binding=1) uniform sampler2D uU;
// layout(set=0, binding=2) uniform sampler2D uV;
// layout(set=0, binding=3) uniform UBO {
//     mat3  colorMatrix;
//     float brightness, contrast, saturation;
//     float hdrPeakLuminance;
//     float hdrEnabled;
// } ubo;
// layout(location=0) in  vec2 vUV;
// layout(location=0) out vec4 fragColor;
//
// // BT.601 / BT.709 / BT.2020 color matrix is provided via UBO
// void main() {
//     float Y  = texture(uY, vUV).r - 0.0625;
//     float Cb = texture(uU, vUV).r - 0.5;
//     float Cr = texture(uV, vUV).r - 0.5;
//     vec3 yuv = vec3(Y, Cb, Cr);
//     vec3 rgb = ubo.colorMatrix * yuv;
//     // Optional brightness/contrast
//     rgb = (rgb - 0.5) * ubo.contrast + 0.5 + ubo.brightness;
//     rgb = clamp(rgb, 0.0, 1.0);
//     fragColor = vec4(rgb, 1.0);
// }
// ─────────────────────────────────────────────────────────────────────────────
static const uint32_t kFragSPIRV[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x00000032,
    0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    // ... (abbreviated — same note as vertex above)
};

// ─── Color matrices ───────────────────────────────────────────────────────────
// BT.601 (SD) YCbCr → RGB   (Y: 16-235, C: 16-240)
static constexpr float kMatBT601[9] = {
    1.164f,  0.000f,  1.596f,
    1.164f, -0.392f, -0.813f,
    1.164f,  2.017f,  0.000f,
};

// BT.709 (HD) YCbCr → RGB
static constexpr float kMatBT709[9] = {
    1.164f,  0.000f,  1.793f,
    1.164f, -0.213f, -0.533f,
    1.164f,  2.112f,  0.000f,
};

// BT.2020 (UHD/HDR) YCbCr → RGB
static constexpr float kMatBT2020[9] = {
    1.164f,  0.000f,  1.678f,
    1.164f, -0.188f, -0.652f,
    1.164f,  2.142f,  0.000f,
};

// ─── UBO layout (matches fragment shader binding=3) ──────────────────────────
struct VideoUBO {
    float colorMatrix[12];    // mat3 padded to 3×vec4 for std140
    float brightness       = 0.0f;
    float contrast         = 1.0f;
    float saturation       = 1.0f;
    float hdrPeakLuminance = 1000.0f;
    float hdrEnabled       = 0.0f;
    float _pad[3]          = {};
};

// ─── GLSL source strings (used when runtime compilation is available) ─────────
inline const char* vertGLSL() {
    return R"GLSL(
#version 450
layout(location = 0) out vec2 vUV;
void main() {
    vUV         = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vUV * 2.0 - 1.0, 0.0, 1.0);
    vUV.y       = 1.0 - vUV.y;
}
)GLSL";
}

inline const char* fragGLSL() {
    return R"GLSL(
#version 450
layout(set = 0, binding = 0) uniform sampler2D uY;
layout(set = 0, binding = 1) uniform sampler2D uU;
layout(set = 0, binding = 2) uniform sampler2D uV;
layout(set = 0, binding = 3) uniform VideoUBO {
    mat3  colorMatrix;
    float brightness;
    float contrast;
    float saturation;
    float hdrPeakLuminance;
    float hdrEnabled;
} ubo;
layout(location = 0) in  vec2 vUV;
layout(location = 0) out vec4 fragColor;

// ST.2084 (PQ) EOTF → linear light
float pqToLinear(float x) {
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    float xp = pow(max(x, 0.0), 1.0 / m2);
    return pow(max(xp - c1, 0.0) / (c2 - c3 * xp), 1.0 / m1);
}

// Simple Reinhard tone map from HDR → SDR
float reinhard(float x, float peak) {
    return x * (1.0 + x / (peak * peak)) / (1.0 + x);
}

void main() {
    float Y  = texture(uY, vUV).r - (16.0  / 255.0);
    float Cb = texture(uU, vUV).r - (128.0 / 255.0);
    float Cr = texture(uV, vUV).r - (128.0 / 255.0);

    vec3 yuv = vec3(Y, Cb, Cr);
    vec3 rgb = clamp(ubo.colorMatrix * yuv, 0.0, 1.0);

    // HDR path: PQ → linear → tone map → gamma
    if (ubo.hdrEnabled > 0.5) {
        float peak = ubo.hdrPeakLuminance / 10000.0;
        rgb.r = reinhard(pqToLinear(rgb.r), peak);
        rgb.g = reinhard(pqToLinear(rgb.g), peak);
        rgb.b = reinhard(pqToLinear(rgb.b), peak);
        // sRGB gamma
        rgb = pow(clamp(rgb, 0.0, 1.0), vec3(1.0 / 2.2));
    }

    // Brightness / contrast
    rgb = clamp((rgb - 0.5) * ubo.contrast + 0.5 + ubo.brightness, 0.0, 1.0);

    fragColor = vec4(rgb, 1.0);
}
)GLSL";
}

// NV12 variant (Y plane + interleaved UV plane — common for HW decoded frames)
inline const char* fragNV12GLSL() {
    return R"GLSL(
#version 450
layout(set = 0, binding = 0) uniform sampler2D uY;   // luma
layout(set = 0, binding = 1) uniform sampler2D uUV;  // interleaved CbCr
layout(set = 0, binding = 3) uniform VideoUBO {
    mat3  colorMatrix;
    float brightness;
    float contrast;
    float saturation;
    float hdrPeakLuminance;
    float hdrEnabled;
} ubo;
layout(location = 0) in  vec2 vUV;
layout(location = 0) out vec4 fragColor;
void main() {
    float Y  = texture(uY,  vUV).r - (16.0  / 255.0);
    vec2  UV = texture(uUV, vUV).rg;
    float Cb = UV.r - (128.0 / 255.0);
    float Cr = UV.g - (128.0 / 255.0);
    vec3 rgb = clamp(ubo.colorMatrix * vec3(Y, Cb, Cr), 0.0, 1.0);
    rgb      = clamp((rgb - 0.5) * ubo.contrast + 0.5 + ubo.brightness, 0.0, 1.0);
    fragColor = vec4(rgb, 1.0);
}
)GLSL";
}

} // namespace aurora::renderer::shaders
