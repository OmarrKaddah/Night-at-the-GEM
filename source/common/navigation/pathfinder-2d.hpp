#pragma once

#include "nav-grid-2d.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <algorithm>

namespace our {

// Hash function for glm::ivec2 (needed for unordered_set/map)
struct IVec2Hash {
    std::size_t operator()(const glm::ivec2& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1);
    }
};

struct PathNode {
    glm::ivec2 cell;
    float g; // Cost from start
    float h; // Heuristic to goal
    float f; // g + h
    glm::ivec2 parent;
    
    PathNode() : cell(0, 0), g(0), h(0), f(0), parent(-1, -1) {}
    PathNode(glm::ivec2 cell, float g, float h, glm::ivec2 parent)
        : cell(cell), g(g), h(h), f(g + h), parent(parent) {}
};

// Comparator for priority queue (min-heap by f-score)
struct PathNodeCompare {
    bool operator()(const PathNode& a, const PathNode& b) const {
        return a.f > b.f; // Greater-than for min-heap
    }
};

class Pathfinder2D {
public:
    // Find path from start to goal using A*
    std::vector<glm::vec3> findPath(glm::vec3 start, glm::vec3 goal, const NavGrid2D* grid) {
        if (!grid) return {};
        
        glm::ivec2 startCell = grid->worldToGrid(start);
        glm::ivec2 goalCell = grid->worldToGrid(goal);
        
        glm::ivec2 gridSize = grid->getGridSize();

        // Clamp to grid bounds to handle out-of-bounds positions gracefully
        auto clampToGrid = [&](glm::ivec2 p) {
            p.x = std::max(0, std::min(p.x, gridSize.x - 1));
            p.y = std::max(0, std::min(p.y, gridSize.y - 1));
            return p;
        };

        startCell = clampToGrid(startCell);
        goalCell = clampToGrid(goalCell);
        
        // Snap to nearest walkable cell if needed (e.g. if clamped position is an obstacle)
        if (!grid->isWalkable(startCell.x, startCell.y)) {
            startCell = findNearestWalkable(startCell, grid, 5);
        }
        if (!grid->isWalkable(goalCell.x, goalCell.y)) {
            goalCell = findNearestWalkable(goalCell, grid, 5);
        }

        // Check if start/goal are walkable (after snapping)
        if (!grid->isWalkable(startCell.x, startCell.y) || 
            !grid->isWalkable(goalCell.x, goalCell.y)) {
            return {};
        }
        
        // A* data structures
        std::priority_queue<PathNode, std::vector<PathNode>, PathNodeCompare> openList;
        std::unordered_set<glm::ivec2, IVec2Hash> closedSet;
        std::unordered_map<glm::ivec2, PathNode, IVec2Hash> nodeMap;
        
        // Add start node
        PathNode startNode(startCell, 0.0f, heuristic(startCell, goalCell), glm::ivec2(-1, -1));
        openList.push(startNode);
        nodeMap[startCell] = startNode;
        
        // A* loop
        while (!openList.empty()) {
            PathNode current = openList.top();
            openList.pop();
            
            // Skip if already processed
            if (closedSet.count(current.cell)) continue;
            
            // Goal reached
            if (current.cell == goalCell) {
                return reconstructPath(nodeMap, current.cell, grid);
            }
            
            closedSet.insert(current.cell);
            
            // Explore neighbors
            auto neighbors = grid->getNeighbors(current.cell);
            for (const auto& neighborCell : neighbors) {
                if (closedSet.count(neighborCell)) continue;
                
                float tentativeG = current.g + 1.0f; // Cost = 1 per cell
                
                // Check if this path is better
                if (!nodeMap.count(neighborCell) || tentativeG < nodeMap[neighborCell].g) {
                    PathNode neighborNode(
                        neighborCell,
                        tentativeG,
                        heuristic(neighborCell, goalCell),
                        current.cell
                    );
                    nodeMap[neighborCell] = neighborNode;
                    openList.push(neighborNode);
                }
            }
        }
        
        // No path found
        return {};
    }

private:
    // Manhattan distance heuristic
    float heuristic(glm::ivec2 a, glm::ivec2 b) const {
        return static_cast<float>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
    }
    
    // BFS to find nearest walkable cell within radius
    glm::ivec2 findNearestWalkable(glm::ivec2 start, const NavGrid2D* grid, int maxRadius) const {
        if (grid->isWalkable(start.x, start.y)) return start;

        std::queue<glm::ivec2> q;
        q.push(start);
        std::unordered_set<glm::ivec2, IVec2Hash> visited;
        visited.insert(start);

        while (!q.empty()) {
            glm::ivec2 current = q.front();
            q.pop();

            if (grid->isWalkable(current.x, current.y)) return current;

            // Stop if too far from original search center
            // (Using Manhattan distance for simplicity in check)
            if (std::abs(current.x - start.x) > maxRadius || std::abs(current.y - start.y) > maxRadius) continue;

            // Neighbors (4-way)
            glm::ivec2 dirs[] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
            for (const auto& d : dirs) {
                glm::ivec2 next = current + d;
                if (visited.find(next) == visited.end()) {
                    visited.insert(next);
                    q.push(next);
                }
            }
        }
        return start; // Failed, return original
    }

    // Reconstruct path from goal to start
    std::vector<glm::vec3> reconstructPath(
        const std::unordered_map<glm::ivec2, PathNode, IVec2Hash>& nodeMap,
        glm::ivec2 goalCell,
        const NavGrid2D* grid
    ) const {
        std::vector<glm::vec3> path;
        glm::ivec2 current = goalCell;
        
        while (nodeMap.count(current) && nodeMap.at(current).parent != glm::ivec2(-1, -1)) {
            path.push_back(grid->gridToWorld(current));
            current = nodeMap.at(current).parent;
        }
        
        // Add start position
        if (nodeMap.count(current)) {
            path.push_back(grid->gridToWorld(current));
        }
        
        // Reverse to get start -> goal
        std::reverse(path.begin(), path.end());
        return path;
    }
};

} // namespace our
