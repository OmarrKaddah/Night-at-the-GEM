#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/free-camera-controller.hpp>
#include <systems/movement.hpp>
#include <systems/physics-system.hpp>
#include <systems/animation-system.hpp>
#include <components/animator.hpp>
#include <systems/zombie-system.hpp>
#include <asset-loader.hpp>
#include <fstream>
#include <string>
#include <material/material.hpp>
#include <mesh/mesh.hpp>

// This state shows how to use the ECS framework and deserialization.
class Playstate: public our::State {

    our::World world;
    our::ForwardRenderer renderer;
    our::FreeCameraControllerSystem cameraController;
    our::MovementSystem movementSystem;
    our::PhysicsSystem physicsSystem;
    our::AnimationSystem animationSystem;
    our::ZombieSystem zombieSystem;
    our::NavGrid2D floor0Grid;
    our::NavGrid2D floor1Grid;
    our::NavGrid2D floor2Grid;
    bool first_frame = true;
    float previousPlayerY = 0.0f;  // Track Y position for stair detection

    // Health System
    float playerHealth = 100.0f;
    float maxHealth = 100.0f;
    float invulnerabilityTimer = 0.0f;
    // Win Condition
    float survivalTimer = 0.0f; // Start at 0
    float nightDuration = 300.0f; // Default 5 mins
    bool isWin = false;
    float clockFlashTimer = 0.0f; // Timer for clock overlay display
    int lastHour = 12; // Track previous hour for chimes

    // Missing members restored
    float blurTimer = 0.0f;
    float gameOverTimer = 0.0f;
    bool isDead = false;

    // UI Resources
    our::Mesh* uiRectangle = nullptr;
    our::TintedMaterial* healthBarMaterial = nullptr;
    our::TintedMaterial* healthBgMaterial = nullptr;
    our::TexturedMaterial* clockMaterial = nullptr;

    void onInitialize() override {
        // First of all, we get the scene configuration from the app config
        auto& config = getApp()->getConfig()["scene"];
        // If we have assets in the scene config, we deserialize them
        if(config.contains("assets")){
            // we rely on the loading state to load the assets
            // our::deserializeAllAssets(config["assets"]);
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
        
        // Initialize navigation grids
        if (config.contains("navigation")) {
            auto& navConfig = config["navigation"];
            
            // Floor 0 (Ground)
            if (navConfig.contains("floor0")) {
                auto& f0 = navConfig["floor0"];
                floor0Grid = our::NavGrid2D(
                    f0.value("cellSize", 0.5f),
                    glm::vec2(f0["origin"][0], f0["origin"][1]),
                    glm::ivec2(f0["size"][0], f0["size"][1]),
                    f0.value("floorY", 0.0f)
                );
                // Load obstacles
                if (f0.contains("obstacles")) {
                    for (auto& obs : f0["obstacles"]) {
                        glm::ivec2 min(obs["min"][0], obs["min"][1]);
                        glm::ivec2 max(obs["max"][0], obs["max"][1]);
                        floor0Grid.setObstacle(min, max);
                    }
                }
            }
            
            // Floor 1
            if (navConfig.contains("floor1")) {
                auto& f1 = navConfig["floor1"];
                floor1Grid = our::NavGrid2D(
                    f1.value("cellSize", 0.5f),
                    glm::vec2(f1["origin"][0], f1["origin"][1]),
                    glm::ivec2(f1["size"][0], f1["size"][1]),
                    f1.value("floorY", 0.0f)
                );
                // Load obstacles
                if (f1.contains("obstacles")) {
                    for (auto& obs : f1["obstacles"]) {
                        glm::ivec2 min(obs["min"][0], obs["min"][1]);
                        glm::ivec2 max(obs["max"][0], obs["max"][1]);
                        floor1Grid.setObstacle(min, max);
                    }
                }
            }
            
            // Floor 2
            if (navConfig.contains("floor2")) {
                auto& f2 = navConfig["floor2"];
                floor2Grid = our::NavGrid2D(
                    f2.value("cellSize", 0.5f),
                    glm::vec2(f2["origin"][0], f2["origin"][1]),
                    glm::ivec2(f2["size"][0], f2["size"][1]),
                    f2.value("floorY", 5.0f)
                );
                // Load obstacles
                if (f2.contains("obstacles")) {
                    for (auto& obs : f2["obstacles"]) {
                        glm::ivec2 min(obs["min"][0], obs["min"][1]);
                        glm::ivec2 max(obs["max"][0], obs["max"][1]);
                        floor2Grid.setObstacle(min, max);
                    }
                }
            }
            
            // Load stair waypoints
            std::vector<our::ZombieSystem::StairWaypoint> stairWaypoints;
            if (navConfig.contains("stairWaypoints")) {
                for (const auto& wp : navConfig["stairWaypoints"]) {
                    our::ZombieSystem::StairWaypoint waypoint;
                    auto pos = wp["position"];
                    waypoint.position = glm::vec3(pos[0], pos[1], pos[2]);
                    waypoint.targetFloor = wp["targetFloor"];
                    
                    // Load 'next' field if present
                    if (wp.contains("next")) {
                        waypoint.next = wp["next"];
                    }
                    
                    stairWaypoints.push_back(waypoint);
                }
            }
            
            // Initialize zombie system with grids
            zombieSystem.initialize(&floor0Grid, &floor1Grid, &floor2Grid, stairWaypoints);
        }
        // Then we initialize the renderer
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
        // Explicitly lock mouse when entering state
        getApp()->getMouse().lockMouse(getApp()->getWindow());
        // Explicitly lock mouse when entering state
        getApp()->getMouse().lockMouse(getApp()->getWindow());
        first_frame = true;

        // Initialize Health System
        playerHealth = maxHealth;
        isDead = false;
        invulnerabilityTimer = 0.0f;
        
        // Reset Win State
        survivalTimer = 0.0f; // Start at 0 (12:00 AM)
        isWin = false;

        // Create UI Rectangle (1x1)


        if (!uiRectangle) {
            uiRectangle = new our::Mesh({
                {{0.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                {{1.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                {{1.0f, 1.0f, 0.0f}, {255, 255, 255, 255}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
                {{0.0f, 1.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            },{
                0, 1, 2, 2, 3, 0,
            });
        }

        // Create Health Materials
        if (!healthBarMaterial) {
            healthBarMaterial = new our::TintedMaterial();
            healthBarMaterial->shader = new our::ShaderProgram();
            healthBarMaterial->shader->attach("assets/shaders/tinted.vert", GL_VERTEX_SHADER);
            healthBarMaterial->shader->attach("assets/shaders/tinted.frag", GL_FRAGMENT_SHADER);
            healthBarMaterial->shader->link();
            healthBarMaterial->tint = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
            healthBarMaterial->pipelineState.depthTesting.enabled = false; // Always on top
            healthBarMaterial->pipelineState.blending.enabled = true;
        }

        if (!healthBgMaterial) {
            healthBgMaterial = new our::TintedMaterial();
            healthBgMaterial->shader = healthBarMaterial->shader; // Share shader
            healthBgMaterial->tint = glm::vec4(0.3f, 0.3f, 0.3f, 0.8f); // Dark Grey background
            healthBgMaterial->pipelineState.depthTesting.enabled = false;
            healthBgMaterial->pipelineState.blending.enabled = true;
            healthBgMaterial->pipelineState.blending.sourceFactor = GL_SRC_ALPHA;
            healthBgMaterial->pipelineState.blending.destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
        }

        if (!clockMaterial) {
            clockMaterial = new our::TexturedMaterial();
            clockMaterial->shader = our::AssetLoader<our::ShaderProgram>::get("textured");
            clockMaterial->tint = glm::vec4(1.0f);
            clockMaterial->pipelineState.depthTesting.enabled = false;
            clockMaterial->pipelineState.blending.enabled = true;
            clockMaterial->pipelineState.blending.sourceFactor = GL_SRC_ALPHA;
            clockMaterial->pipelineState.blending.destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
            clockMaterial->sampler = our::AssetLoader<our::Sampler>::get("default");
            clockMaterial->alphaThreshold = 0.1f; // Ensure transparency works
        }
        
        // Read configuration
        if(config.contains("nightDuration")) {
            nightDuration = config["nightDuration"].get<float>();
        } else {
            nightDuration = 300.0f; // Default 5 mins
        }
        std::cout << "Playstate::onInitialize - Night Duration set to: " << nightDuration << " seconds" << std::endl;
        std::cout << "Playstate::onInitialize - Hour Duration: " << (nightDuration / 6.0f) << " seconds" << std::endl;
        
        // Show 12 AM immediately on start
        clockFlashTimer = 1.5f;
        lastHour = 12;
    }

    void onDraw(double deltaTime) override {
        // Handle mouse rotation and keyboard movement for camera
        auto& mouse = getApp()->getMouse();
        auto& kb = getApp()->getKeyboard();
        
        // Lock mouse on startup

        
        for(auto entity : world.getEntities()){
            auto* collider = entity->getComponent<our::BulletColliderComponent>();
            auto* camera = entity->getComponent<our::CameraComponent>();
            // Only apply mouse rotation to entities with a camera
            if(collider && collider->mass > 0.0f && collider->rigidBody && camera) {
                // Handle mouse rotation (always active now)
                glm::vec2 delta = mouse.getMouseDelta();
                if(first_frame) {
                    delta = glm::vec2(0.0f);
                    first_frame = false;
                }
                
                // Deadzone to prevent drift
                if(glm::length(delta) < 10.0f) delta = glm::vec2(0.0f); 
                
                glm::vec3 rotation = entity->localTransform.rotation;
                rotation.x -= delta.y * 0.01f;
                rotation.y -= delta.x * 0.01f;
                
                // Clamp pitch to prevent flipping
                if(rotation.x < -glm::half_pi<float>() * 0.99f) rotation.x = -glm::half_pi<float>() * 0.99f;
                if(rotation.x > glm::half_pi<float>() * 0.99f) rotation.x = glm::half_pi<float>() * 0.99f;
                entity->localTransform.rotation = rotation;
                
                // Sync rotation to physics body MANUALLY to avoid resetting linear velocity (which breaks gravity)
                // collider->syncFromEntity(); // This was causing the issue because it zeroes velocity!
                
                btTransform trans = collider->rigidBody->getWorldTransform();
                
                // Only sync Y-rotation (Yaw) to physics body so the capsule stays upright!
                // If we pitch the capsule (look down), it will tip over and 'orbit'/'drift'.
                glm::vec3 eulerRot = entity->localTransform.rotation;
                glm::quat yawQuat = glm::quat(glm::vec3(0, eulerRot.y, 0)); 
                
                trans.setRotation(btQuaternion(yawQuat.x, yawQuat.y, yawQuat.z, yawQuat.w));
                collider->rigidBody->setWorldTransform(trans);
                if(collider->rigidBody->getMotionState()) {
                    collider->rigidBody->getMotionState()->setWorldTransform(trans);
                }
                
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
        
        // Run other systems (but NOT camera controller - we handle movement with physics)
        // movementSystem.update(&world, (float)deltaTime); // DISABLED: overwrites physics velocity!
        // cameraController.update(&world, (float)deltaTime); // DISABLED
        
        // Update Zombie AI
        zombieSystem.update(&world, (float)deltaTime);
        
        // Update animations (Calculate bone positions)
        animationSystem.update(&world, (float)deltaTime);
        

        // Update physics simulation (this applies collision response)
        physicsSystem.update((float)deltaTime);
        
        // Sync physics results BACK to entities
        // Sync physics results BACK to entities
        for(auto entity : world.getEntities()){
            auto* collider = entity->getComponent<our::BulletColliderComponent>();
            if(collider && collider->mass > 0.0f) {
                collider->syncToEntity();
            }
        }
        
        // MANUAL COORDINATE LOGGING: Left-click to print current position
        if (mouse.isPressed(GLFW_MOUSE_BUTTON_LEFT)) {
            for (auto* entity : world.getEntities()) {
                if (entity->name == "PlayerCamera") {
                    // Log to console
                    std::cout << "=== POSITION MARKER ===" << std::endl;
                    std::cout << "PLAYER Position: X=" << entity->localTransform.position.x
                              << ", Y=" << entity->localTransform.position.y
                              << ", Z=" << entity->localTransform.position.z << std::endl;
                    
                    std::cout << "=======================" << std::endl;
                    
                    // Log to file (append mode)
                    std::ofstream logFile("coordinates.log", std::ios::app);
                    if (logFile.is_open()) {
                        logFile << "=== POSITION MARKER ===" << std::endl;
                        logFile << "PLAYER Position: X=" << entity->localTransform.position.x
                                << ", Y=" << entity->localTransform.position.y
                                << ", Z=" << entity->localTransform.position.z << std::endl;
                        
                        // Log all zombie positions to file
                        for (auto* zombie : world.getEntities()) {
                            if (zombie->name.find("Zombie") != std::string::npos) {
                                logFile << zombie->name << " Position: X=" << zombie->localTransform.position.x 
                                        << ", Y=" << zombie->localTransform.position.y 
                                        << ", Z=" << zombie->localTransform.position.z << std::endl;
                            }
                        }
                        logFile << "=======================\n" << std::endl;
                        logFile.close();
                    }
                    break;
                }
            }
        }
        
        // --- Health System Logic ---
        if(invulnerabilityTimer > 0.0f) {
            invulnerabilityTimer -= (float)deltaTime;
        }
        
        // Blur decay
        if(blurTimer > 0.0f) {
            blurTimer -= (float)deltaTime;
            if(blurTimer < 0.0f) blurTimer = 0.0f;
        }

        // Game Over Logic
        if(playerHealth <= 0.0f) {
             if(!isDead) {
                 isDead = true;
                 gameOverTimer = 2.0f; // Wait 2 seconds
                 std::cout << "GAME OVER! Starting timer..." << std::endl;
             }
             
             gameOverTimer -= (float)deltaTime;
             // std::cout << "Death Timer: " << gameOverTimer << std::endl; // Debug log
             
             if(gameOverTimer <= 0.0f) {
                 std::cout << "Timer finished! Switching to menu..." << std::endl;
                 getApp()->changeState("menu");
                 // return; // Removed to allow rendering this frame before switch
             }
        }
        
        // Win Logic (FNAF Style: Survive until 6 AM)
        if(!isDead && playerHealth > 0.0f) {
            // Use cached duration
            float gameHourDuration = nightDuration / 6.0f; 
            
            survivalTimer += (float)deltaTime; // Reusing variable as "Total Time Elapsed"
            
            int currentHour = (int)(survivalTimer / gameHourDuration);
            if (currentHour == 0) currentHour = 12; // 0 index is 12 AM
            
            // Track hour changes
            if(currentHour != lastHour && currentHour < 6) { // Don't trigger flash for 6am, handled by win logic
                std::cout << "CLOCK CHIME: It is now " << currentHour << " AM" << std::endl;
                lastHour = currentHour;
                clockFlashTimer = 1.5f; // Smoother fade duration
            }

            // Win at 6 AM
            if(survivalTimer >= (gameHourDuration * 6.0f)) {
                 if(!isWin) {
                     isWin = true;
                     std::cout << "6:00 AM - THE NIGHT IS OVER. YOU WON!" << std::endl;
                     gameOverTimer = 4.0f; // Celebration pause (Screen Display Time)
                 }
                 
                 gameOverTimer -= (float)deltaTime;
                 // State switch handled in Render loop
            }
        }

        glm::vec3 playerPos = glm::vec3(0.0f);
        bool playerFound = false;
        
        // Find player position
        for(auto entity : world.getEntities()) {
            if(entity->name == "PlayerCamera") {
                playerPos = entity->localTransform.position;
                playerFound = true;
                break;
            }
        }
        
        if(playerFound && invulnerabilityTimer <= 0.0f && !isDead) { // Don't take damage if dead
             for(auto entity : world.getEntities()) {
                if(entity->name.find("Zombie") != std::string::npos) {
                    glm::vec3 zombiePos = entity->localTransform.position;
                    // Adjust zombie pos for center offset (approx 1.0 up)
                    zombiePos.y += 1.0f; 
                    
                    float dist = glm::distance(playerPos, zombiePos);
                    
                    // Collision Threshold (1.5m)
                    if(dist < 1.5f) {
                        // Take Damage
                        float damage = 10.0f;
                        playerHealth -= damage;
                        if(playerHealth < 0.0f) playerHealth = 0.0f;
                        
                        invulnerabilityTimer = 1.0f; // 1 second immunity
                        
                        // Trigger Blur
                        blurTimer = 0.5f; 
                        
                        std::cout << "!!! DAMAGE TAKEN !!! Health: " << playerHealth << "/" << maxHealth << std::endl;
                    }
                }
             }
        }

        // Update Post-Process Uniforms
        
        // 1. Blood Material
        if(auto* mat = renderer.getPostProcessMaterial("blood")) {
            float damageFactor = 1.0f - (playerHealth / maxHealth);
            mat->shader->use();
            mat->shader->set("damage_factor", damageFactor);
        }
        
        // 2. Blur Material
        if(auto* mat = renderer.getPostProcessMaterial("blur")) {
            // Map blurTimer (0.5 -> 0.0) to blur_strength (1.0 -> 0.0)
            float blurStrength = (blurTimer / 0.5f); 
            if(blurStrength < 0) blurStrength = 0;
            mat->shader->use();
            mat->shader->set("blur_strength", blurStrength);
        }

        // And finally we use the renderer system to draw the scene
        renderer.render(&world);
        
        // --- Render Health Bar ---
        {
            // Setup Ortho Projection for UI
            glm::ivec2 size = getApp()->getFrameBufferSize();
            glm::mat4 VP = glm::ortho(0.0f, (float)size.x, (float)size.y, 0.0f, 1.0f, -1.0f);
            
            // Bar Positioning (TOP LEFT)
            float padding = 20.0f;
            float barWidth = 300.0f;
            float barHeight = 30.0f;
            float x = padding;
            // float y = size.y - padding - barHeight; // Bottom
            float y = padding; // Top
            
            // 1. Draw Background
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f)) * 
                          glm::scale(glm::mat4(1.0f), glm::vec3(barWidth, barHeight, 1.0f));
            
            healthBgMaterial->setup();
            healthBgMaterial->shader->set("transform", VP*M);
            uiRectangle->draw();
            
            // 2. Draw Health (Foreground)
            if(playerHealth > 0.0f) {
                float healthRatio = playerHealth / maxHealth;
                float currentWidth = barWidth * healthRatio;
                
                glm::mat4 M2 = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f)) * 
                               glm::scale(glm::mat4(1.0f), glm::vec3(currentWidth, barHeight, 1.0f));
                               
                // Dynamic Color Logic
                glm::vec4 barColor;
                if(healthRatio > 0.66f) {
                    barColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
                } else if(healthRatio > 0.33f) {
                    barColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
                } else {
                    barColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
                }
                healthBarMaterial->tint = barColor;
                
                healthBarMaterial->setup();
                healthBarMaterial->shader->set("transform", VP*M2);
                uiRectangle->draw();
            }
            
            // 3. Draw Clock / Win Screen
            
            // Check for 6 AM Win Condition
            if (isWin) {
                // RENDER 6 AM SCREEN (Opaque)
                clockMaterial->texture = our::AssetLoader<our::Texture2D>::get("time-6am");
                if (clockMaterial->texture) {
                    glm::mat4 MClock = glm::scale(glm::mat4(1.0f), glm::vec3((float)size.x, (float)size.y, 1.0f));
                    clockMaterial->setup();
                    clockMaterial->shader->set("tint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // Opaque
                    clockMaterial->shader->set("transform", VP*MClock);
                    uiRectangle->draw();
                }
                
                // Wait for the "Celebration" timer then exit
                if (gameOverTimer <= 0.0f) {
                     getApp()->changeState("menu");
                }
                
            } else if (clockFlashTimer > 0.0f) {
                // HOURLY CHIME (Transparent & Fading)
                clockFlashTimer -= (float)deltaTime;
                
                // Calculate Smooth Fade (0.5 start -> 0.0 end)
                float alpha = (clockFlashTimer / 1.5f) * 0.5f; // Duration 1.5s for smoothness
                if(alpha < 0.0f) alpha = 0.0f;
                
                float gameHourDuration = nightDuration / 6.0f; 
                int hourIndex = (int)(survivalTimer / gameHourDuration);
                 // Don't show 6am here, handled by isWin
                if (hourIndex >= 6) hourIndex = 5;
                if (hourIndex == 0) hourIndex = 12; // Handle 0 as 12
                
                std::string textureName = "time-";
                if(hourIndex == 12) textureName += "12am";
                else textureName += std::to_string(hourIndex) + "am";
                
                clockMaterial->texture = our::AssetLoader<our::Texture2D>::get(textureName);
                
                if(clockMaterial->texture) {
                    // Fullscreen Overlay
                    glm::mat4 MClock = glm::scale(glm::mat4(1.0f), glm::vec3((float)size.x, (float)size.y, 1.0f));
                                    
                    clockMaterial->setup();
                    // Apply fading alpha
                    clockMaterial->shader->set("tint", glm::vec4(1.0f, 1.0f, 1.0f, alpha)); 
                    clockMaterial->shader->set("transform", VP*MClock);
                    uiRectangle->draw();
                }
            }
        }

        // Get a reference to the keyboard object
        auto& keyboard = getApp()->getKeyboard();

        if(keyboard.justPressed(GLFW_KEY_ESCAPE)){
            getApp()->changeState("menu");
        }
        
        // Disable/Enable Skinning (Diagnostic)
        if(keyboard.justPressed(GLFW_KEY_K)){
            // Toggle global uniform or iterate entities? 
            // Simpler: Just toggle a static bool and send it
            static bool skinningEnabled = true;
            skinningEnabled = !skinningEnabled;
            std::cout << "Skinning Enabled: " << skinningEnabled << std::endl;
            
            // Hack: set it on the next draw via material or global uniform
            // Since we can't easily access the shader directly here without iterating materials,
            // let's iterate the world and set a property on the material if we could.
            // Actually, skinned.vert uses a uniform 'useSkinning'. 
            // We need to pass this. For now, let's just print it and realize we need to change how we draw.
            // BUT, we can pause animation easily!
        }

        // Pause/Play Animation (Diagnostic)
        if(keyboard.justPressed(GLFW_KEY_P)){
             for(auto entity : world.getEntities()){
                auto* animator = entity->getComponent<our::AnimatorComponent>();
                if(animator) {
                    animator->isPlaying = !animator->isPlaying;
                    std::cout << "Entity " << entity->name << " animation playing: " << animator->isPlaying << std::endl;
                }
             }
        }
    }

    void onDestroy() override {
        std::cout << "Playstate::onDestroy - Start" << std::endl;

        // Destroy UI resources
        std::cout << "Playstate::onDestroy - Cleaning UI" << std::endl;
        if(uiRectangle) { delete uiRectangle; uiRectangle = nullptr; }
        
        if(healthBarMaterial) {
            delete healthBarMaterial->shader;
            delete healthBarMaterial;
            healthBarMaterial = nullptr;
        }
        
        if(healthBgMaterial) {
             delete healthBgMaterial;
             healthBgMaterial = nullptr;
        }

        if(clockMaterial) {
             // Shader is managed by AssetLoader (if we got it from there), so don't delete it
             delete clockMaterial;
             clockMaterial = nullptr;
        }
        
        // Don't forget to destroy the renderer
        std::cout << "Playstate::onDestroy - Destroying Renderer" << std::endl;
        renderer.destroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        std::cout << "Playstate::onDestroy - Exiting Camera Controller" << std::endl;
        cameraController.exit();
        
        // Clean up systems that hold state (remove objects from simulation)
        std::cout << "Playstate::onDestroy - Destroying Physics System" << std::endl;
        physicsSystem.destroy();
        std::cout << "Playstate::onDestroy - Destroying Zombie System" << std::endl;
        zombieSystem.destroy();

        // Clear the world (destroys entities and their components)
        // BulletColliderComponent destructor will delete the rigid bodies
        std::cout << "Playstate::onDestroy - Clearing World" << std::endl;
        world.clear();
        
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        std::cout << "Playstate::onDestroy - Clearing Assets" << std::endl;
        our::clearAllAssets();
        
        std::cout << "Playstate::onDestroy - End" << std::endl;
    }
};
