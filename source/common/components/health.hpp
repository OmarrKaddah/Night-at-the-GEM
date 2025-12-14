#pragma once

#include "../ecs/component.hpp"

namespace our {

    class HealthComponent : public Component {
    public:
        float maxHealth = 100.0f;
        float currentHealth = 100.0f;

        static std::string getID() { return "Health"; }

        void deserialize(const nlohmann::json& data) override;

        void applyDamage(float amount);
        void heal(float amount);
        [[nodiscard]] bool isDead() const { return currentHealth <= 0.0f; }
    };

}

