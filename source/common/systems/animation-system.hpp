#ifndef ANIMATION_SYSTEM_HPP
#define ANIMATION_SYSTEM_HPP

#include "../ecs/world.hpp"

namespace our {

    class AnimationSystem {
    public:
        // Update all animators in the world
        void update(World* world, float deltaTime);
    };

} // namespace our

#endif // ANIMATION_SYSTEM_HPP
