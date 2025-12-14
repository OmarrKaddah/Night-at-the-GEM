# Sound System Integration Guide

This document explains how to use the sound system integrated into the project.

## Setup Requirements

### OpenAL Installation (Windows)

1. **Download OpenAL Soft**:
   - Download from: https://openal-soft.org/openal-binaries/
   - Get the latest release (e.g., `openal-soft-1.xx.x-bin.zip`)

2. **Install the library**:
   - Copy `OpenAL32.lib` to `vendor/openal/lib/`
   - Copy `OpenAL32.dll` to `bin/` (where the executable is)
   - Headers are already provided in `vendor/openal/include/`

3. **Rebuild the project**:
   ```powershell
   cd build
   cmake ..
   cmake --build . --config Release
   ```

## Quick Start

### 1. Initialize Sound System

In your state's `onInitialize()`:

```cpp
#include <systems/sound-system.hpp>
#include <sound/sound-manager.hpp>

class YourState : public our::State {
    our::SoundSystem* soundSystem;
    
    void onInitialize() override {
        // Create and initialize sound system
        soundSystem = new our::SoundSystem(&world);
        soundSystem->initialize();
        
        // Set the listener (usually the camera)
        for (auto* entity : world.getEntities()) {
            if (entity->name == "PlayerCamera") {
                soundSystem->setListenerEntity(entity);
                break;
            }
        }
    }
};
```

### 2. Preload Sounds

Load sounds during loading screen:

```cpp
// Preload individual sounds
soundSystem->preloadSound("gunshot", "assets/sounds/weapons/gunshot.wav");
soundSystem->preloadSound("footstep", "assets/sounds/footsteps/default.wav");

// Or preload all sounds in a directory
soundSystem->preloadSoundsFromDirectory("assets/sounds/weapons", ".wav");
```

### 3. Play Sounds

#### Simple Sound Effects:
```cpp
// Play a non-positional sound
SOUND_MANAGER->playSound("gunshot", 1.0f, 1.0f, false);
// Arguments: name, volume, pitch, loop

// Play a 3D positional sound
glm::vec3 position(10.0f, 0.0f, 5.0f);
SOUND_MANAGER->playSound3D("explosion", position, 0.8f, 1.0f, false);
```

#### Background Music:
```cpp
// Play background music (loops by default)
soundSystem->playBackgroundMusic("assets/sounds/music/suspense.wav", true, 0.5f);

// Stop music with optional fade
soundSystem->stopBackgroundMusic(2.0f);  // 2 second fade out
```

#### Entity-Based Sounds:
```cpp
// Add SoundSource component to entity
auto* soundSource = entity->addComponent<our::SoundSource>();

// Register audio clips
soundSource->addClip("walk", "assets/sounds/footsteps/default.wav", 0.7f, 1.0f, false, true);
soundSource->addClip("run", "assets/sounds/footsteps/run.wav", 0.9f, 1.2f, false, true);

// Play sounds
soundSource->play("walk");
soundSource->playOneShot("run");
```

### 4. Update Every Frame

```cpp
void onDraw(double deltaTime) override {
    // ... other updates ...
    
    // Update sound system (handles 3D audio positioning)
    soundSystem->update((float)deltaTime);
}
```

### 5. Cleanup

```cpp
void onDestroy() override {
    if (soundSystem) {
        soundSystem->shutdown();
        delete soundSystem;
    }
}
```

## Integration Examples

### Footstep Sounds (in movement/walking code):

```cpp
// In your movement system or player controller
void updateMovement(Entity* player, float deltaTime) {
    static float stepTimer = 0.0f;
    const float stepInterval = 0.5f;  // Time between steps
    
    if (isMoving && isOnGround) {
        stepTimer += deltaTime;
        if (stepTimer >= stepInterval) {
            soundSystem->playFootstepSound(player, "concrete");
            stepTimer = 0.0f;
        }
    }
}
```

### Gun Shooting Sound (in weapon system):

```cpp
// In WeaponSystem::update()
void onShoot(Entity* player) {
    if (auto* soundSource = player->getComponent<SoundSource>()) {
        soundSource->playOneShot("gunshot", 1.0f);
    }
    // Or use global sound:
    // SOUND_MANAGER->playSound("gunshot");
}
```

### Suspense Music During Loading:

```cpp
// In LoadingState::onInitialize()
void onInitialize() override {
    // Start suspense music
    SOUND_MANAGER->initialize();
    SOUND_MANAGER->loadSound("suspense", "assets/sounds/music/suspense.wav");
    SOUND_MANAGER->playMusic("assets/sounds/music/suspense.wav", true, 0.0f);
    SOUND_MANAGER->setMusicVolume(0.6f);
}
```

## Volume Control

```cpp
// Master volume (affects everything)
SOUND_MANAGER->setMasterVolume(0.8f);  // 0.0 to 1.0

// Sound effects volume
SOUND_MANAGER->setSFXVolume(0.9f);

// Music volume
SOUND_MANAGER->setMusicVolume(0.5f);
```

## Audio File Format

- **Recommended Format**: WAV (16-bit PCM)
- **Sample Rates**: 22050 Hz or 44100 Hz
- **Channels**: Mono for 3D sounds, Stereo for music/UI
- **Bit Depth**: 8-bit or 16-bit

Use free tools like Audacity to convert audio files to WAV format.

## Directory Structure

```
assets/sounds/
├── ambient/
│   └── wind.wav
├── environment/
│   ├── door_open.wav
│   └── door_close.wav
├── footsteps/
│   ├── concrete.wav
│   ├── grass.wav
│   └── default.wav
├── music/
│   └── suspense.wav
├── ui/
│   ├── click.wav
│   └── hover.wav
└── weapons/
    ├── gunshot.wav
    └── reload.wav
```

## Troubleshooting

1. **No sound output**: Ensure OpenAL32.dll is in the bin folder
2. **Failed to load WAV**: Check file format (must be standard WAV)
3. **3D sounds not working**: Ensure listener is set and updated each frame
4. **Sounds cut off**: Increase max sources in OpenAL configuration
