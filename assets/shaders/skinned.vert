#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in vec3 normal;
layout(location = 4) in ivec4 boneIDs;
layout(location = 5) in vec4 boneWeights;

out Varyings {
    vec4 color;
    vec2 tex_coord;
    vec3 normal;
    vec3 world; // This is used for lighting later
} vs_out;

uniform mat4 transform; // VP * Model
const int MAX_BONES = 100;
uniform mat4 boneTransforms[MAX_BONES];
uniform bool useSkinning = false;

void main() {
    // 1. Start with raw "T-Pose" data
    vec4 localPosition = vec4(position, 1.0);
    vec3 localNormal = normal;
    
    // 2. Calculate the combined Bone Matrix
    if (useSkinning) {
        mat4 boneTransform = mat4(0.0);
        
        float usedWeight = 0.0;
        
        for(int i = 0; i < 4; i++) {
            int boneID = boneIDs[i];
            // Only use the bone if the index is valid
            if(boneID >= 0 && boneID < MAX_BONES) {
                boneTransform += boneTransforms[boneID] * boneWeights[i];
                usedWeight += boneWeights[i];
            }
        }

        // Safety check: If no VALID bones contributed (usedWeight near 0), 
        // implies the vertex is weighted to missing bones (e.g. Jaw).
        // Fallback to bind pose (original position) to prevent collapse.
        if (usedWeight > 0.001) {
            // Apply the combined movement to position and normal
            localPosition = boneTransform * localPosition;
            localNormal = mat3(boneTransform) * normal;
        }
        // Else: fall through with original localPosition (Bind Pose)
    }

    // 3. Move from Object Space to Clip Space (for the screen)
    gl_Position = transform * localPosition;

    // 4. Pass data to the Fragment Shader (THE FIXES ARE HERE)
    vs_out.normal = normalize(localNormal); // FIX 1: Fixed name and added normalization
    vs_out.tex_coord = tex_coord;
    vs_out.color = color;
    vs_out.world = localPosition.xyz;       // FIX 2: Assigned the world position
}