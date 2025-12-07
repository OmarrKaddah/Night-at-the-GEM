#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in vec3 normal;

out Varyings {
    vec3 world_pos;
    vec3 world_normal;
    vec2 tex_coord;
    vec4 color;
} vs_out;

uniform mat4 transform;    // MVP matrix
uniform mat4 model;        // Model matrix for world position
uniform mat4 model_IT;     // Inverse transpose for normals

void main() {
    gl_Position = transform * vec4(position, 1.0);
    vs_out.world_pos = vec3(model * vec4(position, 1.0));
    vs_out.world_normal = normalize(mat3(model_IT) * normal);
    vs_out.tex_coord = tex_coord;
    vs_out.color = color;
}
