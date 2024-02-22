#version 450

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 o_color;

layout (push_constant) uniform text_push_constant_t {
    mat4 model_matrix;
    vec4 color;
    vec2 tex_coord_min;
    vec2 tex_coord_max;
};

layout (binding = 0, set = 1) uniform sampler2D atlas;

float screen_px_range();
float median(float r, float g, float b);

void main() {
    vec3 msd = texture(atlas, uv).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screen_px_distance = screen_px_range() * (sd - 0.5);
    float opacity = clamp(screen_px_distance + 0.5, 0.0, 1.0);
    if (opacity == 0.0) discard;
    vec4 bg_color = vec4(0.0);
    o_color = mix(bg_color, color, opacity);
}

float screen_px_range() {
    const float px_range = 2.0;
    vec2 unit_range = vec2(px_range) / vec2(textureSize(atlas, 0));
    vec2 screen_tex_size = vec2(1.0) / fwidth(uv);
    return max(0.5 * dot(unit_range, screen_tex_size), 1.0);
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}