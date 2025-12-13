#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cmath>

namespace our {

class NavGrid2D {
private:
    float cellSize;
    glm::vec2 origin;  // Bottom-left corner (world space)
    glm::ivec2 gridSize; // Width x Depth in cells
    std::vector<bool> walkable; // Flat array: walkable[z * gridSize.x + x]
    float floorY; // Y-coordinate of this floor

public:
    NavGrid2D() : cellSize(0.5f), origin(0, 0), gridSize(0, 0), floorY(0.0f) {}
    
    NavGrid2D(float cellSize, glm::vec2 origin, glm::ivec2 gridSize, float floorY)
        : cellSize(cellSize), origin(origin), gridSize(gridSize), floorY(floorY) {
        walkable.resize(gridSize.x * gridSize.y, true); // All walkable by default
    }
    
    // Check if grid cell is walkable
    bool isWalkable(int x, int z) const {
        if (x < 0 || x >= gridSize.x || z < 0 || z >= gridSize.y) {
            return false; // Out of bounds
        }
        return walkable[z * gridSize.x + x];
    }
    
    // Convert world position to grid coordinates
    glm::ivec2 worldToGrid(glm::vec3 worldPos) const {
        int x = static_cast<int>(std::floor((worldPos.x - origin.x) / cellSize));
        int z = static_cast<int>(std::floor((worldPos.z - origin.y) / cellSize));
        return glm::ivec2(x, z);
    }
    
    // Convert grid coordinates to world position (center of cell)
    glm::vec3 gridToWorld(glm::ivec2 gridPos) const {
        float x = origin.x + (gridPos.x + 0.5f) * cellSize;
        float z = origin.y + (gridPos.y + 0.5f) * cellSize;
        return glm::vec3(x, floorY, z);
    }
    
    // Get walkable neighbors (4-directional)
    std::vector<glm::ivec2> getNeighbors(glm::ivec2 cell) const {
        std::vector<glm::ivec2> neighbors;
        neighbors.reserve(4);
        
        const glm::ivec2 directions[4] = {
            {0, 1},   // North
            {1, 0},   // East
            {0, -1},  // South
            {-1, 0}   // West
        };
        
        for (const auto& dir : directions) {
            glm::ivec2 neighbor = cell + dir;
            if (isWalkable(neighbor.x, neighbor.y)) {
                neighbors.push_back(neighbor);
            }
        }
        
        return neighbors;
    }
    
    // Mark a rectangular region as blocked
    void setObstacle(glm::ivec2 min, glm::ivec2 max) {
        for (int z = min.y; z <= max.y && z < gridSize.y; ++z) {
            for (int x = min.x; x <= max.x && x < gridSize.x; ++x) {
                if (x >= 0 && z >= 0) {
                    walkable[z * gridSize.x + x] = false;
                }
            }
        }
    }
    
    // Getters
    float getCellSize() const { return cellSize; }
    glm::vec2 getOrigin() const { return origin; }
    glm::ivec2 getGridSize() const { return gridSize; }
    float getFloorY() const { return floorY; }
};

} // namespace our
