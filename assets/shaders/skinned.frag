#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
    vec3 normal;
    vec3 world;
} fs_in;

out vec4 frag_color;

uniform sampler2D tex;

uniform float alphaThreshold;
uniform vec4 tint;

void main() {
    // Combine texture, vertex color, and tint
    // Removed fract() as it causes distortion on character models
    vec4 tex_color = texture(tex, fs_in.tex_coord);
    
    // Apply tint and vertex color
    vec4 final_color = fs_in.color * tex_color * tint;
    
    // Alpha Thresholding disabled as user requested full texture visibility
    // if(final_color.a < alphaThreshold) discard;
    
    frag_color = final_color;
}
