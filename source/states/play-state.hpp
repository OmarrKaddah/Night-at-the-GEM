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
#include <sound/sound-manager.hpp>

#include <asset-loader.hpp>
#include <fstream>
#include <string>
#include <cstdio>
#include <material/material.hpp>
#include <mesh/mesh.hpp>
#include <algorithm>
// This state shows how to use the ECS framework and deserialization.
namespace our
{
    class Playstate : public State
    {

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
        float previousPlayerY = 0.0f; // Track Y position for stair detection

    // Win/Lose + Nights
    enum class GameOutcome { None, Win, Lose };
    GameOutcome outcome = GameOutcome::None;
    int kills = 0;
    
    // Night System
    int currentNight = 1;
    int totalNightsToWin = 5;
    int nightsCompleted = 0; 
    
    // Stats at End
    int nightsSurvivedAtEnd = 0;
    int killsAtEnd = 0;
    
    // Time System
    float nightTimer = 0.0f; // Counts DOWN from nightDuration
    int currentHour = 12;    // 12, 1, 2, 3, 4, 5
    int lastHourChime = 12;  // Tracks when to trigger hourly events

    // Health System
    float playerHealth = 100.0f;
    float maxHealth = 100.0f;
    float invulnerabilityTimer = 0.0f;
    float healthRegenCooldown = 0.0f;
    float healthRegenDelay = 3.0f;     // seconds after taking damage before regen starts
    float healthRegenRate = 6.0f;      // health per second
    // Win Condition
    float nightDuration = 300.0f; // Default 5 mins (Length of 1 Night)
    bool isWin = false;
    float clockFlashTimer = 0.0f; // Timer for clock overlay display

    // Missing members restored
    float blurTimer = 0.0f;
    float gameOverTimer = 0.0f;
    bool isDead = false;

    // Footstep System
    float footstepTimer = 0.0f;
    float footstepInterval = 0.4f; // Time between footsteps when walking
    bool wasMoving = false;

    // UI Resources
    our::Mesh* uiRectangle = nullptr;
    our::TintedMaterial* healthBarMaterial = nullptr;
    our::TintedMaterial* healthBgMaterial = nullptr;
    our::TexturedMaterial* clockMaterial = nullptr;


    float respawnDelayForNight(int night) const {
        // Night 1: 10s delay
        // Night 5: 2.0s delay
        // Scaling: -2.0s per night
        float delay = 10.0f - (float)(night - 1) * 2.0f;
        if(delay < 1.0f) delay = 1.0f; // Cap at 1s minimum
        return delay;
    }

    void endGame(GameOutcome newOutcome) {
        if(outcome != GameOutcome::None) return;
        outcome = newOutcome;
        nightsSurvivedAtEnd = nightsCompleted;
        killsAtEnd = kills;
        isDead = (newOutcome == GameOutcome::Lose);
        our::Mouse::unlockMouse(getApp()->getWindow());
        
        if (newOutcome == GameOutcome::Win) {
             // Trigger 6 AM Screen (duration 8s)
             clockFlashTimer = 8.0f;
        }
    }

        void onInitialize() override
        {
            // First of all, we get the scene configuration from the app config
            auto &config = getApp()->getConfig()["scene"];
            // If we have assets in the scene config, we deserialize them
            if (config.contains("assets"))
            {
                // we rely on the loading state to load the assets
                // our::deserializeAllAssets(config["assets"]);
            }
            // If we have a world in the scene config, we use it to populate our world
            if (config.contains("world"))
            {
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
        
        currentNight = 1;
        nightsCompleted = 0;
        nightsSurvivedAtEnd = 0;
        killsAtEnd = 0;
        
        // Read configuration (BEFORE calculating timer)
        if(config.contains("nightDuration")) {
            nightDuration = config["nightDuration"].get<float>();
        } else {
            nightDuration = 300.0f; // Default 5 mins
        }
        
        // std::cout << "Playstate::onInitialize - Night Duration set to: " << nightDuration << " seconds" << std::endl;
        
        nightTimer = nightDuration;
        currentHour = 12;
        lastHourChime = 12;
        clockFlashTimer = 8.0f; // Flash 12 AM (Night 1 Intro) on start for full duration
        
        // Initial Difficulty
        zombieSystem.setRespawnDelaySeconds(respawnDelayForNight(currentNight));
        
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
                        glm::ivec2 minMin(obs["min"][0], obs["min"][1]);
                        glm::ivec2 maxMax(obs["max"][0], obs["max"][1]);
                        floor0Grid.setObstacle(minMin, maxMax);
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
                        glm::ivec2 minMin(obs["min"][0], obs["min"][1]);
                        glm::ivec2 maxMax(obs["max"][0], obs["max"][1]);
                        floor1Grid.setObstacle(minMin, maxMax);
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
                        glm::ivec2 minMin(obs["min"][0], obs["min"][1]);
                        glm::ivec2 maxMax(obs["max"][0], obs["max"][1]);
                        floor2Grid.setObstacle(minMin, maxMax);
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

        // Initialize sound system and load sounds
        SOUND_MANAGER->initialize();
        SOUND_MANAGER->loadSound("weapon_gunshot", "assets/sounds/weapons/gunshot.wav");

        // Load footstep sounds
        SOUND_MANAGER->loadSound("footstep_default", "assets/sounds/footsteps/footsteps.wav");
        SOUND_MANAGER->playSound("footstep_default");

        // Load zombie sounds
        SOUND_MANAGER->loadSound("zombie_attack", "assets/sounds/zombie/attack.wav");
        SOUND_MANAGER->loadSound("zombie_death", "assets/sounds/zombie/death.wav");

        // Load player damage sound

        // Play background music
        SOUND_MANAGER->playMusic("assets/sounds/music/suspense.wav", true);
        SOUND_MANAGER->setMusicVolume(0.3f); // Lower volume for background

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

        isWin = false;
        healthRegenCooldown = 0.0f;

            // Create UI Rectangle (1x1)


            if (!uiRectangle)
            {
                uiRectangle = new our::Mesh({
                                                {{0.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                                                {{1.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                                                {{1.0f, 1.0f, 0.0f}, {255, 255, 255, 255}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
                                                {{0.0f, 1.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
                                            },
                                            {
                                                0,
                                                1,
                                                2,
                                                2,
                                                3,
                                                0,
                                            });
            }

            // Create Health Materials
            if (!healthBarMaterial)
            {
                healthBarMaterial = new our::TintedMaterial();
                healthBarMaterial->shader = new our::ShaderProgram();
                healthBarMaterial->shader->attach("assets/shaders/tinted.vert", GL_VERTEX_SHADER);
                healthBarMaterial->shader->attach("assets/shaders/tinted.frag", GL_FRAGMENT_SHADER);
                healthBarMaterial->shader->link();
                healthBarMaterial->tint = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);   // Red
                healthBarMaterial->pipelineState.depthTesting.enabled = false; // Always on top
                healthBarMaterial->pipelineState.blending.enabled = true;
                healthBarMaterial->pipelineState.blending.sourceFactor = GL_SRC_ALPHA;
                healthBarMaterial->pipelineState.blending.destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
            }
            // Create Health Materials
            if (!healthBarMaterial)
            {
                healthBarMaterial = new our::TintedMaterial();
                healthBarMaterial->shader = new our::ShaderProgram();
                healthBarMaterial->shader->attach("assets/shaders/tinted.vert", GL_VERTEX_SHADER);
                healthBarMaterial->shader->attach("assets/shaders/tinted.frag", GL_FRAGMENT_SHADER);
                healthBarMaterial->shader->link();
                healthBarMaterial->tint = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);   // Red
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

        if (!clockMaterial) {
            clockMaterial = new our::TexturedMaterial();
            clockMaterial->shader = our::AssetLoader<our::ShaderProgram>::get("textured");
            clockMaterial->tint = glm::vec4(1.0f);
            clockMaterial->pipelineState.depthTesting.enabled = false;
            clockMaterial->pipelineState.blending.enabled = true;
            clockMaterial->pipelineState.blending.destinationFactor = GL_ONE;
            clockMaterial->sampler = our::AssetLoader<our::Sampler>::get("default");
            clockMaterial->alphaThreshold = 0.0f; // Disable alpha discard for smooth adding
        }
        

        
    }

        void onDraw(double deltaTime) override
        {
            float dt = (float)deltaTime;

            // Update sound system
            SOUND_MANAGER->update(dt);

            // Handle mouse rotation and keyboard movement for camera
            auto &mouse = getApp()->getMouse();
            auto &kb = getApp()->getKeyboard();
            bool gameplayActive = (outcome == GameOutcome::None);

            // Lock mouse on startup

            if (gameplayActive)
                for (auto entity : world.getEntities())
                {
                    auto *collider = entity->getComponent<our::BulletColliderComponent>();
                    auto *camera = entity->getComponent<our::CameraComponent>();
                    // Only apply mouse rotation to entities with a camera
                    if (collider && collider->mass > 0.0f && collider->rigidBody && camera)
                    {
                        // Handle mouse rotation (always active now)
                        glm::vec2 delta = mouse.getMouseDelta();
                        if (first_frame)
                        {
                            delta = glm::vec2(0.0f);
                            first_frame = false;
                        }

                        // Deadzone to prevent drift
                        if (glm::length(delta) < 10.0f)
                            delta = glm::vec2(0.0f);

                        glm::vec3 rotation = entity->localTransform.rotation;
                        float lookSensitivity = 0.01f;
                        if (weaponSystem.isAiming())
                            lookSensitivity *= 0.5f;
                        rotation.x -= delta.y * lookSensitivity;
                        rotation.y -= delta.x * lookSensitivity;

                        // Clamp pitch to prevent flipping
                        if (rotation.x < -glm::half_pi<float>() * 0.99f)
                            rotation.x = -glm::half_pi<float>() * 0.99f;
                        if (rotation.x > glm::half_pi<float>() * 0.99f)
                            rotation.x = glm::half_pi<float>() * 0.99f;
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
                        if (collider->rigidBody->getMotionState())
                        {
                            collider->rigidBody->getMotionState()->setWorldTransform(trans);
                        }

                        glm::vec3 velocity(0, 0, 0);
                        float speed = 5.0f;

                        // Get camera direction
                        glm::mat4 matrix = entity->localTransform.toMat4();
                        glm::vec3 forward = glm::vec3(matrix * glm::vec4(0, 0, -1, 0));
                        glm::vec3 right = glm::vec3(matrix * glm::vec4(1, 0, 0, 0));

                        // WASD movement
                        if (kb.isPressed(GLFW_KEY_W))
                            velocity += forward * speed;
                        if (kb.isPressed(GLFW_KEY_S))
                            velocity -= forward * speed;
                        if (kb.isPressed(GLFW_KEY_D))
                            velocity += right * speed;
                        if (kb.isPressed(GLFW_KEY_A))
                            velocity -= right * speed;

                        // Footstep sounds
                        bool isMoving = glm::length(glm::vec2(velocity.x, velocity.z)) > 0.1f;

                        if (isMoving)
                        {
                            // FIRST STEP → play immediately
                            if (!wasMoving)
                            {
                                SOUND_MANAGER->playSound("footstep_default");
                                footstepTimer = footstepInterval;
                            }
                            else
                            {
                                footstepTimer -= dt;
                                if (footstepTimer <= 0.0f)
                                {
                                    SOUND_MANAGER->playSound("footstep_default");
                                    footstepTimer = footstepInterval;
                                }
                            }
                        }
                        else
                        {
                            footstepTimer = 0.0f;
                        }

                        wasMoving = isMoving;

                        // Apply velocity to physics (gravity handles Y)
                        collider->rigidBody->setLinearVelocity(btVector3(velocity.x, collider->rigidBody->getLinearVelocity().y(), velocity.z));
                        collider->rigidBody->activate();
                    }
                }

            // Run other systems (but NOT camera controller - we handle movement with physics)
            // movementSystem.update(&world, (float)deltaTime); // DISABLED: overwrites physics velocity!
            // cameraController.update(&world, (float)deltaTime); // DISABLED

            // Update Zombie AI
            if (gameplayActive)
            {
                weaponSystem.update(&world, dt);
                zombieSystem.update(&world, dt);
            }

            // Update animations (Calculate bone positions)
            animationSystem.update(&world, dt);
            if (gameplayActive)
                for (auto entity : world.getEntities())
                {
                    auto *collider = entity->getComponent<our::BulletColliderComponent>();
                    auto *camera = entity->getComponent<our::CameraComponent>();
                    // Only apply mouse rotation to entities with a camera
                    if (collider && collider->mass > 0.0f && collider->rigidBody && camera)
                    {
                        // Handle mouse rotation (always active now)
                        glm::vec2 delta = mouse.getMouseDelta();
                        if (first_frame)
                        {
                            delta = glm::vec2(0.0f);
                            first_frame = false;
                        }

                        // Deadzone to prevent drift
                        if (glm::length(delta) < 10.0f)
                            delta = glm::vec2(0.0f);

                        glm::vec3 rotation = entity->localTransform.rotation;
                        float lookSensitivity = 0.01f;
                        if (weaponSystem.isAiming())
                            lookSensitivity *= 0.5f;
                        rotation.x -= delta.y * lookSensitivity;
                        rotation.y -= delta.x * lookSensitivity;

                        // Clamp pitch to prevent flipping
                        if (rotation.x < -glm::half_pi<float>() * 0.99f)
                            rotation.x = -glm::half_pi<float>() * 0.99f;
                        if (rotation.x > glm::half_pi<float>() * 0.99f)
                            rotation.x = glm::half_pi<float>() * 0.99f;
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
                        if (collider->rigidBody->getMotionState())
                        {
                            collider->rigidBody->getMotionState()->setWorldTransform(trans);
                        }

                        glm::vec3 velocity(0, 0, 0);
                        float speed = 5.0f;

                        // Get camera direction
                        glm::mat4 matrix = entity->localTransform.toMat4();
                        glm::vec3 forward = glm::vec3(matrix * glm::vec4(0, 0, -1, 0));
                        glm::vec3 right = glm::vec3(matrix * glm::vec4(1, 0, 0, 0));

                        // WASD movement
                        if (kb.isPressed(GLFW_KEY_W))
                            velocity += forward * speed;
                        if (kb.isPressed(GLFW_KEY_S))
                            velocity -= forward * speed;
                        if (kb.isPressed(GLFW_KEY_D))
                            velocity += right * speed;
                        if (kb.isPressed(GLFW_KEY_A))
                            velocity -= right * speed;

                        // Footstep sounds
                        bool isMoving = glm::length(glm::vec2(velocity.x, velocity.z)) > 0.1f;

                        if (isMoving)
                        {
                            // FIRST STEP → play immediately
                            if (!wasMoving)
                            {
                                SOUND_MANAGER->playSound("footstep_default");
                                footstepTimer = footstepInterval;
                            }
                            else
                            {
                                footstepTimer -= dt;
                                if (footstepTimer <= 0.0f)
                                {
                                    SOUND_MANAGER->playSound("footstep_default");
                                    footstepTimer = footstepInterval;
                                }
                            }
                        }
                        else
                        {
                            footstepTimer = 0.0f;
                        }

                        wasMoving = isMoving;

                        // Apply velocity to physics (gravity handles Y)
                        collider->rigidBody->setLinearVelocity(btVector3(velocity.x, collider->rigidBody->getLinearVelocity().y(), velocity.z));
                        collider->rigidBody->activate();
                    }
                }

            // Run other systems (but NOT camera controller - we handle movement with physics)
            // movementSystem.update(&world, (float)deltaTime); // DISABLED: overwrites physics velocity!
            // cameraController.update(&world, (float)deltaTime); // DISABLED

            // Update Zombie AI
            if (gameplayActive)
            {
                weaponSystem.update(&world, dt);
                zombieSystem.update(&world, dt);
            }

            // Update animations (Calculate bone positions)
            animationSystem.update(&world, dt);

        // Update physics simulation (this applies collision response)
        physicsSystem.update(dt);
        
        // Sync physics results BACK to entities
        // Sync physics results BACK to entities
        for(auto entity : world.getEntities()){
            auto* collider = entity->getComponent<our::BulletColliderComponent>();
            if(collider && collider->mass > 0.0f) {
                collider->syncToEntity();
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
                 // Trigger Game Over Screen Overlay
                 endGame(GameOutcome::Lose);
             }
        }

            glm::vec3 playerPos = glm::vec3(0.0f);
            bool playerFound = false;

            // Find player position
            for (auto entity : world.getEntities())
            {
                if (entity->name == "PlayerCamera")
                {
                    playerPos = entity->localTransform.position;
                    playerFound = true;
                    break;
                }
            }

            if (gameplayActive && playerFound && invulnerabilityTimer <= 0.0f && !isDead)
            { // Don't take damage if dead
                for (auto entity : world.getEntities())
                {
                    if (entity->name.find("Zombie") != std::string::npos)
                    {
                        if (auto *health = entity->getComponent<our::HealthComponent>())
                        {
                            if (health->isDead())
                                continue;
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

                        // Play damage sound
                        SOUND_MANAGER->playSound("player_hurt");
                         
                        //   std::cout << "!!! DAMAGE TAKEN !!! Health: " << playerHealth << "/" << maxHealth << std::endl;
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

            // --- NIGHT / TIME LOGIC ---
            nightTimer -= dt;
            
            // Calculate Hour (12 AM to 6 AM)
            float hourDuration = nightDuration / 6.0f;
            int hoursPassed = (int)((nightDuration - nightTimer) / hourDuration);
            
            // Clamp to 6 to avoid going to 7 AM briefly
            if(hoursPassed > 6) hoursPassed = 6;
            
            int displayHour = (hoursPassed == 0) ? 12 : hoursPassed;
            
            // Check for Hour Change
            if(displayHour != lastHourChime) {
                lastHourChime = displayHour;
                currentHour = displayHour;
                
                // Show Clock Flash (e.g. "1 AM")
                if(displayHour != 12) { // Don't flash 12 immediately at reset if we just flashed 6
                     clockFlashTimer = 4.0f; 
                     // std::cout << "It is now " << displayHour << " AM" << std::endl;
                }
            }

            if(nightTimer <= 0.0f) {
                 // NIGHT COMPLETE (6 AM Reached)
                 nightsCompleted++;
                 currentNight++;
                 
                 // Reset ZOMBIES
                 zombieSystem.resetAll(&world);
                 
                 // Show Night Screen
                 clockFlashTimer = 8.0f; // Give 8 seconds for "Night X" intermission
                 
                 currentHour = 12; // Reset hour to 12
                 
                 // Update Difficulty
                 zombieSystem.setRespawnDelaySeconds(respawnDelayForNight(currentNight));
                 
                 // Check Win Condition
                 if(nightsCompleted >= totalNightsToWin) { 
                     // WIN
                     endGame(GameOutcome::Win);
                 } else {
                     // NEXT NIGHT START
                     nightTimer = nightDuration; // Reset timer for next night
                     // std::cout << "Night " << currentNight << " Started (Intermission)" << std::endl;
                 }
            }
        }

        // INTERMISSION PAUSE LOGIC:
        // If we are showing "Night X" (currentHour == 12 && clockFlashTimer > 0),
        // we should PAUSE zombie updates to let them "regenerate" (stay despawned/reset).
        bool isIntermission = (currentHour == 12 && clockFlashTimer > 0.0f);
        
        // Update Zombie AI (ONLY if not in intermission)
        if(gameplayActive && !isIntermission) {
            weaponSystem.update(&world, dt);
            zombieSystem.update(&world, dt);
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

            // 3. Draw Clock / Hour Chime
            // Determine which hour we are TRANSITIONING TO or completed.
            // If Win -> 6 AM.
            // If Flash -> roundsCompleted AM.
            
            if (outcome == GameOutcome::Win) {
                clockFlashTimer -= dt; // Decrement timer so the sequence progresses!

                // RENDER 6 AM SCREEN (Solid)
                if (clockFlashTimer > 0.0f) {
                    clockMaterial->texture = our::AssetLoader<our::Texture2D>::get("time-6am");
                    if (clockMaterial->texture) {
                        glm::mat4 MClock = glm::scale(glm::mat4(1.0f), glm::vec3((float)size.x, (float)size.y, 1.0f));
                        clockMaterial->setup();
                        
                        // Optional: Fade out in last second
                        float alpha = 1.0f;
                        if(clockFlashTimer < 1.0f) alpha = clockFlashTimer; 
                        
                        clockMaterial->shader->set("tint", glm::vec4(1.0f, 1.0f, 1.0f, alpha)); // Solid -> Fade
                        clockMaterial->shader->set("transform", VP*MClock);
                        uiRectangle->draw();
                    }
                }
            } else if (clockFlashTimer > 0.0f) {
                // HOURLY CHIME / NIGHT INTRO
                clockFlashTimer -= dt;
                
                // Determine Hour Texture & Settings
                std::string textureName;
                our::Texture2D* textureToDraw = nullptr;
                float alpha = 1.0f;
                bool isNightIntro = (currentHour == 12);

                if (isNightIntro) {
                     // NIGHT INTRO (8.0s total)
                     // Fade In (8->7), Hold (7->1), Fade Out (1->0)
                     if (clockFlashTimer > 7.0f) alpha = 8.0f - clockFlashTimer;
                     else if (clockFlashTimer < 1.0f) alpha = clockFlashTimer;
                     else alpha = 1.0f;
                     
                     // Helper to ensure 0-1 range
                     if (alpha < 0.0f) alpha = 0.0f;
                     if (alpha > 1.0f) alpha = 1.0f;

                     // Load Night Texture
                     textureName = "time-night" + std::to_string(currentNight);
                     textureToDraw = our::AssetLoader<our::Texture2D>::get(textureName);
                     
                     // Fallback
                     if(!textureToDraw) {
                         std::string path = "assets/textures/time/night" + std::to_string(currentNight) + ".png";
                         textureToDraw = our::texture_utils::loadImage(path);
                     }
                     
                     // 1. Draw Black Background (Fade with alpha)
                     glm::mat4 MFull = glm::scale(glm::mat4(1.0f), glm::vec3((float)size.x, (float)size.y, 1.0f));
                     
                     healthBgMaterial->tint = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // Solid Black (ignoring alpha? or fade it?)
                     // User said "have black background". Usually means covers everything.
                     // If I fade the background, the game reveals behind it.
                     // If I keep background opaque, it transitions harshly.
                     // User said "fade in and out", implying the whole overlay.
                     healthBgMaterial->tint.a = alpha; 
                     
                     healthBgMaterial->setup();
                     healthBgMaterial->shader->set("transform", VP*MFull);
                     uiRectangle->draw();
                     
                     // Switch to Normal Blending for the Image (since it might have its own background or we want it opaque on top of black)
                     clockMaterial->pipelineState.blending.destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
                     
                } else {
                     // HOURLY CHIME (4.0s total)
                     // Just fade out at end
                     if(clockFlashTimer < 1.0f) alpha = clockFlashTimer; // Fade out last second
                     
                     textureName = "time-" + std::to_string(currentHour) + "am";
                     textureToDraw = our::AssetLoader<our::Texture2D>::get(textureName);
                     
                     // Use Additive Blending for Hour Chimes (Transparent text)
                     clockMaterial->pipelineState.blending.destinationFactor = GL_ONE;
                }
                
                clockMaterial->texture = textureToDraw;
                
                if(clockMaterial->texture) {
                    glm::mat4 MClock = glm::scale(glm::mat4(1.0f), glm::vec3((float)size.x, (float)size.y, 1.0f));
                                    
                    clockMaterial->setup();
                    clockMaterial->shader->set("tint", glm::vec4(1.0f, 1.0f, 1.0f, alpha)); 
                    clockMaterial->shader->set("transform", VP*MClock);
                    uiRectangle->draw();
                }
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
                    healthBarMaterial->shader->set("transform", VP * H);
                    uiRectangle->draw();

                    // Vertical line
                    glm::mat4 V = glm::translate(glm::mat4(1.0f), glm::vec3(centerX - thickness * 0.5f, centerY - length, 0.0f)) *
                                  glm::scale(glm::mat4(1.0f), glm::vec3(thickness, length * 2.0f, 1.0f));
                    healthBarMaterial->shader->set("transform", VP * V);
                    uiRectangle->draw();
                }
            }

            // Get a reference to the keyboard object
            auto &keyboard = getApp()->getKeyboard();


            if (keyboard.justPressed(GLFW_KEY_ESCAPE))
            {
                getApp()->changeState("menu");
            }

            // Disable/Enable Skinning (Diagnostic)
            if (keyboard.justPressed(GLFW_KEY_K))
            {
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

        }

        void onImmediateGui() override
        {
            ImGuiIO &io = ImGui::GetIO();
            ImVec2 displaySize = io.DisplaySize;

        // HUD (non-interactive)
        if(outcome == GameOutcome::None) { // Original condition for HUD
            ImGui::SetNextWindowPos(ImVec2(displaySize.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.35f);
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;
            ImGui::Begin("HUD", nullptr, flags);
            ImGui::SetWindowFontScale(2.5f);
            ImGui::Text("Night: %d/%d", currentNight, totalNightsToWin);
            ImGui::Text("Time: %d AM", currentHour == 0 ? 12 : currentHour); // Handle 0 if happens
            ImGui::Text("Kills: %d", kills);
            ImGui::End();
            return;
        }


        // Game Over / Win screen overlay
        if (outcome == GameOutcome::Win && clockFlashTimer > 1.0f) {
            return;
        }

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(displaySize);
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("GameOverOverlay", nullptr, flags);

            auto centeredText = [&](float y, float scale, const char *text)
            {
                ImGui::SetWindowFontScale(scale);
                ImVec2 textSize = ImGui::CalcTextSize(text);
                ImGui::SetCursorPos(ImVec2((displaySize.x - textSize.x) * 0.5f, y));
                ImGui::TextUnformatted(text);
            };

        ImGui::SetWindowFontScale(1.0f);
        
        if (outcome == GameOutcome::Win) {
             centeredText(displaySize.y * 0.25f, 8.0f, "YOU WIN");
        } else {
             centeredText(displaySize.y * 0.25f, 8.0f, "GAME OVER");
        }

        if(outcome == GameOutcome::Win) {
            centeredText(displaySize.y * 0.40f, 5.0f, "Survived 5 Nights");
        } else {
            centeredText(displaySize.y * 0.40f, 5.0f, "YOU DIED");
        }

        ImGui::SetWindowFontScale(4.0f);
        {
            char line[128];
            std::snprintf(line, sizeof(line), "Nights Survived: %d", nightsSurvivedAtEnd);
            centeredText(displaySize.y * 0.55f, 4.0f, line);

            std::snprintf(line, sizeof(line), "Kills: %d", killsAtEnd);
            centeredText(displaySize.y * 0.65f, 4.0f, line);
        }

        centeredText(displaySize.y * 0.82f, 3.0f, "Press ENTER to return to Menu");

        if(getApp()->getKeyboard().justPressed(GLFW_KEY_ENTER) || getApp()->getKeyboard().justPressed(GLFW_KEY_KP_ENTER)) {
            getApp()->changeState("menu");
        }

            ImGui::End();
        }

        void onDestroy() override
        {
            std::cout << "Playstate::onDestroy - Start" << std::endl;
            SOUND_MANAGER->stopMusic();
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

}