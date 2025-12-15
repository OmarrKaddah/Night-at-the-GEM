#pragma once

#include "../../../vendor/miniaudio.h"
#include <string>
#include <unordered_map>
#include <iostream>

namespace our {

class SoundManager {
private:
    bool initialized = false;

    ma_engine engine{};
    ma_sound musicSound{};
    bool musicPlaying = false;

    float musicVolume = 1.0f;
    float sfxVolume   = 1.0f;
    
    std::unordered_map<std::string, std::string> soundPaths;

    SoundManager() = default;
    ~SoundManager() { shutdown(); }

    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

public:
    static SoundManager* getInstance();

    bool initialize();
    void shutdown();

    void loadSound(const std::string& name, const std::string& path);
    void playSound(const std::string& name);
    void playSoundFromPath(const std::string& path);

    void playMusic(const std::string& path, bool loop = true);
    void stopMusic();

    void setMusicVolume(float volume);
    void setSFXVolume(float volume);

    void update(float) {}
};

} // namespace our

#define SOUND_MANAGER (our::SoundManager::getInstance())
