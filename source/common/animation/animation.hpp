#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <map>

namespace our {

    #define MAX_BONES 100
    #define MAX_BONE_INFLUENCE 4

    // Represents a single bone in the skeleton
    struct Bone {
        std::string name;
        int id;
        glm::mat4 offsetMatrix;  // Transforms from mesh space to bone space
        glm::mat4 localBindTransform; // Node's local bind transform (from the model)
    };

    // Keyframe for position animation
    struct PositionKeyframe {
        float time;
        glm::vec3 position;
    };

    // Keyframe for rotation animation
    struct RotationKeyframe {
        float time;
        glm::quat rotation;
    };

    // Keyframe for scale animation
    struct ScaleKeyframe {
        float time;
        glm::vec3 scale;
    };

    // Animation for a single bone
    struct BoneAnimation {
        std::vector<PositionKeyframe> positionKeyframes;
        std::vector<RotationKeyframe> rotationKeyframes;
        std::vector<ScaleKeyframe> scaleKeyframes;

        // Get interpolated transform at given time
        glm::mat4 getTransform(float time) const;
    };

    // Complete animation clip (e.g., "walk", "die")
    struct AnimationClip {
        std::string name;
        float duration;  // in seconds
        float ticksPerSecond;
        std::map<int, BoneAnimation> boneAnimations;  // bone id -> animation
    };

    // Skeleton with bone hierarchy
    struct Skeleton {
        std::vector<Bone> bones;
        std::map<std::string, int> boneMap;  // name -> bone id
        std::vector<int> parentIndices;  // parent bone id for each bone
        glm::mat4 globalInverseTransform;

        int getBoneId(const std::string& name) const {
            auto it = boneMap.find(name);
            return (it != boneMap.end()) ? it->second : -1;
        }
    };

}
