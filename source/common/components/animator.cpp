#include "animator.hpp"
#include "../asset-loader.hpp"
#include "../animation/animation-loader.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <iomanip>

namespace our {

    void AnimatorComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;

        // Get skeleton from asset loader (loaded with the mesh)
        std::string skeletonName = data.value("skeleton", "");
        if (!skeletonName.empty()) {
            skeleton = getSkeleton(skeletonName);
            if (skeleton) {
                std::cout << "Loaded skeleton '" << skeletonName << "' with " << skeleton->bones.size() << " bones" << std::endl;
            } else {
                std::cerr << "Failed to find skeleton: " << skeletonName << std::endl;
                return;
            }
        }

        // Load additional animation files
        if (data.contains("animations") && data["animations"].is_object()) {
            for (auto& [name, file] : data["animations"].items()) {
                std::string animFile = file.get<std::string>();
                try {
                    auto clip = AnimationLoader::loadAnimation(animFile, skeleton);
                    animations[name] = clip;
                    std::cout << "Loaded animation '" << name << "' from " << animFile << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Failed to load animation '" << name << "': " << e.what() << std::endl;
                }
            }
        }

        currentAnimation = data.value("currentAnimation", "walk");
        isPlaying = data.value("isPlaying", true);
        loop = data.value("loop", true);
        playbackSpeed = data.value("speed", 1.0f);
        
        // Initialize bone and joint transforms
        if (skeleton) {
            boneTransforms.resize(skeleton->bones.size(), glm::mat4(1.0f));
            jointTransforms.resize(skeleton->bones.size(), glm::mat4(1.0f));
            // Log bone list for debugging mapping between mesh and skeleton
            std::cout << "Skeleton bones (count=" << skeleton->bones.size() << "):\n";
            for (size_t i = 0; i < skeleton->bones.size(); ++i) {
                std::cout << "  [" << i << "] " << skeleton->bones[i].name << std::endl;
            }
        }
    }

    void AnimatorComponent::update(float deltaTime) {
        if (!isPlaying || animations.empty() || !skeleton) {
            static bool once = true;
            if (once) {
                std::cout << "Animator update skipped: isPlaying=" << isPlaying 
                          << ", animations.size()=" << animations.size()
                          << ", skeleton=" << (skeleton ? "valid" : "null") << std::endl;
                once = false;
            }
            return;
        }

        auto it = animations.find(currentAnimation);
        if (it == animations.end()) {
            static bool once2 = true;
            if (once2) {
                std::cout << "Animation '" << currentAnimation << "' not found!" << std::endl;
                once2 = false;
            }
            return;
        }

        AnimationClip* clip = it->second.get();
        
        // Update time (scale by playbackSpeed)
        currentTime += deltaTime * clip->ticksPerSecond * playbackSpeed;
        
        // Loop or clamp
        if (loop) {
            currentTime = fmod(currentTime, clip->duration);
        } else {
            if (currentTime > clip->duration) {
                currentTime = clip->duration;
                isPlaying = false;
            }
        }

        // Calculate bone and joint transforms
        boneTransforms.resize(skeleton->bones.size());
        jointTransforms.resize(skeleton->bones.size());
        for (size_t i = 0; i < boneTransforms.size(); i++) {
            boneTransforms[i] = glm::mat4(1.0f);
            jointTransforms[i] = glm::mat4(1.0f);
        }

        // Traverse all root bones (parent == -1) to cover skeletons with multiple roots
        for (size_t i = 0; i < skeleton->parentIndices.size(); ++i) {
            if (skeleton->parentIndices[i] == -1) {
                calculateBoneTransforms(clip, currentTime, glm::mat4(1.0f), (int)i);
            }
        }
        // After calculating transforms, detect abnormally large joint positions and report
        {
            const float threshold = 1000.0f;
            static int cooldown = 0;
            bool anyLarge = false;
            for (size_t i = 0; i < jointTransforms.size(); ++i) {
                glm::vec4 p = jointTransforms[i] * glm::vec4(0,0,0,1);
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
                    anyLarge = true; break;
                }
                if (std::fabs(p.x) > threshold || std::fabs(p.y) > threshold || std::fabs(p.z) > threshold) { anyLarge = true; break; }
            }
            if (anyLarge && cooldown == 0) {
                std::cout << "[Animator] Large joint positions detected. Dumping first 16 joints:\n";
                for (size_t i = 0; i < jointTransforms.size() && i < 16; ++i) {
                    glm::vec4 p = jointTransforms[i] * glm::vec4(0,0,0,1);
                    std::cout << "  Joint[" << i << "] '" << skeleton->bones[i].name << "' pos: (" << p.x << "," << p.y << "," << p.z << ")" << std::endl;
                    // Print offset matrix first row for scale clues
                    const glm::mat4 &off = skeleton->bones[i].offsetMatrix;
                    std::cout << "    offsetMatrix first row: [" << off[0][0] << "," << off[1][0] << "," << off[2][0] << "," << off[3][0] << "]" << std::endl;
                }
                cooldown = 300; // wait a while before printing again
            }
            if (cooldown > 0) --cooldown;
        }
        
        // Debug: print once when animation starts
        static bool printedOnce = false;
        if (!printedOnce) {
            std::cout << "Animator updating: animation='" << currentAnimation 
                      << "', time=" << currentTime 
                      << ", boneTransforms.size()=" << boneTransforms.size() 
                      << ", clip->boneAnimations.size()=" << clip->boneAnimations.size() << std::endl;
            
            // Print which bones have animation data
            std::cout << "Bones with animation data: ";
            for (auto& [boneId, anim] : clip->boneAnimations) {
                if (boneId < (int)skeleton->bones.size()) {
                    std::cout << skeleton->bones[boneId].name << "(" << boneId << ") ";
                }
            }
            std::cout << std::endl;
            
            // Print first bone transform
            if (!boneTransforms.empty()) {
                auto& m = boneTransforms[0];
                std::cout << "BoneTransform[0]: [" << m[0][0] << "," << m[1][1] << "," << m[2][2] << "," << m[3][3] << "]" << std::endl;
            }
            printedOnce = true;
        }

        // Periodic debug: every 60 frames print a few joint world positions (hips, spine, head) if present
        static int frameCounter = 0;
        frameCounter++;
        if (frameCounter % 60 == 0) {
            if (skeleton) {
                auto printIfExists = [&](const std::string& name){
                    auto it = skeleton->boneMap.find(name);
                    if (it != skeleton->boneMap.end()) {
                        int id = it->second;
                        if (id >= 0 && id < (int)jointTransforms.size()) {
                            glm::vec4 p = jointTransforms[id] * glm::vec4(0,0,0,1);
                            std::cout << "Joint " << name << " (" << id << ") pos: (" << p.x << "," << p.y << "," << p.z << ")" << std::endl;
                        }
                    }
                };

                printIfExists("Zombie_Hips");
                printIfExists("Zombie_Spine");
                printIfExists("Zombie_Head");
            }
        }
    }

    void AnimatorComponent::playAnimation(const std::string& name, bool looping) {
        if (animations.find(name) != animations.end()) {
            currentAnimation = name;
            currentTime = 0.0f;
            isPlaying = true;
            loop = looping;
        }
    }

    void AnimatorComponent::calculateBoneTransforms(const AnimationClip* clip, float time, 
                                                     const glm::mat4& parentTransform, int boneIndex) {
        if (!skeleton || boneIndex >= (int)skeleton->bones.size()) return;

        const Bone& bone = skeleton->bones[boneIndex];
        // Start from the bone's local bind transform
        glm::mat4 localBind = bone.localBindTransform;

        // Get animation transform (sampled) for this bone (in local space)
        glm::mat4 animTransform = glm::mat4(1.0f);
        auto it = clip->boneAnimations.find(bone.id);
        if (it != clip->boneAnimations.end()) {
            animTransform = it->second.getTransform(time);
        }

        // Detect NaN/Inf in localBind or animTransform and print diagnostics, then sanitize
        auto mat4_is_finite = [](const glm::mat4 &m){
            for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) if (!std::isfinite(m[c][r])) return false;
            return true;
        };

        static int nanReportCount = 0;
        if (!mat4_is_finite(localBind)) {
            if (nanReportCount < 80) {
                std::cout << std::fixed << std::setprecision(6)
                          << "[Animator] NaN detected in localBind for bone '" << bone.name << "' (id=" << bone.id << ") at time=" << time << "\n";
            }
            localBind = glm::mat4(1.0f);
            nanReportCount++;
        }
        if (!mat4_is_finite(animTransform)) {
            if (nanReportCount < 80) {
                std::cout << std::fixed << std::setprecision(6)
                          << "[Animator] NaN detected in animTransform for bone '" << bone.name << "' (id=" << bone.id << ") at time=" << time << "\n";
            }
            animTransform = glm::mat4(1.0f);
            nanReportCount++;
        }

        // 1. Initialize localTransform
        glm::mat4 localTransform;

        if (it != clip->boneAnimations.end()) {
            // If animation exists, use it as the local transform
            localTransform = it->second.getTransform(time);
        } else {
            // Otherwise, fall back to the default T-Pose (bind transform)
            localTransform = bone.localBindTransform;
        }

        // Manual Fix for Rotated Jaw:
        // 1. Z-axis: -90 degrees (Un-roll from cheek)
        // 2. X-axis: +90 degrees (Yaw from Left to Forward)
        if (bone.name.find("Zombie_low_jaw1") != std::string::npos) {
             glm::mat4 correction = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0, 0, 1));
             correction = glm::rotate(correction, glm::radians(90.0f), glm::vec3(1, 0, 0));
             localTransform = localTransform * correction;
        }

        // Compute global transform by multiplying with parent
        glm::mat4 globalTransform = parentTransform * localTransform;

        // Store joint global transform (model-space) for debugging / visualization
        if (bone.id >= 0 && bone.id < (int)jointTransforms.size()) {
            jointTransforms[bone.id] = globalTransform;
        }

        // Final transform: GlobalTransform * OffsetMatrix
        boneTransforms[bone.id] = skeleton->globalInverseTransform * globalTransform * bone.offsetMatrix;

        // Process children
        for (size_t i = 0; i < skeleton->parentIndices.size(); i++) {
            if (skeleton->parentIndices[i] == boneIndex) {
                calculateBoneTransforms(clip, time, globalTransform, i);
            }
        }
    }

}
