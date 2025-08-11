#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

#include "cg_Config.h"
#include "cg_Graph.h"

namespace cg
{
    class SourceParser
    {
    public:
        struct Result
        {
            Graph graph;
        };

        explicit SourceParser(const Config& config);

        void Run(Result& outResult);

    private:
        void ScanRoot(const std::string& rootDirectory);
        void ParseIncludesForFile(const std::string& filePath, int fileNodeId);

    public:
        static bool IsCppSource(const std::string& path);
        static bool IsHeader(const std::string& path);
        static void NormalizePath(std::string& path);

    private:
        const Config& _config;
        Result* _result;
        std::mutex _graphMutex;
        std::unordered_map<std::string, int> _dirCache;
    };
}


