#version 450
// NV12 variant: Y plane + interleaved UV plane (common for HW decoded frames)

layout(set = 0, binding = 0) uniform sampler2D uY;
layout(set = 0, binding = 1) uniform sampler2D uUV;   // interleaved CbCr

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
