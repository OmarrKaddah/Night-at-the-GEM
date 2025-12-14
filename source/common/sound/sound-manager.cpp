#define MINIAUDIO_IMPLEMENTATION
#include "../../../vendor/miniaudio.h"
#include "sound-manager.hpp"

namespace our {

SoundManager* SoundManager::getInstance() {
    static SoundManager instance;
    return &instance;
}

bool SoundManager::initialize() {
    if (initialized) return true;

    if (ma_engine_init(nullptr, &engine) != MA_SUCCESS) {
        std::cerr << "Miniaudio init failed\n";
        return false;
    }

    initialized = true;
    std::cout << "[Audio] Initialized\n";
    return true;
}

void SoundManager::shutdown() {
    if (!initialized) return;

    stopMusic();
    ma_engine_uninit(&engine);
    initialized = false;

    std::cout << "[Audio] Shutdown\n";
}

void SoundManager::loadSound(const std::string& name, const std::string& path) {
    if (!initialized) return;
    soundPaths[name] = path;
}

void SoundManager::playSound(const std::string& name) {
    if (!initialized) return;

    auto it = soundPaths.find(name);
    if (it != soundPaths.end()) {
        ma_engine_play_sound(&engine, it->second.c_str(), nullptr);
    }
}

void SoundManager::playSoundFromPath(const std::string& path) {
    if (!initialized) return;
    ma_engine_play_sound(&engine, path.c_str(), nullptr);
}

void SoundManager::playMusic(const std::string& path, bool loop) {
    if (!initialized) return;

    stopMusic();

    if (ma_sound_init_from_file(&engine, path.c_str(),
        MA_SOUND_FLAG_STREAM, nullptr, nullptr, &musicSound) != MA_SUCCESS) {
        std::cerr << "Failed to load music: " << path << "\n";
        return;
    }

    ma_sound_set_looping(&musicSound, loop);
    ma_sound_set_volume(&musicSound, musicVolume);
    ma_sound_start(&musicSound);

    musicPlaying = true;
}

void SoundManager::stopMusic() {
    if (!musicPlaying) return;

    ma_sound_stop(&musicSound);
    ma_sound_uninit(&musicSound);
    musicPlaying = false;
}

void SoundManager::setMusicVolume(float volume) {
    musicVolume = volume;
    if (musicPlaying) {
        ma_sound_set_volume(&musicSound, musicVolume);
    }
}

void SoundManager::setSFXVolume(float volume) {
    sfxVolume = volume;
    ma_engine_set_volume(&engine, sfxVolume);
}

} // namespace our
