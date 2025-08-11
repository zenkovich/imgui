#include "cg_Graph.h"

#include <algorithm>

namespace cg
{
    int Graph::AddDirectory(const std::string& path, const std::string& name, int parentDir)
    {
        auto it = pathToNode.find(path);
        if (it != pathToNode.end())
            return it->second;
        Node n;
        n.id = static_cast<int>(nodes.size());
        n.kind = NodeKind::Directory;
        n.name = name;
        n.path = path;
        n.parent_dir = parentDir;
        n.subtreeDirCount = 0;
        n.color_rgba = 0xFF7FE07F; // greenish
        nodes.push_back(n);
        pathToNode[path] = n.id;
        if (parentDir >= 0)
        {
            nodes[parentDir].children.push_back(n.id);
            // Increment subtree count up the chain
            int cur = parentDir;
            while (cur >= 0)
            {
                nodes[cur].subtreeDirCount += 1;
                cur = nodes[cur].parent_dir;
            }
            // Directory link will be added by caller depending on settings
        }
        return n.id;
    }

    int Graph::AddFile(const std::string& path, const std::string& name, int parentDir)
    {
        auto it = pathToNode.find(path);
        if (it != pathToNode.end())
            return it->second;
        Node n;
        n.id = static_cast<int>(nodes.size());
        n.kind = NodeKind::File;
        n.name = name;
        n.path = path;
        n.parent_dir = parentDir;
        n.color_rgba = 0xFF7FB2FF; // bluish
        nodes.push_back(n);
        pathToNode[path] = n.id;
        if (parentDir >= 0)
        {
            nodes[parentDir].children.push_back(n.id);
            // Directory link will be added by caller depending on settings
        }
        return n.id;
    }

    void Graph::AddChild(int dirId, int childId)
    {
        if (dirId >= 0 && dirId < (int)nodes.size())
            nodes[dirId].children.push_back(childId);
    }

    void Graph::AddLink(int a, int b, float restLength, float stiffness)
    {
        if (a < 0 || b < 0 || a == b)
            return;
        Link link;
        link.a = a;
        link.b = b;
        link.restLength = restLength;
        link.stiffness = stiffness;
        links.push_back(link);
    }

    int Graph::FindOrAddDirectory(const std::string& path, int parentDir)
    {
        auto it = pathToNode.find(path);
        if (it != pathToNode.end())
            return it->second;
        // name is last segment
        std::string name = path;
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos)
            name = path.substr(pos + 1);
        return AddDirectory(path, name, parentDir);
    }

    int Graph::FindOrAddFile(const std::string& path, int parentDir)
    {
        auto it = pathToNode.find(path);
        if (it != pathToNode.end())
            return it->second;
        std::string name = path;
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos)
            name = path.substr(pos + 1);
        return AddFile(path, name, parentDir);
    }

    int Graph::FindNodeByPath(const std::string& path) const
    {
        auto it = pathToNode.find(path);
        return it == pathToNode.end() ? -1 : it->second;
    }
}

