#version 330

// The texture holding the scene pixels
uniform sampler2D tex;

// Read "assets/shaders/fullscreen.vert" to know what "tex_coord" holds;
in vec2 tex_coord;
out vec4 frag_color;

// How far (in the texture space) is the distance (on the x-axis) between
// the pixels from which the red/green (or green/blue) channels are sampled
#define STRENGTH 0.005

// Chromatic aberration mimics some old cameras where the lens disperses light
// differently based on its wavelength. In this shader, we will implement a
// cheap version of that effect 

void main(){
    // Sample green from the original coordinate
    vec4 gSample = texture(tex, tex_coord);
    // Sample red shifted to the left
    vec4 rSample = texture(tex, tex_coord - vec2(STRENGTH, 0.0));
    // Sample blue shifted to the right
    vec4 bSample = texture(tex, tex_coord + vec2(STRENGTH, 0.0));

    // Compose final color: red from rSample, green from gSample, blue from bSample, alpha from gSample
    frag_color = vec4(rSample.r, gSample.g, bSample.b, gSample.a);
}