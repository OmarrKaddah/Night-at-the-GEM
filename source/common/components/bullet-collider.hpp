#pragma once

#include "../ecs/component.hpp"
#include "../mesh/mesh.hpp"
#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace our {

    // Collision shape types supported by Bullet
    enum class CollisionShape {
        BOX,
        SPHERE,
        CAPSULE,
        CYLINDER,
        MESH,
        CONVEX_HULL
    };

    // This component adds Bullet Physics collision to an entity
    class BulletColliderComponent : public Component {
    public:
        // Bullet physics objects
        btCollisionShape* collisionShape;
        btRigidBody* rigidBody;
        btDefaultMotionState* motionState;
        
        // Configuration
        CollisionShape shapeType;
        glm::vec3 size;           // For box, sphere radius in x, capsule (radius in x, height in y)
        float mass;               // 0 = static object
        float friction;
        float restitution;        // Bounciness (0-1)
        bool isTrigger;          // If true, no physical response, just collision detection
        glm::vec3 centerOffset;   // Offset from entity position
        
        // Reference to mesh for mesh/convex hull shapes
        Mesh* mesh;
        
        // Constructor
        BulletColliderComponent();
        
        // The ID of this component type is "Bullet Collider"
        static std::string getID() { return "Bullet Collider"; }
        
        // Deserialize from JSON
        void deserialize(const nlohmann::json& data) override;
        
        // Initialize the collision shape and rigid body
        void initialize(btDiscreteDynamicsWorld* world);
        
        // Update entity transform from physics simulation
        void syncToEntity();
        
        // Update physics simulation from entity transform
        void syncFromEntity();
        
        // Clean up Bullet objects
        ~BulletColliderComponent();
        
    private:
        // Helper to create collision shapes
        btCollisionShape* createShape();
        
        // Helper to convert glm to Bullet vectors
        btVector3 glmToBullet(const glm::vec3& v) const {
            return btVector3(v.x, v.y, v.z);
        }
        
        // Helper to convert Bullet to glm vectors
        glm::vec3 bulletToGlm(const btVector3& v) const {
            return glm::vec3(v.x(), v.y(), v.z());
        }
    };

}
