#include "health.hpp"

#include <algorithm>

namespace our {

    void HealthComponent::deserialize(const nlohmann::json& data) {
        if(!data.is_object()) return;

        maxHealth = data.value("maxHealth", maxHealth);
        currentHealth = data.value("currentHealth", currentHealth);

        if(maxHealth < 0.0f) maxHealth = 0.0f;
        currentHealth = std::clamp(currentHealth, 0.0f, maxHealth);
    }

    void HealthComponent::applyDamage(float amount) {
        if(amount <= 0.0f) return;
        if(currentHealth <= 0.0f) return;
        currentHealth = std::max(0.0f, currentHealth - amount);
    }

    void HealthComponent::heal(float amount) {
        if(amount <= 0.0f) return;
        currentHealth = std::min(maxHealth, currentHealth + amount);
    }

}

