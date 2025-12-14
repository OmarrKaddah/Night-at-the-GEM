#pragma once

#include <application.hpp>
#include <shader/shader.hpp>
#include <texture/texture2d.hpp>
#include <texture/texture-utils.hpp>
#include <material/material.hpp>
#include <mesh/mesh.hpp>
#include <asset-loader.hpp>
#include <deque>
#include <functional>

class LoadingState: public our::State {

    our::TexturedMaterial* loadingMaterial;
    our::Mesh* rectangle;
    int framesRendered = 0;

    our::Mesh* bar;
    float time;
    
    std::deque<std::function<void()>> loadingTasks;
    int totalTasks = 0;
    int finishedTasks = 0;
    
    // Coordinates from user:
    // X: 420, Y: 608, W: 695, H: 63 (relative to 1536x1024)
    // Normalized:
    // X: 0.2734375, Y: 0.59375
    // W: 0.452473958, H: 0.0615234375

    void onInitialize() override {
        // Create a material for the loading screen
        loadingMaterial = new our::TexturedMaterial();
        loadingMaterial->shader = new our::ShaderProgram();
        loadingMaterial->shader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
        loadingMaterial->shader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
        loadingMaterial->shader->link();
        
        // Load the loading texture
        loadingMaterial->texture = our::texture_utils::loadImage("assets/textures/loading-screen.png");
        loadingMaterial->tint = glm::vec4(1.0f);

        // Create a full-screen rectangle
        rectangle = new our::Mesh({
            {{0.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 1.0f, 0.0f}, {255, 255, 255, 255}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{0.0f, 1.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        },{
            0, 1, 2, 2, 3, 0,
        });
        
        // Create the loading bar mesh (white rectangle)
        // We start with a 1x1 rectangle and scale it
        bar = new our::Mesh({
            {{0.0f, 0.0f, 0.0f}, {200, 200, 200, 255}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 0.0f, 0.0f}, {200, 200, 200, 255}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 1.0f, 0.0f}, {200, 200, 200, 255}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{0.0f, 1.0f, 0.0f}, {200, 200, 200, 255}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        },{
            0, 1, 2, 2, 3, 0,
        });

        framesRendered = 0;
        time = 0.0f;
        
        // Queue loading tasks
        loadingTasks.clear();
        auto& config = getApp()->getConfig()["scene"];
        if(config.contains("assets")){
            auto& assets = config["assets"];
            
            // Helper to queue assets
            auto queueAssets = [this](const nlohmann::json& dict, std::function<void(const nlohmann::json&)> loader){
                if(!dict.is_object()) return;
                for(auto& [name, desc] : dict.items()){
                    loadingTasks.push_back([=](){ // capture value necessary for json
                         nlohmann::json j; j[name] = desc;
                         loader(j);
                    });
                }
            };
            
            if(assets.contains("shaders"))
                queueAssets(assets["shaders"], [](const auto& j){ our::AssetLoader<our::ShaderProgram>::deserialize(j); });
                
            if(assets.contains("textures"))
                queueAssets(assets["textures"], [](const auto& j){ our::AssetLoader<our::Texture2D>::deserialize(j); });
                
            if(assets.contains("samplers"))
                queueAssets(assets["samplers"], [](const auto& j){ our::AssetLoader<our::Sampler>::deserialize(j); });
                
            if(assets.contains("meshes"))
                queueAssets(assets["meshes"], [](const auto& j){ our::AssetLoader<our::Mesh>::deserialize(j); });
                
            if(assets.contains("materials"))
                queueAssets(assets["materials"], [](const auto& j){ our::AssetLoader<our::Material>::deserialize(j); });
        }
        
        totalTasks = loadingTasks.size();
        finishedTasks = 0;
    }

    void onDraw(double deltaTime) override {
        time += (float)deltaTime;
        
        // Setup viewport and matrices
        glm::ivec2 size = getApp()->getFrameBufferSize();
        glViewport(0, 0, size.x, size.y);
        
        glm::mat4 VP = glm::ortho(0.0f, (float)size.x, (float)size.y, 0.0f, 1.0f, -1.0f);
        
        // 1. Draw Background
        glm::mat4 M = glm::scale(glm::mat4(1.0f), glm::vec3(size.x, size.y, 1.0f));
        
        // Fade in from black
        float fadeInDuration = 0.5f;
        float alpha = glm::clamp(time / fadeInDuration, 0.0f, 1.0f);
        loadingMaterial->tint = glm::vec4(alpha, alpha, alpha, 1.0f); // Fade rgb from 0 to 1
        
        loadingMaterial->setup();
        loadingMaterial->shader->set("transform", VP*M);
        rectangle->draw();
        
        // Process tasks
        // We process multiple tasks per frame to avoid too slow loading if we have tiny assets
        // But for smooth rendering, maybe 1 is enough? Let's try 1 for now.
        if(!loadingTasks.empty()){
             auto task = loadingTasks.front();
             loadingTasks.pop_front();
             task();
             finishedTasks++;
        }

        // 2. Draw Loading Bar
        // Logic: Fill up based on progress
        // float duration = 2.0f;
        // float progress = glm::clamp(time / duration, 0.0f, 1.0f);
        
        float progress = 0.0f;
        if(totalTasks > 0) {
             progress = (float)finishedTasks / (float)totalTasks;
        } else {
             // Just time based fall back or instant
             progress = 1.0f;
        }
        // Minimal progress for visuals
        if(progress < 0.05f) progress = 0.05f;
        
        // Normalized Coords from user (1536x1024)
        // New Rect: X=354, Y=517, W=(946-354)=592, H=(570-517)=53
        float bx = 425.0f / 1537.0f;
        float by = 618.0f / 1027.0f;
        float bw = 689.0f / 1537.0f;
        float bh = 50.0f / 1027.0f;
        
        // Scale to current screen size
        float x = bx * size.x;
        float y = by * size.y;
        float w = bw * size.x * progress; // Scale width by progress
        float h = bh * size.y;
        
        glm::mat4 barM = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f)) * 
                        glm::scale(glm::mat4(1.0f), glm::vec3(w, h, 1.0f));
                        
        // We can reuse the tinted material from MenuState if it was available, 
        // but since we don't have access to shared materials easily, let's use the textured shader but use a white pixel tint.
        // Or better, just use the same material but unbind the texture? 
        // Actually, our TexturedMaterial expects a texture. 
        // Let's create a solid color shader or just use the textured one with a white texture?
        // We can use the 'white' pixel texture from the texture utils if it exists, or just use the loading texture but tint it extremely brightly? No that looks bad.
        // Let's repurpose the textured shader to draw a solid color by unsetting the texture uniform? No, binding 0 might be black.
        // Let's just create a new TintedMaterial locally since it's cheap.
        
        // Actually, let's stick to the plan of minimal changes.
        // We will assume `assets/shaders/tinted.vert` and `.frag` exist (used in MenuState).
        static our::TintedMaterial* barMaterial = nullptr;
        if(!barMaterial){
            barMaterial = new our::TintedMaterial();
            barMaterial->shader = new our::ShaderProgram();
            barMaterial->shader->attach("assets/shaders/tinted.vert", GL_VERTEX_SHADER);
            barMaterial->shader->attach("assets/shaders/tinted.frag", GL_FRAGMENT_SHADER);
            barMaterial->shader->link();
            
            // Use menu highlight color (Gold/Yellowish with transparency)
            barMaterial->tint = glm::vec4(0.0f, 0.92f, 0.6f, 0.45f);
            
            barMaterial->pipelineState.blending.enabled = true;
            barMaterial->pipelineState.blending.equation = GL_FUNC_ADD;
            barMaterial->pipelineState.blending.sourceFactor = GL_SRC_ALPHA;
            barMaterial->pipelineState.blending.destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
        }
        
        barMaterial->setup();
        barMaterial->shader->set("transform", VP*barM);
        bar->draw();
        
        framesRendered++;
        
        // Transition when finished
        if (loadingTasks.empty()) { // Wait a bit after full
              // Ensure we showed the full bar at least once
              if(framesRendered > 60 || totalTasks == 0){ // Wait at least a bit or until done? 
                  // If tasks are done immediately, we might flash.
                  // Let's add a minimum time check too.
              }
              if(time > 1.0f) { // Minimum 1 second loading screen
                  getApp()->changeState("play");
              }
        }
    }

    void onDestroy() override {
        delete rectangle;
        delete bar;
        delete loadingMaterial->texture;
        delete loadingMaterial->shader;
        delete loadingMaterial;
        // Note: we're leaking the static barMaterial strictly speaking, but it's one-time per app run practically.
    }
};
