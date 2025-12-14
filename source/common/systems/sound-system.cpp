#include "sound-system.hpp"
#include "../sound/sound-manager.hpp"
#include <iostream>

namespace our {

SoundSystem::SoundSystem(World* world) : world(world) {}
SoundSystem::~SoundSystem() { shutdown(); }

void SoundSystem::initialize() {
    if (initialized) return;

    if (!SOUND_MANAGER->initialize()) {
        std::cerr << "SoundSystem failed to init SoundManager\n";
        return;
    }

    initialized = true;
}

void SoundSystem::shutdown() {
    if (!initialized) return;

    SOUND_MANAGER->shutdown();
    initialized = false;
}

void SoundSystem::update(float) {
    // nothing needed for miniaudio
}

void SoundSystem::playSound(const std::string& name, const std::string& path) {
    SOUND_MANAGER->loadSound(name, path);
    SOUND_MANAGER->playSound(name);
}

void SoundSystem::playMusic(const std::string& path, bool loop, float volume) {
    SOUND_MANAGER->setMusicVolume(volume);
    SOUND_MANAGER->playMusic(path, loop);
}

}