#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "../common/animation/animation-loader.hpp"
#include "../common/components/animator.hpp"

int main(int argc, char** argv) {
    std::string base = "assets/models/";
    std::string skelFile = base + "zombie.glb";
    std::string animFile = base + "zombie@walk.glb";

    if (argc >= 3) {
        skelFile = argv[1];
        animFile = argv[2];
    } else if (argc == 2) {
        skelFile = argv[1];
    }

    try {
        std::cout << "--- Animation Test Runner (stepping animator) ---" << std::endl;
        std::cout << "Loading skeleton: " << skelFile << std::endl;
        auto skeleton = our::AnimationLoader::loadSkeleton(skelFile);
        if (!skeleton) {
            std::cerr << "Failed to load skeleton" << std::endl;
            return 1;
        }

        std::cout << "Loading animation: " << animFile << std::endl;
        auto clip = our::AnimationLoader::loadAnimation(animFile, skeleton);
        if (!clip) {
            std::cerr << "Failed to load animation" << std::endl;
            return 1;
        }

        // Diagnostic: print keyframe info for a few important bones (hips)
        auto itHip = skeleton->boneMap.find("Zombie_Hips");
        if (itHip != skeleton->boneMap.end()) {
            int hipId = itHip->second;
            std::cout << "Debug: Zombie_Hips id=" << hipId << std::endl;
            auto itAnim = clip->boneAnimations.find(hipId);
            if (itAnim != clip->boneAnimations.end()) {
                const auto &b = itAnim->second;
                std::cout << "  Hip position keys=" << b.positionKeyframes.size()
                          << " rot keys=" << b.rotationKeyframes.size()
                          << " scale keys=" << b.scaleKeyframes.size() << std::endl;
                // Dump up to 32 keys for inspection
                size_t maxDump = std::min<size_t>(b.positionKeyframes.size(), 32);
                for (size_t k = 0; k < maxDump; ++k) {
                    auto &pk = b.positionKeyframes[k];
                    std::cout << "    posKey[" << k << "] time=" << pk.time
                              << " pos=(" << pk.position.x << "," << pk.position.y << "," << pk.position.z << ")" << std::endl;
                }
                size_t maxDumpR = std::min<size_t>(b.rotationKeyframes.size(), 32);
                for (size_t k = 0; k < maxDumpR; ++k) {
                    auto &rk = b.rotationKeyframes[k];
                    std::cout << "    rotKey[" << k << "] time=" << rk.time
                              << " quat=(" << rk.rotation.w << "," << rk.rotation.x << "," << rk.rotation.y << "," << rk.rotation.z << ")" << std::endl;
                }

                // Sample the bone transform at t=0 and at one frame (dt)
                float dt = 1.0f / 60.0f;
                glm::mat4 t0 = b.getTransform(0.0f);
                glm::mat4 t1 = b.getTransform(dt * clip->ticksPerSecond);
                auto printMat = [&](const glm::mat4 &m, const char *label){
                    std::cout << "    " << label << " mat: ";
                    for (int r=0;r<4;++r) for (int c=0;c<4;++c) std::cout << m[c][r] << (r==3 && c==3 ? '\n' : ' ');
                };
                printMat(t0, "transform@0");
                printMat(t1, "transform@1frame");
            } else {
                std::cout << "  No animation data for Zombie_Hips in clip" << std::endl;
            }
        } else {
            std::cout << "Debug: Zombie_Hips not found in skeleton" << std::endl;
        }

        // Print globalInverse and per-bone offset/localBind for hips/spine/head
        auto printMat4 = [&](const glm::mat4 &m, const std::string &label){
            std::cout << label << " =\n";
            for (int r=0;r<4;++r) {
                std::cout << "  ";
                for (int c=0;c<4;++c) std::cout << m[c][r] << (c==3? '\n' : ' ');
            }
        };

        std::cout << "Skeleton globalInverseTransform:" << std::endl;
        printMat4(skeleton->globalInverseTransform, "globalInverse");

        auto dumpBone = [&](const std::string &name){
            auto it = skeleton->boneMap.find(name);
            if (it == skeleton->boneMap.end()) return;
            int id = it->second;
            std::cout << "Bone[" << id << "] " << name << " offsetMatrix:" << std::endl;
            printMat4(skeleton->bones[id].offsetMatrix, "offset");
            std::cout << "Bone[" << id << "] " << name << " localBindTransform:" << std::endl;
            printMat4(skeleton->bones[id].localBindTransform, "localBind");
        };

        dumpBone("Zombie_Hips");
        dumpBone("Zombie_Spine");
        dumpBone("Zombie_Head");

        // Set up a simple animator and attach skeleton + clip
        our::AnimatorComponent animator;
        animator.skeleton = skeleton;
        animator.animations.clear();
        animator.animations["test"] = clip;
        animator.currentAnimation = "test";
        animator.isPlaying = true;
        animator.loop = true;

        // Initialize transforms
        animator.boneTransforms.resize(skeleton->bones.size(), glm::mat4(1.0f));
        animator.jointTransforms.resize(skeleton->bones.size(), glm::mat4(1.0f));

        std::cout << "Starting simulation: stepping animator for 600 frames (10 seconds at 60fps)" << std::endl;
        std::cout << "Clip duration (ticks): " << clip->duration << " ticks, ticksPerSecond: " << clip->ticksPerSecond << std::endl;
        const float dt = 1.0f / 60.0f;
        for (int f = 0; f < 600; ++f) {
            animator.update(dt);

            // Print hips, spine, head positions if present
            auto printIfExists = [&](const std::string& name){
                auto it = skeleton->boneMap.find(name);
                if (it != skeleton->boneMap.end()) {
                    int id = it->second;
                    if (id >= 0 && id < (int)animator.jointTransforms.size()) {
                        glm::vec4 p = animator.jointTransforms[id] * glm::vec4(0,0,0,1);
                        std::cout << "Frame " << f << " Joint " << name << " (" << id << ") pos: (" << p.x << "," << p.y << "," << p.z << ")" << std::endl;
                    }
                }
            };

            // Print current animation time (ticks) and seconds for easier tracing
            float timeTicks = animator.currentTime;
            float timeSeconds = (clip->ticksPerSecond > 0.0f) ? (timeTicks / clip->ticksPerSecond) : timeTicks;
            std::cout << "Frame " << f << " timeTicks=" << timeTicks << " timeSec=" << timeSeconds << "s" << std::endl;
            printIfExists("Zombie_Hips");
            printIfExists("Zombie_Spine");
            printIfExists("Zombie_Head");

            // small sleep to avoid flooding console too fast
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        std::cout << "Simulation complete." << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
