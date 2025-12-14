#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in vec4 color;
layout(location = 4) in ivec4 boneIDs;
layout(location = 5) in vec4 boneWeights;

out Varyings {
    vec3 world_pos;
    vec3 world_normal;
    vec2 tex_coord;
    vec4 color;
} vs_out;

uniform mat4 transform;     // VP * Model
uniform mat4 model;         // Model matrix for world position
uniform mat4 model_IT;      // Inverse transpose of model for normals
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

        // Safety check: If no VALID bones contributed, fallback to bind pose
        if (usedWeight > 0.001) {
            // Apply the combined movement to position and normal
            localPosition = boneTransform * localPosition;
            localNormal = mat3(boneTransform) * normal;
        }
    }

    // 3. Transform to world space for lighting
    vs_out.world_pos = vec3(model * localPosition);
    vs_out.world_normal = normalize(vec3(model_IT * vec4(localNormal, 0.0)));
    vs_out.tex_coord = tex_coord;
    vs_out.color = color;
    
    // 4. Transform to clip space for rendering
    gl_Position = transform * localPosition;
}
