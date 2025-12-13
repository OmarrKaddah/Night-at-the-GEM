#version 330

// The texture holding the scene pixels
uniform sampler2D tex;

// Read "assets/shaders/fullscreen.vert" to know what "tex_coord" holds;
in vec2 tex_coord;

out vec4 frag_color;

uniform float damage_factor; // 0.0 (healthy) -> 1.0 (dead)

// Simple pseudo-random hash
float hash(vec2 pixel) {
    return fract(sin(dot(pixel, vec2(12.9898, 78.233))) * 43758.5453);
}

// Value Noise
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// Fractal Brownian Motion for more complex shape
float fbm(vec2 p) {
    float total = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++) {
        total += noise(p) * amplitude;
        p *= 2.0;
        amplitude *= 0.5;
    }
    return total;
}

void main(){
    vec4 sceneColor = texture(tex, tex_coord);
    
    // --- Static Vignette (Subtle, constant) ---
    vec2 ndc = tex_coord * 2.0 - 1.0;
    float len2 = dot(ndc, ndc);
    // Subtle darkening: 1.0 / (1.0 + len2 * intensity)
    float vignette = 1.0 / (1.0 + len2 * 0.3); // 0.3 strength for "tiny bit"
    
    // Slight Red Tint on edges
    // We mix the scene color with a dark red based on how much the vignette darkens (1.0 - vignette)
    vec3 vignetteColor = vec3(0.2, 0.0, 0.0); // Subtle dark red
    sceneColor.rgb = mix(sceneColor.rgb * vignette, vignetteColor, (1.0 - vignette) * 0.5);
    
    if (damage_factor <= 0.01) {
        frag_color = sceneColor;
        return;
    }

    // Coordinates for noise
    vec2 uv = tex_coord;
    
    // Generate noise map
    // Scale 10.0 for chunks
    float n = fbm(uv * 10.0);
    
    // Bias towards edges using distance from center
    // vec2 ndc = uv * 2.0 - 1.0; // Already declared above
    float dist = length(ndc);
    
    // "Blood Threshold"
    // We want blood to show where n * dist is high enough
    // As damage_factor increases, the threshold should lower (or the value increase)
    
    // Modulate noise by distance to keep center cleaner
    float splatter = n * smoothstep(0.2, 1.0, dist);
    
    // Hard cutoff for droplet look
    float threshold = 1.0 - (damage_factor * 0.7); // At max damage, threshold is 0.3
    
    float bloodMask = 0.0;
    if (splatter > threshold) {
        bloodMask = 1.0;
    }
    
    // Soften edges slightly
    bloodMask = smoothstep(threshold, threshold - 0.05, splatter); // Invert: wait, if splatter > threshold
    
    // Correct smoothstep: edge0 < edge1
    // We want 0 if splatter < threshold, 1 if splatter > threshold + small range
    bloodMask = smoothstep(threshold, threshold + 0.1, splatter);

    // Deep red blood color
    vec3 bloodColor = vec3(0.5, 0.0, 0.0);
    
    // Mix
    frag_color = vec4(mix(sceneColor.rgb, bloodColor, bloodMask * 0.9), 1.0);
}
