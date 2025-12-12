#include "animation-loader.hpp"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <cmath>

namespace our {

    // Helper function to convert Assimp matrix to GLM matrix
    static glm::mat4 aiMatrix4x4ToGlm(const aiMatrix4x4& from) {
        glm::mat4 to;
        to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
        to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
        to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
        to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
        return to;
    }

    // Helper function to convert Assimp vector to GLM vector
    static glm::vec3 aiVector3DToGlm(const aiVector3D& vec) {
        return glm::vec3(vec.x, vec.y, vec.z);
    }

    // Helper function to convert Assimp quaternion to GLM quaternion
    static glm::quat aiQuaternionToGlm(const aiQuaternion& quat) {
        return glm::quat(quat.w, quat.x, quat.y, quat.z);
    }

    // Try a number of heuristics to match an animation channel name to a skeleton bone name.
    // Returns true and sets outBoneId/outMatchedName on success.
    static bool tryFindBoneId(const std::shared_ptr<Skeleton>& skeleton, const std::string& originalName, int &outBoneId, std::string &outMatchedName) {
        // Helper lambdas
        auto stripAfterPipe = [](const std::string &s)->std::string {
            size_t p = s.find('|');
            return (p == std::string::npos) ? s : s.substr(0, p);
        };

        auto stripTrailingDigitsDot = [](const std::string &s)->std::string {
            size_t dot = s.rfind('.');
            if (dot == std::string::npos) return s;
            bool allDigits = true;
            for (size_t i = dot + 1; i < s.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(s[i]))) { allDigits = false; break; }
            }
            return allDigits ? s.substr(0, dot) : s;
        };

        auto startsWith = [](const std::string &s, const std::string &pref)->bool { return s.size() >= pref.size() && s.compare(0, pref.size(), pref) == 0; };

        std::vector<std::string> candidates;
        std::string base = stripAfterPipe(originalName);
        candidates.push_back(base);

        std::string stripped = stripTrailingDigitsDot(base);
        if (stripped != base) candidates.push_back(stripped);

        // Common prefixes used by exporters / rigs
        const std::vector<std::string> prefixes = {"Zombie_Ctrl_", "Zombie_Ctrl", "Zombie_", "Zombie", "Ctrl_", "ctrl_", "Bip01_", ""};
        for (const auto &pref : prefixes) {
            if (!pref.empty() && startsWith(base, pref)) {
                std::string t = base.substr(pref.size());
                candidates.push_back(t);
                // also try stripping trailing numeric suffix from this variant
                std::string t2 = stripTrailingDigitsDot(t);
                if (t2 != t) candidates.push_back(t2);
            }
        }

        // Try removing occurrences of "Ctrl" inside the name (e.g. "Zombie_Ctrl_LeftLeg" -> "Zombie_LeftLeg")
        if (base.find("Ctrl_") != std::string::npos) {
            std::string t = base;
            size_t pos = 0;
            while ((pos = t.find("Ctrl_", pos)) != std::string::npos) {
                t.erase(pos, 5);
            }
            candidates.push_back(t);
            candidates.push_back(stripTrailingDigitsDot(t));
        }

        // Ensure uniqueness while preserving order
        std::vector<std::string> uniq;
        for (const auto &c : candidates) {
            if (c.empty()) continue;
            if (std::find(uniq.begin(), uniq.end(), c) == uniq.end()) uniq.push_back(c);
        }

        // Try each candidate against the skeleton bone map
        for (const auto &cand : uniq) {
            auto it = skeleton->boneMap.find(cand);
            if (it != skeleton->boneMap.end()) {
                outBoneId = it->second;
                outMatchedName = cand;
                return true;
            }
        }

        return false;
    }

    // Recursive function to build bone hierarchy
    static void buildBoneHierarchy(
        aiNode* node, 
        Skeleton* skeleton, 
        int parentIndex,
        const std::map<std::string, int>& boneMapping
    ) {
        int currentIndex = -1;
        
        // Check if this node is a bone
        auto it = boneMapping.find(node->mName.C_Str());
        if (it != boneMapping.end()) {
            currentIndex = it->second;
            // Bounds check before accessing vector
            if (currentIndex >= 0 && currentIndex < (int)skeleton->parentIndices.size()) {
                skeleton->parentIndices[currentIndex] = parentIndex;
                // Store the node's local bind transform for this bone
                skeleton->bones[currentIndex].localBindTransform = aiMatrix4x4ToGlm(node->mTransformation);
            }
        } else {
            // Non-bone node, use parent's index for children
            currentIndex = parentIndex;
        }

        // Process children
        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            buildBoneHierarchy(node->mChildren[i], skeleton, currentIndex, boneMapping);
        }
    }

    std::shared_ptr<Skeleton> AnimationLoader::loadSkeleton(const std::string& filename) {
        std::cout << "AnimationLoader: Attempting to load skeleton from " << filename << std::endl;
        
        Assimp::Importer importer;
        
        const aiScene* scene = importer.ReadFile(filename,
            aiProcess_Triangulate |
            // aiProcess_FlipUVs | // Removed to match mesh loader
            aiProcess_LimitBoneWeights |
            aiProcess_GlobalScale
        );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cerr << "Failed to load animation file: " << importer.GetErrorString() << std::endl;
            throw std::runtime_error("Failed to load animation file: " + std::string(importer.GetErrorString()));
        }

        std::cout << "Scene loaded, processing " << scene->mNumMeshes << " meshes" << std::endl;

        auto skeleton = std::make_shared<Skeleton>();
        skeleton->globalInverseTransform = glm::inverse(aiMatrix4x4ToGlm(scene->mRootNode->mTransformation));

        // First pass: collect all bones from all meshes
        std::map<std::string, int> boneMapping;
        std::map<std::string, glm::mat4> offsetMatrices;

        std::cout << "Collecting bones from meshes..." << std::endl;

        for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
            aiMesh* mesh = scene->mMeshes[m];
            std::cout << "  Mesh " << m << " has " << mesh->mNumBones << " bones" << std::endl;
            
            for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
                aiBone* bone = mesh->mBones[b];
                std::string boneName = bone->mName.C_Str();

                if (boneMapping.find(boneName) == boneMapping.end()) {
                    int boneIndex = skeleton->bones.size();
                    boneMapping[boneName] = boneIndex;
                    offsetMatrices[boneName] = aiMatrix4x4ToGlm(bone->mOffsetMatrix);

                    Bone newBone;
                    newBone.name = boneName;
                    newBone.id = boneIndex;
                    newBone.offsetMatrix = offsetMatrices[boneName];

                    skeleton->bones.push_back(newBone);
                    skeleton->boneMap[boneName] = boneIndex;
                }
            }
        }

        // Initialize parent indices
        std::cout << "Initializing parent indices for " << skeleton->bones.size() << " bones" << std::endl;
        skeleton->parentIndices.resize(skeleton->bones.size(), -1);

        // Build bone hierarchy
        std::cout << "Building bone hierarchy..." << std::endl;
        try {
            buildBoneHierarchy(scene->mRootNode, skeleton.get(), -1, boneMapping);
            std::cout << "Bone hierarchy built successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error building bone hierarchy: " << e.what() << std::endl;
            throw;
        }

        std::cout << "Loaded skeleton with " << skeleton->bones.size() << " bones from " << filename << std::endl;

        // Dump skeleton bone names for debugging name-matching issues
        std::cout << "Skeleton bone list (index: name):" << std::endl;
        for (size_t bi = 0; bi < skeleton->bones.size(); ++bi) {
            std::cout << "  [" << bi << "] " << skeleton->bones[bi].name << std::endl;
        }

        // Recompute offset matrices from the bind-pose hierarchy to ensure they
        // are in the same space used by our skinning math. Some exporters store
        // aiBone->mOffsetMatrix in a different space which causes huge joint
        // positions when used directly. Compute global bind transforms from
        // the local bind transforms then set offset = inverse(globalBind).
        {
            /*
            std::vector<glm::mat4> globalBind(skeleton->bones.size(), glm::mat4(1.0f));
            std::vector<char> computed(skeleton->bones.size(), 0);
            std::function<void(int)> computeGlobal = [&](int idx) {
                if (computed[idx]) return;
                int parent = skeleton->parentIndices[idx];
                if (parent == -1) {
                    globalBind[idx] = skeleton->bones[idx].localBindTransform;
                } else {
                    if (!computed[parent]) computeGlobal(parent);
                    globalBind[idx] = globalBind[parent] * skeleton->bones[idx].localBindTransform;
                }
                computed[idx] = 1;
            };

            for (size_t i = 0; i < skeleton->bones.size(); ++i) computeGlobal((int)i);

            for (size_t i = 0; i < skeleton->bones.size(); ++i) {
                glm::mat4 recomputedOffset = glm::inverse(globalBind[i]);
                skeleton->bones[i].offsetMatrix = recomputedOffset;
            }

            std::cout << "Recomputed " << skeleton->bones.size() << " offset matrices from bind-pose." << std::endl;
            */
        }

        // Load all animations from this file
        for (unsigned int a = 0; a < scene->mNumAnimations; ++a) {
            aiAnimation* anim = scene->mAnimations[a];
            
            auto animClip = std::make_shared<AnimationClip>();
            animClip->name = anim->mName.C_Str();
            animClip->duration = static_cast<float>(anim->mDuration);
            animClip->ticksPerSecond = static_cast<float>(anim->mTicksPerSecond != 0 ? anim->mTicksPerSecond : 25.0f);

            // Load bone animations
            for (unsigned int c = 0; c < anim->mNumChannels; ++c) {
                aiNodeAnim* channel = anim->mChannels[c];
                std::string boneName = channel->mNodeName.C_Str();

                auto it = skeleton->boneMap.find(boneName);
                if (it == skeleton->boneMap.end()) {
                    continue; // Skip if not a bone
                }

                int boneId = it->second;
                BoneAnimation boneAnim;

                // Load position keyframes
                for (unsigned int p = 0; p < channel->mNumPositionKeys; ++p) {
                    PositionKeyframe keyframe;
                    keyframe.time = static_cast<float>(channel->mPositionKeys[p].mTime);
                    keyframe.position = aiVector3DToGlm(channel->mPositionKeys[p].mValue);
                    boneAnim.positionKeyframes.push_back(keyframe);
                }
                // Deduplicate position keyframes
                if (boneAnim.positionKeyframes.size() > 1) {
                    std::sort(boneAnim.positionKeyframes.begin(), boneAnim.positionKeyframes.end(), [](const PositionKeyframe &a, const PositionKeyframe &b){ return a.time < b.time; });
                    const float EPS_TIME = 1e-4f;
                    std::vector<PositionKeyframe> compact;
                    compact.reserve(boneAnim.positionKeyframes.size());
                    for (const auto &k : boneAnim.positionKeyframes) {
                        if (compact.empty()) compact.push_back(k);
                        else {
                            if (std::fabs(k.time - compact.back().time) < EPS_TIME) {
                                compact.back() = k;
                            } else compact.push_back(k);
                        }
                    }
                    boneAnim.positionKeyframes.swap(compact);
                }

                // Load rotation keyframes
                for (unsigned int r = 0; r < channel->mNumRotationKeys; ++r) {
                    RotationKeyframe keyframe;
                    keyframe.time = static_cast<float>(channel->mRotationKeys[r].mTime);
                    keyframe.rotation = glm::normalize(aiQuaternionToGlm(channel->mRotationKeys[r].mValue));
                    // Sanitize degenerate quaternions
                    if (!std::isfinite(keyframe.rotation.w) || !std::isfinite(keyframe.rotation.x) || !std::isfinite(keyframe.rotation.y) || !std::isfinite(keyframe.rotation.z) || glm::dot(keyframe.rotation, keyframe.rotation) < 1e-6f) {
                        keyframe.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                    }
                    boneAnim.rotationKeyframes.push_back(keyframe);
                }
                // Deduplicate rotation keyframes
                if (boneAnim.rotationKeyframes.size() > 1) {
                    std::sort(boneAnim.rotationKeyframes.begin(), boneAnim.rotationKeyframes.end(), [](const RotationKeyframe &a, const RotationKeyframe &b){ return a.time < b.time; });
                    const float EPS_TIME = 1e-4f;
                    std::vector<RotationKeyframe> compact;
                    compact.reserve(boneAnim.rotationKeyframes.size());
                    for (const auto &k : boneAnim.rotationKeyframes) {
                        if (compact.empty()) compact.push_back(k);
                        else {
                            if (std::fabs(k.time - compact.back().time) < EPS_TIME) {
                                compact.back() = k;
                            } else compact.push_back(k);
                        }
                    }
                    boneAnim.rotationKeyframes.swap(compact);
                }

                // Load scale keyframes
                for (unsigned int s = 0; s < channel->mNumScalingKeys; ++s) {
                    ScaleKeyframe keyframe;
                    keyframe.time = static_cast<float>(channel->mScalingKeys[s].mTime);
                    keyframe.scale = aiVector3DToGlm(channel->mScalingKeys[s].mValue);
                    boneAnim.scaleKeyframes.push_back(keyframe);
                }
                // Deduplicate scale keyframes
                if (boneAnim.scaleKeyframes.size() > 1) {
                    std::sort(boneAnim.scaleKeyframes.begin(), boneAnim.scaleKeyframes.end(), [](const ScaleKeyframe &a, const ScaleKeyframe &b){ return a.time < b.time; });
                    const float EPS_TIME = 1e-4f;
                    std::vector<ScaleKeyframe> compact;
                    compact.reserve(boneAnim.scaleKeyframes.size());
                    for (const auto &k : boneAnim.scaleKeyframes) {
                        if (compact.empty()) compact.push_back(k);
                        else {
                            if (std::fabs(k.time - compact.back().time) < EPS_TIME) {
                                compact.back() = k;
                            } else compact.push_back(k);
                        }
                    }
                    boneAnim.scaleKeyframes.swap(compact);
                }

                animClip->boneAnimations[boneId] = boneAnim;
            }

            std::cout << "Loaded animation: " << animClip->name << " (duration: " << animClip->duration 
                      << ", fps: " << animClip->ticksPerSecond << ")" << std::endl;

            // Store animation in a map (you'll need to add this to Skeleton or handle externally)
            // For now, we'll just print the info
        }

        return skeleton;
    }

    std::shared_ptr<AnimationClip> AnimationLoader::loadAnimation(
        const std::string& filename,
        const std::shared_ptr<Skeleton>& skeleton
    ) {
        Assimp::Importer importer;
        
        const aiScene* scene = importer.ReadFile(filename,
            aiProcess_Triangulate |
            // aiProcess_FlipUVs | // Removed to match mesh loader
            aiProcess_LimitBoneWeights |
            aiProcess_GlobalScale
        );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            throw std::runtime_error("Failed to load animation file: " + std::string(importer.GetErrorString()));
        }

        if (scene->mNumAnimations == 0) {
            throw std::runtime_error("No animations found in file: " + filename);
        }

        // Debug dump: list all animations and their channel node names to help
        // diagnose name mismatches between animation channels and skeleton bones.
        std::cout << "Animation file contains " << scene->mNumAnimations << " animations" << std::endl;
        for (unsigned int a = 0; a < scene->mNumAnimations; ++a) {
            aiAnimation* anim = scene->mAnimations[a];
            if (!anim) continue;
            const char* animName = anim->mName.C_Str();
            std::cout << "  Animation[" << a << "] name='" << (animName ? animName : "(unnamed)")
                      << "' channels=" << anim->mNumChannels << std::endl;

            // Limit per-animation channel dump to avoid excessive spam
            unsigned int dumpLimit = anim->mNumChannels;
            const unsigned int MAX_DUMP = 500;
            if (dumpLimit > MAX_DUMP) dumpLimit = MAX_DUMP;
            for (unsigned int c = 0; c < dumpLimit; ++c) {
                aiNodeAnim* channel = anim->mChannels[c];
                if (!channel) continue;
                std::cout << "    Channel[" << c << "] nodeName='" << channel->mNodeName.C_Str() << "'" << std::endl;
            }
            if (anim->mNumChannels > dumpLimit) {
                std::cout << "    ... (" << (anim->mNumChannels - dumpLimit) << " more channels omitted)" << std::endl;
            }
        }

        // Aggregate all animations/channels in the file into a single AnimationClip
        auto animClip = std::make_shared<AnimationClip>();

        // Default name from filename
        {
            size_t lastSlash = filename.find_last_of("/\\");
            size_t lastDot = filename.find_last_of('.');
            animClip->name = filename.substr(lastSlash + 1, lastDot - lastSlash - 1);
        }

        float maxDuration = 0.0f;
        float chosenTicksPerSecond = 0.0f;
        int totalChannels = 0;
        int matchedBones = 0;

        for (unsigned int a = 0; a < scene->mNumAnimations; ++a) {
            aiAnimation* anim = scene->mAnimations[a];
            if (!anim) continue;

            // Prefer a named animation name if available
            if (animClip->name.empty() && anim->mName.C_Str() && std::strlen(anim->mName.C_Str()) > 0) {
                animClip->name = anim->mName.C_Str();
            }

            maxDuration = std::max(maxDuration, static_cast<float>(anim->mDuration));
            if (chosenTicksPerSecond == 0.0f && anim->mTicksPerSecond != 0) {
                chosenTicksPerSecond = static_cast<float>(anim->mTicksPerSecond);
            }

            // Iterate channels and merge them into the single clip
            for (unsigned int c = 0; c < anim->mNumChannels; ++c) {
                totalChannels++;
                aiNodeAnim* channel = anim->mChannels[c];
                std::string originalName = channel->mNodeName.C_Str();

                // Start with a conservative normalization: strip anything after a pipe and trailing numeric suffixes like ".001"
                std::string lookupName = originalName;
                size_t pipePos = lookupName.find('|');
                if (pipePos != std::string::npos) lookupName = lookupName.substr(0, pipePos);
                size_t dotPos = lookupName.rfind('.');
                if (dotPos != std::string::npos && dotPos + 1 < lookupName.size()) {
                    bool allDigits = true;
                    for (size_t k = dotPos + 1; k < lookupName.size(); ++k) {
                        if (!std::isdigit(static_cast<unsigned char>(lookupName[k]))) { allDigits = false; break; }
                    }
                    if (allDigits) lookupName = lookupName.substr(0, dotPos);
                }

                int boneId = -1;
                std::string matchedName;

                // Try a direct lookup with the conservative normalized name first
                auto it = skeleton->boneMap.find(lookupName);
                if (it != skeleton->boneMap.end()) {
                    boneId = it->second;
                    matchedName = lookupName;
                } else {
                    // Fallback: try heuristic matching helper (conservative heuristics inside helper)
                    if (tryFindBoneId(skeleton, originalName, boneId, matchedName)) {
                        // matchedName and boneId set by helper
                    } else {
                        if (matchedBones < 10) {
                            std::cout << "  Channel '" << originalName << "' -> '" << lookupName << "' NOT FOUND in skeleton" << std::endl;
                        }
                        continue; // skip if not a bone in our skeleton
                    }
                }

                matchedBones++;

                // If already present, append keyframes; otherwise create new
                BoneAnimation& boneAnim = animClip->boneAnimations[boneId];

                // Load position keyframes
                for (unsigned int p = 0; p < channel->mNumPositionKeys; ++p) {
                    PositionKeyframe keyframe;
                    keyframe.time = static_cast<float>(channel->mPositionKeys[p].mTime);
                    keyframe.position = aiVector3DToGlm(channel->mPositionKeys[p].mValue);
                    boneAnim.positionKeyframes.push_back(keyframe);
                }

                // Load rotation keyframes
                for (unsigned int r = 0; r < channel->mNumRotationKeys; ++r) {
                    RotationKeyframe keyframe;
                    keyframe.time = static_cast<float>(channel->mRotationKeys[r].mTime);
                    keyframe.rotation = aiQuaternionToGlm(channel->mRotationKeys[r].mValue);
                    boneAnim.rotationKeyframes.push_back(keyframe);
                }

                // Load scale keyframes
                for (unsigned int s = 0; s < channel->mNumScalingKeys; ++s) {
                    ScaleKeyframe keyframe;
                    keyframe.time = static_cast<float>(channel->mScalingKeys[s].mTime);
                    keyframe.scale = aiVector3DToGlm(channel->mScalingKeys[s].mValue);
                    boneAnim.scaleKeyframes.push_back(keyframe);
                }
            }
        }

        // After merging channels, ensure keyframes are sorted by time for correct interpolation
        for (auto &p : animClip->boneAnimations) {
            BoneAnimation &ba = p.second;
            auto posCmp = [](const PositionKeyframe &a, const PositionKeyframe &b){ return a.time < b.time; };
            auto rotCmp = [](const RotationKeyframe &a, const RotationKeyframe &b){ return a.time < b.time; };
            auto sclCmp = [](const ScaleKeyframe &a, const ScaleKeyframe &b){ return a.time < b.time; };
            if (ba.positionKeyframes.size() > 1) std::sort(ba.positionKeyframes.begin(), ba.positionKeyframes.end(), posCmp);
            if (ba.rotationKeyframes.size() > 1) std::sort(ba.rotationKeyframes.begin(), ba.rotationKeyframes.end(), rotCmp);
            if (ba.scaleKeyframes.size() > 1) std::sort(ba.scaleKeyframes.begin(), ba.scaleKeyframes.end(), sclCmp);

            // Deduplicate keyframes that have identical times (within EPS).
            const float EPS_TIME = 1e-4f;
            // Positions: keep the later key if times are equal
            if (ba.positionKeyframes.size() > 1) {
                std::vector<PositionKeyframe> compact;
                compact.reserve(ba.positionKeyframes.size());
                for (const auto &k : ba.positionKeyframes) {
                    if (compact.empty()) compact.push_back(k);
                    else {
                        if (std::fabs(k.time - compact.back().time) < EPS_TIME) {
                            compact.back() = k; // prefer later key
                        } else compact.push_back(k);
                    }
                }
                ba.positionKeyframes.swap(compact);
            }

            // Rotations: keep the later key and sanitize degenerate quaternions
            if (ba.rotationKeyframes.size() > 1) {
                std::vector<RotationKeyframe> compact;
                compact.reserve(ba.rotationKeyframes.size());
                for (auto k : ba.rotationKeyframes) {
                    // normalize and sanitize
                    k.rotation = glm::normalize(k.rotation);
                    if (!std::isfinite(k.rotation.w) || !std::isfinite(k.rotation.x) || !std::isfinite(k.rotation.y) || !std::isfinite(k.rotation.z) || glm::dot(k.rotation, k.rotation) < 1e-6f) {
                        k.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                    }
                    if (compact.empty()) compact.push_back(k);
                    else {
                        if (std::fabs(k.time - compact.back().time) < EPS_TIME) {
                            compact.back() = k; // prefer later key
                        } else compact.push_back(k);
                    }
                }
                ba.rotationKeyframes.swap(compact);
            }

            // Scales: keep the later key if times are equal
            if (ba.scaleKeyframes.size() > 1) {
                std::vector<ScaleKeyframe> compact;
                compact.reserve(ba.scaleKeyframes.size());
                for (const auto &k : ba.scaleKeyframes) {
                    if (compact.empty()) compact.push_back(k);
                    else {
                        if (std::fabs(k.time - compact.back().time) < EPS_TIME) {
                            compact.back() = k; // prefer later key
                        } else compact.push_back(k);
                    }
                }
                ba.scaleKeyframes.swap(compact);
            }
        }

        animClip->duration = maxDuration;
        animClip->ticksPerSecond = (chosenTicksPerSecond != 0.0f) ? chosenTicksPerSecond : 25.0f;

        std::cout << "Loaded animation: " << animClip->name << " from " << filename 
                  << " (duration: " << animClip->duration << ", fps: " << animClip->ticksPerSecond 
                  << ", matched " << matchedBones << "/" << totalChannels << " channels)" << std::endl;

        return animClip;
    }

} // namespace our
