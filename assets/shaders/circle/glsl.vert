#version 450

layout (location = 0) out vec2 o_position;
layout (location = 1) out vec2 o_center;
layout (location = 2) out vec2 o_uv;

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

layout (push_constant) uniform circle_push_constant_t {
    mat4 model_matrix;
    vec4 color;
    float r;
};

layout (binding = 0, set = 0) uniform uniform_buffer_t {
    mat4 projection;
};

void main() {
    vec4 world_position = model_matrix * vec4(positions[gl_VertexIndex], 0, 1);
    gl_Position = projection * world_position;
    o_position = world_position.xy;
    o_center = (model_matrix * vec4(0, 0, 0, 1)).xy;
    o_uv = uv[gl_VertexIndex];
}