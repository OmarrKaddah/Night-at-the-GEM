#version 330

// The texture holding the scene pixels
uniform sampler2D tex;

// Read "assets/shaders/fullscreen.vert" to know what "tex_coord" holds;
in vec2 tex_coord;

out vec4 frag_color;

// Vignette is a postprocessing effect that darkens the corners of the screen
// to grab the attention of the viewer towards the center of the screen

void main(){
    // Compute NDC position from texture coord (0..1 -> -1..1)
    vec2 ndc = tex_coord * 2.0 - vec2(1.0);
    float len2 = dot(ndc, ndc);
    vec4 color = texture(tex, tex_coord);
    // Divide the scene color by (1 + len^2) to darken corners
    frag_color = color / (1.0 + len2);
}