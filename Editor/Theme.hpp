#pragma once

#include <imgui.h>

namespace elysium {

inline void applyElysiumDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 4.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.18f, 0.24f, 0.30f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.33f, 0.41f, 1.00f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.30f, 0.38f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.20f, 0.28f, 0.36f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.27f, 0.37f, 0.47f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.24f, 0.33f, 0.42f, 1.00f);
    c[ImGuiCol_Tab] = ImVec4(0.13f, 0.17f, 0.21f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.31f, 0.39f, 1.00f);
    c[ImGuiCol_TabActive] = ImVec4(0.19f, 0.26f, 0.33f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.17f, 0.20f, 1.00f);
}

} // namespace elysium
