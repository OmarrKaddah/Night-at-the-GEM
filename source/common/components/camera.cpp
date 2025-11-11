#include "camera.hpp"
#include "../ecs/entity.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> 

namespace our {
    // Reads camera parameters from the given json object
    void CameraComponent::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;
        std::string cameraTypeStr = data.value("cameraType", "perspective");
        if(cameraTypeStr == "orthographic"){
            cameraType = CameraType::ORTHOGRAPHIC;
        } else {
            cameraType = CameraType::PERSPECTIVE;
        }
        near = data.value("near", 0.01f);
        far = data.value("far", 100.0f);
        fovY = data.value("fovY", 90.0f) * (glm::pi<float>() / 180);
        orthoHeight = data.value("orthoHeight", 1.0f);
    }

    // Creates and returns the camera view matrix
    glm::mat4 CameraComponent::getViewMatrix() const {
        auto owner = getOwner();
        auto M = owner->getLocalToWorldMatrix();
        //TODO: (Req 8) Complete this function
        //HINT:
        // In the camera space:
        // - eye is the origin (0,0,0)
        // - center is any point on the line of sight. So center can be any point (0,0,z) where z < 0. For simplicity, we let center be (0,0,-1)
        // - up is the direction (0,1,0)
        // but to use glm::lookAt, we need eye, center and up in the world state.
        // Since M (see above) transforms from the camera to thw world space, you can use M to compute:
        // - the eye position which is the point (0,0,0) but after being transformed by M
        // - the center position which is the point (0,0,-1) but after being transformed by M
        // - the up direction which is the vector (0,1,0) but after being transformed by M
        // then you can use glm::lookAt
        // eye in camera space is (0,0,0,1)
        glm::vec4 eye_c(0.0f, 0.0f, 0.0f, 1.0f);
        // center in camera space is (0,0,-1,1)
        glm::vec4 center_c(0.0f, 0.0f, -1.0f, 1.0f);
        // up in camera space is (0,1,0,0) - a direction vector
        glm::vec4 up_c(0.0f, 1.0f, 0.0f, 0.0f);

        // Transform to world space using M (camera -> world)
        glm::vec3 eye_w = glm::vec3(M * eye_c);
        glm::vec3 center_w = glm::vec3(M * center_c);
        glm::vec3 up_w = glm::normalize(glm::vec3(M * up_c));

        return glm::lookAt(eye_w, center_w, up_w);
    }

    // Creates and returns the camera projection matrix
    // "viewportSize" is used to compute the aspect ratio
    glm::mat4 CameraComponent::getProjectionMatrix(glm::ivec2 viewportSize) const {
        float aspect = 1.0f;
        if(viewportSize.y != 0) aspect = float(viewportSize.x) / float(viewportSize.y);
        if(cameraType == CameraType::ORTHOGRAPHIC){
            float halfH = orthoHeight * 0.5f;
            float halfW = halfH * aspect;
            float left = -halfW;
            float right = halfW;
            float bottom = -halfH;
            float top = halfH;
            return glm::ortho(left, right, bottom, top, near, far);
        } else {
            // perspective
            return glm::perspective(fovY, aspect, near, far);
        }
    }
}