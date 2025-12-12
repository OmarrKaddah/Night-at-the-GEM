#include "mesh-loader.hpp"
#include "../animation/animation-loader.hpp"
#include "../texture/texture-utils.hpp"
#include "../asset-loader.hpp"
#include "../material/material.hpp"
#include "../shader/shader.hpp"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include <iostream>
#include <map>

namespace our {

    namespace mesh_utils {

        // ... helpers ...
        #include "bone_matcher.inl"

        Mesh* loadAnimatedMesh(const std::string& filename, std::shared_ptr<Skeleton>& outSkeleton) {
            std::cout << "========================================" << std::endl;
            std::cout << "Animated Mesh Loader: Starting to load " << filename << std::endl;
            
            Assimp::Importer importer;
            
            const aiScene* scene = importer.ReadFile(filename,
                aiProcess_Triangulate |
            // aiProcess_FlipUVs | // Removed as it causes distortion on Zombies
            aiProcess_LimitBoneWeights |
                aiProcess_GlobalScale
            );

            if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode || scene->mNumMeshes == 0) {
                std::cerr << "Failed to load mesh: " << importer.GetErrorString() << std::endl;
                std::cerr << "Scene: " << (scene ? "valid" : "null") << std::endl;
                if (scene) {
                    std::cerr << "Flags incomplete: " << (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) << std::endl;
                    std::cerr << "Root node: " << (scene->mRootNode ? "valid" : "null") << std::endl;
                    std::cerr << "Num meshes: " << scene->mNumMeshes << std::endl;
                }
                return nullptr;
            }

            std::cout << "Scene loaded successfully!" << std::endl;
            std::cout << "  Meshes: " << scene->mNumMeshes << std::endl;
            std::cout << "  Materials: " << scene->mNumMaterials << std::endl;
            std::cout << "  Animations: " << scene->mNumAnimations << std::endl;
            std::cout << "  Textures: " << scene->mNumTextures << std::endl;

            // Load skeleton first
            try {
                std::cout << "Loading skeleton..." << std::endl;
                outSkeleton = AnimationLoader::loadSkeleton(filename);
                std::cout << "Skeleton loaded successfully with " << outSkeleton->bones.size() << " bones" << std::endl;
                std::cout << "Now processing mesh geometry..." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to load skeleton: " << e.what() << std::endl;
                std::cerr << "Will try to load mesh without skeleton..." << std::endl;
                outSkeleton = nullptr;
            }

            // We'll combine all meshes into one
            std::vector<Vertex> vertices;
            std::vector<unsigned int> elements;

            // Preprocess materials: if they reference embedded textures, load them and create simple materials
            std::vector<std::string> materialNames(scene->mNumMaterials);
            for (unsigned int mi = 0; mi < scene->mNumMaterials; ++mi) {
                aiMaterial* aMat = scene->mMaterials[mi];
                aiString path;
                std::string chosenMatName;
                if (aMat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
                    std::string str(path.C_Str());
                    if (!str.empty() && str[0] == '*') {
                        // Embedded texture reference, format is "*N"
                        int texIndex = std::atoi(str.c_str() + 1);
                        if (texIndex >= 0 && texIndex < (int)scene->mNumTextures) {
                            aiTexture* aTex = scene->mTextures[texIndex];
                            our::Texture2D* tex = nullptr;
                            if (aTex->mHeight == 0) {
                                // Compressed texture (e.g., PNG/JPEG) stored in aTex->pcData with size mWidth
                                tex = our::texture_utils::loadImageFromMemory((const unsigned char*)aTex->pcData, (int)aTex->mWidth, true);
                            } else {
                                // Raw texel data not commonly used for GLB; skip for now
                            }
                            if (tex) {
                                // Register texture into asset loader under a generated name
                                chosenMatName = std::string("embedded_tex_") + std::to_string(mi);
                                AssetLoader<Texture2D>::set(chosenMatName, tex);
                                // Create a simple textured material using the loaded texture
                                TexturedMaterial* mat = new TexturedMaterial();
                                // Pick skinned shader if we have a skeleton, otherwise textured
                                if (outSkeleton) mat->shader = AssetLoader<ShaderProgram>::get("skinned");
                                if (!mat->shader) mat->shader = AssetLoader<ShaderProgram>::get("textured");
                                mat->texture = tex;
                                mat->alphaThreshold = 0.0f; // Initialize to avoid garbage
                                mat->pipelineState.faceCulling.enabled = false; // Ensure double-sided rendering
                                AssetLoader<Material>::set(std::string("embedded_mat_") + std::to_string(mi), mat);
                                materialNames[mi] = std::string("embedded_mat_") + std::to_string(mi);
                                continue;
                            }
                        }
                    } else {
                        // External texture path: try to load and register it under a generated name
                        std::string texPath = str;
                        our::Texture2D* tex = our::texture_utils::loadImage(texPath);
                        if (tex) {
                            chosenMatName = std::string("external_tex_") + std::to_string(mi);
                            AssetLoader<Texture2D>::set(chosenMatName, tex);
                            TexturedMaterial* mat = new TexturedMaterial();
                            if (outSkeleton) mat->shader = AssetLoader<ShaderProgram>::get("skinned");
                            if (!mat->shader) mat->shader = AssetLoader<ShaderProgram>::get("textured");
                            mat->texture = tex;
                            mat->alphaThreshold = 0.0f; // Initialize to avoid garbage
                            mat->pipelineState.faceCulling.enabled = false; // Ensure double-sided rendering
                            AssetLoader<Material>::set(std::string("external_mat_") + std::to_string(mi), mat);
                            materialNames[mi] = std::string("external_mat_") + std::to_string(mi);
                            continue;
                        }
                    }
                }
                // Fallback: leave empty so renderer uses entity material
                materialNames[mi] = "";
            }

            std::vector<Mesh::Submesh> localSubmeshes;
            for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
                aiMesh* mesh = scene->mMeshes[m];
                
                unsigned int vertexOffset = vertices.size();
                size_t elementOffsetBefore = elements.size();

                // Create a map to accumulate bone weights per vertex
                std::map<unsigned int, std::vector<std::pair<int, float>>> vertexBoneData;

                // First pass: collect bone weights
                for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
                    aiBone* bone = mesh->mBones[b];
                    std::string boneName = bone->mName.C_Str();
                    
                    int boneId = outSkeleton->getBoneId(boneName);
                    if (boneId == -1) {
                         // Fallback: try heuristic matching
                         std::string matchedName;
                         if (tryFindBoneId(outSkeleton, boneName, boneId, matchedName)) {
                             // Success!
                         } else {
                             continue; // Still not found
                         }
                    }

                    for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
                        unsigned int vertexId = bone->mWeights[w].mVertexId;
                        float weight = bone->mWeights[w].mWeight;
                        vertexBoneData[vertexId].push_back({boneId, weight});
                    }
                }

                // Second pass: create vertices with bone data
                for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
                    Vertex vertex;
                    
                    // Position
                    vertex.position = glm::vec3(
                        mesh->mVertices[v].x,
                        mesh->mVertices[v].y,
                        mesh->mVertices[v].z
                    );

                    // Normal
                    if (mesh->HasNormals()) {
                        vertex.normal = glm::vec3(
                            mesh->mNormals[v].x,
                            mesh->mNormals[v].y,
                            mesh->mNormals[v].z
                        );
                    } else {
                        vertex.normal = glm::vec3(0, 1, 0);
                    }

                    // Texture coordinates
                    if (mesh->HasTextureCoords(0)) {
                        vertex.tex_coord = glm::vec2(
                            mesh->mTextureCoords[0][v].x,
                            mesh->mTextureCoords[0][v].y
                        );
                    } else {
                        vertex.tex_coord = glm::vec2(0, 0);
                    }

                    // Color (default white)
                    vertex.color = Color(255, 255, 255, 255);

                    // Bone data
                    if (vertexBoneData.find(v) != vertexBoneData.end()) {
                        auto& boneList = vertexBoneData[v];
                        
                        // Sort by weight (descending) and take top 4
                        std::sort(boneList.begin(), boneList.end(),
                            [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
                                return a.second > b.second;
                            });

                        // Normalize weights and assign to vertex
                        float totalWeight = 0.0f;
                        for (size_t i = 0; i < std::min(boneList.size(), size_t(4)); ++i) {
                            totalWeight += boneList[i].second;
                        }

                        // Prevent division by zero
                        if (totalWeight > 0.0f) {
                            for (size_t i = 0; i < 4; ++i) {
                                if (i < boneList.size() && i < 4) {
                                    vertex.boneIDs[i] = boneList[i].first;
                                    vertex.boneWeights[i] = boneList[i].second / totalWeight;
                                } else {
                                    vertex.boneIDs[i] = -1;
                                    vertex.boneWeights[i] = 0.0f;
                                }
                            }
                        }
                    }

                    vertices.push_back(vertex);
                }

                // Indices
                for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                    aiFace& face = mesh->mFaces[f];
                    for (unsigned int i = 0; i < face.mNumIndices; ++i) {
                        elements.push_back(vertexOffset + face.mIndices[i]);
                    }
                }
                // Create submesh entry for this aiMesh
                Mesh::Submesh sub;
                sub.offset = static_cast<GLuint>(elementOffsetBefore);
                sub.count = static_cast<GLuint>(elements.size() - elementOffsetBefore);
                // Use generated material name if available
                aiMaterial* aMat = scene->mMaterials[mesh->mMaterialIndex];
                if (mesh->mMaterialIndex < (int)materialNames.size() && !materialNames[mesh->mMaterialIndex].empty()) {
                    sub.materialName = materialNames[mesh->mMaterialIndex];
                } else if (aMat) {
                    // Try to use the material name if provided by Assimp
                    aiString mname;
                    if (aMat->Get(AI_MATKEY_NAME, mname) == AI_SUCCESS) {
                        sub.materialName = std::string(mname.C_Str());
                    } else {
                        sub.materialName = "";
                    }
                } else {
                    sub.materialName = "";
                }
                localSubmeshes.push_back(sub);
            }

            std::cout << "Mesh processing complete: " << vertices.size() 
                      << " vertices, " << elements.size() / 3 << " triangles";
            if (outSkeleton) {
                std::cout << ", " << outSkeleton->bones.size() << " bones";
            }
            std::cout << std::endl;

            std::cout << "Creating Mesh object..." << std::endl;
            Mesh* result = new Mesh(vertices, elements);
            // assign submeshes collected earlier
            result->submeshes = std::move(localSubmeshes);
            std::cout << "Mesh created successfully!" << std::endl;
            std::cout << "========================================" << std::endl;
            
            return result;
        }

    } // namespace mesh_utils

} // namespace our
