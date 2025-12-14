#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in vec4 color;

uniform mat4 transform;
uniform mat4 model;
uniform mat4 model_IT;

out Varyings {
    vec3 world_pos;
    vec3 world_normal;
    vec2 tex_coord;
    vec4 color;
} vs_out;

void main() {
    vs_out.world_pos = vec3(model * vec4(position, 1.0));
    vs_out.world_normal = normalize(vec3(model_IT * vec4(normal, 0.0)));
    vs_out.tex_coord = tex_coord;
    vs_out.color = color;
    gl_Position = transform * vec4(position, 1.0);
}


