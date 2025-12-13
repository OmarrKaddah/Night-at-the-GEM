#version 330

// The texture holding the scene pixels
uniform sampler2D tex;

// Read "assets/shaders/fullscreen.vert" to know what "tex_coord" holds;
in vec2 tex_coord;

out vec4 frag_color;

// Vignette is a postprocessing effect that darkens the corners of the screen
// to grab the attention of the viewer towards the center of the screen

uniform float damage_factor; // 0.0 (healthy) -> 1.0 (dead)

void main(){
    // Standard vignette calculation
    vec2 ndc = tex_coord * 2.0 - vec2(1.0);
    float len2 = dot(ndc, ndc);
    
    vec4 color = texture(tex, tex_coord);

    // --- Vignette & Damage Effect ---
    // Base vignette: Divide the scene color by (1 + len^2) to darken corners
    vec4 vignetteColor = color / (1.0 + len2);
    
    // Red Vignette: Mix red at the edges based on damage
    vec3 red = vec3(1.0, 0.0, 0.0);
    
    // Calculate intensity of red overlay at this pixel
    float redIntensity = smoothstep(0.4, 1.2, len2) * damage_factor * 0.8;
    
    frag_color = vec4(mix(vignetteColor.rgb, red, redIntensity), 1.0);
}