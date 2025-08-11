#include "cg_SourceParser.h"

#if defined(_MSC_VER) && _MSC_VER < 1914
#    include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#    include <filesystem>
namespace fs = std::filesystem;
#endif
#include <fstream>
#include <regex>
#include <thread>
#include <shared_mutex>

namespace cg
{
    SourceParser::SourceParser(const Config& config)
        : _config(config), _result(nullptr)
    {
    }

    static int ensure_directory(Graph& graph, const std::string& directoryPath)
    {
        // Create chain of directories if needed, robustly handling Windows paths
        std::string normalizedPath = directoryPath;
        SourceParser::NormalizePath(normalizedPath);
        if (normalizedPath.empty())
            return -1;

        int parentNodeId = -1;
        std::string accumulatedPath;
        size_t charIndex = 0;
        while (charIndex <= normalizedPath.size())
        {
            size_t nextSlash = normalizedPath.find('/', charIndex);
            std::string pathSegment = (nextSlash == std::string::npos) ? normalizedPath.substr(charIndex)
                                                                       : normalizedPath.substr(charIndex, nextSlash - charIndex);
            if (!pathSegment.empty())
            {
                if (accumulatedPath.empty())
                    accumulatedPath = pathSegment;
                else
                    accumulatedPath = accumulatedPath + "/" + pathSegment;

                int nodeId = graph.FindNodeByPath(accumulatedPath);
                if (nodeId < 0)
                {
                    std::string directoryName = pathSegment;
                    nodeId = graph.AddDirectory(accumulatedPath, directoryName, parentNodeId);
                }
                parentNodeId = nodeId;
            }
            if (nextSlash == std::string::npos)
                break;
            charIndex = nextSlash + 1;
        }
        return parentNodeId;
    }

    void SourceParser::Run(Result& outResult)
    {
        _result = &outResult;
        std::vector<std::thread> threads;
        for (const auto& rootDirectory : _config.source_roots)
        {
            threads.emplace_back([this, rootDirectory]() { ScanRoot(rootDirectory); });
        }
        for (auto& t : threads) t.join();
    }

    void SourceParser::ScanRoot(const std::string& root)
    {
        Graph& graph = _result->graph;
        std::error_code ec;
        if (!fs::exists(root, ec))
            return;

        for (auto it = fs::recursive_directory_iterator(root, ec);
             !ec && it != fs::recursive_directory_iterator();
             ++it)
        {
            if (!it->is_regular_file())
                continue;
            const auto path = it->path();
            std::string ext = path.extension().string();
            for (auto& c : ext)
                c = (char)std::tolower((unsigned char)c);
            if (!(ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".c" || ext == ".hpp" || ext == ".hxx" || ext == ".hh" || ext == ".h"))
                continue;

            std::string file_path = path.string();
            NormalizePath(file_path);
            std::string dir_path = path.parent_path().string();
            NormalizePath(dir_path);

            int dir_id;
            {
                std::lock_guard<std::mutex> lock(_graphMutex);
                dir_id = ensure_directory(graph, dir_path);
                if (_config.graph.enable_directory_links && dir_id >= 0)
                {
                    // Ensure links for the entire chain
                    int currentDirId = dir_id;
                    while (true)
                    {
                        int parentId = graph.nodes[currentDirId].parent_dir;
                        if (parentId < 0)
                            break;
                        int sizeA = graph.nodes[parentId].subtreeDirCount + 1;
                        int sizeB = graph.nodes[currentDirId].subtreeDirCount + 1;
                        float base = 40.0f + static_cast<float>(sizeA + sizeB) * 2.0f;
                        graph.AddLink(parentId, currentDirId, base * _config.graph.dir_dir_length_coef, 0.5f);
                        currentDirId = parentId;
                    }
                }
            }

            int file_id;
            {
                std::lock_guard<std::mutex> lock(_graphMutex);
                file_id = graph.FindOrAddFile(file_path, dir_id);
                if (_config.graph.enable_directory_links && dir_id >= 0)
                {
                    // Base dir-file length derived from directory size (files+dirs) with small base
                    int baseCount = static_cast<int>(graph.nodes[dir_id].children.size());
                    float baseLength = 30.0f + baseCount * 1.0f;
                    graph.AddLink(dir_id, file_id, baseLength * _config.graph.dir_file_length_coef, 0.6f);
                }
            }

            ParseIncludesForFile(file_path, file_id);
        }
    }

    void SourceParser::ParseIncludesForFile(const std::string& file_path, int file_node_id)
    {
        std::ifstream inputFile(file_path);
        if (!inputFile.is_open())
            return;

        std::string line;
        static const std::regex inc_regex(R"(^\s*#\s*include\s*[<\"]([^>\"]+)[>\"])", std::regex::ECMAScript);
        while (std::getline(inputFile, line))
        {
            std::smatch m;
            if (std::regex_search(line, m, inc_regex))
            {
                std::string inc = m[1].str();
                // Try to resolve include to a file under any root
                std::string found;
                for (const auto& rootDirectory : _config.source_roots)
                {
                    fs::path candidatePath = fs::path(rootDirectory) / inc;
                    std::error_code ec;
                    if (fs::exists(candidatePath, ec))
                    {
                        found = candidatePath.string();
                        break;
                    }
                }
                if (found.empty()) continue;
                NormalizePath(found);
                std::string dir_path = fs::path(found).parent_path().string();
                NormalizePath(dir_path);

                int dir_id;
                int inc_id;
                {
                    std::lock_guard<std::mutex> lock(_graphMutex);
                    Graph& graph = _result->graph;
                    dir_id = ensure_directory(graph, dir_path);
                    if (_config.graph.enable_directory_links && dir_id >= 0)
                    {
                        int currentDirId = dir_id;
                        while (true)
                        {
                            int parentId = graph.nodes[currentDirId].parent_dir;
                            if (parentId < 0)
                                break;
                            int sizeA2 = graph.nodes[parentId].subtreeDirCount + 1;
                            int sizeB2 = graph.nodes[currentDirId].subtreeDirCount + 1;
                            float base2 = 40.0f + static_cast<float>(sizeA2 + sizeB2) * 2.0f;
                            graph.AddLink(parentId, currentDirId, base2 * _config.graph.dir_dir_length_coef, 0.5f);
                            currentDirId = parentId;
                        }
                    }
                    inc_id = graph.FindOrAddFile(found, dir_id);
                    if (_config.graph.enable_directory_links && dir_id >= 0)
                    {
                        int baseCount = static_cast<int>(graph.nodes[dir_id].children.size());
                        float baseLength = 30.0f + baseCount * 1.0f;
                        graph.AddLink(dir_id, inc_id, baseLength * _config.graph.dir_file_length_coef, 0.6f);
                    }
                    if (_config.graph.enable_include_links)
                        graph.AddLink(file_node_id, inc_id, static_cast<float>(_config.physics.link_rest_length), static_cast<float>(_config.physics.link_stiffness));
                }
            }
        }
    }

    bool SourceParser::IsCppSource(const std::string& path)
    {
        (void)path; return true;
    }

    bool SourceParser::IsHeader(const std::string& path)
    {
        (void)path; return true;
    }

    void SourceParser::NormalizePath(std::string& p)
    {
        for (char& c : p)
        {
            if (c == '\\')
                c = '/';
        }
        // Optionally lowercase on Windows; left as-is to preserve case
    }
}


