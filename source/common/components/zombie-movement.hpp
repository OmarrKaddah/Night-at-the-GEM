#pragma once

#include "../ecs/component.hpp"

#include <glm/glm.hpp>

namespace our {

    // Marks an entity as a zombie that should perform a simple ambient walk animation.
    // Motion is purely kinematic (we directly update the transform each frame), so it
    // stays deterministic and cheap.
    class ZombieMovementComponent : public Component {
    public:
        // Direction to travel forward/backward around the spawn point.
        glm::vec3 forwardAxis = {0.0f, 0.0f, 1.0f};
        // Axis for a small sideways sway to keep motion from looking stiff.
        glm::vec3 swayAxis = {1.0f, 0.0f, 0.0f};
        // How far to walk away from the spawn point along the forward axis (meters).
        float walkAmplitude = 1.5f;
        // How fast to complete a forward/back cycle (radians per second).
        float walkSpeed = 0.6f;
        // Side sway amplitude (meters).
        float swayAmplitude = 0.5f;
        // Side sway angular speed (radians per second).
        float swaySpeed = 1.2f;
        // Phase offset so multiple zombies are not synchronized.
        float phase = 0.0f;
        // Rotate the model to face the instantaneous movement direction on the XZ plane.
        bool faceMovement = true;

        // Cached spawn position and init flag so we keep motion relative to the starting spot.
        glm::vec3 startPosition = glm::vec3(0.0f);
        bool initialized = false;

        // The ID of this component type is "Zombie Movement"
        static std::string getID() { return "Zombie Movement"; }

        // Reads settings from JSON.
        void deserialize(const nlohmann::json& data) override;
    };

}
