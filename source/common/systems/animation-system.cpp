#include "animation-system.hpp"
#include "../components/animator.hpp"

namespace our {

    void AnimationSystem::update(World* world, float deltaTime) {
        // Iterate through all entities and update their animators
        for (auto entity : world->getEntities()) {
            AnimatorComponent* animator = entity->getComponent<AnimatorComponent>();
            if (animator) {
                animator->update(deltaTime);
            }
        }
    }

} // namespace our
