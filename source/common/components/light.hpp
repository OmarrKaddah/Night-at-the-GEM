#pragma once

#include "../ecs/component.hpp"
#include <glm/glm.hpp>

namespace our {

    // Light types supported by the engine
    enum class LightType {
        DIRECTIONAL = 0,  // Sun/moon - parallel rays, no position
        POINT = 1,        // Bulb - radiates in all directions from a point
        SPOT = 2          // Flashlight - cone of light from a point
    };

    // Light component - attach to an entity to make it a light source
    // Position and direction come from the entity's transform
    class LightComponent : public Component {
    public:
        LightType lightType = LightType::DIRECTIONAL;
        
        // Light color and intensity (can go above 1 for bright lights)
        glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
        
        // Attenuation for point/spot lights: 1/(constant + linear*d + quadratic*d²)
        float attenuation_constant = 1.0f;
        float attenuation_linear = 0.0f;
        float attenuation_quadratic = 0.0f;
        
        // Spot light cone angles (in radians internally, degrees in JSON)
        float inner_angle = glm::radians(15.0f);  // Full intensity cone
        float outer_angle = glm::radians(25.0f);  // Falloff to zero

        static std::string getID() { return "Light"; }

        void deserialize(const nlohmann::json& data) override;
        
        // Get world-space position (for point/spot lights)
        glm::vec3 getPosition() const;
        
        // Get world-space direction (light shines along -Z of entity)
        glm::vec3 getDirection() const;
    };

}

