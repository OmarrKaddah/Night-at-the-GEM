#include "animation.hpp"
#include <glm/gtx/quaternion.hpp>
#include <cmath>

namespace our {

    // Linear interpolation helper
    template<typename T>
    T lerp(const T& a, const T& b, float t) {
        return a + t * (b - a);
    }

    // Find keyframes for interpolation
    template<typename KeyframeType>
    int findKeyframeIndex(const std::vector<KeyframeType>& keyframes, float time) {
        for (size_t i = 0; i < keyframes.size() - 1; i++) {
            if (time < keyframes[i + 1].time) {
                return i;
            }
        }
        return keyframes.size() - 2;
    }

    glm::mat4 BoneAnimation::getTransform(float time) const {
        glm::mat4 transform(1.0f);
        const float EPS = 1e-6f;

        // Interpolate position
        glm::vec3 position(0.0f);
        if (!positionKeyframes.empty()) {
            if (positionKeyframes.size() == 1) {
                position = positionKeyframes[0].position;
            } else {
                int index = findKeyframeIndex(positionKeyframes, time);
                float t1 = positionKeyframes[index].time;
                float t2 = positionKeyframes[index + 1].time;
                float dt = t2 - t1;
                if (std::fabs(dt) < EPS) {
                    position = positionKeyframes[index].position;
                } else {
                    float factor = glm::clamp((time - t1) / dt, 0.0f, 1.0f);
                    position = lerp(positionKeyframes[index].position, positionKeyframes[index + 1].position, factor);
                }
            }
        }

        // Interpolate rotation
        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        if (!rotationKeyframes.empty()) {
            if (rotationKeyframes.size() == 1) {
                rotation = glm::normalize(rotationKeyframes[0].rotation);
                if (!std::isfinite(rotation.w) || !std::isfinite(rotation.x) || !std::isfinite(rotation.y) || !std::isfinite(rotation.z)) {
                    rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                }
            } else {
                int index = findKeyframeIndex(rotationKeyframes, time);
                float t1 = rotationKeyframes[index].time;
                float t2 = rotationKeyframes[index + 1].time;
                float dt = t2 - t1;

                glm::quat q0 = rotationKeyframes[index].rotation;
                glm::quat q1 = rotationKeyframes[index + 1].rotation;
                q0 = glm::normalize(q0);
                q1 = glm::normalize(q1);
                // sanitize degenerate quaternions
                if (!std::isfinite(q0.w) || !std::isfinite(q0.x) || !std::isfinite(q0.y) || !std::isfinite(q0.z) || glm::dot(q0, q0) < EPS) {
                    q0 = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                }
                if (!std::isfinite(q1.w) || !std::isfinite(q1.x) || !std::isfinite(q1.y) || !std::isfinite(q1.z) || glm::dot(q1, q1) < EPS) {
                    q1 = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                }

                if (std::fabs(dt) < EPS) {
                    rotation = q0;
                } else {
                    float factor = glm::clamp((time - t1) / dt, 0.0f, 1.0f);
                    rotation = glm::slerp(q0, q1, factor);
                    rotation = glm::normalize(rotation);
                }
            }
        }

        // Interpolate scale
        glm::vec3 scale(1.0f);
        if (!scaleKeyframes.empty()) {
            if (scaleKeyframes.size() == 1) {
                scale = scaleKeyframes[0].scale;
            } else {
                int index = findKeyframeIndex(scaleKeyframes, time);
                float t1 = scaleKeyframes[index].time;
                float t2 = scaleKeyframes[index + 1].time;
                float dt = t2 - t1;
                if (std::fabs(dt) < EPS) {
                    scale = scaleKeyframes[index].scale;
                } else {
                    float factor = glm::clamp((time - t1) / dt, 0.0f, 1.0f);
                    scale = lerp(scaleKeyframes[index].scale, scaleKeyframes[index + 1].scale, factor);
                }
            }
        }

        // Build transformation matrix
        transform = glm::translate(glm::mat4(1.0f), position);
        transform *= glm::toMat4(rotation);
        transform = glm::scale(transform, scale);

        return transform;
    }

}
