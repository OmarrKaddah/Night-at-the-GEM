#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
} fs_in;

out vec4 frag_color;

uniform vec4 tint;
uniform sampler2D tex;
uniform float alphaThreshold;

void main(){
    //TODO: (Req 7) Modify the following line to compute the fragment color
    //vec4 tex_color = texture(tex, fs_in.tex_coord);
    //vec4 final_color = fs_in.color * tex_color * tint;
    //if(final_color.a < alphaThreshold)
      //  discard;
    //frag_color = final_color;
    //frag_color = texture(tex, fs_in.tex_coord) * tint * fs_in.color;
    vec2 uv = fract(fs_in.tex_coord);
    frag_color = texture(tex, uv) * tint * fs_in.color;

}