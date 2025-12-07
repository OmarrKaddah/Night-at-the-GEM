#pragma once

#include <glad/gl.h>
#include "vertex.hpp"
#include <string>
#include <vector>

namespace our {

#define ATTRIB_LOC_POSITION 0
#define ATTRIB_LOC_COLOR    1
#define ATTRIB_LOC_TEXCOORD 2
#define ATTRIB_LOC_NORMAL   3

    class Mesh {
        unsigned int VBO, EBO;
        unsigned int VAO;
        GLsizei elementCount;

    public:
        // Store mesh data for physics collision
        std::vector<Vertex> vertices;
        std::vector<unsigned int> elements;

        struct Submesh {
            GLuint offset;            // starting index in EBO
            GLuint count;             // number of indices in this submesh
            std::string materialName; // name taken from MTL (newmtl)
        };

        // Only ONE declaration
        std::vector<Submesh> submeshes;

        unsigned int getVAO() const { return VAO; }
        unsigned int getEBO() const { return EBO; }
        GLsizei& getElementCount() { return elementCount; }

        Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& elements)
            : vertices(vertices), elements(elements)
        {
            elementCount = static_cast<GLsizei>(elements.size());
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(unsigned int), elements.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(ATTRIB_LOC_POSITION);
            glVertexAttribPointer(ATTRIB_LOC_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

            glEnableVertexAttribArray(ATTRIB_LOC_COLOR);
            glVertexAttribPointer(ATTRIB_LOC_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));

            glEnableVertexAttribArray(ATTRIB_LOC_TEXCOORD);
            glVertexAttribPointer(ATTRIB_LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tex_coord));

            glEnableVertexAttribArray(ATTRIB_LOC_NORMAL);
            glVertexAttribPointer(ATTRIB_LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

            glBindVertexArray(0);
        }

        void draw()
        {
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, elementCount, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }

        ~Mesh() {
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
        }

        Mesh(Mesh const&) = delete;
        Mesh& operator=(Mesh const&) = delete;
    };

}
