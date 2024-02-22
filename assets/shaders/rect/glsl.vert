#version 450

layout (location = 0) out vec2 o_uv;

vec2 positions[6] = vec2[](
    vec2(-1, -1),
    vec2(-1,  1),
    vec2( 1,  1),
    vec2(-1, -1),
    vec2( 1,  1),
    vec2( 1, -1)
);

vec2 uv[6] = vec2[](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(1, 0)
);

layout (push_constant) uniform rect_push_constant_t {
    mat4 model_matrix;
    vec4 color;
};

layout (binding = 0, set = 0) uniform uniform_buffer_t {
    mat4 projection;
};

void main() {
    gl_Position = projection * model_matrix * vec4(positions[gl_VertexIndex], 0, 1);
    o_uv = uv[gl_VertexIndex];
}