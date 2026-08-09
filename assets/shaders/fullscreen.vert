#version 460

layout(location = 0) out vec2 v_texture;

void main() {
    vec2 pos[3] = vec2[](
            vec2(-1, -1),
            vec2(3, -1),
            vec2(-1, 3)
    );
    gl_Position = vec4(pos[gl_VertexID], 0, 1);
    v_texture = gl_Position.xy * 0.5 + 0.5;
}
