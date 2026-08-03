#version 460

struct BakedMaterial {
    vec4 color;
};

layout(std140, binding = 0) uniform SceneUniforms {
    mat4 projection_view;
    vec3 camera_position;
    float _pad0;
};

layout(std140, binding = 1) uniform MeshUniforms {
    BakedMaterial material;
    mat4 model;
};

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec4 a_color;
layout(location = 3) in vec2 a_texture;
layout(location = 4) in vec4 a_tangent;

layout(location = 0) out vec4 v_position;
layout(location = 1) out vec4 v_normal;
layout(location = 2) out vec4 v_color;
layout(location = 3) out vec2 v_texture;
layout(location = 4) out vec4 v_tangent;

void main() {
    vec2 pos[3] = vec2[](
            vec2(-1, -1),
            vec2(3, -1),
            vec2(-1, 3)
    );

    gl_Position = vec4(pos[gl_VertexID], 0, 1);
}
