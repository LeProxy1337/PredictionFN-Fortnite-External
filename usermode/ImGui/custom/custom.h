#pragma once
#include "../imgui_internal.h"
#include "../imgui.h"

static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }

bool Toggle(const char* label, bool* v)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    float height = 17.0f;
    const ImVec2 pos = window->DC.CursorPos;

    float width = height * 1.8f;
    float radius = height * 0.4f;

    const ImRect total_bb(pos, pos + ImVec2(width + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 5.0f));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id))
        return false;

    float last_active_id_timer = g.LastActiveIdTimer;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
    {
        *v = !(*v);
        ImGui::MarkItemEdited(id);
        g.LastActiveIdTimer = 0.f;
    }

    if (g.LastActiveIdTimer == 0.f && g.LastActiveId == id && !pressed)
        g.LastActiveIdTimer = last_active_id_timer;

    float t = *v ? 1.0f : 0.0f;

    if (g.LastActiveId == id)
    {
        float t_anim = ImSaturate(g.LastActiveIdTimer / 0.16f);
        t = *v ? (t_anim) : (1.0f - t_anim);
    }

    //ImU32 col_bg = ImColor((held && hovered) ? ImColor(80, 184, 250, 255) : hovered ? ImColor(80, 184, 250, 255) : ImColor(80, 184, 250, 255));

    const ImRect frame_bb(pos, pos + ImVec2(width, height));
    ImU32 col_bg;
    ImU32 col_bgdos;

    //if (ImGui::IsItemHovered())
    //	col_bg = *v ? IM_COL32(80, 184, 250, 255) : IM_COL32(80, 184, 250, 255);
    //else
    //	col_bg = *v ? IM_COL32(80, 184, 250, 255) : IM_COL32(50, 50, 50, 255);
    col_bg = *v ? IM_COL32(94, 92, 179, 255) : IM_COL32(20, 20, 20, 255);
    col_bgdos = *v ? IM_COL32(230, 230, 230, 255) : IM_COL32(230, 230, 230, 255);

    ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, col_bg, true, height * 0.5f);
    ImGui::RenderNavHighlight(total_bb, id);

    ImVec2 label_pos = ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y);
    ImGui::RenderText(label_pos, label);
    window->DrawList->AddCircleFilled(ImVec2(pos.x + radius + t * (width - radius * 2.5f) + 2, pos.y + radius + 2), radius - 1.5f, col_bgdos, 36);

    return pressed;
}