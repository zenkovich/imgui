#pragma once

#include <string>
#include <vector>

#include "cg_Json.h"

namespace cg
{
    struct PhysicsParams
    {
        float time_step = 0.016f;
        int solver_iterations = 8;
        float link_rest_length = 80.0f;
        float link_stiffness = 1.0f; // 0..1
        float repulsion_radius = 30.0f;
        float repulsion_strength = 200.0f;
        float damping = 0.02f; // velocity damping
        float max_displacement = 50.0f; // for stability
        // Strength of angular equalization between directory children (0 disables)
        float dir_children_angle_strength = 0.1f;
    };

    struct RenderParams
    {
        float circle_radius = 6.0f;
        float zoom_speed = 1.1f;
        float pan_speed = 1.0f;
    };

    struct GraphParams
    {
        bool enable_directory_links = true;
        bool enable_include_links = false; // default off per request
        float dir_dir_length_coef = 1.0f;   // multiplier for computed base dir-dir length
        float dir_file_length_coef = 1.0f;  // multiplier for base dir-file length
    };

    class Config
    {
    public:
        std::vector<std::string> source_roots;
        PhysicsParams physics;
        RenderParams render;
        GraphParams graph;

        static bool LoadFromFile(const std::string& path, Config& out, std::string& error);
        static bool LoadFromJson(const std::string& json_text, Config& out, std::string& error);

    private:
        static bool Parse(const JsonValue& root, Config& out, std::string& error);
    };
}


