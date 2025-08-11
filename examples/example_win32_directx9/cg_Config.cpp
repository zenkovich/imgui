#include "cg_Config.h"

#include <fstream>
#include <sstream>

namespace cg
{
    static double get_number_or(const JsonValue& v, double def)
    {
        return v.is_number() ? v.as_number() : def;
    }

    static float get_numberf_or(const JsonValue& v, float def)
    {
        return v.is_number() ? static_cast<float>(v.as_number()) : def;
    }

    static void parse_array_of_strings(const JsonValue& v, std::vector<std::string>& out)
    {
        if (!v.is_array()) return;
        for (const auto& it : v.as_array())
            if (it.is_string()) out.push_back(it.as_string());
    }

    bool Config::LoadFromFile(const std::string& path, Config& out, std::string& error)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) { error = "Failed to open config file: " + path; return false; }
        std::ostringstream ss; ss << f.rdbuf();
        return LoadFromJson(ss.str(), out, error);
    }

    bool Config::LoadFromJson(const std::string& json_text, Config& out, std::string& error)
    {
        JsonValue root;
        if (!JsonParser::Parse(json_text, root, error))
            return false;

        return Parse(root, out, error);
    }

    bool Config::Parse(const JsonValue& root, Config& out, std::string& error)
    {
        if (!root.is_object()) { error = "Config root must be object"; return false; }
        const auto& o = root.as_object();

        auto it_src = o.find("source_roots");
        if (it_src != o.end()) parse_array_of_strings(it_src->second, out.source_roots);

        auto it_phys = o.find("physics");
        if (it_phys != o.end() && it_phys->second.is_object())
        {
            const auto& p = it_phys->second.as_object();
            if (auto it = p.find("time_step"); it != p.end()) out.physics.time_step = get_number_or(it->second, out.physics.time_step);
            if (auto it = p.find("time_step"); it != p.end()) out.physics.time_step = get_number_or(it->second, out.physics.time_step);
            if (auto it = p.find("solver_iterations"); it != p.end()) out.physics.solver_iterations = static_cast<int>(get_number_or(it->second, out.physics.solver_iterations));
            if (auto it = p.find("link_rest_length"); it != p.end()) out.physics.link_rest_length = get_number_or(it->second, out.physics.link_rest_length);
            if (auto it = p.find("link_stiffness"); it != p.end()) out.physics.link_stiffness = get_number_or(it->second, out.physics.link_stiffness);
            if (auto it = p.find("repulsion_radius"); it != p.end()) out.physics.repulsion_radius = get_number_or(it->second, out.physics.repulsion_radius);
            if (auto it = p.find("repulsion_strength"); it != p.end()) out.physics.repulsion_strength = get_number_or(it->second, out.physics.repulsion_strength);
            if (auto it = p.find("damping"); it != p.end()) out.physics.damping = get_number_or(it->second, out.physics.damping);
            if (auto it = p.find("max_displacement"); it != p.end()) out.physics.max_displacement = get_number_or(it->second, out.physics.max_displacement);
            if (auto it = p.find("dir_children_angle_strength"); it != p.end()) out.physics.dir_children_angle_strength = get_numberf_or(it->second, out.physics.dir_children_angle_strength);
        }

        auto it_render = o.find("render");
        if (it_render != o.end() && it_render->second.is_object())
        {
            const auto& r = it_render->second.as_object();
            if (auto it = r.find("circle_radius"); it != r.end()) out.render.circle_radius = get_numberf_or(it->second, out.render.circle_radius);
            if (auto it = r.find("zoom_speed"); it != r.end()) out.render.zoom_speed = get_numberf_or(it->second, out.render.zoom_speed);
            if (auto it = r.find("pan_speed"); it != r.end()) out.render.pan_speed = get_numberf_or(it->second, out.render.pan_speed);
        }

        auto it_graph = o.find("graph");
        if (it_graph != o.end() && it_graph->second.is_object())
        {
            const auto& g = it_graph->second.as_object();
            if (auto it = g.find("enable_directory_links"); it != g.end() && it->second.is_bool()) out.graph.enable_directory_links = it->second.as_bool();
            if (auto it = g.find("enable_include_links"); it != g.end() && it->second.is_bool()) out.graph.enable_include_links = it->second.as_bool();
            if (auto it = g.find("dir_dir_length_coef"); it != g.end()) out.graph.dir_dir_length_coef = get_numberf_or(it->second, out.graph.dir_dir_length_coef);
            if (auto it = g.find("dir_file_length_coef"); it != g.end()) out.graph.dir_file_length_coef = get_numberf_or(it->second, out.graph.dir_file_length_coef);
        }

        return true;
    }
}


