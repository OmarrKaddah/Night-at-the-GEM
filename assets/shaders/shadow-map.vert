#version 330 core

layout(location = 0) in vec3 position;

uniform mat4 lightSpaceMatrix;  // Light's view-projection matrix
uniform mat4 model;              // Model matrix

void main() {
    gl_Position = lightSpaceMatrix * model * vec4(position, 1.0);
}