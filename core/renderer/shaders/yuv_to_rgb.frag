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

// ST.2084 PQ EOTF → linear light
float pqToLinear(float x) {
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    float xp = pow(max(x, 0.0), 1.0 / m2);
    return pow(max(xp - c1, 0.0) / (c2 - c3 * xp), 1.0 / m1);
}

void main() {
    float Y  = texture(uY, vUV).r - (16.0  / 255.0);
    float Cb = texture(uU, vUV).r - (128.0 / 255.0);
    float Cr = texture(uV, vUV).r - (128.0 / 255.0);

    vec3 yuv = vec3(Y, Cb, Cr);
    vec3 rgb = clamp(ubo.colorMatrix * yuv, 0.0, 1.0);

    if (ubo.hdrEnabled > 0.5) {
        float peak = ubo.hdrPeakLuminance / 10000.0;
        rgb.r = pqToLinear(rgb.r);
        rgb.g = pqToLinear(rgb.g);
        rgb.b = pqToLinear(rgb.b);
        // Reinhard
        rgb   = rgb * (1.0 + rgb / (peak * peak)) / (1.0 + rgb);
        rgb   = pow(clamp(rgb, 0.0, 1.0), vec3(1.0 / 2.2));
    }

    rgb = clamp((rgb - 0.5) * ubo.contrast + 0.5 + ubo.brightness, 0.0, 1.0);
    fragColor = vec4(rgb, 1.0);
}
