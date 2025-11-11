#include "world.hpp"

namespace our {

    // This will deserialize a json array of entities and add the new entities to the current world
    // If parent pointer is not null, the new entities will be have their parent set to that given pointer
    // If any of the entities has children, this function will be called recursively for these children
    void World::deserialize(const nlohmann::json& data, Entity* parent) {
        if (!data.is_array()) return;
        for (const auto& entityData : data) {
            Entity* e = add();                 // create and insert into this world
            e->parent = parent;                // set parent (may be nullptr)
            e->deserialize(entityData);        // fill entity data & components

            if (entityData.contains("children")) {
                if (const auto& children = entityData["children"]; children.is_array()) {
                    deserialize(children, e); // recursively deserialize children with current entity as parent
                }
            }
        }
    }

}