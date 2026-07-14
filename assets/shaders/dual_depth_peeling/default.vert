#version 460

layout(location = 0) in vec4 a_position;

layout(std140, binding = 0) uniform SceneData {
    mat4 view_projection;
    vec3 camera_position;
    float _pad0;
};

void main() {
    gl_Position = view_projection * a_position;
}
