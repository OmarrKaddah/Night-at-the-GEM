#ifndef ANIMATION_LOADER_HPP
#define ANIMATION_LOADER_HPP

#include "animation.hpp"
#include <memory>
#include <string>

namespace our {

    class AnimationLoader {
    public:
        // Load skeleton and all animations from an FBX file
        // Returns a skeleton with animations attached
        static std::shared_ptr<Skeleton> loadSkeleton(const std::string& filename);
        
        // Load a specific animation from an FBX file into an existing skeleton
        static std::shared_ptr<AnimationClip> loadAnimation(
            const std::string& filename, 
            const std::shared_ptr<Skeleton>& skeleton
        );
    };

} // namespace our

#endif // ANIMATION_LOADER_HPP
