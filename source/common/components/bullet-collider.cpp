#include "bullet-collider.hpp"
#include "../ecs/entity.hpp"
#include "../asset-loader.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>

namespace our {

    BulletColliderComponent::BulletColliderComponent() 
        : collisionShape(nullptr), rigidBody(nullptr), motionState(nullptr),
          shapeType(CollisionShape::BOX), size(1.0f, 1.0f, 1.0f),
          mass(0.0f), friction(0.5f), restitution(0.0f), isTrigger(false),
          centerOffset(0.0f), mesh(nullptr) {
    }

    BulletColliderComponent::~BulletColliderComponent() {
        // Clean up Bullet objects in reverse order of creation
        if (rigidBody) {
            if (rigidBody->getMotionState()) {
                delete rigidBody->getMotionState();
            }
            delete rigidBody;
        }
        if (collisionShape) {
            delete collisionShape;
        }
    }

    void BulletColliderComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;

        // Read shape type
        std::string shapeTypeStr = data.value("shape", "box");
        if (shapeTypeStr == "box") shapeType = CollisionShape::BOX;
        else if (shapeTypeStr == "sphere") shapeType = CollisionShape::SPHERE;
        else if (shapeTypeStr == "capsule") shapeType = CollisionShape::CAPSULE;
        else if (shapeTypeStr == "cylinder") shapeType = CollisionShape::CYLINDER;
        else if (shapeTypeStr == "mesh") shapeType = CollisionShape::MESH;
        else if (shapeTypeStr == "convex") shapeType = CollisionShape::CONVEX_HULL;

        // Read size
        if (data.contains("size")) {
            auto sizeArr = data["size"];
            if (sizeArr.is_array() && sizeArr.size() >= 3) {
                size = glm::vec3(sizeArr[0], sizeArr[1], sizeArr[2]);
            }
        }

        // Read physics properties
        mass = data.value("mass", 0.0f);
        friction = data.value("friction", 0.5f);
        restitution = data.value("restitution", 0.0f);
        isTrigger = data.value("isTrigger", false);
        
        // Read center offset
        if (data.contains("centerOffset")) {
            auto offsetArr = data["centerOffset"];
            if (offsetArr.is_array() && offsetArr.size() >= 3) {
                centerOffset = glm::vec3(offsetArr[0], offsetArr[1], offsetArr[2]);
            }
        }

        // Load mesh if specified (for mesh or convex hull shapes)
        std::string meshName = data.value("mesh", "");
        if (!meshName.empty()) {
            mesh = AssetLoader<Mesh>::get(meshName);
        }
    }

    btCollisionShape* BulletColliderComponent::createShape() {
        btCollisionShape* shape = nullptr;

        switch (shapeType) {
            case CollisionShape::BOX:
                shape = new btBoxShape(btVector3(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f));
                break;

            case CollisionShape::SPHERE:
                shape = new btSphereShape(size.x); // Use x component as radius
                break;

            case CollisionShape::CAPSULE:
                shape = new btCapsuleShape(size.x, size.y); // radius, height
                break;

            case CollisionShape::CYLINDER:
                shape = new btCylinderShape(btVector3(size.x, size.y * 0.5f, size.z));
                break;

            case CollisionShape::MESH:
                // For static mesh collisions (triangle mesh - perfect for buildings)
                if (mesh && !mesh->vertices.empty()) {
                    btTriangleMesh* triMesh = new btTriangleMesh();
                    int triangleCount = 0;
                    
                    // Get entity transform to apply to mesh vertices
                    glm::mat4 transform = getOwner()->getLocalToWorldMatrix();
                    
                    // If mesh has direct elements, use them
                    if (!mesh->elements.empty()) {
                        for (size_t i = 0; i + 2 < mesh->elements.size(); i += 3) {
                            unsigned int idx0 = mesh->elements[i];
                            unsigned int idx1 = mesh->elements[i + 1];
                            unsigned int idx2 = mesh->elements[i + 2];
                            
                            if (idx0 < mesh->vertices.size() && 
                                idx1 < mesh->vertices.size() && 
                                idx2 < mesh->vertices.size()) {
                                
                                // Apply entity transform to vertices
                                glm::vec4 v0 = transform * glm::vec4(mesh->vertices[idx0].position, 1.0f);
                                glm::vec4 v1 = transform * glm::vec4(mesh->vertices[idx1].position, 1.0f);
                                glm::vec4 v2 = transform * glm::vec4(mesh->vertices[idx2].position, 1.0f);
                                
                                triMesh->addTriangle(
                                    btVector3(v0.x, v0.y, v0.z),
                                    btVector3(v1.x, v1.y, v1.z),
                                    btVector3(v2.x, v2.y, v2.z)
                                );
                                triangleCount++;
                            }
                        }
                    }
                    // Otherwise, iterate through submeshes
                    else if (!mesh->submeshes.empty()) {
                        // Get EBO data from GPU
                        GLuint ebo = mesh->getEBO();
                        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
                        
                        GLint eboSize = 0;
                        glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &eboSize);
                        int elementCount = eboSize / sizeof(unsigned int);
                        
                        std::vector<unsigned int> indices(elementCount);
                        glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, eboSize, indices.data());
                        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                        
                        // Add triangles from all submeshes
                        for (const auto& submesh : mesh->submeshes) {
                            for (GLuint i = submesh.offset; i < submesh.offset + submesh.count - 2; i += 3) {
                                if (i + 2 < indices.size()) {
                                    unsigned int idx0 = indices[i];
                                    unsigned int idx1 = indices[i + 1];
                                    unsigned int idx2 = indices[i + 2];
                                    
                                    if (idx0 < mesh->vertices.size() && 
                                        idx1 < mesh->vertices.size() && 
                                        idx2 < mesh->vertices.size()) {
                                        
                                        // Apply entity transform to vertices
                                        glm::vec4 v0 = transform * glm::vec4(mesh->vertices[idx0].position, 1.0f);
                                        glm::vec4 v1 = transform * glm::vec4(mesh->vertices[idx1].position, 1.0f);
                                        glm::vec4 v2 = transform * glm::vec4(mesh->vertices[idx2].position, 1.0f);
                                        
                                        triMesh->addTriangle(
                                            btVector3(v0.x, v0.y, v0.z),
                                            btVector3(v1.x, v1.y, v1.z),
                                            btVector3(v2.x, v2.y, v2.z)
                                        );
                                        triangleCount++;
                                    }
                                }
                            }
                        }
                    }
                    
                    std::cout << "BulletCollider: Created mesh collider with " << triangleCount << " triangles from " 
                              << mesh->vertices.size() << " vertices (" << mesh->submeshes.size() << " submeshes)" << std::endl;
                    
                    if (triangleCount > 0) {
                        // Use BVH for fast collision queries
                        shape = new btBvhTriangleMeshShape(triMesh, true);
                    } else {
                        delete triMesh;
                        shape = new btBoxShape(btVector3(10.0f, 10.0f, 10.0f)); // Large fallback
                    }
                } else {
                    std::cout << "BulletCollider: Mesh collider fallback - mesh is " 
                              << (mesh ? "valid" : "null") << ", vertices: " 
                              << (mesh ? mesh->vertices.size() : 0) << std::endl;
                    shape = new btBoxShape(btVector3(10.0f, 10.0f, 10.0f)); // Fallback
                }
                break;

            case CollisionShape::CONVEX_HULL:
                // For dynamic convex collisions
                if (mesh) {
                    btConvexHullShape* convexShape = new btConvexHullShape();
                    // Note: You'd need to add points from your mesh to convexShape
                    // using convexShape->addPoint()
                    shape = convexShape;
                } else {
                    shape = new btBoxShape(btVector3(0.5f, 0.5f, 0.5f)); // Fallback
                }
                break;
        }

        return shape;
    }

    void BulletColliderComponent::initialize(btDiscreteDynamicsWorld* world) {
        if (!getOwner()) return;

        // Create collision shape
        collisionShape = createShape();
        if (!collisionShape) return;

        // Create motion state
        btTransform startTransform;
        startTransform.setIdentity();
        
        // For mesh colliders (static), transform is baked into vertices, so use identity
        if (shapeType == CollisionShape::MESH && mass == 0.0f) {
            startTransform.setOrigin(btVector3(0, 0, 0));
            startTransform.setRotation(btQuaternion(0, 0, 0, 1));
        } else {
            // For dynamic objects, use entity transform
            glm::mat4 transform = getOwner()->getLocalToWorldMatrix();
            glm::vec3 position = glm::vec3(transform[3]) + centerOffset;
            
            // Extract rotation quaternion from transform matrix
            glm::quat rotation = glm::quat_cast(transform);
            
            startTransform.setOrigin(glmToBullet(position));
            startTransform.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));
        }

        motionState = new btDefaultMotionState(startTransform);

        // Calculate local inertia
        btVector3 localInertia(0, 0, 0);
        if (mass > 0.0f && !isTrigger) {
            collisionShape->calculateLocalInertia(mass, localInertia);
        }

        // Create rigid body
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, collisionShape, localInertia);
        rbInfo.m_friction = friction;
        rbInfo.m_restitution = restitution;

        rigidBody = new btRigidBody(rbInfo);

        // For character controllers (non-zero mass), lock rotation and disable gravity
        if (mass > 0.0f && !isTrigger) {
            rigidBody->setAngularFactor(btVector3(0, 0, 0)); // No rotation
            rigidBody->setGravity(btVector3(0, 0, 0)); // No gravity
            rigidBody->setActivationState(DISABLE_DEACTIVATION); // Always active
            rigidBody->setLinearFactor(btVector3(1, 1, 1)); // Can move in all directions
            rigidBody->setFriction(1.0f); // High friction to prevent sliding
        }

        // Configure as trigger if needed
        if (isTrigger) {
            rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | 
                                        btCollisionObject::CF_NO_CONTACT_RESPONSE);
        }

        // Add to world
        if (world) {
            world->addRigidBody(rigidBody);
        }

        // Store pointer to this component in user data for collision callbacks
        rigidBody->setUserPointer(this);
    }

    void BulletColliderComponent::syncToEntity() {
        if (!rigidBody || !getOwner() || mass == 0.0f) return;

        btTransform trans;
        rigidBody->getMotionState()->getWorldTransform(trans);

        // Get position from Bullet
        btVector3 pos = trans.getOrigin();

        // Convert to glm
        glm::vec3 position = bulletToGlm(pos) - centerOffset;

        // Update entity position only (rotation is controlled by mouse input)
        getOwner()->localTransform.position = position;
    }

    void BulletColliderComponent::syncFromEntity() {
        if (!rigidBody || !getOwner()) return;

        glm::mat4 transform = getOwner()->getLocalToWorldMatrix();
        glm::vec3 position = glm::vec3(transform[3]) + centerOffset;
        glm::quat rotation = glm::quat_cast(transform);

        btTransform trans;
        trans.setOrigin(glmToBullet(position));
        trans.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));

        // For dynamic objects, move directly (Bullet will handle collision response)
        if (mass > 0.0f) {
            rigidBody->setWorldTransform(trans);
            rigidBody->getMotionState()->setWorldTransform(trans);
            rigidBody->setLinearVelocity(btVector3(0, 0, 0)); // Clear velocity
            rigidBody->activate();
        } else {
            // Static objects
            rigidBody->setWorldTransform(trans);
            if (motionState) {
                motionState->setWorldTransform(trans);
            }
        }
    }



}
