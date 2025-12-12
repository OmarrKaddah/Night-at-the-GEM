#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"
#include "../components/animator.hpp"
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

        // Create a simple debug shader for drawing skeleton lines
        debugLineShader = new ShaderProgram();
        debugLineShader->attach("assets/shaders/tinted.vert", GL_VERTEX_SHADER);
        debugLineShader->attach("assets/shaders/tinted.frag", GL_FRAGMENT_SHADER);
        debugLineShader->link();
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
        if(debugLineShader){
            delete debugLineShader;
            debugLineShader = nullptr;
        }
    }

    void ForwardRenderer::render(World* world) {
        // 1) Find camera & collect render commands 
        CameraComponent* camera = nullptr;
        opaqueCommands.clear();
        transparentCommands.clear();

        // Loop through entities to find camera and mesh renderers
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
                
                // Check for animator component
                command.animator = entity->getComponent<AnimatorComponent>();

                // Separate transparent and opaque commands
                if (command.material->transparent)
                    transparentCommands.push_back(command);
                else
                    opaqueCommands.push_back(command);
            }
        }

        // Cannot render without a camera
        if (camera == nullptr) return;

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

        // === 5) Draw opaque objects ==========================================
        for (const auto& cmd : opaqueCommands) {
            
            // MULTI-MATERIAL DRAWING
            if (!cmd.mesh->submeshes.empty()) {
                GLuint vao = cmd.mesh->getVAO();
                glBindVertexArray(vao);

                for (auto& sub : cmd.mesh->submeshes) {
                    Material* matToUse = AssetLoader<Material>::get(sub.materialName);
                    if (!matToUse) matToUse = cmd.material;
                    if (!matToUse) continue;

                    matToUse->setup();
                    matToUse->shader->use();
                    matToUse->shader->set("transform", VP * cmd.localToWorld);

                    // OPTIMIZED SKINNING DATA
                    if (cmd.animator && !cmd.animator->boneTransforms.empty()) {
                        matToUse->shader->set("useSkinning", true);
                        matToUse->shader->set("boneTransforms", cmd.animator->boneTransforms.data(), (uint32_t)cmd.animator->boneTransforms.size());
                    } else {
                        matToUse->shader->set("useSkinning", false);
                    }

                    glDrawElements(GL_TRIANGLES, sub.count, GL_UNSIGNED_INT, (void*)(sub.offset * sizeof(GLuint)));
                }
                glBindVertexArray(0);
            }
            else {
                // SINGLE-MATERIAL MESH
                cmd.material->setup();
                cmd.material->shader->use();
                cmd.material->shader->set("transform", VP * cmd.localToWorld);

                if (cmd.animator && !cmd.animator->boneTransforms.empty()) {
                    cmd.material->shader->set("useSkinning", true);
                    cmd.material->shader->set("boneTransforms", cmd.animator->boneTransforms.data(), (uint32_t)cmd.animator->boneTransforms.size());
                } else {
                    cmd.material->shader->set("useSkinning", false);
                }
                cmd.mesh->draw();
            }
        }

        // Debug: draw skeleton lines for animated meshes
        if (debugLineShader && debugDrawSkeleton) {
            std::vector<float> lineVerts; // position (3) + color (4)

            for (const auto& cmd : opaqueCommands) {
                if (!cmd.animator) continue;
                auto skel = cmd.animator->skeleton;
                if (!skel) continue;
                if (cmd.animator->jointTransforms.size() != skel->bones.size()) continue;

                for (size_t i = 0; i < skel->bones.size(); ++i) {
                    int parent = skel->parentIndices[i];
                    if (parent < 0) continue;

                    glm::vec4 p0 = cmd.localToWorld * (cmd.animator->jointTransforms[parent] * glm::vec4(0,0,0,1));
                    glm::vec4 p1 = cmd.localToWorld * (cmd.animator->jointTransforms[i] * glm::vec4(0,0,0,1));

                    // color: green
                    float r = 0.0f, g = 1.0f, b = 0.0f, a = 1.0f;

                    // parent
                    lineVerts.push_back(p0.x); lineVerts.push_back(p0.y); lineVerts.push_back(p0.z);
                    lineVerts.push_back(r); lineVerts.push_back(g); lineVerts.push_back(b); lineVerts.push_back(a);
                    // child
                    lineVerts.push_back(p1.x); lineVerts.push_back(p1.y); lineVerts.push_back(p1.z);
                    lineVerts.push_back(r); lineVerts.push_back(g); lineVerts.push_back(b); lineVerts.push_back(a);
                }
            }

            if (!lineVerts.empty()) {
                GLuint vao = 0, vbo = 0;
                glGenVertexArrays(1, &vao);
                glGenBuffers(1, &vbo);
                glBindVertexArray(vao);

                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, lineVerts.size() * sizeof(float), lineVerts.data(), GL_DYNAMIC_DRAW);

                // position (location = 0)
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
                // color (location = 1)
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));

                // draw
                debugLineShader->use();
                debugLineShader->set("transform", VP);
                glLineWidth(2.0f);
                glBindVertexArray(vao);
                glDrawArrays(GL_LINES, 0, (GLsizei)(lineVerts.size() / 7));
                glBindVertexArray(0);

                glDeleteBuffers(1, &vbo);
                glDeleteVertexArrays(1, &vao);
            }
        }

        // === 6) Draw sky (Req 10) ============================================
        if (skyMaterial) {
            // Apply sky pipeline state (depth test ON, depth mask OFF, cull front)
            skyMaterial->setup();
            skyMaterial->shader->use();

            // Get camera position
            glm::vec3 cameraPos = glm::vec3(cameraWorld * glm::vec4(0, 0, 0, 1));

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

                    if (cmd.animator && !cmd.animator->boneTransforms.empty()) {
                        matToUse->shader->set("useSkinning", true);
                        matToUse->shader->set("boneTransforms", cmd.animator->boneTransforms.data(), (uint32_t)cmd.animator->boneTransforms.size());
                    } else {
                        matToUse->shader->set("useSkinning", false);
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
                
                // Check for animator component
                if (cmd.animator && !cmd.animator->boneTransforms.empty()) {
                    cmd.material->shader->set("useSkinning", true);
                    cmd.material->shader->set("boneTransforms", cmd.animator->boneTransforms.data(), (uint32_t)cmd.animator->boneTransforms.size());
                } else {
                    cmd.material->shader->set("useSkinning", false);
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