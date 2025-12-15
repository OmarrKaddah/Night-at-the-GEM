#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

namespace our {

    // Since we may want to store colors in bytes instead of floats for efficiency,
    // we are creating our own 32-bit R8G8B8A8 Color data type with the default GLM precision
    typedef glm::vec<4, glm::uint8, glm::defaultp> Color;

    // Maximum number of bones that can influence a single vertex
    constexpr int MAX_BONE_INFLUENCE = 4;

    struct Vertex {
        glm::vec3 position;     // The vertex position in the local space
        Color color;            // The vertex color
        glm::vec2 tex_coord;    // The texture coordinates (the vertex position in the texture space)
        glm::vec3 normal;       // The surface normal at the vertex (This will be used for lighting in the final phase)
        
        // Bone data for skeletal animation
        int boneIDs[MAX_BONE_INFLUENCE] = {-1, -1, -1, -1};      // IDs of bones that influence this vertex
        float boneWeights[MAX_BONE_INFLUENCE] = {0.0f, 0.0f, 0.0f, 0.0f}; // Weights of bone influence

        Vertex() = default;

        Vertex(glm::vec3 position, Color color, glm::vec2 tex_coord, glm::vec3 normal) 
            : position(position), color(color), tex_coord(tex_coord), normal(normal) {}

        // We plan to use this as a key for a map so we need to define the equality operator
        bool operator==(const Vertex& other) const {
            return position == other.position &&
                   color == other.color &&
                   tex_coord == other.tex_coord &&
                   normal == other.normal;
        }
    };

}

// We plan to use struct Vertex as a key for a map so we need to define a hash function for it
namespace std {
    //A Simple method to combine two hash values
    inline size_t hash_combine(size_t h1, size_t h2){ return h1 ^ (h2 << 1); }

    //A Hash function for struct Vertex
    template<> struct hash<our::Vertex> {
        size_t operator()(our::Vertex const& vertex) const {
            size_t combined = hash<glm::vec3>()(vertex.position);
            combined = hash_combine(combined, hash<our::Color>()(vertex.color));
            combined = hash_combine(combined, hash<glm::vec2>()(vertex.tex_coord));
            combined = hash_combine(combined, hash<glm::vec3>()(vertex.normal));
            return combined;
        }
    };
}
