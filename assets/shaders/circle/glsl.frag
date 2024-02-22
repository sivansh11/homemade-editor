#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 center;
layout (location = 2) in vec2 uv;

layout (location = 0) out vec4 o_color;

layout (push_constant) uniform circle_push_constant_t {
    mat4 model_matrix;
    vec4 color;
    float r;
};

void main() {
    if (length(position - center) > r) discard;
    o_color = color;
}