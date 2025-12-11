#include "zombie-movement.hpp"

namespace our {

    void ZombieMovementComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;

        forwardAxis = data.value("forwardAxis", forwardAxis);
        swayAxis = data.value("swayAxis", swayAxis);
        walkAmplitude = data.value("walkAmplitude", walkAmplitude);
        walkSpeed = data.value("walkSpeed", walkSpeed);
        swayAmplitude = data.value("swayAmplitude", swayAmplitude);
        swaySpeed = data.value("swaySpeed", swaySpeed);
        phase = data.value("phase", phase);
        faceMovement = data.value("faceMovement", faceMovement);
    }

}
