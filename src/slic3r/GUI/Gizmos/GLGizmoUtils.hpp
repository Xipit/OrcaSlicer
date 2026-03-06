#ifndef GL_GIZMO_UTIL_HPP
#define GL_GIZMO_UTIL_HPP

#include <map>
#include <vector>
#include <string>
#include <wx/string.h>
#include "imgui.h"

class ImGuiWrapper;

namespace Slic3r::GUI {

class GLCanvas3D; // Forward declaration

namespace GLGizmoUtils {

// Renders a tooltip button using the provided shortcuts
void TooltipButton(
    ImGuiWrapper*                                       imgui_wrapper,
    GLCanvas3D&                                         canvas,
    const std::vector<std::pair<wxString, wxString>>&   shortcuts,
    float                                               x,
    float                                               y
);

} // namespace GLGizmoUtils
} // namespace Slic3r::GUI

#endif // GL_GIZMO_UTIL_HPP