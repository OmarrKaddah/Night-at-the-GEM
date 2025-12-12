#pragma once

#include "../ecs/world.hpp"
#include "../components/movement.hpp"
#include "../components/bullet-collider.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/fast_trigonometry.hpp>

namespace our
{

    // The movement system is responsible for moving every entity which contains a MovementComponent.
    // This system is added as a simple example for how use the ECS framework to implement logic. 
    // For more information, see "common/components/movement.hpp"
    class MovementSystem {
    public:

        // This should be called every frame to update all entities containing a MovementComponent. 
        void update(World* world, float deltaTime) {
            // For each entity in the world
            for(auto entity : world->getEntities()){
                // Get the movement component if it exists
                MovementComponent* movement = entity->getComponent<MovementComponent>();
                // If the movement component exists
                if(movement){
                    // 1. Check if the entity has physics enabled
                    BulletColliderComponent* collider = entity->getComponent<BulletColliderComponent>();

                    if(collider && collider->rigidBody && collider->mass > 0.0f) {
                        // 2. Instead of "teleporting" the entity, set the velocity of the physics body
                        btVector3 vel(movement->linearVelocity.x, movement->linearVelocity.y, movement->linearVelocity.z);
                        collider->rigidBody->setLinearVelocity(vel);
                        
                        // Let Bullet handle the rotation too if you have angular velocity
                        btVector3 angVel(movement->angularVelocity.x, movement->angularVelocity.y, movement->angularVelocity.z);
                        collider->rigidBody->setAngularVelocity(angVel);
                    } else {
                        // 3. Only do manual movement if there is NO physics component
                        entity->localTransform.position += deltaTime * movement->linearVelocity;
                        entity->localTransform.rotation += deltaTime * movement->angularVelocity;
                    }
                }
            }
        }

    };

}
