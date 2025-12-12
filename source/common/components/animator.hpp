#pragma once

#include "../ecs/component.hpp"
#include "../animation/animation.hpp"
#include <memory>

namespace our {

    class AnimatorComponent : public Component {
    public:
        std::shared_ptr<Skeleton> skeleton;
        std::map<std::string, std::shared_ptr<AnimationClip>> animations;
        
        std::string currentAnimation = "idle1";
        float currentTime = 0.0f;
        bool isPlaying = true;
        bool loop = true;
        float playbackSpeed = 1.0f; // multiplier for animation playback (1.0 = normal)
        
        std::vector<glm::mat4> boneTransforms;  // Final bone transforms to send to shader
        std::vector<glm::mat4> jointTransforms; // Global joint transforms (model-space) for debug drawing

        static std::string getID() { return "Animator"; }

        void deserialize(const nlohmann::json& data) override;
        
        // Update animation and calculate bone transforms
        void update(float deltaTime);
        
        // Play specific animation
        void playAnimation(const std::string& name, bool looping = true);
        
    private:
        void calculateBoneTransforms(const AnimationClip* clip, float time, const glm::mat4& parentTransform, int boneIndex);
    };

}
