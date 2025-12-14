#pragma once

#include "../ecs/world.hpp"

#include "../components/camera.hpp"
#include "../components/bullet-collider.hpp"
#include "../components/health.hpp"
#include <application.hpp>

namespace our {

class PhysicsSystem;

class WeaponSystem {
private:
    Application* app = nullptr;
    PhysicsSystem* physics = nullptr;
    Entity* cachedPlayerCamera = nullptr;
    int* killCounter = nullptr;

    float damage = 25.0f;
    float range  = 100.0f;

    // Aim Down Sights (ADS)
    bool aimInitialized = false;
    float hipFovY = 0.0f;
    float adsFovY = 0.0f;
    float adsLerpSpeed = 14.0f; // Higher = snappier

    float hipFireSpreadRadians = 0.035f; // ~2 degrees
    float adsSpreadRadians = 0.006f;     // ~0.35 degrees

public:
    void enter(Application* app, PhysicsSystem* physicsSystem);
    void setKillCounter(int* counter) { killCounter = counter; }
    void update(World* world, float deltaTime);

    [[nodiscard]] bool isAiming() const;
};

}
