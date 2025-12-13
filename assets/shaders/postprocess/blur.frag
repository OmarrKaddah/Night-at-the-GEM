#version 330

// The texture holding the scene pixels
uniform sampler2D tex;

// Read "assets/shaders/fullscreen.vert" to know what "tex_coord" holds;
in vec2 tex_coord;

out vec4 frag_color;

uniform float blur_strength; // 0.0 (no blur) -> 1.0 (max blur)

void main(){
    if(blur_strength > 0.01) {
        vec2 tex_offset = 1.0 / textureSize(tex, 0) * (2.0 + 8.0 * blur_strength); // Offset amount
        vec4 result = texture(tex, tex_coord); 
        // Simple 9-tap box blur
        result += texture(tex, tex_coord + vec2(tex_offset.x, 0.0));
        result += texture(tex, tex_coord - vec2(tex_offset.x, 0.0));
        result += texture(tex, tex_coord + vec2(0.0, tex_offset.y));
        result += texture(tex, tex_coord - vec2(0.0, tex_offset.y));
        result += texture(tex, tex_coord + vec2(tex_offset.x, tex_offset.y));
        result += texture(tex, tex_coord + vec2(-tex_offset.x, tex_offset.y));
        result += texture(tex, tex_coord + vec2(tex_offset.x, -tex_offset.y));
        result += texture(tex, tex_coord + vec2(-tex_offset.x, -tex_offset.y));
        frag_color = result / 9.0;
    } else {
        frag_color = texture(tex, tex_coord);
    }
}
