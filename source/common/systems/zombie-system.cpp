#include "zombie-system.hpp"
#include "../sound/sound-manager.hpp"
#include <random>

namespace our {

void ZombieSystem::initialize(NavGrid2D* floor0Grid, NavGrid2D* floor1Grid, NavGrid2D* floor2Grid, const std::vector<StairWaypoint>& stairWaypoints) {
    this->floor0Grid = floor0Grid;
    this->floor1Grid = floor1Grid;
    this->floor2Grid = floor2Grid;
    this->stairWaypoints = stairWaypoints;
}

void ZombieSystem::update(World* world, float deltaTime) {
    for (auto* entity : world->getEntities()) {
        if (entity->name.find("Zombie") == std::string::npos) continue;

        auto* health = entity->getComponent<HealthComponent>();
        if (health && health->isDead()) continue;

        // Get or create zombie state
        auto& state = states[entity];

        // Random ambient growl sounds
        state.soundTimer -= deltaTime;
        if (state.soundTimer <= 0.0f) {
            // Random chance to growl
            static thread_local std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) < 0.3f) { // 30% chance when timer expires
                SOUND_MANAGER->playSound("zombie_growl");
            }
            // Reset timer (5-10 seconds)
            std::uniform_real_distribution<float> timerDist(5.0f, 10.0f);
            state.soundTimer = timerDist(rng);
        }

        // Attack sound cooldown
        if (state.attackSoundCooldown > 0.0f) {
            state.attackSoundCooldown -= deltaTime;
        }

        // ...existing zombie movement/AI code...
    }

    // ...existing code...
}

// When zombie attacks player (if you have attack logic):
// if (attacking && state.attackSoundCooldown <= 0.0f) {
//     SOUND_MANAGER->playSound("zombie_attack");
//     state.attackSoundCooldown = 1.5f;
// }

// When zombie dies, add in the death handling code:
// SOUND_MANAGER->playSound("zombie_death");

} // namespace our