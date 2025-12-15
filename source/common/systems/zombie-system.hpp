#pragma once

#include "../ecs/world.hpp"
#include "../components/animator.hpp"
#include "../components/bullet-collider.hpp"
#include "../components/health.hpp"
#include "../navigation/nav-grid-2d.hpp"
#include "../navigation/pathfinder-2d.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <unordered_map>
#include <vector>
#include <iostream>

namespace our
{

    class ZombieSystem {
    private:
        Entity* cachedPlayer = nullptr;
        
        // Navigation
        NavGrid2D* floor0Grid = nullptr;
        NavGrid2D* floor1Grid = nullptr;
        NavGrid2D* floor2Grid = nullptr;
        Pathfinder2D pathfinder;
        
        float respawnDelaySeconds = 10.0f;

    public:
        struct ZombieState {
            std::vector<glm::vec3> path;
            size_t pathIndex = 0;
            float pathTimer = 0.0f;
            glm::vec3 lastPosition = glm::vec3(0);
            float stuckTimer = 0.0f;
            bool dead = false;
            float respawnTimer = 0.0f;

            bool spawnSaved = false;
            glm::vec3 spawnPosition = glm::vec3(0);
            glm::vec3 spawnRotation = glm::vec3(0);
            glm::vec3 spawnScale = glm::vec3(1);
        };
        
        std::unordered_map<Entity*, ZombieState> states;

        void setRespawnDelaySeconds(float seconds) {
            respawnDelaySeconds = (seconds < 0.5f) ? 0.5f : seconds;
        }
        [[nodiscard]] float getRespawnDelaySeconds() const { return respawnDelaySeconds; }

        struct StairWaypoint {
            glm::vec3 position;
            int targetFloor;
            int next = -1;  // Index of next waypoint in chain (-1 = end)
        };
        std::vector<StairWaypoint> stairWaypoints;

        // Initialize with navigation grids and stair waypoints
        void initialize(NavGrid2D* f0Grid, NavGrid2D* f1Grid, NavGrid2D* f2Grid, const std::vector<StairWaypoint>& stairs) {
            floor0Grid = f0Grid;
            floor1Grid = f1Grid;
            floor2Grid = f2Grid;
            stairWaypoints = stairs;
        }

        // Reset all zombies to spawn state (Regenerate)
        void resetAll(World* world) {
             for(auto entity : world->getEntities()){
                if(entity->name.find("Zombie") != 0) continue;
                
                auto& state = states[entity];
                
                // Reset State vars
                state.dead = false;
                state.respawnTimer = 0.0f;
                state.path.clear();
                state.pathIndex = 0;
                state.pathTimer = 0.0f;
                state.stuckTimer = 0.0f;

                // Reset Transform
                if (state.spawnSaved) {
                    entity->localTransform.position = state.spawnPosition;
                    entity->localTransform.rotation = state.spawnRotation;
                    entity->localTransform.scale = state.spawnScale;
                }

                // Reset Health
                if(auto* health = entity->getComponent<HealthComponent>()) {
                    health->currentHealth = health->maxHealth;
                }

                // Reset Physics
                if(auto* collider = entity->getComponent<BulletColliderComponent>()) {
                    if(collider->rigidBody) {
                        collider->rigidBody->setLinearVelocity(btVector3(0,0,0));
                        collider->rigidBody->setAngularVelocity(btVector3(0,0,0));
                        collider->rigidBody->clearForces();
                        collider->rigidBody->activate(true);
                        
                        // Sync Transform to Physics immediately
                        btTransform trans = collider->rigidBody->getWorldTransform();
                        trans.setOrigin(btVector3(state.spawnPosition.x, state.spawnPosition.y, state.spawnPosition.z));
                        
                        glm::quat q(state.spawnRotation);
                        trans.setRotation(btQuaternion(q.x, q.y, q.z, q.w));
                        
                        collider->rigidBody->setWorldTransform(trans);
                    }
                }
                
                // Reset Animation
                if(auto* animator = entity->getComponent<AnimatorComponent>()) {
                    animator->playAnimation("walk", true);
                }
             }
             std::cout << "ZombieSystem: All zombies reset (Regenerated)." << std::endl;
        }

        // Cleanup zombie states so we don't hold invalid entity pointers
        void destroy() {
            states.clear();
            cachedPlayer = nullptr;
        }
        
        // Determine which floor based on Y position
        int getFloor(float y) const {
            if (y < 1.5f) return 0;       // Ground floor + stairs (Y around -0.4 to 1.5)
            else if (y < 4.0f) return 1;  // First floor (Y around 1.5 to 4.0)
            else return 2;                 // Second floor (Y >= 4.0)
        }
        
        NavGrid2D* getGridForFloor(int floor) const {
            if (floor == 0) return floor0Grid;
            if (floor == 1) return floor1Grid;
            return floor2Grid;
        }
        
        void update(World* world, float deltaTime) {
            // Cache player lookup
            if(!cachedPlayer || cachedPlayer->name != "PlayerCamera") {
                cachedPlayer = nullptr;
                for(auto entity : world->getEntities()){
                    if(entity->name == "PlayerCamera") {
                        cachedPlayer = entity;
                        break;
                    }
                }
            }
            
            if(!cachedPlayer) return;

            glm::vec3 playerPos = cachedPlayer->localTransform.position;

            // Update all zombies
            for(auto entity : world->getEntities()){
                if(entity->name.find("Zombie") != 0) continue;
                
                auto& state = states[entity];

                if(!state.spawnSaved) {
                    state.spawnSaved = true;
                    state.spawnPosition = entity->localTransform.position;
                    state.spawnRotation = entity->localTransform.rotation;
                    state.spawnScale = entity->localTransform.scale;
                }

                // If zombie has 0 health, play death animation once and stop movement.
                if(auto* health = entity->getComponent<HealthComponent>()) {
                    if(health->isDead()) {
                        if(!state.dead) {
                            state.dead = true;
                            state.path.clear();
                            state.pathIndex = 0;
                            state.pathTimer = 0.0f;
                            state.stuckTimer = 0.0f;

                            if(auto* animator = entity->getComponent<AnimatorComponent>()) {
                                animator->playAnimation("death", false);
                            }

                            if(auto* collider = entity->getComponent<BulletColliderComponent>()) {
                                if(collider->rigidBody) {
                                    collider->rigidBody->setLinearVelocity(btVector3(0, 0, 0));
                                    collider->rigidBody->setAngularVelocity(btVector3(0, 0, 0));
                                    collider->rigidBody->setLinearFactor(btVector3(0, 0, 0));
                                    collider->rigidBody->setAngularFactor(btVector3(0, 0, 0));
                                }
                            }

                            // std::cout << entity->name << " died." << std::endl;
                            state.respawnTimer = respawnDelaySeconds;
                        } else {
                            state.respawnTimer -= deltaTime;
                        }

                        if(state.respawnTimer > 0.0f) continue;

                        // Respawn
                        state.dead = false;
                        state.respawnTimer = 0.0f;
                        state.path.clear();
                        state.pathIndex = 0;
                        state.pathTimer = 0.0f;
                        state.stuckTimer = 0.0f;

                        health->currentHealth = health->maxHealth;
                        entity->localTransform.position = state.spawnPosition;
                        entity->localTransform.rotation = state.spawnRotation;
                        entity->localTransform.scale = state.spawnScale;

                        if(auto* collider = entity->getComponent<BulletColliderComponent>()) {
                            if(collider->rigidBody) {
                                collider->rigidBody->setLinearFactor(btVector3(1, 1, 1));
                                collider->rigidBody->setAngularFactor(btVector3(0, 1, 0));
                                collider->rigidBody->activate(true);
                            }
                            collider->syncFromEntity();
                        }

                        if(auto* animator = entity->getComponent<AnimatorComponent>()) {
                            animator->playAnimation("walk", true);
                        }

                        // std::cout << entity->name << " respawned." << std::endl;
                    }
                }

                state.pathTimer -= deltaTime;
                
                glm::vec3 zombiePos = entity->localTransform.position;
                
                // Recalculate path every 0.5 seconds (but not while actively climbing stairs)
                if (state.pathTimer <= 0.0f) {
                    state.pathTimer = 0.5f;
                    
                    // Determine which floor each is on
                    int zombieFloor = getFloor(zombiePos.y);
                    int playerFloor = getFloor(playerPos.y);
                    
                    // Only recalculate if: same floor OR path is empty OR reached end of path
                    // Don't recalculate while actively climbing between floors
                    bool shouldRecalculate = true;  // Always recalculate to get proper stair waypoints
                    
                    if (!shouldRecalculate) {
                        continue;  // Skip recalculation, keep following current path
                    }
                    
                    // Check if player is in a stair transition zone (mid-landing area)
                    bool playerInStairZone = (playerPos.y >= 0.5f && playerPos.y <= 1.0f);
                    
                    // Check if zombie is in a stair transition zone (between floors)
                    bool zombieInStairZone = (zombiePos.y >= 0.3f && zombiePos.y <= 2.0f);
                    
                    if (zombieFloor == playerFloor && !playerInStairZone && !zombieInStairZone) {
                        // Same floor AND neither on stairs: direct pathfinding
                        NavGrid2D* grid = getGridForFloor(zombieFloor);
                        if (grid) {
                            state.path = pathfinder.findPath(zombiePos, playerPos, grid);
                        }
                    } else {
                        // Different floors OR someone in stair zone: Use generic stair navigation
                        state.path.clear();
                        
                        // Determine immediate next floor needed
                        int nextFloor = zombieFloor;
                        if (playerFloor > zombieFloor) nextFloor = zombieFloor + 1;
                        else if (playerFloor < zombieFloor) nextFloor = zombieFloor - 1;
                        
                        // If same floor (but in stair zone), just try to reach player floor or keep going depending on zone
                        // Actually if same floor but in stair zone, 'nextFloor' would be same.
                        // But findNearestStair expects a target floor. 
                        // If we are in stair zone, we are likely transitioning.
                        // Ideally we target playerFloor.
                        
                        if (nextFloor == zombieFloor) nextFloor = playerFloor;

                        StairWaypoint* stair = findNearestStair(zombiePos, nextFloor);
                        
                        if (stair) {
                            // Add initial waypoint
                            state.path.push_back(stair->position);
                            
                            // Follow the chain
                            int currentIdx = stair->next;
                            int safety = 0;
                            while(currentIdx != -1 && currentIdx >= 0 && currentIdx < (int)stairWaypoints.size() && safety++ < 10) {
                                state.path.push_back(stairWaypoints[currentIdx].position);
                                currentIdx = stairWaypoints[currentIdx].next;
                            }
                        }
                    }
                    
                    state.pathIndex = 0;
                }
                
                // Follow path
                if (!state.path.empty() && state.pathIndex < state.path.size()) {
                    glm::vec3 target = state.path[state.pathIndex];
                    glm::vec3 zombiePos = entity->localTransform.position;
                    glm::vec3 diff = target - zombiePos;
                    
                    // For horizontal distance check (ignore Y for "reached" check)
                    glm::vec3 flatDiff = glm::vec3(diff.x, 0, diff.z);
                    float flatDist = glm::length(flatDiff);
                    
                    // Show current waypoint progress
                    float zombieVisualY = zombiePos.y + 0.6f;  // Account for model offset
                    float yDiff = std::abs(target.y - zombieVisualY);
                    
                    // For stair waypoints, check BOTH horizontal and vertical distance
                    // NOTE: Zombie models have ~0.6m offset (skeleton root at feet, visual mesh above)
                    // Since zombies rely on ramp geometry for height, we use a relaxed vertical threshold
                    bool reachedHorizontally = (flatDist < 1.5f);  // Relaxed from 1.0m to help progression
                    bool reachedVertically = (yDiff < 0.5f);  // Relaxed threshold - ramp geometry handles Y naturally
                    
                    if (reachedHorizontally && reachedVertically) {
                        // Reached waypoint in 3D space, move to next
                        state.pathIndex++;
                    } else {
                        // Move toward waypoint in 3D
                        // Safety check: prevent NaN from normalizing zero vectors
                        if (glm::length(diff) < 0.05f || glm::length(flatDiff) < 0.05f) {
                            // Very close to waypoint, mark as reached to avoid getting stuck
                            state.pathIndex++;
                            continue;
                        }
                        
                        glm::vec3 dir = glm::normalize(diff);
                        
                        // Rotation based on horizontal direction only
                        glm::vec3 flatDir = glm::normalize(flatDiff);
                        float yaw = glm::atan(flatDir.x, flatDir.z);
                        entity->localTransform.rotation.y = yaw;
                        
                        float speed = 1.0f;
                        
                        // Stuck detection: check if zombie has moved
                        float distMoved = glm::distance(zombiePos, state.lastPosition);
                        if (distMoved < 0.1f) {
                            state.stuckTimer += deltaTime;
                        } else {
                            state.stuckTimer = 0.0f;
                            state.lastPosition = zombiePos;
                        }
                        
                        // Apply physics velocity (horizontal movement + stuck assist)
                        if(auto* collider = entity->getComponent<BulletColliderComponent>()) {
                            if(collider->rigidBody) {
                                collider->rigidBody->activate(true);
                                
                                float currentY = collider->rigidBody->getLinearVelocity().getY();
                                float finalYVelocity = currentY;
                                
                                // If stuck for >1 second, apply upward thrust
                                if (state.stuckTimer > 1.0f) {
                                    finalYVelocity = 0.5f;  // Upward thrust to unstuck
                                }
                                
                                collider->rigidBody->setLinearVelocity(btVector3(
                                    dir.x * speed, 
                                    finalYVelocity,
                                    dir.z * speed
                                ));
                                
                                // Sync rotation
                                btTransform trans = collider->rigidBody->getWorldTransform();
                                glm::quat yawQuat = glm::quat(glm::vec3(0, yaw, 0));
                                trans.setRotation(btQuaternion(yawQuat.x, yawQuat.y, yawQuat.z, yawQuat.w));
                                collider->rigidBody->setWorldTransform(trans);
                            }
                        }
                    }
                }
                
                // Animation
                if(auto* animator = entity->getComponent<AnimatorComponent>()) {
                    // If a non-looping animation is currently playing (e.g., hit), don't overwrite it.
                    if(animator->isPlaying && !animator->loop) {
                        continue;
                    }
                    // Otherwise, keep the zombie in the walk loop.
                    if(animator->currentAnimation != "walk" || !animator->isPlaying || !animator->loop) {
                        animator->playAnimation("walk", true);
                    }
                }
            }
        }
        
    private:
        StairWaypoint* findNearestStair(glm::vec3 pos, int targetFloor) {
            StairWaypoint* nearest = nullptr;
            float minDist = FLT_MAX;
            
            int currentFloor = getFloor(pos.y);
            
            for (auto& stair : stairWaypoints) {
                // Only consider waypoints that:
                // 1. Lead to the target floor
                // 2. Are on the same floor as the zombie (reachable)
                int waypointFloor = getFloor(stair.position.y);
                
                if (stair.targetFloor == targetFloor && waypointFloor == currentFloor) {
                    float dist = glm::distance(pos, stair.position);
                    if (dist < minDist) {
                        minDist = dist;
                        nearest = &stair;
                    }
                }
            }
            
            return nearest;
        }
    };

}
