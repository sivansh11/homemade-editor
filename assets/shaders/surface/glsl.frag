#version 450

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 o_color;

layout (push_constant) uniform rect_push_constant_t {
    mat4 model_matrix;
};

layout (binding = 0, set = 1) uniform sampler2D surface_image;

void main() {
    o_color = texture(surface_image, uv);
}