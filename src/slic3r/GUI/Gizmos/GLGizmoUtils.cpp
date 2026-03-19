#include "GLGizmoUtils.hpp"
#include "slic3r/GUI/ImGuiWrapper.hpp"
#include "GLGizmosManager.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/GLCanvas3D.hpp"
#include <wx/app.h>
#include <boost/algorithm/string.hpp>

#ifdef WIN32
#include <wx/msw/winundef.h>
#endif

/*
    GizmoUI Footer Structure:

    ~ Content ~
    ----------------------------------------
    [Button1] [Button2]
    ----------------------------------------
    [?] [Reset]           [Confirm] [Cancel]
    ----------------------------------------
    ~ Warnings ~


    Additional details:
        - [Confirm], [Cancel], [Done], ... are buttons that close the Tool Dialog
        - [Reset], [Button1], ... are buttons that do not!
        - Non-consequential buttons like [Cancel] and [Done] are always the right-most buttons
        - [Confirm] buttons should use the orca_button_style to differentiate them from other buttons
        - Multiple warnings can show, but should only have one ImGui::Separator above
        - If no warnings is shown, dont render the ImGui::Separator

*/

namespace Slic3r::GUI::GLGizmoUtils {

void begin_right_aligned_buttons(const std::vector<wxString>& labels)
{
    float       total_width = 0.0f;
    ImGuiStyle& style       = ImGui::GetStyle();
    float       spacing     = style.ItemSpacing.x;
    float       padding     = style.FramePadding.x * 2.0f;

    // Calculate width
    for (size_t i = 0; i < labels.size(); ++i) {
        total_width += ImGuiWrapper::calc_text_size(labels[i]).x + padding;
        if (i < labels.size() - 1)
            total_width += spacing;
    }

    float avail = ImGui::GetContentRegionAvail().x;

    // Handle Overlap: If the total width of the buttons exceeds available space, move to a new line
    if (total_width > avail) {
        ImGui::NewLine();
        avail = ImGui::GetContentRegionAvail().x; // Reset to full window width
    }

    float posX = ImGui::GetCursorPosX() + std::max(0.0f, avail - total_width);
    ImGui::SetCursorPosX(posX);
}

void push_orca_button_style()
{
    ImVec4 base_orca = ImGuiWrapper::COL_ORCA;

    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(base_orca.x, base_orca.y, base_orca.z, h, s, v);

    ImVec4 hover, active;

    // Lighter variant for Hover (Increase Value by ~12%)
    ImGui::ColorConvertHSVtoRGB(h, s, std::min(v + 0.12f, 1.0f), hover.x, hover.y, hover.z);
    hover.w = base_orca.w;

    // Darker variant for Active (Decrease Value by ~12%)
    ImGui::ColorConvertHSVtoRGB(h, s, std::max(v - 0.12f, 0.0f), active.x, active.y, active.z);
    active.w = base_orca.w;

    ImGui::PushStyleColor(ImGuiCol_Button, base_orca);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);

    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiWrapper::COL_WINDOW_BG);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
}

void pop_orca_button_style()
{
    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(4);
}

} // namespace Slic3r::GUI::GLGizmoUtils