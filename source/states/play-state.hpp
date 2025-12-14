#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/free-camera-controller.hpp>
#include <systems/movement.hpp>
#include <systems/physics-system.hpp>
#include <systems/animation-system.hpp>
#include <systems/weapon-system.hpp>
#include <components/animator.hpp>
#include <components/health.hpp>
#include <systems/zombie-system.hpp>
#include <asset-loader.hpp>
#include <fstream>
#include <cstdio>
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
    our::WeaponSystem weaponSystem;
    our::ZombieSystem zombieSystem;
    our::NavGrid2D floor0Grid;
    our::NavGrid2D floor1Grid;
    our::NavGrid2D floor2Grid;
    bool first_frame = true;
    float previousPlayerY = 0.0f;  // Track Y position for stair detection

    // Win/Lose + Rounds
    enum class GameOutcome { None, Win, Lose };
    GameOutcome outcome = GameOutcome::None;
    int kills = 0;
    int currentRound = 1;
    int roundsCompleted = 0; // rounds fully survived
    int roundsSurvivedAtEnd = 0;
    int killsAtEnd = 0;
    float roundTimer = 0.0f;
    float roundDurationSeconds = 45.0f;
    int totalRoundsToWin = 5;

    // Health System
    float playerHealth = 100.0f;
    float maxHealth = 100.0f;
    float invulnerabilityTimer = 0.0f;
    float healthRegenCooldown = 0.0f;
    float healthRegenDelay = 3.0f;     // seconds after taking damage before regen starts
    float healthRegenRate = 6.0f;      // health per second
    float blurTimer = 0.0f;      // Timer for blur effect
    float gameOverTimer = 0.0f;  // Timer before switching state on death
    bool isDead = false;
    
    // UI Resources
    our::Mesh* uiRectangle = nullptr;
    our::TintedMaterial* healthBarMaterial = nullptr;
    our::TintedMaterial* healthBgMaterial = nullptr;

    float respawnDelayForRound(int round) const {
        // Round 1: 10s, then ramps down each round (faster respawns = higher spawn rate)
        float delay = 10.0f - (float)(round - 1) * 1.5f;
        if(delay < 2.0f) delay = 2.0f;
        return delay;
    }

    void endGame(GameOutcome newOutcome) {
        if(outcome != GameOutcome::None) return;
        outcome = newOutcome;
        roundsSurvivedAtEnd = roundsCompleted;
        killsAtEnd = kills;
        isDead = (newOutcome == GameOutcome::Lose);
        our::Mouse::unlockMouse(getApp()->getWindow());
    }

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
        weaponSystem.enter(getApp(), &physicsSystem);
        weaponSystem.setKillCounter(&kills);

        // Game loop state
        outcome = GameOutcome::None;
        kills = 0;
        currentRound = 1;
        roundsCompleted = 0;
        roundsSurvivedAtEnd = 0;
        killsAtEnd = 0;
        roundTimer = roundDurationSeconds;
        zombieSystem.setRespawnDelaySeconds(respawnDelayForRound(currentRound));
        
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
        healthRegenCooldown = 0.0f;

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
            healthBarMaterial->pipelineState.blending.sourceFactor = GL_SRC_ALPHA;
            healthBarMaterial->pipelineState.blending.destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
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
    }

    void onDraw(double deltaTime) override {
        float dt = (float)deltaTime;

        // Handle mouse rotation and keyboard movement for camera
        auto& mouse = getApp()->getMouse();
        auto& kb = getApp()->getKeyboard();
        bool gameplayActive = (outcome == GameOutcome::None);
        
        // Lock mouse on startup

        
        if(gameplayActive) for(auto entity : world.getEntities()){
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
                float lookSensitivity = 0.01f;
                if(weaponSystem.isAiming()) lookSensitivity *= 0.5f;
                rotation.x -= delta.y * lookSensitivity;
                rotation.y -= delta.x * lookSensitivity;
                
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
        if(gameplayActive) {
            weaponSystem.update(&world, dt);
            zombieSystem.update(&world, dt);
        }
        
        // Update animations (Calculate bone positions)
        animationSystem.update(&world, dt);
         

        // Update physics simulation (this applies collision response)
        physicsSystem.update(dt);
        
        // Sync physics results BACK to entities
        for(auto entity : world.getEntities()){
            auto* collider = entity->getComponent<our::BulletColliderComponent>();
            if(collider && collider->mass > 0.0f) {
                collider->syncToEntity(); // Update entity from physics
            }
        }
        
        // MANUAL COORDINATE LOGGING: Press F1 to print current position
        if (kb.justPressed(GLFW_KEY_F1)) {
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
            invulnerabilityTimer -= dt;
        }
        
        // Blur decay
        if(blurTimer > 0.0f) {
            blurTimer -= dt;
            if(blurTimer < 0.0f) blurTimer = 0.0f;
        }

        // Win/Lose conditions + round progression
        if(gameplayActive) {
            if(playerHealth <= 0.0f) {
                endGame(GameOutcome::Lose);
            }

            roundTimer -= dt;
            if(roundTimer <= 0.0f) {
                roundsCompleted++;
                if(roundsCompleted >= totalRoundsToWin) {
                    endGame(GameOutcome::Win);
                } else {
                    currentRound = roundsCompleted + 1;
                    roundTimer = roundDurationSeconds;
                    zombieSystem.setRespawnDelaySeconds(respawnDelayForRound(currentRound));
                    std::cout << "=== ROUND " << currentRound << " === RespawnDelay="
                              << zombieSystem.getRespawnDelaySeconds() << "s" << std::endl;
                }
            }
        }
        gameplayActive = (outcome == GameOutcome::None);

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
        
        if(gameplayActive && playerFound && invulnerabilityTimer <= 0.0f && !isDead) { // Don't take damage if dead
             for(auto entity : world.getEntities()) {
                 if(entity->name.find("Zombie") != std::string::npos) {
                     if(auto* health = entity->getComponent<our::HealthComponent>()) {
                         if(health->isDead()) continue;
                     }

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
                         healthRegenCooldown = healthRegenDelay;
                         
                         // Trigger Blur
                         blurTimer = 0.5f; 
                         
                         std::cout << "!!! DAMAGE TAKEN !!! Health: " << playerHealth << "/" << maxHealth << std::endl;
                     }
                }
             }
        }

        // Health regeneration (out of combat)
        if(gameplayActive && !isDead) {
            if(healthRegenCooldown > 0.0f) {
                healthRegenCooldown -= dt;
                if(healthRegenCooldown < 0.0f) healthRegenCooldown = 0.0f;
            } else if(playerHealth < maxHealth) {
                playerHealth += healthRegenRate * dt;
                if(playerHealth > maxHealth) playerHealth = maxHealth;
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
         
        // --- Render UI (Health + Crosshair) ---
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

            // 3. Draw Crosshair (center of screen)
            {
                float centerX = size.x * 0.5f;
                float centerY = size.y * 0.5f;

                bool aiming = weaponSystem.isAiming();
                float length = aiming ? 8.0f : 14.0f;
                float thickness = 3.0f;
                float alpha = aiming ? 0.9f : 0.75f;

                healthBarMaterial->tint = glm::vec4(1.0f, 1.0f, 1.0f, alpha);
                healthBarMaterial->setup();

                // Horizontal line
                glm::mat4 H = glm::translate(glm::mat4(1.0f), glm::vec3(centerX - length, centerY - thickness * 0.5f, 0.0f)) *
                              glm::scale(glm::mat4(1.0f), glm::vec3(length * 2.0f, thickness, 1.0f));
                healthBarMaterial->shader->set("transform", VP*H);
                uiRectangle->draw();

                // Vertical line
                glm::mat4 V = glm::translate(glm::mat4(1.0f), glm::vec3(centerX - thickness * 0.5f, centerY - length, 0.0f)) *
                              glm::scale(glm::mat4(1.0f), glm::vec3(thickness, length * 2.0f, 1.0f));
                healthBarMaterial->shader->set("transform", VP*V);
                uiRectangle->draw();
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

    void onImmediateGui() override {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 displaySize = io.DisplaySize;

        // HUD (non-interactive)
        if(outcome == GameOutcome::None) {
            ImGui::SetNextWindowPos(ImVec2(displaySize.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.35f);
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;
            ImGui::Begin("HUD", nullptr, flags);
            ImGui::SetWindowFontScale(1.6f);
            ImGui::Text("Round: %d/%d", currentRound, totalRoundsToWin);
            ImGui::Text("Time: %.0fs", roundTimer);
            ImGui::Text("Kills: %d", kills);
            ImGui::End();
            return;
        }

        // Game Over / Win screen overlay
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(displaySize);
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("GameOverOverlay", nullptr, flags);

        auto centeredText = [&](float y, float scale, const char* text) {
            ImGui::SetWindowFontScale(scale);
            ImVec2 textSize = ImGui::CalcTextSize(text);
            ImGui::SetCursorPos(ImVec2((displaySize.x - textSize.x) * 0.5f, y));
            ImGui::TextUnformatted(text);
        };

        ImGui::SetWindowFontScale(1.0f);
        centeredText(displaySize.y * 0.18f, 3.0f, "GAME OVER");

        if(outcome == GameOutcome::Win) {
            centeredText(displaySize.y * 0.30f, 2.0f, "YOU WIN! Survived the Night");
        } else {
            centeredText(displaySize.y * 0.30f, 2.0f, "YOU DIED");
        }

        ImGui::SetWindowFontScale(1.6f);
        {
            char line[128];
            std::snprintf(line, sizeof(line), "Rounds Survived: %d", roundsSurvivedAtEnd);
            centeredText(displaySize.y * 0.42f, 1.6f, line);

            std::snprintf(line, sizeof(line), "Kills: %d", killsAtEnd);
            centeredText(displaySize.y * 0.48f, 1.6f, line);
        }

        centeredText(displaySize.y * 0.62f, 1.3f, "Press ENTER to return to Menu");

        if(ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            getApp()->changeState("menu");
        }

        ImGui::End();
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
