#include "physics-system.hpp"
#include "../components/bullet-collider.hpp"
#include <iostream>

namespace our {

    PhysicsSystem::PhysicsSystem() 
        : collisionConfiguration(nullptr), dispatcher(nullptr),
          overlappingPairCache(nullptr), solver(nullptr),
          dynamicsWorld(nullptr), gravity(0, -9.81f, 0) {
    }

    PhysicsSystem::~PhysicsSystem() {
        destroy();
    }

    void PhysicsSystem::destroy() {
        // Clean up in reverse order of creation
        if (dynamicsWorld) {
            // Remove all rigid bodies
            for (int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
                btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
                // We do NOT delete the motion state or the body here, as they are owned by the BulletColliderComponent
                // which will be destroyed when the World is cleared.
                dynamicsWorld->removeCollisionObject(obj);
            }
            delete dynamicsWorld;
            dynamicsWorld = nullptr;
        }
        
        if (solver) { delete solver; solver = nullptr; }
        if (overlappingPairCache) { delete overlappingPairCache; overlappingPairCache = nullptr; }
        if (dispatcher) { delete dispatcher; dispatcher = nullptr; }
        if (collisionConfiguration) { delete collisionConfiguration; collisionConfiguration = nullptr; }
        
        colliders.clear();
    }

    void PhysicsSystem::initialize(const glm::vec3& gravityVec) {
        // Create collision configuration
        collisionConfiguration = new btDefaultCollisionConfiguration();
        
        // Create collision dispatcher
        dispatcher = new btCollisionDispatcher(collisionConfiguration);
        
        // Create broadphase
        overlappingPairCache = new btDbvtBroadphase();
        
        // Create constraint solver
        solver = new btSequentialImpulseConstraintSolver();
        
        // Create dynamics world
        dynamicsWorld = new btDiscreteDynamicsWorld(
            dispatcher, overlappingPairCache, solver, collisionConfiguration
        );
        
        // Set gravity
        setGravity(gravityVec);
    }

    void PhysicsSystem::registerCollider(BulletColliderComponent* collider) {
        if (!collider || !dynamicsWorld) return;
        
        // Initialize the collider with the dynamics world
        collider->initialize(dynamicsWorld);
        
        // Track it
        colliders.push_back(collider);
    }

    void PhysicsSystem::removeCollider(BulletColliderComponent* collider) {
        if (!collider || !dynamicsWorld) return;
        
        // Remove from dynamics world
        if (collider->rigidBody) {
            dynamicsWorld->removeRigidBody(collider->rigidBody);
        }
        
        // Remove from tracking list
        auto it = std::find(colliders.begin(), colliders.end(), collider);
        if (it != colliders.end()) {
            colliders.erase(it);
        }
    }

    void PhysicsSystem::syncFromEntities() {
        // Sync entity transforms to physics bodies
        for (auto* collider : colliders) {
            if (collider) {
                collider->syncFromEntity();
            }
        }
    }

    void PhysicsSystem::update(float deltaTime) {
        if (!dynamicsWorld) return;
        
        // Step the physics simulation
        // Parameters: timeStep, maxSubSteps, fixedTimeStep
        dynamicsWorld->stepSimulation(deltaTime, 10, 1.0f / 60.0f);
        
        // Sync physics transforms back to entities
        for (auto* collider : colliders) {
            if (collider && collider->mass > 0.0f) {
                collider->syncToEntity();
            }
        }
    }

    void PhysicsSystem::registerWorldColliders(World* world) {
        if (!world) return;
        
        std::cout << "PhysicsSystem: Registering colliders from world..." << std::endl;
        int colliderCount = 0;
        
        // Iterate through all entities and register colliders
        for (auto* entity : world->getEntities()) {
            if (!entity) continue;
            
            // Check if entity has a BulletColliderComponent
            auto* collider = entity->getComponent<BulletColliderComponent>();
            if (collider) {
                std::cout << "  Found collider on entity: " << entity->name << std::endl;
                registerCollider(collider);
                colliderCount++;
            }
            
            // Recursively check children
            // Note: You'd need to implement a method to get all descendants
        }
        
        std::cout << "PhysicsSystem: Registered " << colliderCount << " colliders" << std::endl;
    }

    void PhysicsSystem::setGravity(const glm::vec3& gravityVec) {
        gravity = btVector3(gravityVec.x, gravityVec.y, gravityVec.z);
        if (dynamicsWorld) {
            dynamicsWorld->setGravity(gravity);
        }
    }

    bool PhysicsSystem::raycast(const glm::vec3& start, const glm::vec3& end,
                                glm::vec3& hitPoint, glm::vec3& hitNormal,
                                BulletColliderComponent** hitCollider) {
        return raycast(start, end, hitPoint, hitNormal, hitCollider, nullptr);
    }

    bool PhysicsSystem::raycast(const glm::vec3& start, const glm::vec3& end,
                                glm::vec3& hitPoint, glm::vec3& hitNormal,
                                BulletColliderComponent** hitCollider,
                                const btCollisionObject* ignore) {
        if (!dynamicsWorld) return false;
        
        btVector3 btStart(start.x, start.y, start.z);
        btVector3 btEnd(end.x, end.y, end.z);
        
        struct ClosestNotMeCallback : btCollisionWorld::ClosestRayResultCallback {
            const btCollisionObject* ignore = nullptr;
            ClosestNotMeCallback(const btVector3& from, const btVector3& to, const btCollisionObject* ignore)
                : btCollisionWorld::ClosestRayResultCallback(from, to), ignore(ignore) {}

            btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
                if(ignore && rayResult.m_collisionObject == ignore) {
                    return 1.0f;
                }
                return btCollisionWorld::ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
            }
        };

        ClosestNotMeCallback rayCallback(btStart, btEnd, ignore);
        dynamicsWorld->rayTest(btStart, btEnd, rayCallback);
        
        if (rayCallback.hasHit()) {
            hitPoint = glm::vec3(
                rayCallback.m_hitPointWorld.x(),
                rayCallback.m_hitPointWorld.y(),
                rayCallback.m_hitPointWorld.z()
            );
            
            hitNormal = glm::vec3(
                rayCallback.m_hitNormalWorld.x(),
                rayCallback.m_hitNormalWorld.y(),
                rayCallback.m_hitNormalWorld.z()
            );
            
            if (hitCollider) {
                const btCollisionObject* obj = rayCallback.m_collisionObject;
                if (obj) {
                    *hitCollider = static_cast<BulletColliderComponent*>(obj->getUserPointer());
                }
            }
            
            return true;
        }
        
        return false;
    }

}
