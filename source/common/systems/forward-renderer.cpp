#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"
#include "../material/material.hpp"
#include <iostream>

namespace our {

    void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json& config){
        // First, we store the window size for later use
        this->windowSize = windowSize;

        // Then we check if there is a sky texture in the configuration
        if(config.contains("sky")){
            // First, we create a sphere which will be used to draw the sky
            this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));
            
            // We can draw the sky using the same shader used to draw textured objects
            ShaderProgram* skyShader = new ShaderProgram();
            skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
            skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
            skyShader->link();
            
            //TODO: (Req 10) Pick the correct pipeline state to draw the sky
            // Hints: the sky will be draw after the opaque objects so we would need depth testing but which depth funtion should we pick?
            // We will draw the sphere from the inside, so what options should we pick for the face culling.
            PipelineState skyPipelineState{};
            skyPipelineState.depthTesting.enabled = true;
            skyPipelineState.depthTesting.function = GL_LEQUAL;
			skyPipelineState.depthMask = false;
			skyPipelineState.faceCulling.enabled = true;
			skyPipelineState.faceCulling.culledFace = GL_FRONT;
			skyPipelineState.blending.enabled = false;
            
            // Load the sky texture (note that we don't need mipmaps since we want to avoid any unnecessary blurring while rendering the sky)
            std::string skyTextureFile = config.value<std::string>("sky", "");
            Texture2D* skyTexture = texture_utils::loadImage(skyTextureFile, false);

            // Setup a sampler for the sky 
            Sampler* skySampler = new Sampler();
            skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
            skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Combine all the aforementioned objects (except the mesh) into a material 
            this->skyMaterial = new TexturedMaterial();
            this->skyMaterial->shader = skyShader;
            this->skyMaterial->texture = skyTexture;
            this->skyMaterial->sampler = skySampler;
            this->skyMaterial->pipelineState = skyPipelineState;
            this->skyMaterial->tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            this->skyMaterial->alphaThreshold = 1.0f;
            this->skyMaterial->transparent = false;
        }

        // Then we check if there is a postprocessing shader in the configuration
        if(config.contains("postprocess")){
            // Create a framebuffer
            glGenFramebuffers(1, &postprocessFrameBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, postprocessFrameBuffer);

            // Create a color and a depth texture and attach them to the framebuffer
            // Color: RGBA8, Depth: 24-bit depth
            colorTarget = texture_utils::empty(GL_RGBA8, windowSize);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTarget->getOpenGLName(), 0);

            depthTarget = texture_utils::empty(GL_DEPTH_COMPONENT24, windowSize);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTarget->getOpenGLName(), 0);

            // Check framebuffer completeness
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
                std::cerr << "ERROR: Postprocess framebuffer is not complete" << std::endl;
            }

            // Unbind the framebuffer just to be safe
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Create a vertex array to use for drawing the texture
            glGenVertexArrays(1, &postProcessVertexArray);

            // Create a sampler to use for sampling the scene texture in the post processing shader
            Sampler* postprocessSampler = new Sampler();
            postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Create the post processing shader
            ShaderProgram* postprocessShader = new ShaderProgram();
            postprocessShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
            postprocessShader->attach(config.value<std::string>("postprocess", ""), GL_FRAGMENT_SHADER);
            postprocessShader->link();

            // Create a post processing material
            postprocessMaterial = new TexturedMaterial();
            postprocessMaterial->shader = postprocessShader;
            postprocessMaterial->texture = colorTarget;
            postprocessMaterial->sampler = postprocessSampler;
            // The default options are fine but we don't need to interact with the depth buffer
            // so it is more performant to disable the depth mask
            postprocessMaterial->pipelineState.depthMask = false;
        }

        // Initialize shadow map for flashlight shadows
        glGenFramebuffers(1, &shadowMapFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
        
        // Create shadow map depth texture
        shadowMap = texture_utils::empty(GL_DEPTH_COMPONENT24, glm::ivec2(shadowMapSize, shadowMapSize));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap->getOpenGLName(), 0);
        
        // Configure shadow map texture for shadow sampling
        shadowMap->bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        // Use manual depth comparison (GL_NONE for compare mode)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        Texture2D::unbind();
        
        // No color buffer needed for shadow map
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
            std::cerr << "ERROR: Shadow map framebuffer is not complete" << std::endl;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        // Create shadow map shader (simple depth-only shader)
        shadowMapShader = new ShaderProgram();
        shadowMapShader->attach("assets/shaders/shadow-map.vert", GL_VERTEX_SHADER);
        shadowMapShader->attach("assets/shaders/shadow-map.frag", GL_FRAGMENT_SHADER);
        shadowMapShader->link();
    }

    void ForwardRenderer::destroy(){
        // Delete all objects related to the sky
        if(skyMaterial){
            delete skySphere;
            delete skyMaterial->shader;
            delete skyMaterial->texture;
            delete skyMaterial->sampler;
            delete skyMaterial;
        }
        // Delete all objects related to post processing
        if(postprocessMaterial){
            glDeleteFramebuffers(1, &postprocessFrameBuffer);
            glDeleteVertexArrays(1, &postProcessVertexArray);
            delete colorTarget;
            delete depthTarget;
            delete postprocessMaterial->sampler;
            delete postprocessMaterial->shader;
            delete postprocessMaterial;
        }
        // Delete shadow map resources
        if(shadowMapFBO != 0){
            glDeleteFramebuffers(1, &shadowMapFBO);
            delete shadowMap;
            if(shadowMapShader){
                delete shadowMapShader;
            }
            shadowMapFBO = 0;
        }
    }

    void ForwardRenderer::render(World* world) {
        // 1) Find camera & collect render commands and lights
        CameraComponent* camera = nullptr;
        opaqueCommands.clear();
        transparentCommands.clear();
        std::vector<LightComponent*> lights;
        glm::mat4 lightSpaceMatrix(1.0f);
        LightComponent* flashlight = nullptr;
        int flashlightIndex = -1;

        // Loop through entities to find camera, mesh renderers, and lights
        for (auto entity : world->getEntities()) {
            // If no camera yet, try to get one
            if (!camera) camera = entity->getComponent<CameraComponent>();

            // If this entity has a mesh renderer, collect its draw data
            if (auto meshRenderer = entity->getComponent<MeshRendererComponent>()) {
                RenderCommand command;
                command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
                command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
                command.mesh = meshRenderer->mesh;
                command.material = meshRenderer->material;

                // Separate transparent and opaque commands
                if (command.material->transparent)
                    transparentCommands.push_back(command);
                else
                    opaqueCommands.push_back(command);
            }

            // Collect lights
            if (auto light = entity->getComponent<LightComponent>()) {
                lights.push_back(light);
            }
        }

        // Cannot render without a camera
        if (camera == nullptr) return;

        // === 0) Render shadow map from flashlight perspective ==================
        // Find the first spotlight (flashlight) and its index
        for (size_t i = 0; i < lights.size(); i++) {
            if (lights[i]->lightType == LightType::SPOT) {
                flashlight = lights[i];
                flashlightIndex = (int)i;
                break;
            }
        }
        
        // Debug: Check if flashlight was found
        if (!flashlight) {
            // No spotlight found, disable shadows
            flashlightIndex = -1;
        }
        
        // Only render shadow map if we have a flashlight and valid shadow map resources
        if (flashlight && shadowMapFBO != 0 && shadowMap && shadowMapShader) {
            // Debug output to verify shadow map is being rendered
            // std::cout << "Rendering shadow map from flashlight at position: " 
            //           << flashlight->getPosition().x << ", " 
            //           << flashlight->getPosition().y << ", " 
            //           << flashlight->getPosition().z << std::endl;
            // Calculate light's view-projection matrix
            glm::vec3 lightPos = flashlight->getPosition();
            glm::vec3 lightDir = flashlight->getDirection();
            glm::vec3 lightUp = glm::vec3(0.0f, 1.0f, 0.0f);
            // If light is pointing straight up/down, use different up vector
            if (abs(glm::dot(lightDir, lightUp)) > 0.9f) {
                lightUp = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            
            // Create light's view matrix (looking in light direction)
            glm::mat4 lightView = glm::lookAt(lightPos, lightPos + lightDir, lightUp);
            
            // Create light's projection matrix (perspective for spotlight)
            // outer_angle is already in radians, multiply by 2 to get full FOV
            float lightFOV = flashlight->outer_angle * 2.0f; // Use outer angle for projection
            // Clamp FOV to reasonable range
            lightFOV = glm::clamp(lightFOV, glm::radians(10.0f), glm::radians(120.0f));
            float lightNear = 0.1f;
            float lightFar = 100.0f; // Increased far plane to cover more area
            glm::mat4 lightProj = glm::perspective(lightFOV, 1.0f, lightNear, lightFar);
            
            lightSpaceMatrix = lightProj * lightView;
            
            // Render shadow map - MUST be done every frame to update shadows
            glViewport(0, 0, shadowMapSize, shadowMapSize);
            glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
            
            // CRITICAL: Clear depth buffer completely every frame
            // This ensures old shadows are removed before rendering new ones
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);  // Enable depth writing
            glClearDepth(1.0);     // Set clear value to far plane (white = no shadow)
            glClear(GL_DEPTH_BUFFER_BIT);  // Clear the depth buffer - removes all old shadows
            
            // Disable color writing
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            // Use back face culling for shadow map (standard approach)
            // Front face culling can cause issues with some models
            glCullFace(GL_BACK);
            glEnable(GL_CULL_FACE);
            
            shadowMapShader->use();
            
            // Render all opaque objects to shadow map with their CURRENT positions
            // This ensures shadows update as objects move
            // IMPORTANT: We render ALL objects (zombies, ground, walls) to the shadow map
            // so we can detect when the ground/walls are behind zombies
            for (const auto& cmd : opaqueCommands) {
                // Use current world matrix (updated each frame from entity's current transform)
                shadowMapShader->set("model", cmd.localToWorld);
                shadowMapShader->set("lightSpaceMatrix", lightSpaceMatrix);
                
                if (!cmd.mesh->submeshes.empty()) {
                    GLuint vao = cmd.mesh->getVAO();
                    glBindVertexArray(vao);
                    for (auto& sub : cmd.mesh->submeshes) {
                        glDrawElements(GL_TRIANGLES, sub.count, GL_UNSIGNED_INT, 
                                     (void*)(sub.offset * sizeof(GLuint)));
                    }
                    glBindVertexArray(0);
                } else {
                    cmd.mesh->draw();
                }
            }
            
            // Flush rendering to ensure shadow map is complete
            // Note: glFinish() can be expensive, but ensures shadow map is ready
            // For better performance, you could remove this, but shadows might lag slightly
            // glFinish();
            
            // Unbind and restore state
            glCullFace(GL_BACK); // Restore back face culling
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            // Restore viewport
            glViewport(0, 0, windowSize.x, windowSize.y);
        }

        // === 2) Sort transparent objects from far → near ======================
        glm::mat4 cameraWorld = camera->getOwner()->getLocalToWorldMatrix();

        // Camera faces -Z in its local space, so transform that into world space
        glm::vec3 cameraForward = glm::normalize(glm::vec3(cameraWorld * glm::vec4(0, 0, -1, 0)));

        // Sort transparent objects by distance along the camera forward direction
        std::sort(transparentCommands.begin(), transparentCommands.end(),
            [cameraForward](const RenderCommand& a, const RenderCommand& b) {
                float da = glm::dot(cameraForward, a.center);
                float db = glm::dot(cameraForward, b.center);
                return da > db; // draw farther objects first
            }
        );

        // === 3) Get ViewProjection matrix =====================================
        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 proj = camera->getProjectionMatrix(windowSize);
        glm::mat4 VP = proj * view;

        // === 4) Setup viewport & clear buffers ================================
        glViewport(0, 0, windowSize.x, windowSize.y);
        glClearColor(0, 0, 0, 1);
        glClearDepth(1.0);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);

        // If postprocessing enabled → render to framebuffer
        if (postprocessMaterial) {
            // Bind framebuffer before rendering scene
            glBindFramebuffer(GL_FRAMEBUFFER, postprocessFrameBuffer);
        }

        // Clear color and depth
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Get camera position for lighting
        glm::vec3 cameraPos = glm::vec3(cameraWorld * glm::vec4(0, 0, 0, 1));

        // === 5) Draw opaque objects ==========================================
        for (const auto& cmd : opaqueCommands) {
            cmd.material->setup();
            cmd.material->shader->use();

            // Compute MVP transform and send to shader
            glm::mat4 transform = VP * cmd.localToWorld;
            cmd.material->shader->set("transform", transform);
            
            // For lit materials, also send model matrix for world position calculation
            if (dynamic_cast<LitMaterial*>(cmd.material)) {
                cmd.material->shader->set("model", cmd.localToWorld);
                cmd.material->shader->set("model_IT", glm::transpose(glm::inverse(cmd.localToWorld)));
            }

            // Send shadow map data BEFORE lighting data (so it's bound when shader uses it)
            if (dynamic_cast<LitMaterial*>(cmd.material)) {
                if (flashlight && shadowMap && flashlightIndex >= 0 && shadowMapFBO != 0) {
                    cmd.material->shader->set("use_shadows", 1);
                    cmd.material->shader->set("light_space_matrix", lightSpaceMatrix);
                    cmd.material->shader->set("flashlight_index", flashlightIndex);
                    // Bind shadow map to texture unit 5
                    glActiveTexture(GL_TEXTURE5);
                    shadowMap->bind();
                    cmd.material->shader->set("shadow_map", 5);
                    glActiveTexture(GL_TEXTURE0); // Reset to texture unit 0
                } else {
                    cmd.material->shader->set("use_shadows", 0);
                }
            }

            // Send lighting data if this is a lit material
            if (dynamic_cast<LitMaterial*>(cmd.material)) {
                // Send lights (up to 8)
                int lightCount = std::min((int)lights.size(), 8);
                // Ensure light_count is set even if 0
                cmd.material->shader->set("light_count", lightCount);
                
                for (int i = 0; i < lightCount; i++) {
                    auto* light = lights[i];
                    std::string lightPrefix = "lights[" + std::to_string(i) + "]";
                    
                    cmd.material->shader->set(lightPrefix + ".type", (int)light->lightType);
                    cmd.material->shader->set(lightPrefix + ".color", light->color);
                    cmd.material->shader->set(lightPrefix + ".constant", light->attenuation_constant);
                    cmd.material->shader->set(lightPrefix + ".linear", light->attenuation_linear);
                    cmd.material->shader->set(lightPrefix + ".quadratic", light->attenuation_quadratic);
                    cmd.material->shader->set(lightPrefix + ".inner_angle", cos(light->inner_angle));
                    cmd.material->shader->set(lightPrefix + ".outer_angle", cos(light->outer_angle));
                    
                    // Position and direction depend on light type
                    if (light->lightType == LightType::DIRECTIONAL) {
                        cmd.material->shader->set(lightPrefix + ".direction", light->getDirection());
                        cmd.material->shader->set(lightPrefix + ".position", glm::vec3(0.0f)); // Not used for directional
                    } else {
                        cmd.material->shader->set(lightPrefix + ".position", light->getPosition());
                        if (light->lightType == LightType::SPOT) {
                            cmd.material->shader->set(lightPrefix + ".direction", light->getDirection());
                        } else {
                            cmd.material->shader->set(lightPrefix + ".direction", glm::vec3(0.0f)); // Not used for point
                        }
                    }
                }
                
                // Send camera position and ambient light
                cmd.material->shader->set("camera_pos", cameraPos);
                cmd.material->shader->set("ambient_light", glm::vec3(0.15f, 0.15f, 0.15f)); // Lighter ambient so you can see around the flashlight
            }

            // Draw the mesh
            //cmd.mesh->draw();
            // MULTI-MATERIAL DRAWING
            if (!cmd.mesh->submeshes.empty()) {
                GLuint vao = cmd.mesh->getVAO();
                glBindVertexArray(vao);

                for (auto& sub : cmd.mesh->submeshes) {

                    // Try material matching the .mtl name
                    Material* matToUse = AssetLoader<Material>::get(sub.materialName);

                    // If not found, fallback to the material set in JSON
                    if (!matToUse) matToUse = cmd.material;
                    if (!matToUse) continue;

                    matToUse->setup();
                    matToUse->shader->use();

                    glm::mat4 transform = VP * cmd.localToWorld;
                    matToUse->shader->set("transform", transform);

                    glDrawElements(
                        GL_TRIANGLES,
                        sub.count,
                        GL_UNSIGNED_INT,
                        (void*)(sub.offset * sizeof(GLuint))
                    );
                }

                glBindVertexArray(0);
            }
            else {
                // Single-material mesh
                cmd.material->setup();
                cmd.material->shader->use();

                glm::mat4 transform = VP * cmd.localToWorld;
                cmd.material->shader->set("transform", transform);

                cmd.mesh->draw();
            }

        }

        // === 6) Draw sky (Req 10) ============================================
        if (skyMaterial) {
            // Apply sky pipeline state (depth test ON, depth mask OFF, cull front)
            skyMaterial->setup();
            skyMaterial->shader->use();

            // Camera position already calculated above

            // Model matrix for sky: center it around camera
            glm::mat4 model = glm::translate(glm::mat4(1.0f), cameraPos);

            // Optionally scale sky sphere (large enough to cover entire view)
            model = glm::scale(model, glm::vec3(100.0f));

            // Transform = VP * model
            glm::mat4 transform = VP * model;

            // Force the sky to be at the far plane by setting NDC z = 1 for all vertices.
            // This is achieved by making the 3rd row of the transform equal to the 4th row
            // so that gl_Position.z == gl_Position.w after the vertex shader transform.
            for(int c = 0; c < 4; ++c){
                transform[c][2] = transform[c][3];
            }

            // Set transform uniform
            skyMaterial->shader->set("transform", transform);

            // Draw sky sphere
            skySphere->draw();

        }

        // === 7) Draw transparent objects =====================================
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE); // don't overwrite depth

        for (const auto& cmd : transparentCommands) {
            cmd.material->setup();
            cmd.material->shader->use();

            glm::mat4 transform = VP * cmd.localToWorld;
            cmd.material->shader->set("transform", transform);
            
            // Set model matrix for shaders that need world position (like light-beam)
            // Check if shader has "model" uniform by trying to set it
            if (cmd.material->shader->getUniformLocation("model") != -1) {
                cmd.material->shader->set("model", cmd.localToWorld);
                if (cmd.material->shader->getUniformLocation("model_IT") != -1) {
                    cmd.material->shader->set("model_IT", glm::transpose(glm::inverse(cmd.localToWorld)));
                }
            }
            // MULTI-MATERIAL DRAWING
            if (!cmd.mesh->submeshes.empty()) {
                GLuint vao = cmd.mesh->getVAO();
                glBindVertexArray(vao);

                for (auto& sub : cmd.mesh->submeshes) {

                    // Try material matching the .mtl name
                    Material* matToUse = AssetLoader<Material>::get(sub.materialName);

                    // If not found, fallback to the material set in JSON
                    if (!matToUse) matToUse = cmd.material;
                    if (!matToUse) continue;

                    matToUse->setup();
                    matToUse->shader->use();

                    glm::mat4 transform = VP * cmd.localToWorld;
                    matToUse->shader->set("transform", transform);
                    
                    // Set model matrix for shaders that need world position
                    if (matToUse->shader->getUniformLocation("model") != -1) {
                        matToUse->shader->set("model", cmd.localToWorld);
                        if (matToUse->shader->getUniformLocation("model_IT") != -1) {
                            matToUse->shader->set("model_IT", glm::transpose(glm::inverse(cmd.localToWorld)));
                        }
                    }

                    glDrawElements(
                        GL_TRIANGLES,
                        sub.count,
                        GL_UNSIGNED_INT,
                        (void*)(sub.offset * sizeof(GLuint))
                    );
                }

                glBindVertexArray(0);
            }
            else {
                // Single-material mesh
                cmd.material->setup();
                cmd.material->shader->use();

                glm::mat4 transform = VP * cmd.localToWorld;
                cmd.material->shader->set("transform", transform);
                
                // Set model matrix for shaders that need world position
                if (cmd.material->shader->getUniformLocation("model") != -1) {
                    cmd.material->shader->set("model", cmd.localToWorld);
                    if (cmd.material->shader->getUniformLocation("model_IT") != -1) {
                        cmd.material->shader->set("model_IT", glm::transpose(glm::inverse(cmd.localToWorld)));
                    }
                }

                cmd.mesh->draw();
            }
        }

        // Reset blend and depth state
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // === 8) Postprocessing (Req 11) ======================================
        if (postprocessMaterial) {
            // Unbind framebuffer (return to default)
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Setup postprocess material and draw fullscreen triangle
            postprocessMaterial->setup();
            postprocessMaterial->shader->use();
            glBindVertexArray(postProcessVertexArray);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
        }
    }



}