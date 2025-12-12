#ifndef MESH_LOADER_HPP
#define MESH_LOADER_HPP

#include "mesh.hpp"
#include "../animation/animation.hpp"
#include <string>
#include <memory>

namespace our {

    namespace mesh_utils {
        
        // Load an animated mesh from various formats (FBX, glTF, GLB, etc.) with bone weights
        // Uses Assimp to support multiple 3D model formats
        // Also returns the skeleton for this mesh
        Mesh* loadAnimatedMesh(const std::string& filename, std::shared_ptr<Skeleton>& outSkeleton);
        
    } // namespace mesh_utils

} // namespace our

#endif // MESH_LOADER_HPP
