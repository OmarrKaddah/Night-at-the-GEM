#include "material.hpp"

#include "../asset-loader.hpp"
#include "deserialize-utils.hpp"

namespace our {

    // This function should setup the pipeline state and set the shader to be used
    void Material::setup() const {
        //TODO: (Req 7) Write this function
        pipelineState.setup();
        shader->use();

    }

    // This function read the material data from a json object
    void Material::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;

        if (data.contains("pipelineState")) {
            pipelineState.deserialize(data["pipelineState"]);
        }
        shader = AssetLoader<ShaderProgram>::get(data["shader"].get<std::string>());
        transparent = data.value("transparent", false);
    }

    // This function should call the setup of its parent and
    // set the "tint" uniform to the value in the member variable tint 
    void TintedMaterial::setup() const {
        //TODO: (Req 7) Write this function
        Material::setup();
        shader->set("tint", tint);
    }

    // This function read the material data from a json object
    void TintedMaterial::deserialize(const nlohmann::json& data) {
        Material::deserialize(data);
        if (!data.is_object()) return;
        tint = data.value("tint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // This function should call the setup of its parent and
    // set the "alphaThreshold" uniform to the value in the member variable alphaThreshold
    // Then it should bind the texture and sampler to a texture unit and send the unit number to the uniform variable "tex" 
    void TexturedMaterial::setup() const {
        //TODO: (Req 7) Write this function
        // Setup the parent and the alpha threshold uniform
        TintedMaterial::setup();
        shader->set("alphaThreshold", alphaThreshold);
        // Bind the texture to unit 0 if present. Do not require a sampler to be set
        // (some code paths create a texture directly without assigning a sampler).
        if (texture) {
            glActiveTexture(GL_TEXTURE0);
            texture->bind();
            if (sampler) sampler->bind(0);
            shader->set("tex", 0);
        }
    }

    // This function read the material data from a json object
    void TexturedMaterial::deserialize(const nlohmann::json& data) {
        TintedMaterial::deserialize(data);
        if (!data.is_object()) return;
        alphaThreshold = data.value("alphaThreshold", 0.0f);
        texture = AssetLoader<Texture2D>::get(data.value("texture", ""));
        sampler = AssetLoader<Sampler>::get(data.value("sampler", ""));
    }

    // LitMaterial setup - binds all texture maps to their units
    void LitMaterial::setup() const {
        Material::setup();
        
        // Set tint/fallback uniforms
        shader->set("material.albedo", albedo_tint);
        shader->set("material.specular", specular_tint);
        shader->set("material.emissive", emissive_tint);
        shader->set("material.roughness", roughness);
        shader->set("material.ao", ao);
        
        // Bind textures to units 0-4
        // Unit 0: Albedo
        glActiveTexture(GL_TEXTURE0);
        if (albedo_map) {
            albedo_map->bind();
            shader->set("material.use_albedo_map", 1);
        } else {
            shader->set("material.use_albedo_map", 0);
        }
        if (sampler) sampler->bind(0);
        shader->set("material.albedo_map", 0);
        
        // Unit 1: Specular
        glActiveTexture(GL_TEXTURE1);
        if (specular_map) {
            specular_map->bind();
            shader->set("material.use_specular_map", 1);
        } else {
            shader->set("material.use_specular_map", 0);
        }
        if (sampler) sampler->bind(1);
        shader->set("material.specular_map", 1);
        
        // Unit 2: Roughness
        glActiveTexture(GL_TEXTURE2);
        if (roughness_map) {
            roughness_map->bind();
            shader->set("material.use_roughness_map", 1);
        } else {
            shader->set("material.use_roughness_map", 0);
        }
        if (sampler) sampler->bind(2);
        shader->set("material.roughness_map", 2);
        
        // Unit 3: Ambient Occlusion
        glActiveTexture(GL_TEXTURE3);
        if (ao_map) {
            ao_map->bind();
            shader->set("material.use_ao_map", 1);
        } else {
            shader->set("material.use_ao_map", 0);
        }
        if (sampler) sampler->bind(3);
        shader->set("material.ao_map", 3);
        
        // Unit 4: Emissive
        glActiveTexture(GL_TEXTURE4);
        if (emissive_map) {
            emissive_map->bind();
            shader->set("material.use_emissive_map", 1);
        } else {
            shader->set("material.use_emissive_map", 0);
        }
        if (sampler) sampler->bind(4);
        shader->set("material.emissive_map", 4);
        
        glActiveTexture(GL_TEXTURE0);
    }

    void LitMaterial::deserialize(const nlohmann::json& data) {
        Material::deserialize(data);
        if (!data.is_object()) return;
        
        // Load texture maps
        albedo_map = AssetLoader<Texture2D>::get(data.value("albedo_map", ""));
        specular_map = AssetLoader<Texture2D>::get(data.value("specular_map", ""));
        roughness_map = AssetLoader<Texture2D>::get(data.value("roughness_map", ""));
        ao_map = AssetLoader<Texture2D>::get(data.value("ao_map", ""));
        emissive_map = AssetLoader<Texture2D>::get(data.value("emissive_map", ""));
        sampler = AssetLoader<Sampler>::get(data.value("sampler", ""));
        
        // Load tints/fallbacks
        albedo_tint = data.value("albedo", glm::vec3(1.0f));
        specular_tint = data.value("specular", glm::vec3(0.5f));
        emissive_tint = data.value("emissive", glm::vec3(0.0f));
        roughness = data.value("roughness", 0.5f);
        ao = data.value("ao", 1.0f);
    }
}