#pragma once

#include <optional>
#include <string>

#include "../..//imgui.h"

#include "cg_Config.h"
#include "cg_Graph.h"

namespace cg
{
    struct Camera
    {
        ImVec2 position = ImVec2(0, 0);
        float zoom = 1.0f; // world->screen multiplier

        ImVec2 world_to_screen(const ImVec2& w) const
        {
            return ImVec2((w.x - position.x) * zoom, (w.y - position.y) * zoom);
        }

        ImVec2 screen_to_world(const ImVec2& s) const
        {
            return ImVec2(s.x / zoom + position.x, s.y / zoom + position.y);
        }
    };

    class GraphRenderer
    {
    public:
        GraphRenderer(const Config& cfg, Graph& graph);
        void Draw(const ImVec2& canvasPos, const ImVec2& canvasSize);
        void Update(float dtMs);

        float GetZoom() const { return _camera.zoom; }
        void SetZoom(float z) { _camera.zoom = (z < 0.05f ? 0.05f : (z > 20.0f ? 20.0f : z)); }
        ImVec2 GetCameraPos() const { return _camera.position; }
        void SetCameraPos(ImVec2 p) { _camera.position = p; }

    private:
        void HandleInput(const ImVec2& canvasPos, const ImVec2& canvasSize);
        void DrawLinks(ImDrawList* drawList, const ImVec2& canvasPos);
        void DrawNodes(ImDrawList* drawList, const ImVec2& canvasPos);

        const Config& _config;
        Graph& _graph;
        Camera _camera;

        int _hoveredNode = -1;
        int _draggedNode = -1;
        ImVec2 _dragOffset{0, 0};
    };

} // namespace cg


