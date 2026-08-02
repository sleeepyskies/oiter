#version 460

layout(std140, binding = 0) uniform SceneData {
    mat4 view_projection;
    vec3 camera_position;
    float _pad0;
};

layout(std140, binding = 2) uniform DrawCallData {
    mat4 model;
    uint index;
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
    v_position = model * a_position;
    v_normal = vec4(normalize(mat3(transpose(inverse(model))) * a_normal.xyz), 0.0);
    v_color = a_color;
    v_texture = a_texture;
    v_tangent = a_tangent;
    gl_Position = view_projection * model * a_position;
}
