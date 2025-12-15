#pragma once

#include <ecs/world.hpp>
#include <string>

namespace our {

class SoundSystem {
private:
    World* world = nullptr;
    bool initialized = false;

public:
    SoundSystem(World* world);
    ~SoundSystem();

    void initialize();
    void shutdown();
    void update(float dt);

    void playSound(const std::string& name, const std::string& path);
    void playMusic(const std::string& path, bool loop = true, float volume = 0.7f);
};

}
