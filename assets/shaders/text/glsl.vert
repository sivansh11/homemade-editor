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
    vec2(0, 0),  // 0 | v0
    vec2(0, 1),  // 1 | v1
    vec2(1, 1),  // 2 | v2
    vec2(0, 0),  // 3 | v0
    vec2(1, 1),  // 4 | v2
    vec2(1, 0)   // 5 | v3
);

layout (push_constant) uniform text_push_constant_t {
    mat4 model_matrix;
    vec4 color;
    vec2 tex_coord_min;
    vec2 tex_coord_max;
};

layout (binding = 0, set = 0) uniform uniform_buffer_t {
    mat4 projection;
};

void main() {
    gl_Position = projection * model_matrix * vec4(positions[gl_VertexIndex], 0, 1);
    switch (gl_VertexIndex) {
        case 0:
        case 3:
            o_uv = tex_coord_min;
            break;
        case 1:
            o_uv = vec2(tex_coord_min.x, tex_coord_max.y);
            break;
        case 2:
        case 4:
            o_uv = tex_coord_max;
            break;
        case 5:
            o_uv = vec2(tex_coord_max.x, tex_coord_min.y);
            break;
    }
}