#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cg
{
    enum class NodeKind
    {
        Directory,
        File
    };

    struct Node
    {
        int id = -1;
        NodeKind kind = NodeKind::File;
        std::string name;          // file or directory name
        std::string path;          // absolute or canonical path for files/dirs
        int parent_dir = -1;       // parent directory node id
        std::vector<int> children; // for directory: child dirs + files
        int subtreeDirCount = 0;   // number of directories in subtree (excluding self)

        // physics state
        float px = 0.0f, py = 0.0f;    // current position
        float ppx = 0.0f, ppy = 0.0f;  // previous position (for Verlet)
        float mass = 1.0f;
        bool fixed = false;
        int color_rgba = 0xFF7FB2FF;   // ABGR or RGBA as needed by ImGui
    };

    struct Link
    {
        int a = -1;
        int b = -1;
        float restLength = 80.0f;
        float stiffness = 1.0f; // 0..1
    };

    class Graph
    {
    public:
        // New CamelCase API
        int AddDirectory(const std::string& path, const std::string& name, int parentDir);
        int AddFile(const std::string& path, const std::string& name, int parentDir);
        void AddChild(int dirId, int childId);
        void AddLink(int a, int b, float restLength, float stiffness);

        int FindOrAddDirectory(const std::string& path, int parentDir);
        int FindOrAddFile(const std::string& path, int parentDir);
        int FindNodeByPath(const std::string& path) const;

        // Backward-compatible wrappers (to be removed later)
        int add_directory(const std::string& path, const std::string& name, int parentDir) { return AddDirectory(path, name, parentDir); }
        int add_file(const std::string& path, const std::string& name, int parentDir) { return AddFile(path, name, parentDir); }
        void add_child(int dirId, int childId) { AddChild(dirId, childId); }
        void add_link(int a, int b, float restLength, float stiffness) { AddLink(a, b, restLength, stiffness); }
        int find_or_add_directory(const std::string& path, int parentDir) { return FindOrAddDirectory(path, parentDir); }
        int find_or_add_file(const std::string& path, int parentDir) { return FindOrAddFile(path, parentDir); }
        int find_node_by_path(const std::string& path) const { return FindNodeByPath(path); }

        Node& node(int id) { return nodes[id]; }
        const Node& node(int id) const { return nodes[id]; }

        std::vector<Node> nodes;
        std::vector<Link> links;

        std::unordered_map<std::string, int> pathToNode;
    };

} // namespace cg

