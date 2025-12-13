#include "zombie-movement.hpp"
#include "../deserialize-utils.hpp"

namespace our {

    void ZombieMovementComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;

        if(data.contains("forwardAxis")) forwardAxis = data["forwardAxis"].get<glm::vec3>();
        if(data.contains("swayAxis")) swayAxis = data["swayAxis"].get<glm::vec3>();
        walkAmplitude = data.value("walkAmplitude", walkAmplitude);
        walkSpeed = data.value("walkSpeed", walkSpeed);
        swayAmplitude = data.value("swayAmplitude", swayAmplitude);
        swaySpeed = data.value("swaySpeed", swaySpeed);
        phase = data.value("phase", phase);
        faceMovement = data.value("faceMovement", faceMovement);
    }

}
