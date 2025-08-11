#include "cg_GraphRenderer.h"
#include "../..//imgui_internal.h"

#include <algorithm>
#include <cmath>

namespace cg
{
    GraphRenderer::GraphRenderer(const Config& cfg, Graph& g)
        : _config(cfg), _graph(g)
    {
        _camera.zoom = 1.0f;
        _camera.position = ImVec2(0, 0);
    }

    void GraphRenderer::Draw(const ImVec2& canvas_p0, const ImVec2& avail)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(canvas_p0, ImVec2(canvas_p0.x + avail.x, canvas_p0.y + avail.y), IM_COL32(20, 20, 25, 255));
        HandleInput(canvas_p0, avail);
        DrawLinks(drawList, canvas_p0);
        DrawNodes(drawList, canvas_p0);
    }

    void GraphRenderer::Update(float dt_ms)
    {
        (void)dt_ms;
    }

    void GraphRenderer::HandleInput(const ImVec2& canvas_pos, const ImVec2& canvas_size)
    {
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::SetCursorScreenPos(canvas_pos);
        ImGui::InvisibleButton("canvas-input", canvas_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool is_hovered = ImGui::IsItemHovered();
        const bool is_active = ImGui::IsItemActive();

        // Zoom with wheel
        if (is_hovered && io.MouseWheel != 0.0f)
        {
            const float scroll_steps = io.MouseWheel;
            const float zoom_factor = std::pow(_config.render.zoom_speed, scroll_steps);
            ImVec2 mouse = ImGui::GetMousePos();
            ImVec2 mouse_local = ImVec2(mouse.x - canvas_pos.x, mouse.y - canvas_pos.y);
            ImVec2 before = _camera.screen_to_world(mouse_local);
            _camera.zoom *= zoom_factor;
            if (_camera.zoom < 0.05f)
                _camera.zoom = 0.05f;
            if (_camera.zoom > 20.0f)
                _camera.zoom = 20.0f;
            ImVec2 after = _camera.screen_to_world(mouse_local);
            _camera.position.x += (before.x - after.x);
            _camera.position.y += (before.y - after.y);
        }

        // Pan with right mouse drag
        if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
        {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            _camera.position.x -= d.x / _camera.zoom;
            _camera.position.y -= d.y / _camera.zoom;
        }

        // Detect hovered node
        _hoveredNode = -1;
        ImVec2 mouse = ImGui::GetMousePos();
        ImVec2 ml(mouse.x - canvas_pos.x, mouse.y - canvas_pos.y);
        auto get_node_screen_radius = [&](const cg::Node& node) -> float {
            float base = _config.render.circle_radius;
            float mult = 1.0f;
            if (node.kind == NodeKind::Directory)
            {
                float mass = node.mass;
                if (mass > 1.05f)
                {
                    float s = std::log2(1.0f + mass);
                    mult = 0.75f + 0.5f * s; // clamp below
                    if (mult < 0.75f) mult = 0.75f;
                    if (mult > 3.0f) mult = 3.0f;
                }
            }
            return base * mult * _camera.zoom;
        };
        for (int i = static_cast<int>(_graph.nodes.size()) - 1; i >= 0; --i)
        {
            const auto& n = _graph.nodes[i];
            ImVec2 sp = _camera.world_to_screen(ImVec2(n.px, n.py));
            ImVec2 p(sp.x + canvas_pos.x, sp.y + canvas_pos.y);
            float dx = mouse.x - p.x;
            float dy = mouse.y - p.y;
            float r = get_node_screen_radius(n);
            float r2 = r * r;
            if (dx * dx + dy * dy <= r2)
            {
                _hoveredNode = i;
                break;
            }
        }

        // Drag with left mouse
        if (_hoveredNode >= 0 && is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            _draggedNode = _hoveredNode;
            const auto& n = _graph.nodes[_draggedNode];
            ImVec2 sp = _camera.world_to_screen(ImVec2(n.px, n.py));
            ImVec2 p(sp.x + canvas_pos.x, sp.y + canvas_pos.y);
            _dragOffset = ImVec2(mouse.x - p.x, mouse.y - p.y);
        }
        if (_draggedNode >= 0)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                ImVec2 target(mouse.x - canvas_pos.x - _dragOffset.x, mouse.y - canvas_pos.y - _dragOffset.y);
                ImVec2 w = _camera.screen_to_world(target);
                auto& n = _graph.nodes[_draggedNode];
                n.px = w.x;
                n.py = w.y;
            }
            else
            {
                _draggedNode = -1;
            }
        }
    }

    void GraphRenderer::DrawLinks(ImDrawList* drawList, const ImVec2& canvas_pos)
    {
        for (const auto& l : _graph.links)
        {
            const auto& a = _graph.nodes[l.a];
            const auto& b = _graph.nodes[l.b];
            ImVec2 ap = _camera.world_to_screen(ImVec2(a.px, a.py));
            ImVec2 bp = _camera.world_to_screen(ImVec2(b.px, b.py));
            const bool aDir = (a.kind == NodeKind::Directory);
            const bool bDir = (b.kind == NodeKind::Directory);
            const bool isDirDir = aDir && bDir;
            const bool isDirFile = (aDir != bDir);
            const bool isFileFile = !aDir && !bDir;

            // Respect config visibility toggles
            if ((isDirDir || isDirFile) && !_config.graph.enable_directory_links)
                continue;
            if (isFileFile && !_config.graph.enable_include_links)
                continue;

            ImU32 col; float thickness;
            if (isDirDir)
            {
                col = IM_COL32(255, 180, 70, 180);
                thickness = 2.0f;
            }
            else if (isDirFile)
            {
                col = IM_COL32(200, 200, 200, 110);
                thickness = 1.5f;
            }
            else /* file-file */
            {
                col = IM_COL32(120, 220, 255, 100);
                thickness = 1.0f;
            }
            drawList->AddLine(ImVec2(ap.x + canvas_pos.x, ap.y + canvas_pos.y), ImVec2(bp.x + canvas_pos.x, bp.y + canvas_pos.y), col, thickness);
        }
    }

    void GraphRenderer::DrawNodes(ImDrawList* drawList, const ImVec2& canvas_pos)
    {
        const float base_r = _config.render.circle_radius;
        for (size_t i = 0; i < _graph.nodes.size(); ++i)
        {
            const auto& n = _graph.nodes[i];
            ImVec2 sp = _camera.world_to_screen(ImVec2(n.px, n.py));
            ImVec2 p(sp.x + canvas_pos.x, sp.y + canvas_pos.y);
            float r = base_r * _camera.zoom;
            if (n.kind == NodeKind::Directory)
            {
                float mass = n.mass;
                if (mass > 1.05f)
                {
                    float s = std::log2(1.0f + mass);
                    float mult = 0.75f + 0.5f * s;
                    if (mult < 0.75f) mult = 0.75f;
                    if (mult > 3.0f) mult = 3.0f;
                    r = base_r * mult * _camera.zoom;
                }
            }
            ImU32 col;
            if (i == static_cast<size_t>(_hoveredNode))
            {
                col = IM_COL32(255, 220, 70, 255);
            }
            else
            {
                if (n.kind == NodeKind::Directory)
                {
                    col = IM_COL32(120, 180, 255, 255);
                }
                else
                {
                    // Deterministic random-ish color per file based on path hash
                    size_t h = std::hash<std::string>{}(n.path);
                    float hue = static_cast<float>(h % 360) / 360.0f;
                    ImColor c = ImColor::HSV(hue, 0.65f, 0.95f, 1.0f);
                    col = (ImU32)c;
                }
            }
            drawList->AddCircleFilled(p, r, col, 20);

            if (i == static_cast<size_t>(_hoveredNode))
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40.0f);
                ImGui::TextUnformatted(n.name.c_str());
                ImGui::Separator();
                ImGui::TextUnformatted(n.path.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
                // Highlight neighbors
                for (const auto& l : _graph.links)
                {
                    if (l.a == (int)i || l.b == (int)i)
                    {
                        int j = (l.a == (int)i) ? l.b : l.a;
                        const auto& m = _graph.nodes[j];
                        ImVec2 sp2 = _camera.world_to_screen(ImVec2(m.px, m.py));
                        ImVec2 p2(sp2.x + canvas_pos.x, sp2.y + canvas_pos.y);
                        drawList->AddLine(p, p2, IM_COL32(255, 0, 0, 255), 2.0f);
                    }
                }
            }
        }
    }
}


