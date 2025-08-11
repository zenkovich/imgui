#include "cg_Physics.h"

#include <cmath>
#include <algorithm>

namespace cg
{
    Physics::Physics(const Config& configRef, Graph& graphRef)
        : _config(configRef), _graph(graphRef)
    {
        // Initialize positions in a stable spiral distribution.
        for (size_t nodeIndex = 0; nodeIndex < _graph.nodes.size(); ++nodeIndex)
        {
            float angle = static_cast<float>(nodeIndex) * 0.618f; // golden angle-ish
            float radius = 5.0f * std::sqrt(static_cast<float>(nodeIndex));

            float posX = radius * std::cos(angle);
            float posY = radius * std::sin(angle);

            _graph.nodes[nodeIndex].px = _graph.nodes[nodeIndex].ppx = posX;
            _graph.nodes[nodeIndex].py = _graph.nodes[nodeIndex].ppy = posY;
        }

        InitializeDirectoryMasses();
    }

    void Physics::Step()
    {
        Integrate();

        for (int iterationIndex = 0; iterationIndex < _config.physics.solver_iterations; ++iterationIndex)
        {
            SatisfyConstraints();
            ApplyRepulsion();
        }
    }

    void Physics::IntegrateOnly() { Integrate(); }
    void Physics::ConstraintsOnly() { SatisfyConstraints(); }
    void Physics::RepulsionOnly() { ApplyRepulsion(); }
    void Physics::AngleEqualizationOnly() { EqualizeChildAngles(); }

    void Physics::Integrate()
    {
        const float timeStep = static_cast<float>(_config.physics.time_step);
        const float velocityDamping = static_cast<float>(1.0 - _config.physics.damping);
        const float maxAllowedDisplacement = static_cast<float>(_config.physics.max_displacement);

        for (auto& node : _graph.nodes)
        {
            if (node.fixed)
                continue;

            float velocityX = (node.px - node.ppx) * velocityDamping;
            float velocityY = (node.py - node.ppy) * velocityDamping;

            if (velocityX > maxAllowedDisplacement) velocityX = maxAllowedDisplacement;
            if (velocityX < -maxAllowedDisplacement) velocityX = -maxAllowedDisplacement;
            if (velocityY > maxAllowedDisplacement) velocityY = maxAllowedDisplacement;
            if (velocityY < -maxAllowedDisplacement) velocityY = -maxAllowedDisplacement;

            float nextX = node.px + velocityX;
            float nextY = node.py + velocityY;

            node.ppx = node.px;
            node.ppy = node.py;
            node.px = nextX;
            node.py = nextY;
        }

        (void)timeStep; // Verlet integration doesn't directly use dt here.
    }

    void Physics::SatisfyConstraints()
    {
        for (const auto& link : _graph.links)
        {
            auto& nodeA = _graph.nodes[link.a];
            auto& nodeB = _graph.nodes[link.b];

            const bool nodeAIsDirectory = (nodeA.kind == NodeKind::Directory);
            const bool nodeBIsDirectory = (nodeB.kind == NodeKind::Directory);
            const bool isDirectoryToDirectory = nodeAIsDirectory && nodeBIsDirectory;
            const bool isDirectoryToFile = (nodeAIsDirectory != nodeBIsDirectory);
            const bool isFileToFile = !nodeAIsDirectory && !nodeBIsDirectory; // include link

            if (isDirectoryToDirectory || isDirectoryToFile)
            {
                if (!_config.graph.enable_directory_links)
                    continue;
            }
            else if (isFileToFile)
            {
                if (!_config.graph.enable_include_links)
                    continue;
            }

            float desiredRestLength = link.restLength;
            if (isDirectoryToDirectory)
            {
                // Base depends on sum of subtree sizes; +1 to avoid zero
                int sizeA = nodeA.subtreeDirCount + 1;
                int sizeB = nodeB.subtreeDirCount + 1;
                float base = 40.0f + static_cast<float>(sizeA + sizeB) * 2.0f;
                desiredRestLength = base * _config.graph.dir_dir_length_coef;
            }
            else if (isDirectoryToFile)
            {
                // Base depends on number of children in directory
                int dirIdx = nodeA.kind == NodeKind::Directory ? link.a : link.b;
                const auto& dirNode = _graph.nodes[dirIdx];
                float base = 30.0f + static_cast<float>(dirNode.children.size()) * 1.0f;
                desiredRestLength = base * _config.graph.dir_file_length_coef;
            }
            else // include link
            {
                desiredRestLength = static_cast<float>(_config.physics.link_rest_length);
            }

            float deltaX = nodeB.px - nodeA.px;
            float deltaY = nodeB.py - nodeA.py;

            float distanceSquared = deltaX * deltaX + deltaY * deltaY + 1e-6f;
            float currentDistance = std::sqrt(distanceSquared);

            float distanceError = (currentDistance - desiredRestLength) / currentDistance;
            float correctionFactor = link.stiffness * 0.5f;

            float offsetX = deltaX * distanceError * correctionFactor;
            float offsetY = deltaY * distanceError * correctionFactor;

            // Mass-aware distribution: heavier nodes move less.
            float massA = std::max(0.0001f, nodeA.mass);
            float massB = std::max(0.0001f, nodeB.mass);
            float invA = 1.0f / massA;
            float invB = 1.0f / massB;
            float invSum = invA + invB;
            float wA = (invSum > 0.0f) ? (invA / invSum) : 0.5f;
            float wB = (invSum > 0.0f) ? (invB / invSum) : 0.5f;

            if (!nodeA.fixed)
            {
                nodeA.px += offsetX * wA;
                nodeA.py += offsetY * wA;
            }

            if (!nodeB.fixed)
            {
                nodeB.px -= offsetX * wB;
                nodeB.py -= offsetY * wB;
            }
        }
    }

    void Physics::ApplyRepulsion()
    {
        // Apply near-neighbor repulsion only between directory nodes
        const float repulsionRadius = static_cast<float>(_config.physics.repulsion_radius);
        const float repulsionStrength = static_cast<float>(_config.physics.repulsion_strength);
        const float repulsionRadiusSquared = repulsionRadius * repulsionRadius;
        const float cellSize = repulsionRadius;

        // Compute bounds only over directories
        float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
        int numDirectories = 0;
        for (const auto& node : _graph.nodes)
        {
            if (node.kind != NodeKind::Directory)
                continue;
            numDirectories++;
            if (node.px < minX) minX = node.px;
            if (node.py < minY) minY = node.py;
            if (node.px > maxX) maxX = node.px;
            if (node.py > maxY) maxY = node.py;
        }
        if (numDirectories <= 1)
            return;

        int gridSizeX = std::max(1, static_cast<int>((maxX - minX) / cellSize) + 1);
        int gridSizeY = std::max(1, static_cast<int>((maxY - minY) / cellSize) + 1);

        std::vector<std::vector<int>> grid(static_cast<size_t>(gridSizeX) * static_cast<size_t>(gridSizeY));

        auto get_cell_index = [&](float x, float y) -> int {
            int cellX = static_cast<int>((x - minX) / cellSize);
            if (cellX < 0) cellX = 0;
            if (cellX >= gridSizeX) cellX = gridSizeX - 1;

            int cellY = static_cast<int>((y - minY) / cellSize);
            if (cellY < 0) cellY = 0;
            if (cellY >= gridSizeY) cellY = gridSizeY - 1;

            return cellY * gridSizeX + cellX;
        };

        for (int nodeIndex = 0; nodeIndex < static_cast<int>(_graph.nodes.size()); ++nodeIndex)
        {
            const auto& node = _graph.nodes[static_cast<size_t>(nodeIndex)];
            if (node.kind != NodeKind::Directory)
                continue;
            int index = get_cell_index(node.px, node.py);
            grid[static_cast<size_t>(index)].push_back(nodeIndex);
        }

        auto process_pair = [&](int indexA, int indexB) {
            auto& nodeA = _graph.nodes[static_cast<size_t>(indexA)];
            auto& nodeB = _graph.nodes[static_cast<size_t>(indexB)];

            // Safety: ensure we only repel directories
            if (nodeA.kind != NodeKind::Directory || nodeB.kind != NodeKind::Directory)
            {
                return;
            }

            float deltaX = nodeB.px - nodeA.px;
            float deltaY = nodeB.py - nodeA.py;

            float distanceSquared = deltaX * deltaX + deltaY * deltaY + 1e-6f;
            if (distanceSquared > repulsionRadiusSquared)
                return;

            float distance = std::sqrt(distanceSquared);
            float inverseDistance = 1.0f / distance;

            float force = repulsionStrength * (1.0f - distance / repulsionRadius);
            float offsetX = deltaX * inverseDistance * force * 0.5f;
            float offsetY = deltaY * inverseDistance * force * 0.5f;

            if (!nodeA.fixed)
            {
                nodeA.px -= offsetX;
                nodeA.py -= offsetY;
            }

            if (!nodeB.fixed)
            {
                nodeB.px += offsetX;
                nodeB.py += offsetY;
            }
        };

        for (int gridY = 0; gridY < gridSizeY; ++gridY)
        {
            for (int gridX = 0; gridX < gridSizeX; ++gridX)
            {
                int selfIndex = gridY * gridSizeX + gridX;

                const auto& cellNodes = grid[static_cast<size_t>(selfIndex)];

                for (size_t aIndex = 0; aIndex < cellNodes.size(); ++aIndex)
                {
                    for (size_t bIndex = aIndex + 1; bIndex < cellNodes.size(); ++bIndex)
                    {
                        process_pair(cellNodes[aIndex], cellNodes[bIndex]);
                    }
                }

                for (int offsetY = -1; offsetY <= 1; ++offsetY)
                {
                    for (int offsetX = -1; offsetX <= 1; ++offsetX)
                    {
                        if (offsetX == 0 && offsetY == 0)
                            continue;

                        int neighborX = gridX + offsetX;
                        int neighborY = gridY + offsetY;

                        if (neighborX < 0 || neighborY < 0 || neighborX >= gridSizeX || neighborY >= gridSizeY)
                            continue;

                        const auto& neighborNodes = grid[static_cast<size_t>(neighborY * gridSizeX + neighborX)];
                        for (int indexA : cellNodes)
                        {
                            for (int indexB : neighborNodes)
                            {
                                if (indexA < indexB)
                                    process_pair(indexA, indexB);
                            }
                        }
                    }
                }
            }
        }
    }

    void Physics::EqualizeChildAngles()
    {
        const float strength = _config.physics.dir_children_angle_strength;
        if (strength <= 0.0f)
        {
            return;
        }

        if  (_graph.nodes.size() < 2)
        {
            return; 
        }

        auto wrapDiffPos = [](float d) {
            const float twoPi = 6.28318530718f;
            while (d <= 0.0f) d += twoPi;
            while (d > twoPi) d -= twoPi;
            return d;
        };

        for (size_t parentIndex = 0; parentIndex < _graph.nodes.size(); ++parentIndex)
        {
            auto& parent = _graph.nodes[parentIndex];
            if (parent.kind != NodeKind::Directory)
            {
                continue;
            }

            struct ChildData { int index; float angle; float radius; };
            std::vector<ChildData> dirChildren;
            dirChildren.reserve(parent.children.size());
            for (int childIndex : parent.children)
            {
                const auto& node = _graph.nodes[static_cast<size_t>(childIndex)];
                if (node.kind != NodeKind::Directory)
                {
                    continue;
                }
                float dx = node.px - parent.px;
                float dy = node.py - parent.py;
                float r = std::sqrt(dx * dx + dy * dy);
                if (r < 1e-5f)
                {
                    continue;
                }
                float a = std::atan2(dy, dx);
                dirChildren.push_back({ childIndex, a, r });
            }

            const int n = static_cast<int>(dirChildren.size());
            if (n == 0)
            {
                continue;
            }
            if (n == 1)
            {
                // Align single child direction to extend the chain: P->C should be opposite to P->GP vector
                int childIndex = dirChildren[0].index;
                auto& child = _graph.nodes[static_cast<size_t>(childIndex)];
                int grandparentId = parent.parent_dir;
                if (grandparentId >= 0)
                {
                    const auto& gp = _graph.nodes[static_cast<size_t>(grandparentId)];
                    float dxgp = parent.px - gp.px;
                    float dygp = parent.py - gp.py;
                    float desiredAngle = std::atan2(dygp, dxgp); // direction continuing the chain

                    float dx = child.px - parent.px;
                    float dy = child.py - parent.py;
                    float r = std::sqrt(dx * dx + dy * dy);
                    if (r > 1e-5f)
                    {
                        float currentAngle = std::atan2(dy, dx);
                        // Wrap to (-pi, pi)
                        float angleDelta = desiredAngle - currentAngle;
                        while (angleDelta > 3.14159265359f) angleDelta -= 6.28318530718f;
                        while (angleDelta < -3.14159265359f) angleDelta += 6.28318530718f;

                        // Tangential move only
                        float tx = -std::sin(currentAngle);
                        float ty =  std::cos(currentAngle);
                        float stepMag = r * angleDelta * strength;
                        if (!child.fixed)
                        {
                            child.px += tx * stepMag;
                            child.py += ty * stepMag;
                        }
                    }
                }
                continue;
            }

            std::sort(dirChildren.begin(), dirChildren.end(), [](const ChildData& a, const ChildData& b) {
                return a.angle < b.angle;
            });

            const float twoPi = 6.28318530718f;
            const float targetGap = twoPi / static_cast<float>(n);

            std::vector<float> angleDelta(static_cast<size_t>(n), 0.0f);
            for (int i = 0; i < n; ++i)
            {
                int j = (i + 1) % n;
                float diff = dirChildren[static_cast<size_t>(j)].angle - dirChildren[static_cast<size_t>(i)].angle;
                diff = wrapDiffPos(diff);
                float error = diff - targetGap;
                float adj = 0.5f * error * strength;
                angleDelta[static_cast<size_t>(i)] += adj;
                angleDelta[static_cast<size_t>(j)] -= adj;
            }

            for (int i = 0; i < n; ++i)
            {
                ChildData& cd = dirChildren[static_cast<size_t>(i)];
                auto& child = _graph.nodes[static_cast<size_t>(cd.index)];

                float a = cd.angle;
                float tx = -std::sin(a);
                float ty =  std::cos(a);
                float stepMag = cd.radius * angleDelta[static_cast<size_t>(i)];
                float moveX = tx * stepMag;
                float moveY = ty * stepMag;

                if (!child.fixed)
                {
                    child.px += moveX;
                    child.py += moveY;
                }
            }
        }
    }

    void Physics::InitializeDirectoryMasses()
    {
        // Base mass for files remains 1.0; directories scaled by total descendant count
        std::vector<int> memo(_graph.nodes.size(), -1);
        for (size_t i = 0; i < _graph.nodes.size(); ++i)
        {
            auto& node = _graph.nodes[i];
            if (node.kind == NodeKind::Directory)
            {
                int totalDesc = ComputeDescendantCount(static_cast<int>(i), memo);
                // Mass grows sublinearly to avoid freezing: 1 + sqrt(descendants)
                node.mass = 1.0f + std::sqrt(static_cast<float>(std::max(0, totalDesc)));
            }
            else
            {
                node.mass = 1.0f;
            }
        }
    }

    int Physics::ComputeDescendantCount(int nodeId, std::vector<int>& memo)
    {
        int& cached = memo[static_cast<size_t>(nodeId)];
        if (cached >= 0)
        {
            return cached;
        }
        const auto& node = _graph.nodes[static_cast<size_t>(nodeId)];
        if (node.kind != NodeKind::Directory)
        {
            cached = 0;
            return cached;
        }
        int count = 0;
        for (int childId : node.children)
        {
            const auto& child = _graph.nodes[static_cast<size_t>(childId)];
            if (child.kind == NodeKind::File)
            {
                count += 1; // count files
            }
            else
            {
                count += 1; // count the directory itself
                count += ComputeDescendantCount(childId, memo); // plus its subtree
            }
        }
        cached = count;
        return cached;
    }
}


