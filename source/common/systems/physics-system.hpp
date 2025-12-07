#pragma once

#include <btBulletDynamicsCommon.h>
#include "../ecs/world.hpp"
#include "../components/bullet-collider.hpp"

namespace our {

    // This system manages the Bullet Physics simulation
    class PhysicsSystem {
    private:
        // Bullet physics world and configuration
        btDefaultCollisionConfiguration* collisionConfiguration;
        btCollisionDispatcher* dispatcher;
        btBroadphaseInterface* overlappingPairCache;
        btSequentialImpulseConstraintSolver* solver;
        btDiscreteDynamicsWorld* dynamicsWorld;
        
        // Gravity
        btVector3 gravity;
        
        // Track initialized colliders
        std::vector<BulletColliderComponent*> colliders;

    public:
        PhysicsSystem();
        ~PhysicsSystem();

        // Initialize the physics world
        void initialize(const glm::vec3& gravityVec = glm::vec3(0.0f, -9.81f, 0.0f));
        
        // Register a collider with the physics world
        void registerCollider(BulletColliderComponent* collider);
        
        // Remove a collider from the physics world
        void removeCollider(BulletColliderComponent* collider);
        
        // Update physics simulation
        void update(float deltaTime);
        
        // Sync entity transforms to physics bodies (call before update)
        void syncFromEntities();
        
        // Find all colliders in the world and register them
        void registerWorldColliders(World* world);
        
        // Set gravity
        void setGravity(const glm::vec3& gravityVec);
        
        // Get the dynamics world (for advanced usage)
        btDiscreteDynamicsWorld* getDynamicsWorld() { return dynamicsWorld; }
        
        // Raycast from start to end, returns true if hit
        bool raycast(const glm::vec3& start, const glm::vec3& end, 
                    glm::vec3& hitPoint, glm::vec3& hitNormal,
                    BulletColliderComponent** hitCollider = nullptr);
    };

}
