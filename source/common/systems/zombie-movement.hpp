#pragma once

#include "../ecs/world.hpp"
#include "../components/zombie-movement.hpp"
#include "../components/bullet-collider.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/compatibility.hpp>
#include <cmath>

namespace our {

    // Simple kinematic motion for zombies: oscillate around the spawn point with a
    // forward/back stride and a sideways sway. Colliders are kept in sync so physics
    // can still block the player.
    class ZombieMovementSystem {
        float elapsed = 0.0f;

        static glm::vec3 normalizeOr(const glm::vec3& v, const glm::vec3& fallback) {
            float len2 = glm::length2(v);
            return (len2 > 1e-4f) ? v / std::sqrt(len2) : fallback;
        }

    public:
        void update(World* world, float deltaTime) {
            if (!world) return;
            elapsed += deltaTime;

            for (auto* entity : world->getEntities()) {
                auto* zombie = entity->getComponent<ZombieMovementComponent>();
                if (!zombie) continue;

                if (!zombie->initialized) {
                    zombie->startPosition = entity->localTransform.position;
                    zombie->initialized = true;
                }

                float t = elapsed + zombie->phase;
                glm::vec3 forwardAxis = normalizeOr(zombie->forwardAxis, glm::vec3(0.0f, 0.0f, 1.0f));
                glm::vec3 swayAxis = normalizeOr(zombie->swayAxis, glm::vec3(1.0f, 0.0f, 0.0f));

                float forwardOffset = zombie->walkAmplitude * std::sin(t * zombie->walkSpeed);
                float swayOffset = zombie->swayAmplitude * std::sin(t * zombie->swaySpeed + zombie->phase * 0.5f);

                glm::vec3 previousPosition = entity->localTransform.position;
                glm::vec3 targetPosition = zombie->startPosition +
                                           forwardAxis * forwardOffset +
                                           swayAxis * swayOffset;

                entity->localTransform.position = targetPosition;

                if (zombie->faceMovement) {
                    glm::vec3 delta = targetPosition - previousPosition;
                    glm::vec3 flatDelta = glm::vec3(delta.x, 0.0f, delta.z);
                    if (glm::length2(flatDelta) > 1e-6f) {
                        glm::vec3 dir = glm::normalize(flatDelta);
                        float yaw = std::atan2(dir.x, -dir.z); // -Z is forward
                        entity->localTransform.rotation.y = yaw;
                    }
                }

                if (auto* collider = entity->getComponent<BulletColliderComponent>()) {
                    if (collider->mass > 0.0f && collider->rigidBody) {
                        glm::vec3 velocity = (targetPosition - previousPosition) / glm::max(deltaTime, 1e-4f);
                        collider->rigidBody->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
                        collider->rigidBody->activate();
                    } else {
                        collider->syncFromEntity();
                    }
                }
            }
        }
    };

}
