#ifndef GL_GIZMO_UTIL_HPP
#define GL_GIZMO_UTIL_HPP

#include <map>
#include <vector>
#include <string>
#include <wx/string.h>
#include "imgui.h"


namespace Slic3r::GUI {

// Forward declaration
class ImGuiWrapper;
class GLCanvas3D; 

namespace GLGizmoUtils {

// Renders a tooltip button using the provided shortcuts
void TooltipButton(
    ImGuiWrapper*                                       imgui_wrapper,
    const GLCanvas3D&                                   canvas,
    const std::vector<std::pair<wxString, wxString>>&   shortcuts,
    float                                               x,
    float                                               y
);

// Sets up ImGui to render buttons that are right-aligned within the current window, using the provided labels to calculate spacing.
void BeginRightAlignedButtons(
    const           std::vector<wxString>& labels
);

void PushOrcaButtonStyle();

void PopOrcaButtonStyle();

void PushDangerButtonStyle();

void PopDangerButtonStyle();


} // namespace GLGizmoUtils
} // namespace Slic3r::GUI

#endif // GL_GIZMO_UTIL_HPP