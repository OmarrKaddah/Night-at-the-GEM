# Sound System Integration Examples

This file shows practical examples of how to integrate the sound system into your game.

## 1. One-Time Setup in main.cpp

Add audio initialization at application startup:

```cpp
#include "sound/audio-helper.hpp"

int main(int argc, char** argv) {
    // ... existing setup code ...
    
    // Initialize audio system
    our::AudioHelper::init();
    
    // ... rest of main ...
    
    // Before exit:
    our::AudioHelper::shutdown();
    return 0;
}
```

## 2. Loading State - Play Suspense Music

In `loading-state.hpp`, add:

```cpp
#include "sound/audio-helper.hpp"

class LoadingState : public our::State {
    void onInitialize() override {
        // ... existing initialization ...
        
        // Start suspense music while loading
        our::AudioHelper::playMusic("assets/sounds/music/suspense.wav", true, 0.6f);
    }
    
    void onDestroy() override {
        // Stop or fade out music when loading completes
        our::AudioHelper::stopMusic();
        
        // ... existing cleanup ...
    }
};
```

## 3. Play State - Full Integration

In `play-state.hpp`, add the SoundSystem:

```cpp
#include <systems/sound-system.hpp>
#include <sound/audio-helper.hpp>

class Playstate : public our::State {
    our::SoundSystem* soundSystem = nullptr;
    
    // Footstep timing
    float footstepTimer = 0.0f;
    const float footstepInterval = 0.4f;  // Time between footsteps
    bool wasMoving = false;
    
    void onInitialize() override {
        // ... existing initialization ...
        
        // Create and initialize sound system
        soundSystem = new our::SoundSystem(&world);
        soundSystem->initialize();
        
        // Preload common sounds
        soundSystem->preloadSound("gunshot", "assets/sounds/weapons/gunshot.wav");
        soundSystem->preloadSound("footstep", "assets/sounds/footsteps/default.wav");
        soundSystem->preloadSound("zombie_death", "assets/sounds/enemies/death.wav");
        
        // Set the camera as audio listener
        for (auto* entity : world.getEntities()) {
            if (entity->name == "PlayerCamera") {
                soundSystem->setListenerEntity(entity);
                break;
            }
        }
        
        // Start background ambiance
        soundSystem->playBackgroundMusic("assets/sounds/music/game_ambiance.wav", true, 0.3f);
    }
    
    void onDraw(double deltaTime) override {
        float dt = (float)deltaTime;
        
        // ... existing game logic ...
        
        // Update sound system (3D audio positioning)
        soundSystem->update(dt);
        
        // Handle footstep sounds based on player movement
        updateFootsteps(dt);
    }
    
    void updateFootsteps(float dt) {
        // Check if player is moving
        auto& kb = getApp()->getKeyboard();
        bool isMoving = kb.isPressed(GLFW_KEY_W) || kb.isPressed(GLFW_KEY_S) ||
                        kb.isPressed(GLFW_KEY_A) || kb.isPressed(GLFW_KEY_D);
        
        if (isMoving) {
            footstepTimer += dt;
            if (footstepTimer >= footstepInterval) {
                our::AudioHelper::playByName("footstep", 0.7f);
                footstepTimer = 0.0f;
            }
        } else {
            footstepTimer = 0.0f;
        }
        
        wasMoving = isMoving;
    }
    
    void onDestroy() override {
        if (soundSystem) {
            soundSystem->stopBackgroundMusic();
            soundSystem->shutdown();
            delete soundSystem;
            soundSystem = nullptr;
        }
        
        // ... existing cleanup ...
    }
};
```

## 4. Weapon System - Gun Shooting Sound

In `weapon-system.cpp`, add sound when firing:

```cpp
#include "sound/audio-helper.hpp"

void WeaponSystem::update(World* world, float deltaTime) {
    // ... existing code until fire check ...
    
    // Single shot per click
    if (!mouse.justPressed(GLFW_MOUSE_BUTTON_LEFT)) return;

    // Play gunshot sound
    our::AudioHelper::playSFX("assets/sounds/weapons/gunshot.wav", 1.0f);
    
    std::cout << "[Weapon] Fire" << std::endl;
    
    // ... rest of firing logic ...
    
    // When hitting an enemy:
    if (health->isDead()) {
        // Play death sound at enemy position
        glm::vec3 enemyPos = hitEntity->localTransform.position;
        our::AudioHelper::playSFX3D("assets/sounds/enemies/death.wav", enemyPos, 1.0f);
    }
}
```

## 5. Menu State - UI Sounds

In `menu-state.hpp`, add button click sounds:

```cpp
#include "sound/audio-helper.hpp"

class Menustate : public our::State {
    void onInitialize() override {
        // ... existing code ...
        
        // Initialize audio if not already
        our::AudioHelper::init();
        
        // Play menu background music
        our::AudioHelper::playMusic("assets/sounds/music/menu_theme.wav", true, 0.5f);
    }
    
    void onMouseButtonEvent(int button, int action, int mods) override {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            glm::vec2 mousePos = /* get mouse position */;
            
            for (auto& btn : buttons) {
                if (btn.isInside(mousePos)) {
                    // Play click sound before action
                    our::AudioHelper::playSFX("assets/sounds/ui/click.wav", 0.6f);
                    btn.action();
                    break;
                }
            }
        }
    }
    
    void onDraw(double deltaTime) override {
        // ... existing drawing code ...
        
        // Check for hover sounds
        glm::vec2 mousePos = /* get mouse position */;
        for (int i = 0; i < buttons.size(); i++) {
            if (buttons[i].isInside(mousePos)) {
                if (!buttonHovered[i]) {
                    // Just started hovering
                    our::AudioHelper::playSFX("assets/sounds/ui/hover.wav", 0.3f);
                    buttonHovered[i] = true;
                }
            } else {
                buttonHovered[i] = false;
            }
        }
    }
    
    void onDestroy() override {
        our::AudioHelper::stopMusic();
        // ... existing cleanup ...
    }
    
private:
    bool buttonHovered[3] = {false, false, false};
};
```

## 6. Zombie System - Enemy Sounds

In your zombie AI, add sounds for growls and attacks:

```cpp
#include "sound/audio-helper.hpp"

void ZombieSystem::update(World* world, float deltaTime) {
    for (auto* entity : world->getEntities()) {
        auto* zombie = entity->getComponent<ZombieComponent>();
        if (!zombie) continue;
        
        glm::vec3 zombiePos = entity->localTransform.position;
        
        // Random growl sounds
        zombie->growlTimer -= deltaTime;
        if (zombie->growlTimer <= 0.0f) {
            our::AudioHelper::playSFX3D("assets/sounds/enemies/growl.wav", zombiePos, 0.8f);
            zombie->growlTimer = 5.0f + (rand() % 10);  // Random 5-15 seconds
        }
        
        // Attack sound when dealing damage
        if (zombie->isAttacking && zombie->justHitPlayer) {
            our::AudioHelper::playSFX3D("assets/sounds/enemies/attack.wav", zombiePos, 1.0f);
        }
    }
}
```

## 7. Entity JSON Configuration

Add sound components to entities in your scene config:

```jsonc
{
    "name": "PlayerCamera",
    "components": [
        { "type": "Camera", ... },
        { "type": "Free Camera Controller", ... },
        {
            "type": "Sound Source",
            "volumeMultiplier": 1.0,
            "clips": [
                {
                    "name": "footstep",
                    "file": "assets/sounds/footsteps/default.wav",
                    "volume": 0.7
                },
                {
                    "name": "jump",
                    "file": "assets/sounds/player/jump.wav",
                    "volume": 0.8
                }
            ]
        }
    ]
}
```

## Quick Reference - AudioHelper Functions

```cpp
// Initialization
our::AudioHelper::init();
our::AudioHelper::shutdown();

// Sound Effects
our::AudioHelper::playSFX("path/to/sound.wav", 1.0f);
our::AudioHelper::playSFX3D("path/to/sound.wav", position, 1.0f);
our::AudioHelper::playByName("preloaded_sound_name", 1.0f);

// Music
our::AudioHelper::playMusic("path/to/music.wav", true, 0.7f);
our::AudioHelper::stopMusic();
our::AudioHelper::pauseMusic();
our::AudioHelper::resumeMusic();

// Volume (0.0 to 1.0)
our::AudioHelper::setMasterVolume(0.8f);
our::AudioHelper::setSFXVolume(1.0f);
our::AudioHelper::setMusicVolume(0.5f);

// Preloading
our::AudioHelper::loadSound("name", "path/to/sound.wav");

// 3D Audio Listener
our::AudioHelper::updateListener(cameraPosition, forwardVector, upVector);

// Common game sounds (if files exist)
our::AudioHelper::playGunshot();
our::AudioHelper::playFootstep();
our::AudioHelper::playUIClick();
our::AudioHelper::playSuspenseMusic();
```
