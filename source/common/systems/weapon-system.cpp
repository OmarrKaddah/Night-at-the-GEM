#include "weapon-system.hpp"
#include "physics-system.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <random>
#include "../components/animator.hpp"

namespace our {

void WeaponSystem::enter(Application* app, PhysicsSystem* physicsSystem) {
    this->app = app;
    this->physics = physicsSystem;
}

bool WeaponSystem::isAiming() const {
    if(!app) return false;
    return app->getKeyboard().isPressed(GLFW_KEY_LEFT_SHIFT);
}

static float expSmoothingAlpha(float deltaTime, float speed) {
    if(deltaTime <= 0.0f) return 1.0f;
    if(speed <= 0.0f) return 1.0f;
    // 1 - e^(-speed*dt)
    return 1.0f - std::exp(-speed * deltaTime);
}

static glm::vec3 applySpreadToDirection(const glm::vec3& forward, float maxAngleRadians) {
    if(maxAngleRadians <= 0.0f) return glm::normalize(forward);

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> uniform01(0.0f, 1.0f);

    // Build an orthonormal basis around forward
    glm::vec3 f = glm::normalize(forward);
    glm::vec3 arbitraryUp = (std::abs(f.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 right = glm::normalize(glm::cross(f, arbitraryUp));
    glm::vec3 up = glm::normalize(glm::cross(right, f));

    // Sample a point uniformly in a disk, then map to an angular offset (small-angle approximation via tan)
    float u1 = uniform01(rng);
    float u2 = uniform01(rng);
    float radius = std::sqrt(u1) * std::tan(maxAngleRadians);
    float theta = 2.0f * glm::pi<float>() * u2;

    glm::vec3 offset = right * (radius * std::cos(theta)) + up * (radius * std::sin(theta));
    return glm::normalize(f + offset);
}

void WeaponSystem::update(World* world, float deltaTime) {
    if (!app || !physics) return;
    if (!world) return;

    auto& mouse = app->getMouse();

    // Find player camera (prefer entity named "PlayerCamera", otherwise any entity with a camera)
    Entity* player = nullptr;
    if(cachedPlayerCamera) {
        // Validate cached pointer still belongs to this world by re-finding it quickly
        for (auto* entity : world->getEntities()) {
            if(entity == cachedPlayerCamera) {
                player = cachedPlayerCamera;
                break;
            }
        }
    }
    if(!player) {
        for (auto* entity : world->getEntities()) {
            if(entity && entity->name == "PlayerCamera" && entity->getComponent<CameraComponent>()) {
                player = entity;
                break;
            }
        }
    }
    if(!player) {
        for (auto* entity : world->getEntities()) {
            if(entity && entity->getComponent<CameraComponent>()) {
                player = entity;
                break;
            }
        }
    }
    cachedPlayerCamera = player;

    if(!player) return;

    auto* camera = player->getComponent<CameraComponent>();
    if(!camera) return;

    // --- ADS (Aim Down Sights) camera zoom ---
    if(!aimInitialized) {
        hipFovY = camera->fovY;
        adsFovY = std::min(hipFovY, glm::radians(60.0f));
        aimInitialized = true;
    }

    float targetFovY = isAiming() ? adsFovY : hipFovY;
    float t = expSmoothingAlpha(deltaTime, adsLerpSpeed);
    camera->fovY = glm::mix(camera->fovY, targetFovY, t);

    // Single shot per click
    if (!mouse.justPressed(GLFW_MOUSE_BUTTON_LEFT)) return;

    std::cout << "[Weapon] Fire" << std::endl;

    // Ray origin (camera position in world space)
    glm::mat4 cameraTransform = player->getLocalToWorldMatrix();
    glm::vec3 origin = glm::vec3(cameraTransform[3]);

    // Forward direction (-Z) in world space
    glm::vec3 forward = glm::normalize(glm::vec3(cameraTransform * glm::vec4(0, 0, -1, 0)));

    float spread = isAiming() ? adsSpreadRadians : hipFireSpreadRadians;
    glm::vec3 shotDirection = applySpreadToDirection(forward, spread);

    glm::vec3 end = origin + shotDirection * range;

    glm::vec3 hitPoint, hitNormal;
    BulletColliderComponent* hitCollider = nullptr;

    const btCollisionObject* ignore = nullptr;
    if(auto* playerCollider = player->getComponent<BulletColliderComponent>()) {
        ignore = playerCollider->rigidBody;
    }

    if (physics->raycast(origin, end, hitPoint, hitNormal, &hitCollider, ignore)) {
        if (!hitCollider) return;

        Entity* hitEntity = hitCollider->getOwner();
        if(!hitEntity) return;

        if (auto* health = hitEntity->getComponent<HealthComponent>()) {
            bool wasAlive = !health->isDead();
            health->applyDamage(damage);

            if(wasAlive && health->isDead()) {
                if(killCounter) (*killCounter)++;
            } else if(!health->isDead()) {
                if(auto* animator = hitEntity->getComponent<AnimatorComponent>()) {
                    if(animator->animations.find("hit") != animator->animations.end()) {
                        animator->playAnimation("hit", false);
                    }
                }
            }

            std::cout << "[Weapon] Hit " << hitEntity->name
                      << " | Health: " << health->currentHealth << std::endl;
        } else {
            std::cout << "[Weapon] Hit " << hitEntity->name << " | No HealthComponent" << std::endl;
        }
    } else {
        std::cout << "[Weapon] Miss" << std::endl;
    }
}

}
