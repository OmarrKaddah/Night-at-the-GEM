#include "mesh-utils.hpp"

// We will use "Tiny OBJ Loader" to read and process '.obj" files
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobj/tiny_obj_loader.h>

#include <iostream>
#include <vector>
#include <unordered_map>

our::Mesh* our::mesh_utils::loadOBJ(const std::string& filename) {

    // --- OUTPUT DATA ---
    std::vector<our::Vertex> vertices;
    std::vector<GLuint> elements;
    std::unordered_map<our::Vertex, GLuint> vertex_map;

    // --- TINYOBJ DATA ---
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // --- BASE PATH ---
    std::string obj_path = filename;
    std::string basepath = "";
    size_t pos = obj_path.find_last_of("/\\");
    if (pos != std::string::npos) basepath = obj_path.substr(0, pos + 1);

    // --- LOAD OBJ ---
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
        obj_path.c_str(), basepath.c_str())) {
        std::cerr << "Failed to load obj file: " << err << std::endl;
        return nullptr;
    }
    if (!warn.empty()) std::cout << "WARN: " << warn << std::endl;

    // Map: material_id -> list of indices
    std::unordered_map<int, std::vector<GLuint>> perMaterialIndices;
    std::unordered_map<int, std::string> materialNames;

    // --- PROCESS SHAPES ---
    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        const auto& mesh = shape.mesh;

        for (size_t f = 0; f < mesh.num_face_vertices.size(); ++f) {
            int fv = mesh.num_face_vertices[f];
            int mat_id = mesh.material_ids.size() > f ? mesh.material_ids[f] : -1;

            std::string matName = "default";
            if (mat_id >= 0 && mat_id < (int)materials.size()) {
                matName = materials[mat_id].name;
            }
            materialNames[mat_id] = matName;

            for (int v = 0; v < fv; v++) {
                tinyobj::index_t idx = mesh.indices[index_offset + v];
                our::Vertex vertex = {};

                // POSITION
                vertex.position = {
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                };

                // NORMAL
                if (idx.normal_index >= 0) {
                    vertex.normal = {
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2]
                    };
                }
                else {
                    vertex.normal = { 0, 0, 0 };
                }

                // UV
                if (idx.texcoord_index >= 0) {
                    vertex.tex_coord = {
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        attrib.texcoords[2 * idx.texcoord_index + 1]
                    };
                }
                else {
                    vertex.tex_coord = { 0, 0 };
                }

                // COLOR
                if (!attrib.colors.empty()) {
                    vertex.color = {
                        (unsigned char)(attrib.colors[3 * idx.vertex_index + 0] * 255),
                        (unsigned char)(attrib.colors[3 * idx.vertex_index + 1] * 255),
                        (unsigned char)(attrib.colors[3 * idx.vertex_index + 2] * 255),
                        255
                    };
                }
                else {
                    vertex.color = our::Color(255, 255, 255, 255);
                }

                // DEDUPLICATE
                auto it = vertex_map.find(vertex);
                GLuint idx_final;
                if (it == vertex_map.end()) {
                    idx_final = vertices.size();
                    vertex_map[vertex] = idx_final;
                    vertices.push_back(vertex);
                }
                else {
                    idx_final = it->second;
                }

                // ADD INDEX TO THIS MATERIAL
                perMaterialIndices[mat_id].push_back(idx_final);
            }

            index_offset += fv; // advance index
        }
    }

    // --- BUILD FINAL EBO + SUBMESHES ---
    our::Mesh* mesh = new our::Mesh(vertices, {}); // temporary EBO until we fill it
    mesh->submeshes.clear();
    elements.clear();

    for (auto& kv : perMaterialIndices) {
        int mat_id = kv.first;
        auto& idxList = kv.second;

        if (idxList.empty()) continue;

        our::Mesh::Submesh sub;
        sub.offset = elements.size();
        sub.count = idxList.size();
        sub.materialName = materialNames[mat_id];

        // Append indices to the EBO buffer
        elements.insert(elements.end(), idxList.begin(), idxList.end());

        mesh->submeshes.push_back(sub);
    }

    // Now replace mesh EBO with the real full EBO
    glBindVertexArray(mesh->getVAO()); // <— protected, so MODIFY Mesh class or add a method (see below)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->getEBO());
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(GLuint),
        elements.data(), GL_STATIC_DRAW);

    mesh->getElementCount() = elements.size();
    glBindVertexArray(0);

    return mesh;
}


// Create a sphere (the vertex order in the triangles are CCW from the outside)
// Segments define the number of divisions on the both the latitude and the longitude
our::Mesh* our::mesh_utils::sphere(const glm::ivec2& segments){
    std::vector<our::Vertex> vertices;
    std::vector<GLuint> elements;

    // We populate the sphere vertices by looping over its longitude and latitude
    for(int lat = 0; lat <= segments.y; lat++){
        float v = (float)lat / segments.y;
        float pitch = v * glm::pi<float>() - glm::half_pi<float>();
        float cos = glm::cos(pitch), sin = glm::sin(pitch);
        for(int lng = 0; lng <= segments.x; lng++){
            float u = (float)lng/segments.x;
            float yaw = u * glm::two_pi<float>();
            glm::vec3 normal = {cos * glm::cos(yaw), sin, cos * glm::sin(yaw)};
            glm::vec3 position = normal;
            glm::vec2 tex_coords = glm::vec2(u, v);
            our::Color color = our::Color(255, 255, 255, 255);
            vertices.push_back({position, color, tex_coords, normal});
        }
    }

    for(int lat = 1; lat <= segments.y; lat++){
        int start = lat*(segments.x+1);
        for(int lng = 1; lng <= segments.x; lng++){
            int prev_lng = lng-1;
            elements.push_back(lng + start);
            elements.push_back(lng + start - segments.x - 1);
            elements.push_back(prev_lng + start - segments.x - 1);
            elements.push_back(prev_lng + start - segments.x - 1);
            elements.push_back(prev_lng + start);
            elements.push_back(lng + start);
        }
    }

    return new our::Mesh(vertices, elements);
}