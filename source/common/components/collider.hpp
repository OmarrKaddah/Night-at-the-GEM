#pragma once

#include "../ecs/component.hpp"
#include <glm/glm.hpp>

namespace our {

    class ColliderComponent : public Component {
    public:
        glm::vec3 halfSize = glm::vec3(0.5f); // half extents of the AABB box

        static std::string getID() { return "Collider"; }

        void deserialize(const nlohmann::json& data) override {
            if(data.contains("halfSize"))
                halfSize = glm::vec3(data["halfSize"][0], data["halfSize"][1], data["halfSize"][2]);
        }
    };

}
