#include "light.hpp"
#include "../ecs/entity.hpp"

namespace our {

    void LightComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;

        // Parse light type
        std::string typeStr = data.value("lightType", "directional");
        if (typeStr == "directional") {
            lightType = LightType::DIRECTIONAL;
        } else if (typeStr == "point") {
            lightType = LightType::POINT;
        } else if (typeStr == "spot") {
            lightType = LightType::SPOT;
        }

        // Parse color (default white)
        if (data.contains("color") && data["color"].is_array() && data["color"].size() >= 3) {
            color = glm::vec3(
                data["color"][0].get<float>(),
                data["color"][1].get<float>(),
                data["color"][2].get<float>()
            );
        } else {
            color = glm::vec3(1.0f);
        }

        // Parse attenuation
        if (data.contains("attenuation")) {
            auto& att = data["attenuation"];
            attenuation_constant = att.value("constant", 1.0f);
            attenuation_linear = att.value("linear", 0.0f);
            attenuation_quadratic = att.value("quadratic", 0.0f);
        }

        // Parse cone angles (JSON in degrees, store in radians)
        inner_angle = glm::radians(data.value("inner_angle", 15.0f));
        outer_angle = glm::radians(data.value("outer_angle", 25.0f));
    }

    glm::vec3 LightComponent::getPosition() const {
        if (!getOwner()) return glm::vec3(0.0f);
        glm::mat4 M = getOwner()->getLocalToWorldMatrix();
        return glm::vec3(M * glm::vec4(0, 0, 0, 1));
    }

    glm::vec3 LightComponent::getDirection() const {
        if (!getOwner()) return glm::vec3(0, -1, 0);
        glm::mat4 M = getOwner()->getLocalToWorldMatrix();
        // Light points down -Z axis in local space
        return glm::normalize(glm::vec3(M * glm::vec4(0, 0, -1, 0)));
    }

}

