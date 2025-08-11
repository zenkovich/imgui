#include "cg_App.h"

#include <memory>
#include "perfmon/NanoProfiler.h"
#include "../..//imgui.h"

namespace cg
{
    App::App() {}

    bool App::Initialize(const std::string& configJsonText)
    {
        std::string error;
        if (!Config::LoadFromJson(configJsonText, _config, error))
            return false;

        SourceParser parser(_config);
        {
            NANO_PROFILE_SCOPE("Parse Sources");
            parser.Run(_parseResult);
        }

        _physics = new Physics(_config, _parseResult.graph);
        _renderer = new GraphRenderer(_config, _parseResult.graph);
        return true;
    }

    void App::Frame(float dtMs, const ImVec2& canvasPos, const ImVec2& canvasSize)
    {
        if (!_physics || !_renderer)
            return;
        // Toggle pause on P
        if (ImGui::IsKeyPressed(ImGuiKey_P, false))
            _paused = !_paused;

        if (!_paused)
        {
            NANO_PROFILE_SCOPE("Physics");
            // Detailed breakdown
            if (!_pauseIntegrate)
            {
                NANO_PROFILE_SCOPE("Integrate");
                _physics->IntegrateOnly();
            }
            for (int iterationIndex = 0; iterationIndex < _config.physics.solver_iterations; ++iterationIndex)
            {
                NANO_PROFILE_SCOPE("Iter");
                if (!_pauseConstraints)
                {
                    NANO_PROFILE_SCOPE("Constraints");
                    _physics->ConstraintsOnly();
                }
                {
                    NANO_PROFILE_SCOPE("AngleEq");
                    _physics->AngleEqualizationOnly();
                }
                if (!_pauseRepulsion)
                {
                    NANO_PROFILE_SCOPE("Repulsion");
                    _physics->RepulsionOnly();
                }
            }
        }
        {
            NANO_PROFILE_SCOPE("UI Update");
            _renderer->Update(dtMs);
        }
        {
            NANO_PROFILE_SCOPE("UI Draw");
            _renderer->Draw(canvasPos, canvasSize);
        }

        // Controls overlay (top-right)
        ImGui::SetNextWindowPos(ImVec2(canvasPos.x + canvasSize.x - 10, canvasPos.y + 10), ImGuiCond_Always, ImVec2(1,0));
        ImGui::SetNextWindowBgAlpha(0.8f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("Controls", nullptr, flags))
        {
            ImGui::Text("Simulation");
            ImGui::Separator();
            ImGui::Checkbox("Pause (P)", &_paused);
            ImGui::Checkbox("Pause Integrate", &_pauseIntegrate);
            ImGui::Checkbox("Pause Constraints", &_pauseConstraints);
            ImGui::Checkbox("Pause Repulsion", &_pauseRepulsion);

            ImGui::Separator();
            ImGui::Text("Physics");
            ImGui::SliderFloat("dt", (float*)&_config.physics.time_step, 0.001f, 0.05f, "%.3f");
            ImGui::SliderInt("Iterations", &_config.physics.solver_iterations, 1, 32);
            ImGui::SliderFloat("RestLen", (float*)&_config.physics.link_rest_length, 5.0f, 200.0f, "%.0f");
            ImGui::SliderFloat("Stiffness", (float*)&_config.physics.link_stiffness, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("RepulseR", (float*)&_config.physics.repulsion_radius, 1.0f, 200.0f, "%.0f");
            ImGui::SliderFloat("RepulseK", (float*)&_config.physics.repulsion_strength, 0.0f, 1000.0f, "%.0f");
            ImGui::SliderFloat("Damping", (float*)&_config.physics.damping, 0.0f, 0.2f, "%.3f");
            ImGui::SliderFloat("MaxDisp", (float*)&_config.physics.max_displacement, 1.0f, 200.0f, "%.0f");
            ImGui::SliderFloat("Dir AngleEq Strength", &_config.physics.dir_children_angle_strength, 0.0f, 2.0f, "%.3f");

            ImGui::Separator();
            ImGui::Text("Graph Links");
            ImGui::Checkbox("Directory Links", &_config.graph.enable_directory_links);
            ImGui::Checkbox("Include Links", &_config.graph.enable_include_links);
            ImGui::SliderFloat("Dir-Dir Coef", &_config.graph.dir_dir_length_coef, 0.1f, 10.0f, "%.2f");
            ImGui::SliderFloat("Dir-File Coef", &_config.graph.dir_file_length_coef, 0.1f, 10.0f, "%.2f");

            ImGui::Separator();
            ImGui::Text("Render");
            {
                float z = _renderer->GetZoom();
                if (ImGui::SliderFloat("Zoom", &z, 0.05f, 20.0f, "%.2f"))
                    _renderer->SetZoom(z);
            }
            ImGui::SliderFloat("CircleR", &_config.render.circle_radius, 1.0f, 20.0f, "%.1f");
            ImGui::SliderFloat("ZoomSpd", &_config.render.zoom_speed, 1.01f, 1.5f, "%.2f");
            ImGui::SliderFloat("PanSpd", &_config.render.pan_speed, 0.1f, 3.0f, "%.1f");
        }
        ImGui::End();
    }
}


