#version 330



// This vertex shader should be used to render a triangle whose normalized device coordinates are:
// (-0.5, -0.5, 0.0), ( 0.5, -0.5, 0.0), ( 0.0,  0.5, 0.0)
// And it also should send the vertex color as a varying to the fragment shader where the colors are (in order):
// (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)

//each vertex will output a color to the fragment shader
out vec3 vertexColor;


// Currently, the triangle is always in the same position, but we don't want that.
// So two uniforms should be added: translation (vec2) and scale (vec2).
// Each vertex "v" should be transformed to be "scale * v + translation".
// The default value for "translation" is (0.0, 0.0) and for "scale" is (1.0, 1.0).

//TODO: (Req 1) Finish this shader

//uniform is variable that stays the same for every draw call
//VBO is memory on gpu that stores vertex data -> here we hardcode vertices

uniform vec2 translation = vec2(0.0,0.0);
uniform vec2 scale = vec2(1.0,1.0);




void main(){

//hardcode 3 vertices

    vec2 positions[3] = vec2[](
        vec2(-0.5,-0.5),
        vec2(0.5,-0.5),
        vec2(0.0,0.5)
    );

    vec3 colors[3] = vec3[](
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0)
    );


    //set position and color based on built in gl_VertexID
   

    vec2 position = positions[gl_VertexID];
    vec3 col = colors[gl_VertexID];

    //edrab el hardcoded position fel uniforms

    position = scale*position + translation;

    //write final position to gpu last 2 variables are z=0 since 2d and w which controls perspective & normalization
    

    gl_Position = vec4(position, 0.0, 1.0);

    //send color to fragment shader

    vertexColor = col;



}