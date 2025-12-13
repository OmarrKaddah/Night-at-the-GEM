#version 330 core

in Varyings {
    vec3 world_pos;
    vec3 world_normal;
    vec2 tex_coord;
    vec4 color;
} fs_in;

out vec4 frag_color;

// Beam properties - can be set as uniforms or hardcoded
uniform vec3 beam_color;
uniform float beam_top_y;      // Y position of ceiling
uniform float beam_bottom_y;   // Y position of floor
uniform float beam_intensity;
uniform float beam_fade;

void main() {
    // Default values if uniforms not set
    vec3 color = beam_color;
    if (length(color) < 0.01) color = vec3(2.5, 2.8, 3.2); // Default moonlight color
    
    float top = beam_top_y;
    if (top < 0.01) top = 8.0; // Default ceiling height
    
    float bottom = beam_bottom_y;
    if (bottom < 0.01) bottom = 0.0; // Default floor height
    
    float intensity = beam_intensity;
    if (intensity < 0.01) intensity = 1.0;
    
    float fade = beam_fade;
    if (fade < 0.01) fade = 1.5;
    
    // Calculate distance along beam (0 = top, 1 = bottom)
    float beam_length = top - bottom;
    if (beam_length < 0.01) beam_length = 1.0;
    
    float t = (fs_in.world_pos.y - bottom) / beam_length;
    t = clamp(t, 0.0, 1.0);
    
    // Create gradient: bright at top, fading at bottom
    float gradient = 1.0 - t;
    gradient = pow(gradient, fade);
    
    // Create radial falloff (brighter in center, darker at edges)
    // Use texture coordinate to determine distance from center
    vec2 center = vec2(0.5, 0.5);
    float dist_from_center = length(fs_in.tex_coord - center);
    float radial_falloff = 1.0 - smoothstep(0.0, 0.5, dist_from_center);
    
    // Combine gradient and radial falloff
    float alpha = gradient * radial_falloff * intensity;
    
    // Output color with alpha
    frag_color = vec4(color * alpha, alpha);
}

