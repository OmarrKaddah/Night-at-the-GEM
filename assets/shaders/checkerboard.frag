#version 330 core

out vec4 frag_color;

// In this shader, we want to draw a checkboard where the size of each tile is (size x size).
// The color of the top-left most tile should be "colors[0]" and the 2 tiles adjacent to it
// should have the color "colors[1]".

//TODO: (Req 1) Finish this shader.

uniform int size = 32;
uniform vec3 colors[2];

void main(){

    //hat pixel coordinates
    ivec2 pixel_coords = ivec2(gl_FragCoord.xy);

    //which checker tile are we in
    ivec2 checker_coords = pixel_coords / size;

    int sum = checker_coords.x + checker_coords.y;
    if (sum % 2 == 0)
        frag_color = vec4(colors[0], 1.0);
    else
        frag_color = vec4(colors[1], 1.0);

       

  
}