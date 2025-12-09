#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/free-camera-controller.hpp>
#include <systems/movement.hpp>
#include <systems/physics-system.hpp>
#include <asset-loader.hpp>

// This state shows how to use the ECS framework and deserialization.
class Playstate: public our::State {

    our::World world;
    our::ForwardRenderer renderer;
    our::FreeCameraControllerSystem cameraController;
    our::MovementSystem movementSystem;
    our::PhysicsSystem physicsSystem;
    bool first_frame = true;  // Instance variable to track first frame

    void onInitialize() override {
        // First of all, we get the scene configuration from the app config
        auto& config = getApp()->getConfig()["scene"];
        // If we have assets in the scene config, we deserialize them
        if(config.contains("assets")){
            our::deserializeAllAssets(config["assets"]);
        }
        // If we have a world in the scene config, we use it to populate our world
        if(config.contains("world")){
            world.deserialize(config["world"]);
        }
        // We initialize the camera controller system since it needs a pointer to the app
        cameraController.enter(getApp());
        // Initialize physics system with gravity
        physicsSystem.initialize(glm::vec3(0.0f, -9.8f, 0.0f));
        physicsSystem.registerWorldColliders(&world);
        // Then we initialize the renderer
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
    }

    void onDraw(double deltaTime) override {
        // Handle mouse rotation and keyboard movement for camera
        auto& mouse = getApp()->getMouse();
        auto& kb = getApp()->getKeyboard();
        
        // Lock mouse on startup
        static bool mouse_locked = false;
        if(!mouse_locked){
            mouse.lockMouse(getApp()->getWindow());
            mouse_locked = true;
            first_frame = true;  // Reset first_frame when entering play state
        }
        
        for(auto entity : world.getEntities()){
            auto* collider = entity->getComponent<our::BulletColliderComponent>();
            auto* camera = entity->getComponent<our::CameraComponent>();
            // Only apply mouse rotation to entities with a camera
            if(collider && collider->mass > 0.0f && collider->rigidBody && camera) {
                // Handle mouse rotation (always active now)
                glm::vec2 delta = mouse.getMouseDelta();
                // Skip the first frame to ignore initial mouse position
                if(first_frame) {
                    delta = glm::vec2(0.0f);
                    first_frame = false;
                }
                glm::vec3 rotation = entity->localTransform.rotation;
                rotation.x -= delta.y * 0.01f;
                rotation.y -= delta.x * 0.01f;
                // Clamp pitch to prevent flipping
                if(rotation.x < -glm::half_pi<float>() * 0.99f) rotation.x = -glm::half_pi<float>() * 0.99f;
                if(rotation.x > glm::half_pi<float>() * 0.99f) rotation.x = glm::half_pi<float>() * 0.99f;
                entity->localTransform.rotation = rotation;
                
                glm::vec3 velocity(0, 0, 0);
                float speed = 5.0f;
                
                // Get camera direction
                glm::mat4 matrix = entity->localTransform.toMat4();
                glm::vec3 forward = glm::vec3(matrix * glm::vec4(0, 0, -1, 0));
                glm::vec3 right = glm::vec3(matrix * glm::vec4(1, 0, 0, 0));
                
                // WASD movement
                if(kb.isPressed(GLFW_KEY_W)) velocity += forward * speed;
                if(kb.isPressed(GLFW_KEY_S)) velocity -= forward * speed;
                if(kb.isPressed(GLFW_KEY_D)) velocity += right * speed;
                if(kb.isPressed(GLFW_KEY_A)) velocity -= right * speed;
                
                // Apply velocity to physics (gravity handles Y)
                collider->rigidBody->setLinearVelocity(btVector3(velocity.x, collider->rigidBody->getLinearVelocity().y(), velocity.z));
                collider->rigidBody->activate();
            }
        }
        
        // Update physics simulation (this applies collision response)
        physicsSystem.update((float)deltaTime);
        
        // Sync physics results BACK to entities
        for(auto entity : world.getEntities()){
            auto* collider = entity->getComponent<our::BulletColliderComponent>();
            if(collider && collider->mass > 0.0f) {
                collider->syncToEntity(); // Update entity from physics
            }
        }
        
        // Run other systems (but NOT camera controller - we handle movement with physics)
        movementSystem.update(&world, (float)deltaTime);
        // cameraController.update(&world, (float)deltaTime); // DISABLED
        
        // And finally we use the renderer system to draw the scene
        renderer.render(&world);

        // Get a reference to the keyboard object
        auto& keyboard = getApp()->getKeyboard();

        if(keyboard.justPressed(GLFW_KEY_ESCAPE)){
            // If the escape  key is pressed in this frame, go to the play state
            getApp()->changeState("menu");
        }
    }

    void onDestroy() override {
        // Don't forget to destroy the renderer
        renderer.destroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Clear the world
        world.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();
    }
};