#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include "ObjColorDialog.hpp"
#include "BitmapCache.hpp"
#include "GUI.hpp"
#include "I18N.hpp"
#include "GUI_App.hpp"
#include "MsgDialog.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/FilamentCardMixed.hpp"
#include "slic3r/Utils/ColorSpaceConvert.hpp"
#include "MainFrame.hpp"
#include "Plater.hpp"
#include "libslic3r/Config.hpp"
#include "BitmapComboBox.hpp"
#include "Widgets/ComboBox.hpp"
#include "MFDTheme.hpp"
#include "Widgets/Label.hpp"
#include <wx/choicdlg.h>
#include <wx/filedlg.h>
#include <wx/sizer.h>
#include <wx/filename.h>
#include <wx/dcgraph.h>
#include <wx/dcbuffer.h>

#include "libslic3r/ObjColorUtils.hpp"
#include "libslic3r/ImageMap/Sampling.hpp"
#include "MixedColorMatchHelpers.hpp"

using namespace Slic3r;
using namespace Slic3r::GUI;

// ---------------------------------------------------------------------------
// Helpers & constants
// ---------------------------------------------------------------------------
int objcolor_scale(const int val) { return val * Slic3r::GUI::wxGetApp().em_unit() / 10; }
int OBJCOLOR_ITEM_WIDTH() { return objcolor_scale(30); }

static const wxColour g_text_color = wxColour(107, 107, 107, 255);
const int HEADER_BORDER  = 5;
const int CONTENT_BORDER = 3;
const int PANEL_WIDTH    = 540;
const int COLOR_LABEL_WIDTH = 140;

#undef  ICON_SIZE
#define ICON_SIZE                 wxSize(FromDIP(16), FromDIP(16))
#define MIN_OBJCOLOR_DIALOG_WIDTH FromDIP(560)
#define FIX_SCROLL_HEIGTH         FromDIP(360)
#define BTN_SIZE                  wxSize(FromDIP(58), FromDIP(24))
#define BTN_GAP                   FromDIP(20)

static void update_ui(wxWindow* window)
{
    Slic3r::GUI::wxGetApp().UpdateDarkUI(window);
}

static std::vector<wxColour> wx_spectrum_colors(const std::vector<RGBA>& representative)
{
    std::vector<wxColour> colors;
    colors.reserve(representative.size());
    for (const RGBA& color : representative) {
        colors.emplace_back(std::clamp(int(std::lround(color[0] * 255.f)), 0, 255),
                            std::clamp(int(std::lround(color[1] * 255.f)), 0, 255),
                            std::clamp(int(std::lround(color[2] * 255.f)), 0, 255));
    }
    return colors;
}

static std::vector<wxColour> sampled_spectrum_colors(const std::vector<RGBA>& input_colors)
{
    return wx_spectrum_colors(ImageMap::representative_source_colors(input_colors));
}

static const char g_min_cluster_color = 1;
static const char g_max_color = 16;

// Button StateColor presets
const StateColor ok_btn_bg(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed),
                    std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                    std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
const StateColor ok_btn_disable_bg(std::pair<wxColour, int>(wxColour(205, 201, 201), StateColor::Pressed),
                                   std::pair<wxColour, int>(wxColour(205, 201, 201), StateColor::Hovered),
                                   std::pair<wxColour, int>(wxColour(205, 201, 201), StateColor::Normal));

static StateColor teal_btn_bg()
{
    return StateColor(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed),
                      std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                      std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
}
static StateColor teal_btn_bd()
{
    return StateColor(std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
}
static StateColor teal_btn_text()
{
    return StateColor(std::pair<wxColour, int>(wxColour(255, 255, 254), StateColor::Normal));
}
static StateColor disabled_btn_bg()
{
    return StateColor(std::pair<wxColour, int>(wxColour(100, 100, 100), StateColor::Pressed),
                      std::pair<wxColour, int>(wxColour(100, 100, 100), StateColor::Hovered),
                      std::pair<wxColour, int>(wxColour(100, 100, 100), StateColor::Normal));
}
static StateColor disabled_btn_bd()
{
    return StateColor(std::pair<wxColour, int>(wxColour(100, 100, 100), StateColor::Normal));
}

static void set_button_enabled(Button* btn, bool enabled)
{
    if (!btn) return;
    btn->Enable(enabled);
    btn->SetBackgroundColor(enabled ? teal_btn_bg() : disabled_btn_bg());
    btn->SetBorderColor(enabled ? teal_btn_bd() : disabled_btn_bd());
    btn->SetCursor(enabled ? wxCursor(wxCURSOR_HAND) : wxCursor(wxCURSOR_ARROW));
    btn->Refresh();
}

// ---------------------------------------------------------------------------
// Help Icon Creator (Bigger green circle with white '?' centered)
// ---------------------------------------------------------------------------
wxPanel* ObjColorPanel::create_help_icon(wxWindow* parent, const wxString& tooltip)
{
    int size = FromDIP(20);
    auto* icon_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(size, size), wxBORDER_NONE);
    icon_panel->SetMinSize(wxSize(size, size));
    icon_panel->SetMaxSize(wxSize(size, size));
    icon_panel->SetBackgroundStyle(wxBG_STYLE_PAINT);
    icon_panel->SetToolTip(tooltip);
    icon_panel->SetCursor(wxCursor(wxCURSOR_HAND));

    icon_panel->Bind(wxEVT_PAINT, [icon_panel, size](wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(icon_panel);
        wxGCDC gcdc(dc);
        wxGraphicsContext* gc = gcdc.GetGraphicsContext();
        if (!gc) return;

        wxColour bg = icon_panel->GetParent() ? icon_panel->GetParent()->GetBackgroundColour() : MFDTheme::dialog_background();
        gc->SetBrush(wxBrush(bg));
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawRectangle(0, 0, size, size);

        // Green circle fill (#009688 Orca Teal)
        wxColour green_color(0, 150, 136);
        gc->SetBrush(wxBrush(green_color));
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawEllipse(0.5, 0.5, size - 1.0, size - 1.0);

        // White bold "?" centered vertically and horizontally
        wxFont font = icon_panel->GetFont();
        font.SetPointSize(std::max(10, font.GetPointSize() + 1));
        font.SetWeight(wxFONTWEIGHT_BOLD);
        gc->SetFont(font, *wxWHITE);

        wxDouble text_w, text_h, descent, leading;
        gc->GetTextExtent("?", &text_w, &text_h, &descent, &leading);
        wxDouble x = (size - text_w) / 2.0;
        wxDouble y = (size - (text_h - descent)) / 2.0 - icon_panel->FromDIP(1.5);
        gc->DrawText("?", x, y);
    });

    return icon_panel;
}

// ---------------------------------------------------------------------------
// RGBA / wxColour conversions
// ---------------------------------------------------------------------------
static RGBA convert_to_rgba(const wxColour& color)
{
    RGBA rgba;
    rgba[0] = std::clamp(color.Red()   / 255.f, 0.f, 1.f);
    rgba[1] = std::clamp(color.Green() / 255.f, 0.f, 1.f);
    rgba[2] = std::clamp(color.Blue()  / 255.f, 0.f, 1.f);
    rgba[3] = std::clamp(color.Alpha() / 255.f, 0.f, 1.f);
    return rgba;
}

static wxColour convert_to_wxColour(const RGBA& color)
{
    return wxColour(std::clamp(int(color[0] * 255.f), 0, 255),
                    std::clamp(int(color[1] * 255.f), 0, 255),
                    std::clamp(int(color[2] * 255.f), 0, 255),
                    std::clamp(int(color[3] * 255.f), 0, 255));
}

// ---------------------------------------------------------------------------
// ObjColorDialog — thin wrapper dialog
// ---------------------------------------------------------------------------
wxBoxSizer* ObjColorDialog::create_btn_sizer(long flags)
{
    auto btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->AddStretchSpacer();

    StateColor ok_btn_bd(std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    StateColor ok_btn_text(std::pair<wxColour, int>(wxColour(255, 255, 254), StateColor::Normal));
    StateColor cancel_btn_bg(std::pair<wxColour, int>(wxColour(206, 206, 206), StateColor::Pressed),
                              std::pair<wxColour, int>(wxColour(238, 238, 238), StateColor::Hovered),
                              std::pair<wxColour, int>(wxColour(255, 255, 255), StateColor::Normal));
    StateColor cancel_btn_bd_(std::pair<wxColour, int>(wxColour(38, 46, 48), StateColor::Normal));
    StateColor cancel_btn_text(std::pair<wxColour, int>(wxColour(38, 46, 48), StateColor::Normal));

    if (flags & wxOK) {
        Button* ok_btn = new Button(this, _L("OK"));
        ok_btn->SetMinSize(BTN_SIZE);
        ok_btn->SetCornerRadius(FromDIP(12));
        ok_btn->Enable(false);
        ok_btn->SetBackgroundColor(ok_btn_disable_bg);
        ok_btn->SetBorderColor(ok_btn_bd);
        ok_btn->SetTextColor(ok_btn_text);
        ok_btn->SetCursor(wxCursor(wxCURSOR_HAND));
        ok_btn->SetFocus();
        ok_btn->SetId(wxID_OK);
        btn_sizer->Add(ok_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, BTN_GAP);
        m_button_list[wxOK] = ok_btn;
    }
    if (flags & wxCANCEL) {
        Button* cancel_btn = new Button(this, _L("Cancel"));
        cancel_btn->SetMinSize(BTN_SIZE);
        cancel_btn->SetCornerRadius(FromDIP(12));
        cancel_btn->SetBackgroundColor(cancel_btn_bg);
        cancel_btn->SetBorderColor(cancel_btn_bd_);
        cancel_btn->SetTextColor(cancel_btn_text);
        cancel_btn->SetCursor(wxCursor(wxCURSOR_HAND));
        cancel_btn->SetId(wxID_CANCEL);
        btn_sizer->Add(cancel_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, BTN_GAP);
        m_button_list[wxCANCEL] = cancel_btn;
    }
    return btn_sizer;
}

void ObjColorDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    for (auto button_item : m_button_list) {
        if (button_item.first == wxOK || button_item.first == wxCANCEL) {
            button_item.second->SetMinSize(BTN_SIZE);
            button_item.second->SetCornerRadius(FromDIP(12));
        }
    }
    m_panel_ObjColor->msw_rescale();
    this->Refresh();
}

ObjColorDialog::ObjColorDialog(wxWindow*                       parent,
                               std::vector<Slic3r::RGBA>&      input_colors,
                               bool                            is_single_color,
                               Slic3r::ObjColorImportContext&  import_context,
                               const std::vector<std::string>& extruder_colours,
                               std::vector<unsigned char>&     filament_ids,
                               unsigned char&                  first_extruder_id,
                               const std::string&              obj_filename)
    : DPIDialog(parent ? parent : static_cast<wxWindow*>(wxGetApp().mainframe),
                wxID_ANY,
                import_context.mode == ObjColorImportMode::ImageMap ? _(L("Import OBJ Image Map")) : _(L("Import OBJ Colors")),
                wxDefaultPosition,
                wxDefaultSize,
                wxDEFAULT_DIALOG_STYLE)
    , m_filament_ids(filament_ids)
    , m_first_extruder_id(first_extruder_id)
{
    SetDoubleBuffered(true);

    std::string icon_path = (boost::format("%1%/images/Snapmaker_OrcaTitle.ico") % Slic3r::resources_dir()).str();
    SetIcon(wxIcon(Slic3r::encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    auto m_line_top = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    m_line_top->SetBackgroundColour(wxColour(166, 169, 170));

    SetBackgroundColour(MFDTheme::dialog_background());
    SetMinSize(wxSize(MIN_OBJCOLOR_DIALOG_WIDTH, -1));

    m_panel_ObjColor = new ObjColorPanel(this, input_colors, is_single_color, import_context,
                                         extruder_colours, filament_ids, first_extruder_id,
                                         obj_filename);

    auto main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->Add(m_line_top, 0, wxEXPAND, 0);

    auto sizer_width = (int)(2.8 * OBJCOLOR_ITEM_WIDTH());
    sizer_width = sizer_width > MIN_OBJCOLOR_DIALOG_WIDTH ? sizer_width : MIN_OBJCOLOR_DIALOG_WIDTH;
    main_sizer->SetMinSize(wxSize(sizer_width, -1));
    main_sizer->Add(m_panel_ObjColor, 1, wxEXPAND | wxALL, 0);

    // Footer divider
    auto* footer_divider = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    footer_divider->SetBackgroundColour(MFDTheme::divider());
    main_sizer->Add(footer_divider, 0, wxEXPAND);

    auto btn_sizer = create_btn_sizer(wxOK | wxCANCEL);
    {
        m_button_list[wxOK]->Bind(wxEVT_UPDATE_UI, ([this](wxUpdateUIEvent& e) {
           if (m_panel_ObjColor->is_ok() == m_button_list[wxOK]->IsEnabled()) { return; }
           m_button_list[wxOK]->Enable(m_panel_ObjColor->is_ok());
           m_button_list[wxOK]->SetBackgroundColor(m_panel_ObjColor->is_ok() ? ok_btn_bg : ok_btn_disable_bg);
           m_button_list[wxOK]->SetCursor(m_panel_ObjColor->is_ok() ? wxCursor(wxCURSOR_HAND) : wxCursor(wxCURSOR_ARROW));
        }));
    }
    main_sizer->Add(btn_sizer, 0, wxTOP | wxBOTTOM | wxRIGHT | wxEXPAND, FromDIP(10));
    SetSizer(main_sizer);
    main_sizer->SetSizeHints(this);

    if (this->FindWindowById(wxID_OK, this)) {
        this->FindWindowById(wxID_OK, this)->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
              if (m_panel_ObjColor->update_filament_ids())
                  EndModal(wxID_OK);
            }, wxID_OK);
    }
    if (this->FindWindowById(wxID_CANCEL, this)) {
        update_ui(static_cast<wxButton*>(this->FindWindowById(wxID_CANCEL, this)));
        this->FindWindowById(wxID_CANCEL, this)->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxCANCEL); });
    }
    this->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) { EndModal(wxCANCEL); });

    wxGetApp().UpdateDlgDarkUI(this);
}

// ---------------------------------------------------------------------------
// ObjColorPanel — constructor & top-level layout
// ---------------------------------------------------------------------------
ObjColorPanel::ObjColorPanel(wxWindow*                       parent,
                             std::vector<Slic3r::RGBA>&      input_colors,
                             bool                            is_single_color,
                             Slic3r::ObjColorImportContext&  import_context,
                             const std::vector<std::string>& extruder_colours,
                             std::vector<unsigned char>&     filament_ids,
                             unsigned char&                  first_extruder_id,
                             const std::string&              obj_filename)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    , m_obj_filename(obj_filename)
    , m_input_colors(input_colors)
    , m_is_image_map(import_context.mode == ObjColorImportMode::ImageMap)
    , m_import_context(import_context)
    , m_first_extruder_id(first_extruder_id)
    , m_filament_ids(filament_ids)
{
    SetDoubleBuffered(true);

    if (input_colors.empty())
        return;

    for (const std::string& color : extruder_colours)
        m_colours.push_back(wxColor(color));

    m_input_colors_size = int(input_colors.size());
    for (size_t i = 0; i < input_colors.size(); i++) {
        if (color_is_equal(input_colors[i], UNDEFINE_COLOR) && !m_colours.empty())
            input_colors[i] = convert_to_rgba(m_colours[0]);
    }

    if (m_is_image_map)
        m_source_spectrum_colours = sampled_spectrum_colors(input_colors);

    if (is_single_color && input_colors.size() >= 1) {
        m_cluster_colors_from_algo.emplace_back(input_colors[0]);
        m_cluster_colours.emplace_back(convert_to_wxColour(input_colors[0]));
        m_cluster_labels_from_algo.reserve(m_input_colors_size);
        for (size_t i = 0; i < size_t(m_input_colors_size); i++)
            m_cluster_labels_from_algo.emplace_back(0);
        m_cluster_map_filaments.resize(m_cluster_colors_from_algo.size());
        m_color_num_recommend = m_color_cluster_num_by_algo = int(m_cluster_colors_from_algo.size());
    } else {
        deal_algo(-1);
    }

    // ----- Build the panel layout -----
    SetBackgroundColour(MFDTheme::dialog_background());

    m_sizer_simple = new wxBoxSizer(wxVERTICAL);
    m_page_simple  = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_page_simple->SetSizer(m_sizer_simple);
    m_page_simple->SetBackgroundColour(MFDTheme::dialog_background());
    update_ui(m_page_simple);

    m_sizer_simple->AddSpacer(FromDIP(8));

    // Section 1: Source
    if (m_is_image_map)
        build_source_section(m_page_simple, m_sizer_simple);

    // Section 2: Method
    if (m_is_image_map)
        build_method_section(m_page_simple, m_sizer_simple);

    // Section 3 Header: Color
    {
        auto* color_header = new wxStaticText(m_page_simple, wxID_ANY, _L("Color"));
        color_header->SetFont(Label::Head_14);
        MFDTheme::apply_text(color_header, MFDTheme::primary_text(), m_page_simple->GetBackgroundColour());
        m_sizer_simple->Add(color_header, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    }

    // Section 3: Method body panels
    // -- Standard sub-panel --
    m_standard_sub_panel = new wxPanel(m_page_simple, wxID_ANY);
    m_standard_sub_panel->SetBackgroundColour(MFDTheme::dialog_background());
    {
        auto* std_sizer = new wxBoxSizer(wxVERTICAL);
        m_standard_sub_panel->SetSizer(std_sizer);
        build_standard_body(m_standard_sub_panel, std_sizer);
    }
    m_sizer_simple->Add(m_standard_sub_panel, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));

    // -- Simple PM sub-panel --
    m_simple_pm_sub_panel = new wxPanel(m_page_simple, wxID_ANY);
    m_simple_pm_sub_panel->SetBackgroundColour(MFDTheme::dialog_background());
    {
        auto* pm_sizer = new wxBoxSizer(wxVERTICAL);
        m_simple_pm_sub_panel->SetSizer(pm_sizer);

        // Title outside card
        auto* phys_title_sizer = new wxBoxSizer(wxHORIZONTAL);
        auto* phys_title = new wxStaticText(m_simple_pm_sub_panel, wxID_ANY, _L("Physical filament"));
        phys_title->SetFont(Label::Body_14);
        MFDTheme::apply_text(phys_title, MFDTheme::secondary_text(), m_simple_pm_sub_panel->GetBackgroundColour());
        phys_title_sizer->Add(phys_title, 0, wxALIGN_CENTER_VERTICAL);
        phys_title_sizer->AddSpacer(FromDIP(4));
        phys_title_sizer->Add(create_help_icon(m_simple_pm_sub_panel, _L("Physical filaments dictate how good mixed filaments can match the colors")), 0, wxALIGN_CENTER_VERTICAL);
        pm_sizer->Add(phys_title_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

        // Physical filament card (swatches only)
        auto* phys_card = new wxPanel(m_simple_pm_sub_panel, wxID_ANY);
        phys_card->SetBackgroundColour(MFDTheme::content_background());
        auto* phys_card_sizer = new wxBoxSizer(wxVERTICAL);
        phys_card->SetSizer(phys_card_sizer);

        wxBoxSizer* phys_colors_sizer = new wxBoxSizer(wxHORIZONTAL);
        phys_colors_sizer->AddSpacer(FromDIP(4));
        for (size_t i = 0; i < m_colours.size(); i++) {
            auto extruder_icon_sizer = create_extruder_icon_and_rgba_sizer(phys_card, int(i), m_colours[i]);
            phys_colors_sizer->Add(extruder_icon_sizer, 0, wxALIGN_CENTER | wxALIGN_CENTER_VERTICAL, 0);
        }
        phys_card_sizer->Add(phys_colors_sizer, 0, wxEXPAND | wxALL, FromDIP(8));
        pm_sizer->Add(phys_card, 0, wxEXPAND | wxBOTTOM, FromDIP(10));

        // Spectrum title
        auto* spec_title = new wxStaticText(m_simple_pm_sub_panel, wxID_ANY, _L("Spectrum"));
        spec_title->SetFont(Label::Body_14);
        MFDTheme::apply_text(spec_title, MFDTheme::secondary_text(), m_simple_pm_sub_panel->GetBackgroundColour());
        pm_sizer->Add(spec_title, 0, wxEXPAND | wxBOTTOM, FromDIP(4));

        m_image_map_spectrum_sizer = new wxBoxSizer(wxHORIZONTAL);
        wxString spectrum_label;
        switch (import_context.source) {
        case ObjColorImportSource::ImageTexture: spectrum_label = _L("Texture colors"); break;
        case ObjColorImportSource::VertexColors: spectrum_label = _L("Vertex colors");  break;
        case ObjColorImportSource::FaceColors:   spectrum_label = _L("Material colors"); break;
        }
        auto* spectrum_card = new FilamentCardImageMap(
            m_simple_pm_sub_panel, spectrum_label, m_source_spectrum_colours, false);
        spectrum_card->SetMinSize(wxSize(FromDIP(PANEL_WIDTH - 20), FromDIP(34)));
        m_image_map_spectrum_sizer->Add(spectrum_card, 1, wxEXPAND);
        pm_sizer->Add(m_image_map_spectrum_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(10));

        // Description label for Simple PM
        auto* pm_warn = new wxStaticText(m_simple_pm_sub_panel, wxID_ANY,
            _L("Colors are sampled from the image source. One shared physical-filament sequence will reproduce them by perimeter exposure."));
        pm_warn->Wrap(FromDIP(PANEL_WIDTH - 40));
        MFDTheme::apply_text(pm_warn, MFDTheme::muted_text(), m_simple_pm_sub_panel->GetBackgroundColour());
        pm_sizer->Add(pm_warn, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
    }
    m_sizer_simple->Add(m_simple_pm_sub_panel, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));

    // -- Adaptive sub-panel --
    m_adaptive_sub_panel = new wxPanel(m_page_simple, wxID_ANY);
    m_adaptive_sub_panel->SetBackgroundColour(MFDTheme::dialog_background());
    {
        auto* adp_sizer = new wxBoxSizer(wxVERTICAL);
        m_adaptive_sub_panel->SetSizer(adp_sizer);

        // Title outside card
        auto* phys_title_sizer = new wxBoxSizer(wxHORIZONTAL);
        auto* phys_title = new wxStaticText(m_adaptive_sub_panel, wxID_ANY, _L("Physical filament"));
        phys_title->SetFont(Label::Body_14);
        MFDTheme::apply_text(phys_title, MFDTheme::secondary_text(), m_adaptive_sub_panel->GetBackgroundColour());
        phys_title_sizer->Add(phys_title, 0, wxALIGN_CENTER_VERTICAL);
        phys_title_sizer->AddSpacer(FromDIP(4));
        phys_title_sizer->Add(create_help_icon(m_adaptive_sub_panel, _L("Physical filaments dictate how good mixed filaments can match the colors")), 0, wxALIGN_CENTER_VERTICAL);
        adp_sizer->Add(phys_title_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

        // Physical filament card (swatches only)
        auto* phys_card = new wxPanel(m_adaptive_sub_panel, wxID_ANY);
        phys_card->SetBackgroundColour(MFDTheme::content_background());
        auto* phys_card_sizer = new wxBoxSizer(wxVERTICAL);
        phys_card->SetSizer(phys_card_sizer);

        wxBoxSizer* phys_colors_sizer = new wxBoxSizer(wxHORIZONTAL);
        phys_colors_sizer->AddSpacer(FromDIP(4));
        for (size_t i = 0; i < m_colours.size(); i++) {
            auto extruder_icon_sizer = create_extruder_icon_and_rgba_sizer(phys_card, int(i), m_colours[i]);
            phys_colors_sizer->Add(extruder_icon_sizer, 0, wxALIGN_CENTER | wxALIGN_CENTER_VERTICAL, 0);
        }
        phys_card_sizer->Add(phys_colors_sizer, 0, wxEXPAND | wxALL, FromDIP(8));
        adp_sizer->Add(phys_card, 0, wxEXPAND | wxBOTTOM, FromDIP(10));

        // Mapping title
        auto* map_title = new wxStaticText(m_adaptive_sub_panel, wxID_ANY, _L("Mapping"));
        map_title->SetFont(Label::Body_14);
        MFDTheme::apply_text(map_title, MFDTheme::secondary_text(), m_adaptive_sub_panel->GetBackgroundColour());
        adp_sizer->Add(map_title, 0, wxEXPAND | wxBOTTOM, FromDIP(4));

        // Adaptive cycle spectrum window
        m_adaptive_spectrum_window = new wxScrolledWindow(
            m_adaptive_sub_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL);
        m_adaptive_spectrum_window->SetBackgroundColour(m_adaptive_sub_panel->GetBackgroundColour());
        adp_sizer->Add(m_adaptive_spectrum_window, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

        // Description text for Adaptive mode (AFTER mapping spectrum window)
        m_adaptive_warning_text = new wxStaticText(m_adaptive_sub_panel, wxID_ANY, "");
        m_adaptive_warning_text->Wrap(FromDIP(PANEL_WIDTH - 40));
        MFDTheme::apply_text(m_adaptive_warning_text, MFDTheme::muted_text(), m_adaptive_sub_panel->GetBackgroundColour());
        adp_sizer->Add(m_adaptive_warning_text, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
    }
    m_sizer_simple->Add(m_adaptive_sub_panel, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));

    m_sizer = new wxBoxSizer(wxVERTICAL);
    m_sizer->Add(m_page_simple, 0, wxEXPAND, 0);
    m_sizer->SetSizeHints(this);
    SetSizer(m_sizer);
    Layout();

    deal_default_strategy_new();
    rebuild_adaptive_cycle_spectrum_table();
    update_image_map_mode_ui();
}

// ---------------------------------------------------------------------------
// Section Source
// ---------------------------------------------------------------------------
void ObjColorPanel::build_source_section(wxWindow* parent, wxBoxSizer* parent_sizer)
{
    // Section header
    auto* header = new wxStaticText(parent, wxID_ANY, _L("Source"));
    header->SetFont(Label::Head_14);
    MFDTheme::apply_text(header, MFDTheme::primary_text(), parent->GetBackgroundColour());
    parent_sizer->Add(header, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    // Card background panel
    auto* card = new wxPanel(parent, wxID_ANY);
    card->SetBackgroundColour(MFDTheme::content_background());
    auto* card_sizer = new wxBoxSizer(wxVERTICAL);
    card->SetSizer(card_sizer);

    // Row 1: File
    if (!m_obj_filename.empty()) {
        auto* file_row = new wxBoxSizer(wxHORIZONTAL);
        auto* file_label = new wxStaticText(card, wxID_ANY, _L("File:"));
        file_label->SetFont(Label::Body_14);
        MFDTheme::apply_text(file_label, MFDTheme::secondary_text(), card->GetBackgroundColour());
        m_filename_text = new wxStaticText(card, wxID_ANY, wxString::FromUTF8(m_obj_filename));
        m_filename_text->SetFont(Label::Body_14);
        MFDTheme::apply_text(m_filename_text, MFDTheme::muted_text(), card->GetBackgroundColour());
        file_row->Add(file_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
        file_row->Add(m_filename_text, 1, wxALIGN_CENTER_VERTICAL);
        card_sizer->Add(file_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));
    }

    // Row 2: Texture dropdown + "Set Texture..." button + filename
    {
        auto* source_row = new wxBoxSizer(wxHORIZONTAL);
        auto* texture_label = new wxStaticText(card, wxID_ANY, _L("Texture:"));
        texture_label->SetFont(Label::Body_14);
        MFDTheme::apply_text(texture_label, MFDTheme::secondary_text(), card->GetBackgroundColour());

        m_source_choice = new wxChoice(card, wxID_ANY);
        m_source_choice->SetCursor(wxCursor(wxCURSOR_HAND));
        int sel_idx = 0;
        int current_idx = 0;

        if (m_import_context.detected_texture_available) {
            m_source_choice->Append(_L("Detected"));
            if (m_import_context.source == ObjColorImportSource::ImageTexture && m_import_context.requested_texture_file.empty())
                sel_idx = current_idx;
            current_idx++;
        }
        if (m_import_context.texture_coordinates_available) {
            m_source_choice->Append(_L("Specified"));
            if (m_import_context.source == ObjColorImportSource::ImageTexture && !m_import_context.requested_texture_file.empty())
                sel_idx = current_idx;
            current_idx++;
        }
        if (m_import_context.vertex_colors_available) {
            m_source_choice->Append(_L("Vertex Colors"));
            if (m_import_context.source == ObjColorImportSource::VertexColors)
                sel_idx = current_idx;
            current_idx++;
        }
        if (m_import_context.face_colors_available) {
            m_source_choice->Append(_L("Material Colors"));
            if (m_import_context.source == ObjColorImportSource::FaceColors)
                sel_idx = current_idx;
            current_idx++;
        }
        if (m_source_choice->GetCount() > 0)
            m_source_choice->SetSelection(sel_idx);

        m_source_filename_text = new wxStaticText(card, wxID_ANY, "");
        m_source_filename_text->SetFont(Label::Body_14);
        MFDTheme::apply_text(m_source_filename_text, MFDTheme::muted_text(), card->GetBackgroundColour());

        m_btn_browse_texture = new Button(card, _L("Set Texture\u2026"));
        m_btn_browse_texture->SetFont(Label::Body_13);
        m_btn_browse_texture->SetMinSize(wxSize(FromDIP(95), FromDIP(22)));
        m_btn_browse_texture->SetCornerRadius(FromDIP(6));
        m_btn_browse_texture->SetBackgroundColor(teal_btn_bg());
        m_btn_browse_texture->SetBorderColor(teal_btn_bd());
        m_btn_browse_texture->SetTextColor(teal_btn_text());
        m_btn_browse_texture->SetCursor(wxCursor(wxCURSOR_HAND));

        auto update_texture_display = [this]() {
            wxString sel = m_source_choice->GetStringSelection();
            const bool is_specified = (sel == _L("Specified"));
            m_btn_browse_texture->Show(is_specified);

            wxString tex_fn;
            if (!m_import_context.requested_texture_file.empty()) {
                wxFileName fn(wxString::FromUTF8(m_import_context.requested_texture_file));
                tex_fn = fn.GetFullName();
            } else if (!m_obj_filename.empty() && m_import_context.source == ObjColorImportSource::ImageTexture) {
                wxFileName fn(wxString::FromUTF8(m_obj_filename));
                fn.SetExt("png");
                tex_fn = fn.GetFullName();
            }
            m_source_filename_text->SetLabelText(tex_fn);
            m_source_filename_text->Show(is_specified || sel == _L("Detected"));
        };

        update_texture_display();

        m_btn_browse_texture->Bind(wxEVT_BUTTON, [this, update_texture_display](wxCommandEvent&) {
            wxFileDialog texture_dialog(this, _L("Choose an image texture for the OBJ"), wxEmptyString, wxEmptyString,
                _L("Image files (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg|PNG files (*.png)|*.png|JPEG files (*.jpg;*.jpeg)|*.jpg;*.jpeg"),
                wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if (texture_dialog.ShowModal() != wxID_OK)
                return;

            Freeze();
            m_import_context.requested_texture_file = into_u8(texture_dialog.GetPath());
            m_import_context.source = ObjColorImportSource::ImageTexture;
            update_texture_display();
            update_image_map_mode_ui();
            Thaw();
        });

        m_source_choice->Bind(wxEVT_CHOICE, [this, update_texture_display](wxCommandEvent&) {
            Freeze();
            wxString sel_str = m_source_choice->GetStringSelection();
            if (sel_str == _L("Specified")) {
                m_import_context.source = ObjColorImportSource::ImageTexture;
            } else if (sel_str == _L("Detected")) {
                m_import_context.source = ObjColorImportSource::ImageTexture;
                m_import_context.requested_texture_file.clear();
            } else if (sel_str == _L("Vertex Colors")) {
                m_import_context.source = ObjColorImportSource::VertexColors;
            } else if (sel_str == _L("Material Colors")) {
                m_import_context.source = ObjColorImportSource::FaceColors;
            }
            update_texture_display();
            update_image_map_mode_ui();
            Thaw();
        });

        source_row->Add(texture_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
        source_row->Add(m_source_choice, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        source_row->Add(m_btn_browse_texture, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        source_row->Add(m_source_filename_text, 1, wxALIGN_CENTER_VERTICAL);
        card_sizer->Add(source_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));
    }
    card_sizer->AddSpacer(FromDIP(10));

    parent_sizer->Add(card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    // Divider
    auto* div = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    div->SetBackgroundColour(MFDTheme::divider());
    parent_sizer->Add(div, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
}

// ---------------------------------------------------------------------------
// Section Method
// ---------------------------------------------------------------------------
void ObjColorPanel::build_method_section(wxWindow* parent, wxBoxSizer* parent_sizer)
{
    // Section header
    auto* header = new wxStaticText(parent, wxID_ANY, _L("Method"));
    header->SetFont(Label::Head_14);
    MFDTheme::apply_text(header, MFDTheme::primary_text(), parent->GetBackgroundColour());
    parent_sizer->Add(header, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    const int initial_mode = m_import_context.image_map_render_mode == ObjImageMapRenderMode::PerimeterModulationV2 ? 1 :
                             m_import_context.image_map_render_mode == ObjImageMapRenderMode::AdaptiveLocalizedCycles ? 2 : 0;

    auto make_radio_row = [&](const wxString& label, const wxString& tooltip, int idx) -> wxRadioButton* {
        auto* row_sizer = new wxBoxSizer(wxHORIZONTAL);
        long style = (idx == 0) ? wxRB_GROUP : 0;
        auto* rb = new wxRadioButton(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, style);
        rb->SetFont(Label::Body_14);
        rb->SetCursor(wxCursor(wxCURSOR_HAND));
        MFDTheme::apply_text(rb, MFDTheme::primary_text(), parent->GetBackgroundColour());
        rb->SetValue(initial_mode == idx);

        auto* help = create_help_icon(parent, tooltip);

        row_sizer->Add(rb, 0, wxALIGN_CENTER_VERTICAL);
        row_sizer->AddSpacer(FromDIP(4));
        row_sizer->Add(help, 0, wxALIGN_CENTER_VERTICAL);

        parent_sizer->Add(row_sizer, 0, wxLEFT | wxBOTTOM, FromDIP(14));
        return rb;
    };

    m_method_standard_radio = make_radio_row(
        _L("Standard"),
        _L("Quantizes object colors into distinct color clusters and maps each cluster to a physical filament or a generated mixed filament."),
        0);
    m_method_simple_pm_radio = make_radio_row(
        _L("Simple Perimeter Modulation"),
        _L("Uses one shared physical filament sequence per layer. Color variations are created by modulating perimeter width and exposure."),
        1);
    m_method_adaptive_radio = make_radio_row(
        _L("Adaptive Perimeter Modulation"),
        _L("Clusters localized color regions and assigns a specialized mixed-filament cycle to each region for enhanced local color fidelity."),
        2);

    auto on_method_change = [this](wxCommandEvent&) {
        Freeze();
        if (uses_adaptive_local_cycles_image_map())
            deal_add_btn();
        rebuild_adaptive_cycle_spectrum_table();
        update_image_map_mode_ui();
        Thaw();
    };
    m_method_standard_radio->Bind(wxEVT_RADIOBUTTON, on_method_change);
    m_method_simple_pm_radio->Bind(wxEVT_RADIOBUTTON, on_method_change);
    m_method_adaptive_radio->Bind(wxEVT_RADIOBUTTON, on_method_change);

    // Divider
    auto* div = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    div->SetBackgroundColour(MFDTheme::divider());
    parent_sizer->Add(div, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
}

// ---------------------------------------------------------------------------
// Section Color (Standard mode body: quantized settings + 3-column table)
// ---------------------------------------------------------------------------
void ObjColorPanel::build_standard_body(wxWindow* parent, wxBoxSizer* parent_sizer)
{
    // 1. Physical filament section (Title OUTSIDE card)
    {
        auto* phys_title_sizer = new wxBoxSizer(wxHORIZONTAL);
        auto* phys_title = new wxStaticText(parent, wxID_ANY, _L("Physical filament"));
        phys_title->SetFont(Label::Body_14);
        MFDTheme::apply_text(phys_title, MFDTheme::secondary_text(), parent->GetBackgroundColour());
        phys_title_sizer->Add(phys_title, 0, wxALIGN_CENTER_VERTICAL);
        phys_title_sizer->AddSpacer(FromDIP(4));
        phys_title_sizer->Add(create_help_icon(parent, _L("Physical filaments dictate how good mixed filaments can match the colors")), 0, wxALIGN_CENTER_VERTICAL);
        parent_sizer->Add(phys_title_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

        // Card containing swatches only
        auto* phys_card = new wxPanel(parent, wxID_ANY);
        phys_card->SetBackgroundColour(MFDTheme::content_background());
        auto* phys_card_sizer = new wxBoxSizer(wxVERTICAL);
        phys_card->SetSizer(phys_card_sizer);

        wxBoxSizer* phys_colors_sizer = new wxBoxSizer(wxHORIZONTAL);
        phys_colors_sizer->AddSpacer(FromDIP(4));
        for (size_t i = 0; i < m_colours.size(); i++) {
            auto extruder_icon_sizer = create_extruder_icon_and_rgba_sizer(phys_card, int(i), m_colours[i]);
            phys_colors_sizer->Add(extruder_icon_sizer, 0, wxALIGN_CENTER | wxALIGN_CENTER_VERTICAL, 0);
        }
        m_physical_colors_sizer = phys_colors_sizer;
        phys_card_sizer->Add(phys_colors_sizer, 0, wxEXPAND | wxALL, FromDIP(8));
        parent_sizer->Add(phys_card, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
    }

    // 2. Quantized colors section: Title row (NO colon)
    {
        m_color_cluster_title = new wxStaticText(parent, wxID_ANY, _L("Quantized colors"));
        m_color_cluster_title->SetFont(Label::Body_14);
        m_color_cluster_title->SetToolTip(_L("Number of source-color clusters mapped to printable filaments."));
        MFDTheme::apply_text(m_color_cluster_title, MFDTheme::secondary_text(), parent->GetBackgroundColour());
        parent_sizer->Add(m_color_cluster_title, 0, wxEXPAND | wxBOTTOM, FromDIP(4));

        // Controls on row BELOW title: [-] [input] [+] [Recommended (X)]
        wxBoxSizer* quant_row = new wxBoxSizer(wxHORIZONTAL);

        // [-] button
        m_btn_quant_minus = new Button(parent, "-");
        m_btn_quant_minus->SetMinSize(wxSize(FromDIP(24), FromDIP(24)));
        m_btn_quant_minus->SetCornerRadius(FromDIP(6));
        m_btn_quant_minus->SetBackgroundColor(teal_btn_bg());
        m_btn_quant_minus->SetBorderColor(teal_btn_bd());
        m_btn_quant_minus->SetTextColor(teal_btn_text());
        m_btn_quant_minus->SetCursor(wxCursor(wxCURSOR_HAND));
        m_btn_quant_minus->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            int val = wxAtoi(m_color_cluster_num_by_user_ebox->GetValue());
            if (val > g_min_cluster_color) {
                m_color_cluster_num_by_user_ebox->SetValue(wxString::Format("%d", val - 1));
                wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, m_color_cluster_num_by_user_ebox->GetId());
                m_color_cluster_num_by_user_ebox->GetEventHandler()->ProcessEvent(evt);
            }
        });
        quant_row->Add(m_btn_quant_minus, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));

        // Input textctrl
        m_color_cluster_num_by_user_ebox = new wxTextCtrl(parent, wxID_ANY, wxEmptyString,
            wxDefaultPosition, wxSize(FromDIP(45), FromDIP(24)), wxTE_PROCESS_ENTER);
        m_color_cluster_num_by_user_ebox->SetValue(std::to_string(m_color_cluster_num_by_algo).c_str());
        MFDTheme::apply_input(m_color_cluster_num_by_user_ebox);
        {
            auto on_apply = [this](wxEvent& e) {
                wxString str = m_color_cluster_num_by_user_ebox->GetValue();
                int number = wxAtoi(str);
                if (number > m_color_num_recommend || number < g_min_cluster_color) {
                    number = std::clamp(number, int(g_min_cluster_color), m_color_num_recommend);
                    m_color_cluster_num_by_user_ebox->SetValue(wxString::Format("%d", number));
                }
                update_quantized_button_states();
                e.Skip();
            };
            m_color_cluster_num_by_user_ebox->Bind(wxEVT_TEXT_ENTER, on_apply);
            m_color_cluster_num_by_user_ebox->Bind(wxEVT_KILL_FOCUS, on_apply);
            m_color_cluster_num_by_user_ebox->Bind(wxEVT_COMMAND_TEXT_UPDATED, [this](wxCommandEvent&) {
                wxString str = m_color_cluster_num_by_user_ebox->GetValue();
                int number = wxAtoi(str);
                if (number > m_color_num_recommend || number < g_min_cluster_color) {
                    number = std::clamp(number, int(g_min_cluster_color), m_color_num_recommend);
                    m_color_cluster_num_by_user_ebox->SetValue(wxString::Format("%d", number));
                    m_color_cluster_num_by_user_ebox->SetInsertionPointEnd();
                }
                update_quantized_button_states();
                if (m_last_cluster_num != number) {
                    deal_algo(char(number), true);
                    deal_default_strategy_new();
                    rebuild_color_table();
                    Layout();
                    Refresh();
                    Update();
                    m_last_cluster_num = number;
                }
            });
        }
        quant_row->Add(m_color_cluster_num_by_user_ebox, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));

        // [+] button
        m_btn_quant_plus = new Button(parent, "+");
        m_btn_quant_plus->SetMinSize(wxSize(FromDIP(24), FromDIP(24)));
        m_btn_quant_plus->SetCornerRadius(FromDIP(6));
        m_btn_quant_plus->SetBackgroundColor(teal_btn_bg());
        m_btn_quant_plus->SetBorderColor(teal_btn_bd());
        m_btn_quant_plus->SetTextColor(teal_btn_text());
        m_btn_quant_plus->SetCursor(wxCursor(wxCURSOR_HAND));
        m_btn_quant_plus->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            int val = wxAtoi(m_color_cluster_num_by_user_ebox->GetValue());
            if (val < m_color_num_recommend) {
                m_color_cluster_num_by_user_ebox->SetValue(wxString::Format("%d", val + 1));
                wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, m_color_cluster_num_by_user_ebox->GetId());
                m_color_cluster_num_by_user_ebox->GetEventHandler()->ProcessEvent(evt);
            }
        });
        quant_row->Add(m_btn_quant_plus, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));

        // [Recommended (X)] button
        m_btn_recommended = new Button(parent, wxString::Format(_L("Recommended (%d)"), m_color_num_recommend));
        m_btn_recommended->SetFont(Label::Body_13);
        m_btn_recommended->SetMinSize(wxSize(FromDIP(130), FromDIP(24)));
        m_btn_recommended->SetCornerRadius(FromDIP(6));
        m_btn_recommended->SetBackgroundColor(teal_btn_bg());
        m_btn_recommended->SetBorderColor(teal_btn_bd());
        m_btn_recommended->SetTextColor(teal_btn_text());
        m_btn_recommended->SetCursor(wxCursor(wxCURSOR_HAND));
        m_btn_recommended->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            m_color_cluster_num_by_user_ebox->SetValue(wxString::Format("%d", m_color_num_recommend));
            wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, m_color_cluster_num_by_user_ebox->GetId());
            m_color_cluster_num_by_user_ebox->GetEventHandler()->ProcessEvent(evt);
        });
        quant_row->Add(m_btn_recommended, 0, wxALIGN_CENTER_VERTICAL);

        update_quantized_button_states();
        parent_sizer->Add(quant_row, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
    }

    // 3. "Mapping" title above the color grid
    {
        auto* map_title = new wxStaticText(parent, wxID_ANY, _L("Mapping"));
        map_title->SetFont(Label::Body_14);
        MFDTheme::apply_text(map_title, MFDTheme::secondary_text(), parent->GetBackgroundColour());
        parent_sizer->Add(map_title, 0, wxEXPAND | wxBOTTOM, FromDIP(4));
    }

    // 4. 3-column scrolled table (Color Grid)
    m_scrolledWindow = new wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL);
    m_scrolledWindow->SetBackgroundColour(parent->GetBackgroundColour());
    m_table_sizer = new wxBoxSizer(wxVERTICAL);
    m_scrolledWindow->SetSizer(m_table_sizer);
    m_scrolledWindow->SetScrollRate(0, 20);
    m_scrolledWindow->EnableScrolling(false, true);
    m_scrolledWindow->ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_DEFAULT);
    parent_sizer->Add(m_scrolledWindow, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    // 5. Description text BELOW the color grid table
    {
        wxBoxSizer* warn_sizer = new wxBoxSizer(wxHORIZONTAL);
        m_warning_text = new wxStaticText(parent, wxID_ANY, "");
        if (!m_import_context.warning_message.empty())
            m_warning_text->SetLabelText(wxString::FromUTF8(m_import_context.warning_message));
        else
            m_warning_text->SetLabelText(_L("Review the color mappings above. Rows defaulting to 'Existing Filament' are close color matches."));
        m_warning_text->Wrap(FromDIP(PANEL_WIDTH - 40));
        MFDTheme::apply_text(m_warning_text, MFDTheme::muted_text(), parent->GetBackgroundColour());
        warn_sizer->Add(m_warning_text, 1, wxEXPAND);
        parent_sizer->Add(warn_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
    }

    // 6. Min mix component AFTER description text
    {
        wxBoxSizer* min_row = new wxBoxSizer(wxHORIZONTAL);
        auto* min_label = new wxStaticText(parent, wxID_ANY, _L("Min. mix component (generated mixes):"));
        min_label->SetFont(Label::Body_14);
        MFDTheme::apply_text(min_label, MFDTheme::secondary_text(), parent->GetBackgroundColour());
        min_row->Add(min_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));

        m_min_component_percent_ctrl = new wxSpinCtrl(parent, wxID_ANY, wxEmptyString,
            wxDefaultPosition, wxSize(FromDIP(58), -1), wxSP_ARROW_KEYS, 1, 49, 15);
        m_min_component_percent_ctrl->SetToolTip(_L("Smallest physical-filament percentage allowed in generated mixed colors."));
        m_min_component_percent_ctrl->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) {
            deal_default_strategy_new();
            rebuild_color_table();
        });
        min_row->Add(m_min_component_percent_ctrl, 0, wxALIGN_CENTER_VERTICAL);

        parent_sizer->Add(min_row, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
    }

    // Populate rows
    rebuild_color_table();
}

// ---------------------------------------------------------------------------
// Helper to update quantized minus/plus/recommended button states
// ---------------------------------------------------------------------------
void ObjColorPanel::update_quantized_button_states()
{
    if (!m_color_cluster_num_by_user_ebox) return;
    int val = wxAtoi(m_color_cluster_num_by_user_ebox->GetValue());
    set_button_enabled(m_btn_quant_minus, val > g_min_cluster_color);
    set_button_enabled(m_btn_quant_plus,  val < m_color_num_recommend);
    set_button_enabled(m_btn_recommended, val != m_color_num_recommend);
}

// ---------------------------------------------------------------------------
// Helper to update "All existing" / "All generated" button states
// ---------------------------------------------------------------------------
void ObjColorPanel::update_all_buttons_state()
{
    if (m_row_wants_mix.empty()) return;
    const bool all_existing  = std::all_of(m_row_wants_mix.begin(), m_row_wants_mix.end(), [](bool mix){ return !mix; });
    const bool all_generated = std::all_of(m_row_wants_mix.begin(), m_row_wants_mix.end(), [](bool mix){ return mix; });

    set_button_enabled(m_btn_all_existing,  !all_existing);
    set_button_enabled(m_btn_all_generated, !all_generated);
}

// ---------------------------------------------------------------------------
// ComboBox factory — physical filaments only (reduced container & dropdown width)
// ---------------------------------------------------------------------------
ComboBox* ObjColorPanel::create_physical_filament_combo(wxWindow* parent, int row_id)
{
    const double em         = Slic3r::GUI::wxGetApp().em_unit();
    const bool   thin_icon  = false;
    m_combox_icon_width     = int(std::lround((thin_icon ? 2 : 4.4) * em));
    m_combox_icon_height    = int(std::lround(2 * em));

    auto* combo = new ::ComboBox(parent, wxID_ANY, wxEmptyString,
                                 wxDefaultPosition, wxSize(FromDIP(58), -1),
                                 0, nullptr, wxCB_READONLY | CB_NO_DROP_ICON | CB_NO_TEXT);
    combo->SetMinSize(wxSize(FromDIP(58), -1));
    combo->GetDropDown().SetUseContentWidth(false);
    combo->GetDropDown().SetSize(FromDIP(70), -1);
    combo->SetCursor(wxCursor(wxCURSOR_HAND));

    for (size_t i = 0; i < m_colours.size(); ++i) {
        auto* bmp = get_extruder_color_icon(
            m_colours[i].GetAsString(wxC2S_HTML_SYNTAX).ToStdString(),
            std::to_string(i + 1), m_combox_icon_width, m_combox_icon_height);
        combo->Append(wxString::Format("%zu", i + 1), *bmp);
        combo->SetItemTooltip(int(i), m_colours[i].GetAsString(wxC2S_HTML_SYNTAX));
    }
    combo->SetSelection(0);
    combo->SetName(wxString::Format("%d", row_id));
    combo->Bind(wxEVT_COMBOBOX, [this, row_id](wxCommandEvent&) {
        on_row_combo_changed(row_id);
    });
    return combo;
}

// ---------------------------------------------------------------------------
// Build / rebuild the 3-column mapping table
// ---------------------------------------------------------------------------
void ObjColorPanel::rebuild_color_table()
{
    if (!m_scrolledWindow || !m_table_sizer)
        return;

    m_scrolledWindow->Freeze();

    // Destroy all existing children
    m_scrolledWindow->DestroyChildren();
    m_table_sizer->Clear(false);
    m_color_table_rows.clear();

    if (m_row_wants_mix.size() != m_cluster_colours.size()) {
        m_row_wants_mix.assign(m_cluster_colours.size(), true);
    }

    const wxColour row_even  = MFDTheme::content_background();
    const wxColour row_odd   = MFDTheme::dialog_background();
    const wxColour head_bg   = MFDTheme::card_background();

    // ----- Column widths (total = 510px) -----
    const int col_color_w     = FromDIP(150);
    const int col_existing_w  = FromDIP(180);
    const int col_mix_w       = FromDIP(180);
    const int total_w         = col_color_w + col_existing_w + col_mix_w;
    const int row_h           = FromDIP(32);

    auto make_cell = [&](wxWindow* row_panel, int w) -> wxPanel* {
        auto* cell = new wxPanel(row_panel, wxID_ANY, wxDefaultPosition, wxSize(w, row_h));
        cell->SetMinSize(wxSize(w, row_h));
        cell->SetMaxSize(wxSize(w, row_h));
        return cell;
    };

    // ----- Header row -----
    {
        auto* head_panel = new wxPanel(m_scrolledWindow, wxID_ANY, wxDefaultPosition, wxSize(total_w, row_h));
        head_panel->SetBackgroundColour(head_bg);
        auto* head_row = new wxBoxSizer(wxHORIZONTAL);
        head_panel->SetSizer(head_row);

        // "Color" column header
        auto* col_color_head = make_cell(head_panel, col_color_w);
        col_color_head->SetBackgroundColour(head_bg);
        {
            auto* s = new wxBoxSizer(wxHORIZONTAL);
            col_color_head->SetSizer(s);
            auto* t = new wxStaticText(col_color_head, wxID_ANY, _L("Color"));
            t->SetFont(Label::Head_13);
            MFDTheme::apply_text(t, MFDTheme::primary_text(), head_bg);
            s->Add(t, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        }
        head_row->Add(col_color_head, 0);

        // "Existing Filament" column header
        auto* col_exist_head = make_cell(head_panel, col_existing_w);
        col_exist_head->SetBackgroundColour(head_bg);
        {
            auto* s = new wxBoxSizer(wxHORIZONTAL);
            col_exist_head->SetSizer(s);
            auto* lbl = new wxStaticText(col_exist_head, wxID_ANY, _L("Existing Filament"));
            lbl->SetFont(Label::Head_13);
            MFDTheme::apply_text(lbl, MFDTheme::primary_text(), head_bg);
            s->Add(lbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        }
        head_row->Add(col_exist_head, 0);

        // "Generated Mix" column header (FULL name)
        auto* col_mix_head = make_cell(head_panel, col_mix_w);
        col_mix_head->SetBackgroundColour(head_bg);
        {
            auto* s = new wxBoxSizer(wxHORIZONTAL);
            col_mix_head->SetSizer(s);
            auto* lbl = new wxStaticText(col_mix_head, wxID_ANY, _L("Generated Mix"));
            lbl->SetFont(Label::Head_13);
            MFDTheme::apply_text(lbl, MFDTheme::primary_text(), head_bg);
            s->Add(lbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        }
        head_row->Add(col_mix_head, 0);

        m_table_sizer->Add(head_panel, 0, wxEXPAND);

        auto* head_div = new wxPanel(m_scrolledWindow, wxID_ANY, wxDefaultPosition, wxSize(total_w, 1));
        head_div->SetBackgroundColour(MFDTheme::divider());
        m_table_sizer->Add(head_div, 0, wxEXPAND);
    }

    // ----- "All" row with BUTTONS -----
    {
        const wxColour all_bg = row_even;
        auto* all_panel = new wxPanel(m_scrolledWindow, wxID_ANY, wxDefaultPosition, wxSize(total_w, row_h));
        all_panel->SetBackgroundColour(all_bg);
        auto* all_row = new wxBoxSizer(wxHORIZONTAL);
        all_panel->SetSizer(all_row);

        // Col 1: "All" text
        auto* col_all_text = make_cell(all_panel, col_color_w);
        col_all_text->SetBackgroundColour(all_bg);
        {
            auto* s = new wxBoxSizer(wxHORIZONTAL);
            col_all_text->SetSizer(s);
            auto* t = new wxStaticText(col_all_text, wxID_ANY, _L("All"));
            t->SetFont(Label::Head_13);
            MFDTheme::apply_text(t, MFDTheme::primary_text(), all_bg);
            s->Add(t, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        }
        all_row->Add(col_all_text, 0);

        // Col 2: "All existing" button
        auto* col_all_exist = make_cell(all_panel, col_existing_w);
        col_all_exist->SetBackgroundColour(all_bg);
        {
            auto* s = new wxBoxSizer(wxHORIZONTAL);
            col_all_exist->SetSizer(s);

            m_btn_all_existing = new Button(col_all_exist, _L("All existing"));
            m_btn_all_existing->SetFont(Label::Body_13);
            m_btn_all_existing->SetMinSize(wxSize(FromDIP(100), FromDIP(22)));
            m_btn_all_existing->SetCornerRadius(FromDIP(6));
            m_btn_all_existing->SetBackgroundColor(teal_btn_bg());
            m_btn_all_existing->SetBorderColor(teal_btn_bd());
            m_btn_all_existing->SetTextColor(teal_btn_text());
            m_btn_all_existing->SetCursor(wxCursor(wxCURSOR_HAND));
            m_btn_all_existing->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { on_all_existing_click(); });

            s->Add(m_btn_all_existing, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
        }
        all_row->Add(col_all_exist, 0);

        // Col 3: "All generated" button
        auto* col_all_mix = make_cell(all_panel, col_mix_w);
        col_all_mix->SetBackgroundColour(all_bg);
        {
            auto* s = new wxBoxSizer(wxHORIZONTAL);
            col_all_mix->SetSizer(s);

            m_btn_all_generated = new Button(col_all_mix, _L("All generated"));
            m_btn_all_generated->SetFont(Label::Body_13);
            m_btn_all_generated->SetMinSize(wxSize(FromDIP(105), FromDIP(22)));
            m_btn_all_generated->SetCornerRadius(FromDIP(6));
            m_btn_all_generated->SetBackgroundColor(teal_btn_bg());
            m_btn_all_generated->SetBorderColor(teal_btn_bd());
            m_btn_all_generated->SetTextColor(teal_btn_text());
            m_btn_all_generated->SetCursor(wxCursor(wxCURSOR_HAND));
            m_btn_all_generated->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { on_all_generated_click(); });

            s->Add(m_btn_all_generated, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
        }
        all_row->Add(col_all_mix, 0);

        m_table_sizer->Add(all_panel, 0, wxEXPAND);

        auto* all_div = new wxPanel(m_scrolledWindow, wxID_ANY, wxDefaultPosition, wxSize(total_w, 1));
        all_div->SetBackgroundColour(MFDTheme::divider());
        m_table_sizer->Add(all_div, 0, wxEXPAND);
    }

    // Pre-calculate physical color strings for mix predictions
    std::vector<std::string> physical_colors;
    physical_colors.reserve(m_colours.size());
    for (const wxColour& c : m_colours)
        physical_colors.emplace_back(c.GetAsString(wxC2S_HTML_SYNTAX).ToStdString());

    const MixedFilamentDisplayContext display_context = build_mixed_filament_display_context(physical_colors);

    // ----- Data rows -----
    for (size_t i = 0; i < m_cluster_colours.size(); ++i) {
        const wxColour& cluster_col = m_cluster_colours[i];
        const wxColour  row_bg      = (i % 2 == 0) ? row_odd : row_even;
        const bool      wants_mix   = m_row_wants_mix[i];

        auto* row_panel = new wxPanel(m_scrolledWindow, wxID_ANY, wxDefaultPosition, wxSize(total_w, row_h));
        row_panel->SetBackgroundColour(row_bg);
        auto* row_sizer = new wxBoxSizer(wxHORIZONTAL);
        row_panel->SetSizer(row_sizer);

        ColorTableRow row_state;
        row_state.row_panel = row_panel;

        // -- Col 1: Color cell --
        auto* col_color_cell = make_cell(row_panel, col_color_w);
        col_color_cell->SetBackgroundColour(row_bg);
        {
            auto* s = new wxBoxSizer(wxHORIZONTAL);
            col_color_cell->SetSizer(s);

            auto* icon = new wxButton(col_color_cell, wxID_ANY, {},
                                      wxDefaultPosition, ICON_SIZE, wxBORDER_NONE | wxBU_AUTODRAW);
            icon->SetBitmap(*get_extruder_color_icon(
                cluster_col.GetAsString(wxC2S_HTML_SYNTAX).ToStdString(),
                std::to_string(i + 1), FromDIP(16), FromDIP(16)));
            icon->SetCanFocus(false);
            row_state.color_icon = icon;

            auto* rgb_text = new wxStaticText(col_color_cell, wxID_ANY,
                wxString::FromUTF8(get_color_str(cluster_col)));
            rgb_text->SetFont(Label::Body_13);
            MFDTheme::apply_text(rgb_text, MFDTheme::secondary_text(), row_bg);
            row_state.rgba_text = rgb_text;

            s->AddSpacer(FromDIP(4));
            s->Add(icon, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
            s->Add(rgb_text, 0, wxALIGN_CENTER_VERTICAL);
        }
        row_sizer->Add(col_color_cell, 0);

        // -- Col 2: Existing Filament cell --
        auto* col_exist_cell = make_cell(row_panel, col_existing_w);
        col_exist_cell->SetBackgroundColour(row_bg);
        {
            auto* s = new wxBoxSizer(wxHORIZONTAL);
            col_exist_cell->SetSizer(s);

            auto* rb = new wxRadioButton(col_exist_cell, wxID_ANY, wxEmptyString,
                                         wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
            rb->SetValue(!wants_mix);
            rb->SetBackgroundColour(row_bg);
            rb->SetCursor(wxCursor(wxCURSOR_HAND));
            rb->Bind(wxEVT_RADIOBUTTON, [this, idx = int(i)](wxCommandEvent&) { on_row_existing_radio(idx); });
            row_state.existing_radio = rb;

            auto* combo = create_physical_filament_combo(col_exist_cell, int(i));
            combo->Enable(!wants_mix);
            if (!wants_mix && int(i) < int(m_cluster_map_filaments.size()) && m_cluster_map_filaments[i] >= 1) {
                const int sel = m_cluster_map_filaments[i] - 1;
                if (sel < int(combo->GetCount()))
                    combo->SetSelection(sel);
            }
            row_state.existing_combo = combo;

            s->Add(rb,    0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
            s->Add(combo, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(2));
        }
        row_sizer->Add(col_exist_cell, 0);

        // -- Col 3: Generated Mix cell (clicking swatch or label selects radio button) --
        auto* col_mix_cell = make_cell(row_panel, col_mix_w);
        col_mix_cell->SetBackgroundColour(row_bg);
        {
            auto* s = new wxBoxSizer(wxHORIZONTAL);
            col_mix_cell->SetSizer(s);

            auto* rb = new wxRadioButton(col_mix_cell, wxID_ANY, wxEmptyString);
            rb->SetValue(wants_mix);
            rb->SetBackgroundColour(row_bg);
            rb->SetCursor(wxCursor(wxCURSOR_HAND));
            rb->Bind(wxEVT_RADIOBUTTON, [this, idx = int(i)](wxCommandEvent&) { on_row_mix_radio(idx); });
            row_state.mix_radio = rb;

            // Predict mix color for this cluster
            wxColour mix_col = cluster_col;
            if (!physical_colors.empty()) {
                const MixedColorMatchRecipeResult recipe = build_best_color_match_recipe(
                    physical_colors, cluster_col, min_component_percent(),
                    display_context.physical_tds, display_context.physical_material_ids,
                    MixedFilamentManager::color_engine(),
                    MixedFilamentManager::use_td_for_color_prediction());
                if (recipe.valid)
                    mix_col = recipe.preview_color;
            }

            auto* mix_icon = new wxButton(col_mix_cell, wxID_ANY, {},
                                          wxDefaultPosition, ICON_SIZE, wxBORDER_NONE | wxBU_AUTODRAW);
            mix_icon->SetBitmap(*get_extruder_color_icon(
                mix_col.GetAsString(wxC2S_HTML_SYNTAX).ToStdString(), "", FromDIP(16), FromDIP(16)));
            mix_icon->SetCanFocus(false);
            mix_icon->SetToolTip(_L("Predicted mixed filament color preview"));
            mix_icon->SetCursor(wxCursor(wxCURSOR_HAND));
            mix_icon->Bind(wxEVT_BUTTON, [this, idx = int(i)](wxCommandEvent&) { on_row_mix_radio(idx); });
            row_state.mix_preview_icon = mix_icon;

            auto* mix_text = new wxStaticText(col_mix_cell, wxID_ANY, _L("Mix"));
            mix_text->SetFont(Label::Body_13);
            mix_text->SetCursor(wxCursor(wxCURSOR_HAND));
            MFDTheme::apply_text(mix_text, MFDTheme::secondary_text(), row_bg);
            mix_text->Bind(wxEVT_LEFT_DOWN, [this, idx = int(i)](wxMouseEvent&) { on_row_mix_radio(idx); });
            row_state.mix_preview_text = mix_text;

            s->Add(rb,        0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
            s->Add(mix_icon,  0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
            s->Add(mix_text,  0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
        }
        row_sizer->Add(col_mix_cell, 0);

        m_color_table_rows.push_back(row_state);
        m_table_sizer->Add(row_panel, 0, wxEXPAND);
    }

    // Update button states for "All existing" / "All generated"
    update_all_buttons_state();

    // Fit scrolled window height
    const int n_rows      = int(m_cluster_colours.size()) + 2; // +1 header, +1 "All" row
    const int total_h     = n_rows * (row_h + 1);
    const int visible_h   = std::min(total_h, FIX_SCROLL_HEIGTH);
    m_scrolledWindow->SetMinSize(wxSize(total_w, visible_h));
    m_scrolledWindow->SetMaxSize(wxSize(total_w, visible_h));
    m_scrolledWindow->SetVirtualSize(total_w, total_h);
    m_scrolledWindow->Layout();
    m_scrolledWindow->FitInside();
    m_scrolledWindow->Thaw();

    if (wxWindow* top = wxGetTopLevelParent(this)) {
        top->Layout();
        top->Fit();
    }
}

// ---------------------------------------------------------------------------
// Row interaction handlers
// ---------------------------------------------------------------------------
void ObjColorPanel::on_row_existing_radio(int row_id)
{
    set_row_mode(row_id, /*wants_mix=*/false);
    update_all_buttons_state();
}

void ObjColorPanel::on_row_mix_radio(int row_id)
{
    set_row_mode(row_id, /*wants_mix=*/true);
    update_all_buttons_state();
}

void ObjColorPanel::on_row_combo_changed(int row_id)
{
    if (row_id < 0 || row_id >= int(m_color_table_rows.size()))
        return;
    const auto& row = m_color_table_rows[size_t(row_id)];
    if (!row.existing_combo || row_id >= int(m_cluster_map_filaments.size()))
        return;
    m_cluster_map_filaments[size_t(row_id)] = row.existing_combo->GetSelection() + 1;
}

void ObjColorPanel::set_row_mode(int row_id, bool wants_mix, int physical_filament_idx)
{
    if (row_id < 0 || row_id >= int(m_color_table_rows.size()))
        return;

    m_row_wants_mix[size_t(row_id)] = wants_mix;
    auto& row = m_color_table_rows[size_t(row_id)];

    if (row.existing_radio) row.existing_radio->SetValue(!wants_mix);
    if (row.mix_radio)      row.mix_radio->SetValue(wants_mix);
    if (row.existing_combo) {
        row.existing_combo->Enable(!wants_mix);
        if (!wants_mix && physical_filament_idx >= 0 && physical_filament_idx < int(row.existing_combo->GetCount()))
            row.existing_combo->SetSelection(physical_filament_idx);
    }

    if (!wants_mix && row_id < int(m_cluster_map_filaments.size())) {
        const int sel = row.existing_combo ? row.existing_combo->GetSelection() : 0;
        m_cluster_map_filaments[size_t(row_id)] = sel + 1;
    } else if (wants_mix && row_id < int(m_cluster_map_filaments.size())) {
        m_cluster_map_filaments[size_t(row_id)] = 0;
    }
}

void ObjColorPanel::on_all_existing_click()
{
    Freeze();
    for (int i = 0; i < int(m_color_table_rows.size()); ++i) {
        set_row_mode(i, /*wants_mix=*/false, /*physical_filament_idx=*/-1);
    }
    update_all_buttons_state();
    Thaw();
}

void ObjColorPanel::on_all_generated_click()
{
    Freeze();
    reset_to_generated_mixes();
    update_all_buttons_state();
    Thaw();
}

// ---------------------------------------------------------------------------
// Default strategy — DeltaE76 threshold per row
// ---------------------------------------------------------------------------
void ObjColorPanel::deal_default_strategy_new()
{
    if (m_cluster_colours.empty() || m_colours.empty()) {
        m_row_wants_mix.assign(m_cluster_colours.size(), true);
        m_cluster_map_filaments.resize(m_cluster_colours.size(), 0);
        return;
    }

    m_row_wants_mix.resize(m_cluster_colours.size());
    m_cluster_map_filaments.resize(m_cluster_colours.size(), 0);

    for (size_t i = 0; i < m_cluster_colours.size(); ++i) {
        const wxColour& cluster = m_cluster_colours[i];
        float best_dist = std::numeric_limits<float>::max();
        int   best_idx  = 0;

        for (size_t j = 0; j < m_colours.size(); ++j) {
            const wxColour& phys = m_colours[j];
            float lab_c[3], lab_p[3];
            RGB2Lab(cluster.Red(), cluster.Green(), cluster.Blue(), &lab_c[0], &lab_c[1], &lab_c[2]);
            RGB2Lab(phys.Red(),    phys.Green(),    phys.Blue(),    &lab_p[0], &lab_p[1], &lab_p[2]);
            float dist = DeltaE76(lab_c[0], lab_c[1], lab_c[2], lab_p[0], lab_p[1], lab_p[2]);
            if (dist < best_dist) {
                best_dist = dist;
                best_idx  = int(j);
            }
        }

        if (best_dist < k_close_match_threshold) {
            m_row_wants_mix[i]          = false;
            m_cluster_map_filaments[i]  = best_idx + 1;
        } else {
            m_row_wants_mix[i]          = true;
            m_cluster_map_filaments[i]  = 0;
        }
    }

    if (m_warning_text) {
        m_warning_text->SetLabelText(_L("Review the color mappings above. Rows defaulting to 'Existing Filament' are close color matches."));
        m_warning_text->Wrap(FromDIP(PANEL_WIDTH - 40));
    }

    update_all_buttons_state();
}

void ObjColorPanel::reset_to_generated_mixes()
{
    m_new_add_colors.clear();
    m_row_wants_mix.assign(m_cluster_colours.size(), true);
    std::fill(m_cluster_map_filaments.begin(), m_cluster_map_filaments.end(), 0);

    for (int i = 0; i < int(m_color_table_rows.size()); ++i) {
        auto& row = m_color_table_rows[size_t(i)];
        if (row.existing_radio) row.existing_radio->SetValue(false);
        if (row.mix_radio)      row.mix_radio->SetValue(true);
        if (row.existing_combo) row.existing_combo->Enable(false);
    }

    update_all_buttons_state();
}

// ---------------------------------------------------------------------------
// update_filament_ids — lazy per-row mix creation
// ---------------------------------------------------------------------------
bool ObjColorPanel::update_filament_ids()
{
    const int existing_filament_count = int(m_colours.size());

    auto report_progress = [this](size_t current, size_t total) {
        if (!m_import_context.image_map_progress_fn) return true;
        total = std::max<size_t>(total, 1);
        const size_t stride = std::max<size_t>(total / 100, 1);
        if (current != 0 && current < total && current % stride != 0) return true;
        return report_image_map_progress(ObjImageMapProgressStage::CreateMixedFilaments,
                                         std::min(current, total), total);
    };

    auto physical_color_strings = [this]() {
        std::vector<std::string> colors;
        colors.reserve(m_colours.size());
        for (const wxColour& color : m_colours)
            colors.emplace_back(color.GetAsString(wxC2S_HTML_SYNTAX).ToStdString());
        return colors;
    };

    // --- Layer-sequence (Simple PM) ---
    if (m_is_image_map && uses_layer_sequence_image_map()) {
        if (m_source_spectrum_colours.empty() && m_cluster_colours.empty())
            return false;
        if (!report_progress(0, 1)) return false;

        const std::vector<std::string> physical_colors = physical_color_strings();
        const wxColour cadence_color = m_cluster_colours.empty() ? m_source_spectrum_colours.front() : m_cluster_colours.front();
        const MixedColorMatchCreationResult match = create_mixed_filament_color_match(
            cadence_color, physical_colors, min_component_percent(), g_max_color,
            MixedColorMatchEncoding::PerimeterModulatedLayerSequence);
        if (!report_progress(1, 1)) return false;
        if (!match.valid || match.filament_id == 0 || match.filament_id > unsigned(g_max_color)) {
            if (m_warning_text)
                m_warning_text->SetLabelText(_L("Unable to create a shared layer-sequence color within the 16 printable color slots."));
            return false;
        }
        const unsigned char cadence_filament_id = static_cast<unsigned char>(match.filament_id);
        m_filament_ids.assign(size_t(m_input_colors_size), cadence_filament_id);
        m_first_extruder_id = cadence_filament_id;
        store_layer_sequence_image_map_palette(cadence_filament_id, cadence_color);
        if (match.created && wxGetApp().plater() != nullptr)
            wxGetApp().plater()->on_filaments_change(m_colours.size());
        return !m_filament_ids.empty();
    }

    // --- Adaptive ---
    if (m_is_image_map && uses_adaptive_local_cycles_image_map() && !adaptive_cycle_mixes_ready()) {
        if (m_warning_text)
            m_warning_text->SetLabelText(_L("Unable to generate every adaptive cycle within the 16 printable color slots."));
        return false;
    }

    // --- Standard ---
    const std::vector<std::string> physical_colors = physical_color_strings();
    bool created_mixed_filament = false;

    std::vector<unsigned char> cluster_filament_ids(m_cluster_colours.size(), 0);
    const size_t total_steps = m_cluster_colours.size() + size_t(m_input_colors_size);
    size_t step = 0;
    if (!report_progress(0, total_steps)) return false;

    for (size_t ci = 0; ci < m_cluster_colours.size(); ++ci) {
        const bool row_wants_mix = (ci < m_row_wants_mix.size()) ? m_row_wants_mix[ci] : true;
        const int  stored_id     = (ci < m_cluster_map_filaments.size()) ? m_cluster_map_filaments[ci] : 0;

        if (!row_wants_mix && stored_id >= 1 && stored_id <= existing_filament_count) {
            cluster_filament_ids[ci] = static_cast<unsigned char>(stored_id);
        } else {
            const wxColour& cur_color = m_cluster_colours[ci];
            const MixedColorMatchEncoding encoding =
                uses_adaptive_local_cycles_image_map() ? MixedColorMatchEncoding::AdaptiveLocalizedCycles :
                m_is_image_map                         ? MixedColorMatchEncoding::SurfaceBias :
                                                         MixedColorMatchEncoding::LayerRatio;
            const MixedColorMatchCreationResult match = create_mixed_filament_color_match(
                cur_color, physical_colors, min_component_percent(), g_max_color, encoding);
            if (!match.valid || match.filament_id == 0 || match.filament_id > unsigned(g_max_color)) {
                if (m_warning_text)
                    m_warning_text->SetLabelText(_L("Unable to create a printable mixed color for one or more OBJ colors."));
                return false;
            }
            cluster_filament_ids[ci] = static_cast<unsigned char>(match.filament_id);
            created_mixed_filament |= match.created;
        }
        if (!report_progress(++step, total_steps)) return false;
    }

    m_filament_ids.clear();
    m_filament_ids.reserve(size_t(m_input_colors_size));
    for (size_t si = 0; si < size_t(m_input_colors_size); ++si) {
        const int label = m_cluster_labels_from_algo[si];
        m_filament_ids.emplace_back(label < int(cluster_filament_ids.size()) ? cluster_filament_ids[size_t(label)] : 1u);
        if (!report_progress(++step, total_steps)) return false;
    }
    m_first_extruder_id = cluster_filament_ids.empty() ? 1 : cluster_filament_ids[0];

    if (m_is_image_map)
        store_image_map_palette(cluster_filament_ids);
    if (created_mixed_filament && wxGetApp().plater() != nullptr)
        wxGetApp().plater()->on_filaments_change(m_colours.size());

    return !m_filament_ids.empty();
}

// ---------------------------------------------------------------------------
// msw_rescale
// ---------------------------------------------------------------------------
void ObjColorPanel::msw_rescale()
{
    for (size_t i = 0; i < m_extruder_icon_list.size(); ++i) {
        auto bitmap = *get_extruder_color_icon(m_colours[i].GetAsString(wxC2S_HTML_SYNTAX).ToStdString(),
                                                std::to_string(i + 1), FromDIP(16), FromDIP(16));
        m_extruder_icon_list[i]->SetBitmap(bitmap);
    }
    for (size_t i = 0; i < m_color_table_rows.size() && i < m_cluster_colours.size(); ++i) {
        if (m_color_table_rows[i].color_icon) {
            m_color_table_rows[i].color_icon->SetBitmap(
                *get_extruder_color_icon(m_cluster_colours[i].GetAsString(wxC2S_HTML_SYNTAX).ToStdString(),
                                         std::to_string(i + 1), FromDIP(16), FromDIP(16)));
        }
    }
}

// ---------------------------------------------------------------------------
// is_ok
// ---------------------------------------------------------------------------
bool ObjColorPanel::is_ok()
{
    if (m_is_image_map && uses_layer_sequence_image_map())
        return m_colours.size() >= 2 && !m_source_spectrum_colours.empty();
    if (m_is_image_map && uses_adaptive_local_cycles_image_map())
        return adaptive_cycle_mixes_ready() && m_adaptive_cycle_preview_valid &&
               std::all_of(m_adaptive_cycle_spectrum_colours.begin(), m_adaptive_cycle_spectrum_colours.end(),
                           [](const std::vector<wxColour>& colors) { return !colors.empty(); });

    for (size_t i = 0; i < m_cluster_colours.size(); ++i) {
        const bool wants_mix   = (i < m_row_wants_mix.size()) ? m_row_wants_mix[i] : true;
        const int  stored_id   = (i < m_cluster_map_filaments.size()) ? m_cluster_map_filaments[i] : 0;
        if (!wants_mix && (stored_id < 1 || stored_id > int(m_colours.size())))
            return false;
    }
    return !m_cluster_colours.empty();
}

// ---------------------------------------------------------------------------
// update_image_map_mode_ui — show/hide three sub-panels
// ---------------------------------------------------------------------------
void ObjColorPanel::update_image_map_mode_ui()
{
    if (!m_is_image_map)
        return;

    Freeze();

    const bool is_simple_pm = uses_layer_sequence_image_map();
    const bool is_adaptive  = uses_adaptive_local_cycles_image_map();
    const bool is_standard  = !is_simple_pm && !is_adaptive;

    if (m_standard_sub_panel)   m_standard_sub_panel->Show(is_standard);
    if (m_simple_pm_sub_panel)  m_simple_pm_sub_panel->Show(is_simple_pm);
    if (m_adaptive_sub_panel)   m_adaptive_sub_panel->Show(is_adaptive);

    if (m_color_cluster_title)
        m_color_cluster_title->SetLabelText(is_adaptive ? _L("Adaptive color regions") : _L("Quantized colors"));

    if (m_warning_text) {
        if (!m_import_context.warning_message.empty()) {
            m_warning_text->SetLabelText(wxString::FromUTF8(m_import_context.warning_message));
        } else {
            m_warning_text->SetLabelText(
                _L("Review the color mappings above. Rows defaulting to 'Existing Filament' are close color matches."));
        }
        m_warning_text->Wrap(FromDIP(PANEL_WIDTH - 40));
    }

    if (m_adaptive_warning_text) {
        if (!m_import_context.warning_message.empty()) {
            m_adaptive_warning_text->SetLabelText(wxString::FromUTF8(m_import_context.warning_message));
        } else if (!m_adaptive_cycle_preview_valid) {
            m_adaptive_warning_text->SetLabelText(_L("Unable to preview the adaptive region-to-cycle mapping."));
        } else if (!adaptive_cycle_mixes_ready()) {
            m_adaptive_warning_text->SetLabelText(
                _L("Unable to generate every adaptive cycle within the 16 printable color slots. Reduce the adaptive color region count."));
        } else {
            wxString summary = wxString::Format(
                _L("%llu adaptive color regions map to %llu unique mixed-filament cycles"),
                static_cast<unsigned long long>(m_cluster_colors_from_algo.size()),
                static_cast<unsigned long long>(m_adaptive_cycle_display_filament_ids.size()));
            if (m_adaptive_direct_physical_region_count > 0)
                summary += wxString::Format(_L(" and %llu regions use a physical filament directly"),
                    static_cast<unsigned long long>(m_adaptive_direct_physical_region_count));
            summary += _L(".");
            m_adaptive_warning_text->SetLabelText(summary);
        }
        m_adaptive_warning_text->Wrap(FromDIP(PANEL_WIDTH - 40));
    }

    m_page_simple->Layout();
    Layout();
    if (wxWindow* top = wxGetTopLevelParent(this); top != nullptr && top->GetSizer() != nullptr) {
        top->Layout();
        top->Fit();
    }

    Thaw();
}

// ---------------------------------------------------------------------------
// Radio-button query helpers
// ---------------------------------------------------------------------------
bool ObjColorPanel::uses_layer_sequence_image_map() const
{
    return m_method_simple_pm_radio != nullptr && m_method_simple_pm_radio->GetValue();
}

bool ObjColorPanel::uses_adaptive_local_cycles_image_map() const
{
    return m_method_adaptive_radio != nullptr && m_method_adaptive_radio->GetValue();
}

// ---------------------------------------------------------------------------
// Source display label
// ---------------------------------------------------------------------------
wxString ObjColorPanel::source_display_label() const
{
    switch (m_import_context.source) {
    case ObjColorImportSource::ImageTexture:
        if (!m_import_context.requested_texture_file.empty()) {
            wxFileName fn(wxString::FromUTF8(m_import_context.requested_texture_file));
            return _L("Custom") + " (" + fn.GetFullName() + ")";
        }
        return _L("Detected texture");
    case ObjColorImportSource::VertexColors:
        return _L("Vertex colors");
    case ObjColorImportSource::FaceColors:
        return _L("Material colors");
    }
    return wxEmptyString;
}

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------
wxBoxSizer* ObjColorPanel::create_extruder_icon_and_rgba_sizer(wxWindow* parent, int id, const wxColour& color)
{
    auto icon_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* icon  = new wxButton(parent, wxID_ANY, {}, wxDefaultPosition, ICON_SIZE, wxBORDER_NONE | wxBU_AUTODRAW);
    icon->SetBitmap(*get_extruder_color_icon(color.GetAsString(wxC2S_HTML_SYNTAX).ToStdString(),
                                              std::to_string(id + 1), FromDIP(16), FromDIP(16)));
    icon->SetCanFocus(false);
    m_extruder_icon_list.emplace_back(icon);
    icon_sizer->Add(icon, 0, wxALIGN_CENTER | wxALIGN_CENTER_VERTICAL, FromDIP(10));
    icon_sizer->AddSpacer(FromDIP(5));
    return icon_sizer;
}

std::string ObjColorPanel::get_color_str(const wxColour& color)
{
    return "R:" + std::to_string(color.Red()) +
           " G:" + std::to_string(color.Green()) +
           " B:" + std::to_string(color.Blue());
}

wxBoxSizer* ObjColorPanel::create_image_map_btn_sizer(wxWindow* parent)
{
    return new wxBoxSizer(wxHORIZONTAL);
}

void ObjColorPanel::store_image_map_palette(const std::vector<unsigned char>& cluster_filament_ids)
{
    if (uses_layer_sequence_image_map())
        m_import_context.image_map_render_mode = ObjImageMapRenderMode::PerimeterModulationV2;
    else if (uses_adaptive_local_cycles_image_map())
        m_import_context.image_map_render_mode = ObjImageMapRenderMode::AdaptiveLocalizedCycles;
    else
        m_import_context.image_map_render_mode = ObjImageMapRenderMode::NormalMix;

    m_import_context.image_map_minimum_component_percent = min_component_percent();
    m_import_context.image_map_palette_colors.clear();
    m_import_context.image_map_palette_filament_ids.clear();
    m_import_context.image_map_palette_mixed_stable_ids.clear();
    m_import_context.image_map_palette_colors.reserve(m_cluster_colours.size());
    m_import_context.image_map_palette_filament_ids.reserve(m_cluster_colours.size());
    m_import_context.image_map_palette_mixed_stable_ids.reserve(m_cluster_colours.size());

    PresetBundle* preset_bundle = wxGetApp().preset_bundle;
    for (size_t ci = 0; ci < m_cluster_colours.size() && ci < cluster_filament_ids.size(); ++ci) {
        const wxColour& color = m_cluster_colours[ci];
        m_import_context.image_map_palette_colors.push_back(
            RGBA{float(color.Red())/255.f, float(color.Green())/255.f, float(color.Blue())/255.f, float(color.Alpha())/255.f});
        const unsigned char filament_id = cluster_filament_ids[ci];
        m_import_context.image_map_palette_filament_ids.push_back(filament_id);

        uint64_t stable_id = 0;
        if (preset_bundle != nullptr && filament_id > m_colours.size()) {
            const std::optional<MixedFilamentDefinition> definition =
                preset_bundle->mixed_filaments.mixed_filament_definition_from_id(filament_id, m_colours.size());
            if (definition) stable_id = definition->identity.stable_id;
        }
        m_import_context.image_map_palette_mixed_stable_ids.push_back(stable_id);
    }
}

void ObjColorPanel::store_layer_sequence_image_map_palette(unsigned char filament_id, const wxColour& representative_color)
{
    m_import_context.image_map_render_mode               = ObjImageMapRenderMode::PerimeterModulationV2;
    m_import_context.image_map_minimum_component_percent = min_component_percent();
    m_import_context.image_map_palette_colors.assign(1, convert_to_rgba(representative_color));
    m_import_context.image_map_palette_filament_ids.assign(1, filament_id);
    m_import_context.image_map_palette_mixed_stable_ids.assign(1, 0);
}

void ObjColorPanel::choose_image_map_source()
{
    struct ImageSourceChoice {
        wxString             label;
        ObjColorImportSource source;
        bool                 select_texture_file{false};
    };

    std::vector<ImageSourceChoice> source_choices;
    if (m_import_context.detected_texture_available)
        source_choices.push_back({_L("Use the detected OBJ texture"), ObjColorImportSource::ImageTexture, false});
    if (m_import_context.texture_coordinates_available)
        source_choices.push_back({_L("Select a PNG or JPEG texture\u2026"), ObjColorImportSource::ImageTexture, true});
    if (m_import_context.vertex_colors_available)
        source_choices.push_back({_L("Use OBJ vertex colors"), ObjColorImportSource::VertexColors, false});
    if (m_import_context.face_colors_available)
        source_choices.push_back({_L("Use OBJ material colors"), ObjColorImportSource::FaceColors, false});

    if (source_choices.empty()) {
        MessageDialog dialog(this,
            _L("This OBJ has no UV coordinates, vertex colors, or material colors that can be used for image mapping."),
            _L("OBJ image map"), wxOK | wxICON_INFORMATION);
        dialog.ShowModal();
        return;
    }

    wxArrayString labels;
    int default_selection = 0;
    for (size_t ci = 0; ci < source_choices.size(); ++ci) {
        labels.Add(source_choices[ci].label);
        if (source_choices[ci].source == m_import_context.source &&
            !(m_import_context.source == ObjColorImportSource::ImageTexture && source_choices[ci].select_texture_file))
            default_selection = int(ci);
    }

    wxSingleChoiceDialog source_dialog(this, _L("Choose the surface color source for image mapping."),
                                       _L("OBJ image map source"), labels);
    source_dialog.SetSelection(default_selection);
    if (source_dialog.ShowModal() != wxID_OK) return;

    const int selection = source_dialog.GetSelection();
    if (selection < 0 || selection >= int(source_choices.size())) return;
    const ImageSourceChoice& choice = source_choices[size_t(selection)];

    std::string texture_file;
    if (choice.select_texture_file) {
        wxFileDialog texture_dialog(this, _L("Choose an image texture for the OBJ"), wxEmptyString, wxEmptyString,
            _L("Image files (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg|PNG files (*.png)|*.png|JPEG files (*.jpg;*.jpeg)|*.jpg;*.jpeg"),
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (texture_dialog.ShowModal() != wxID_OK) return;
        texture_file = into_u8(texture_dialog.GetPath());
    }

    Freeze();
    m_import_context.requested_source        = choice.source;
    m_import_context.requested_mode          = ObjColorImportMode::ImageMap;
    m_import_context.requested_texture_file  = std::move(texture_file);
    m_import_context.warning_message.clear();
    update_image_map_mode_ui();
    Thaw();
}

int ObjColorPanel::min_component_percent() const
{
    return m_min_component_percent_ctrl != nullptr ?
        std::clamp(m_min_component_percent_ctrl->GetValue(), 1, 49) : 15;
}

bool ObjColorPanel::colors_are_equal(const wxColour& lhs, const wxColour& rhs)
{
    return lhs.Red() == rhs.Red() && lhs.Green() == rhs.Green() &&
           lhs.Blue() == rhs.Blue() && lhs.Alpha() == rhs.Alpha();
}

int ObjColorPanel::find_filament_selection_by_color(const wxColour& color) const
{
    for (size_t i = 0; i < m_colours.size(); ++i) {
        if (colors_are_equal(m_colours[i], color))
            return int(i + 1);
    }
    for (size_t i = 0; i < m_new_add_colors.size(); ++i) {
        if (colors_are_equal(m_new_add_colors[i], color))
            return int(m_colours.size() + i + 1);
    }
    return 0;
}

int ObjColorPanel::append_new_filament_option(const wxColour& color,
                                               std::vector<unsigned int>* component_filament_ids)
{
    if (m_colours.size() < 2 || m_colours.size() + m_new_add_colors.size() >= g_max_color)
        return 0;

    std::vector<std::string> physical_colors;
    physical_colors.reserve(m_colours.size());
    for (const wxColour& c : m_colours)
        physical_colors.emplace_back(c.GetAsString(wxC2S_HTML_SYNTAX).ToStdString());

    const MixedFilamentDisplayContext context = build_mixed_filament_display_context(physical_colors);
    const MixedColorMatchRecipeResult recipe = build_best_color_match_recipe(
        physical_colors, color, min_component_percent(),
        context.physical_tds, context.physical_material_ids,
        MixedFilamentManager::color_engine(),
        MixedFilamentManager::use_td_for_color_prediction());
    if (!recipe.valid) return 0;

    if (component_filament_ids != nullptr) {
        component_filament_ids->clear();
        const MixedFilamentDefinition definition = mixed_filament_definition_from_color_match_recipe(
            recipe, physical_colors.size(), MixedColorMatchEncoding::AdaptiveLocalizedCycles);
        for (const MixedFilamentWeightedComponent& component : definition.recipe.blend.components) {
            if (component.percent > 0 && component.filament.id >= 1 && component.filament.id <= physical_colors.size())
                component_filament_ids->push_back(component.filament.id);
        }
    }
    return append_new_filament_color_option(color);
}

int ObjColorPanel::append_new_filament_color_option(const wxColour& color)
{
    if (m_colours.size() + m_new_add_colors.size() >= g_max_color)
        return 0;
    m_new_add_colors.emplace_back(color);
    return int(m_colours.size() + m_new_add_colors.size());
}

void ObjColorPanel::deal_reset_btn()
{
    m_new_add_colors.clear();
    std::fill(m_cluster_map_filaments.begin(), m_cluster_map_filaments.end(), 0);
    if (m_warning_text) m_warning_text->SetLabelText("");
}

void ObjColorPanel::deal_add_btn()
{
    if (m_colours.size() >= g_max_color) return;
    deal_reset_btn();

    if (uses_adaptive_local_cycles_image_map()) {
        std::vector<std::string> physical_colors;
        physical_colors.reserve(m_colours.size());
        for (const wxColour& c : m_colours)
            physical_colors.emplace_back(c.GetAsString(wxC2S_HTML_SYNTAX).ToStdString());

        std::vector<wxColour> target_colors;
        target_colors.reserve(m_cluster_colors_from_algo.size());
        for (const RGBA& t : m_cluster_colors_from_algo)
            target_colors.emplace_back(convert_to_wxColour(t));

        const AdaptiveColorMatchPreviewResult preview = preview_adaptive_localized_color_matches(
            target_colors, physical_colors, min_component_percent(), g_max_color);
        if (!preview.valid) {
            update_adaptive_cycle_spectra(&preview);
            if (m_warning_text) m_warning_text->SetLabelText(_L("Unable to generate the adaptive region-to-cycle mapping."));
            return;
        }

        std::map<unsigned int, int> cycle_selections;
        for (const AdaptiveColorMatchPreviewCycle& cycle : preview.mixed_cycles) {
            if (cycle.target_indices.empty() || cycle.target_indices.front() >= target_colors.size())
                continue;
            const int selection = append_new_filament_color_option(target_colors[cycle.target_indices.front()]);
            if (selection == 0) {
                if (m_warning_text) m_warning_text->SetLabelText(_L("The adaptive cycles would exceed the 16 printable color slots."));
                return;
            }
            cycle_selections.emplace(cycle.filament_id, selection);
        }

        for (size_t ri = 0; ri < preview.target_filament_ids.size() && ri < m_cluster_map_filaments.size(); ++ri) {
            const unsigned int preview_filament_id = preview.target_filament_ids[ri];
            int selection = int(preview_filament_id);
            if (preview_filament_id > m_colours.size()) {
                const auto found = cycle_selections.find(preview_filament_id);
                if (found == cycle_selections.end()) {
                    if (m_warning_text) m_warning_text->SetLabelText(_L("Unable to map one or more adaptive color regions to a physical cycle."));
                    return;
                }
                selection = found->second;
            }
            m_cluster_map_filaments[ri] = selection;
        }
        update_adaptive_cycle_spectra(&preview);
        return;
    }

    bool is_exceed = false;
    std::vector<int> appended_selections;
    appended_selections.reserve(m_cluster_colors_from_algo.size());
    for (size_t i = 0; i < m_cluster_colors_from_algo.size(); i++) {
        const wxColour cur_color = convert_to_wxColour(m_cluster_colors_from_algo[i]);
        const int selection = append_new_filament_option(cur_color);
        if (selection == 0) { is_exceed = true; break; }
        appended_selections.emplace_back(selection);
    }
    if (is_exceed) {
        if (m_warning_text) m_warning_text->SetLabelText(_L("Some colors could not be generated within the 16 printable color slots."));
        return;
    }
    for (size_t i = 0; i < m_cluster_colours.size() && i < appended_selections.size(); i++)
        m_cluster_map_filaments[i] = appended_selections[i];

    update_adaptive_cycle_spectra();
}

void ObjColorPanel::deal_algo(char cluster_number, bool redraw_ui)
{
    if (m_last_cluster_number == cluster_number) return;
    const char previous_cluster_number = m_last_cluster_number;
    m_last_cluster_number = cluster_number;

    QuantKMeans quant(10);
    const size_t existing_mixed = wxGetApp().preset_bundle != nullptr ?
        wxGetApp().preset_bundle->mixed_filaments.visible_count() : 0;
    const int printable_mix_slots = std::max(1, int(g_max_color) - int(m_colours.size()) - int(existing_mixed));
    const bool completed = quant.apply(
        m_input_colors, m_cluster_colors_from_algo, m_cluster_labels_from_algo,
        int(cluster_number), printable_mix_slots, 2,
        [this](int current, int total) {
            return report_image_map_progress(ObjImageMapProgressStage::QuantizeColors,
                                             size_t(std::max(current, 0)),
                                             size_t(std::max(total, 1)));
        });
    if (!completed) { m_last_cluster_number = previous_cluster_number; return; }

    m_cluster_colours.clear();
    m_cluster_colours.reserve(m_cluster_colors_from_algo.size());
    for (const RGBA& c : m_cluster_colors_from_algo)
        m_cluster_colours.emplace_back(convert_to_wxColour(c));

    if (m_cluster_colours.empty()) return;
    m_cluster_map_filaments.resize(m_cluster_colors_from_algo.size());
    m_color_cluster_num_by_algo = int(m_cluster_colors_from_algo.size());
    if (cluster_number == -1)
        m_color_num_recommend = m_color_cluster_num_by_algo;

    if (redraw_ui) {
        rebuild_adaptive_cycle_spectrum_table();
    }
}

bool ObjColorPanel::adaptive_cycle_mixes_ready() const
{
    if (m_colours.size() < 2 || m_cluster_map_filaments.size() < m_cluster_colors_from_algo.size() ||
        m_cluster_colors_from_algo.empty() || !m_adaptive_cycle_preview_valid)
        return false;
    return std::all_of(m_cluster_map_filaments.begin(),
                       m_cluster_map_filaments.begin() + m_cluster_colors_from_algo.size(),
                       [](int id) { return id > 0; });
}

void ObjColorPanel::update_adaptive_cycle_spectra(const AdaptiveColorMatchPreviewResult* supplied_preview)
{
    m_adaptive_cycle_spectrum_colours.clear();
    m_adaptive_cycle_display_component_filament_ids.clear();
    m_adaptive_cycle_display_filament_ids.clear();
    m_adaptive_cycle_display_region_counts.clear();
    m_adaptive_direct_physical_region_count = 0;
    m_adaptive_cycle_preview_valid          = false;

    const bool adaptive_requested =
        uses_adaptive_local_cycles_image_map() ||
        (m_method_adaptive_radio == nullptr &&
         m_import_context.image_map_render_mode == ObjImageMapRenderMode::AdaptiveLocalizedCycles);
    if (!m_is_image_map || !adaptive_requested || m_cluster_colors_from_algo.empty())
        return;

    const std::vector<std::vector<RGBA>> representative = ImageMap::representative_labeled_source_colors(
        m_input_colors, m_cluster_labels_from_algo, m_cluster_colors_from_algo.size(), 64, 512);
    std::vector<std::string> physical_colors;
    physical_colors.reserve(m_colours.size());
    for (const wxColour& c : m_colours)
        physical_colors.emplace_back(c.GetAsString(wxC2S_HTML_SYNTAX).ToStdString());
    const MixedFilamentDisplayContext context = build_mixed_filament_display_context(physical_colors);

    std::vector<wxColour> target_colors;
    target_colors.reserve(m_cluster_colors_from_algo.size());
    for (const RGBA& t : m_cluster_colors_from_algo)
        target_colors.emplace_back(convert_to_wxColour(t));

    AdaptiveColorMatchPreviewResult computed_preview;
    if (supplied_preview == nullptr) {
        computed_preview = preview_adaptive_localized_color_matches(
            target_colors, physical_colors, min_component_percent(), g_max_color);
        supplied_preview = &computed_preview;
    }
    const AdaptiveColorMatchPreviewResult& preview = *supplied_preview;
    if (!preview.valid) return;

    m_adaptive_cycle_preview_valid          = true;
    m_adaptive_direct_physical_region_count = preview.direct_physical_target_count;
    m_adaptive_cycle_spectrum_colours.reserve(preview.mixed_cycles.size());
    m_adaptive_cycle_display_component_filament_ids.reserve(preview.mixed_cycles.size());
    m_adaptive_cycle_display_filament_ids.reserve(preview.mixed_cycles.size());
    m_adaptive_cycle_display_region_counts.reserve(preview.mixed_cycles.size());

    for (const AdaptiveColorMatchPreviewCycle& cycle : preview.mixed_cycles) {
        std::vector<unsigned int> component_filament_ids;
        for (const MixedFilamentWeightedComponent& comp : cycle.definition.recipe.blend.components) {
            if (comp.percent > 0 && comp.filament.id >= 1 && comp.filament.id <= m_colours.size())
                component_filament_ids.emplace_back(comp.filament.id);
        }
        std::vector<RGBA> represented_colors;
        for (const size_t target_index : cycle.target_indices) {
            if (target_index >= representative.size()) continue;
            represented_colors.insert(represented_colors.end(),
                                      representative[target_index].begin(), representative[target_index].end());
        }
        represented_colors = ImageMap::representative_source_colors(represented_colors, 64, 512);
        std::vector<wxColour> attainable =
            build_adaptive_cycle_attainable_colors(component_filament_ids, represented_colors, context);

        m_adaptive_cycle_spectrum_colours.emplace_back(
            attainable.empty() ? wx_spectrum_colors(represented_colors) : std::move(attainable));
        m_adaptive_cycle_display_component_filament_ids.emplace_back(std::move(component_filament_ids));
        m_adaptive_cycle_display_filament_ids.emplace_back(cycle.filament_id);
        m_adaptive_cycle_display_region_counts.emplace_back(cycle.target_indices.size());
    }
}

void ObjColorPanel::rebuild_adaptive_cycle_spectrum_table()
{
    if (!m_adaptive_spectrum_window) return;

    m_adaptive_spectrum_window->Freeze();
    wxSizer* spectrum_sizer = m_adaptive_spectrum_window->GetSizer();
    if (!spectrum_sizer) {
        spectrum_sizer = new wxBoxSizer(wxVERTICAL);
        m_adaptive_spectrum_window->SetSizer(spectrum_sizer);
    } else {
        spectrum_sizer->Clear(true);
    }

    update_adaptive_cycle_spectra();

    for (size_t ci = 0; ci < m_adaptive_cycle_spectrum_colours.size(); ++ci) {
        std::vector<FilamentCardImageMap::ComponentFilament> component_filaments;
        if (ci < m_adaptive_cycle_display_component_filament_ids.size()) {
            for (unsigned int fid : m_adaptive_cycle_display_component_filament_ids[ci]) {
                if (fid >= 1 && fid <= m_colours.size())
                    component_filaments.emplace_back(fid, m_colours[fid - 1]);
            }
        }
        const unsigned int filament_id = ci < m_adaptive_cycle_display_filament_ids.size() ?
                                             m_adaptive_cycle_display_filament_ids[ci] :
                                             unsigned(m_colours.size() + ci + 1);
        const size_t region_count = ci < m_adaptive_cycle_display_region_counts.size() ?
                                        m_adaptive_cycle_display_region_counts[ci] : 0;
        auto* card = new FilamentCardImageMap(
            m_adaptive_spectrum_window,
            wxString::Format(_L("Mixed filament %u \u2014 %llu regions"),
                             filament_id, static_cast<unsigned long long>(region_count)),
            m_adaptive_cycle_spectrum_colours[ci], false,
            wxString::Format(
                _L("%llu adaptive color regions use this cycle."),
                static_cast<unsigned long long>(region_count)),
            std::move(component_filaments));
        card->SetMinSize(wxSize(FromDIP(PANEL_WIDTH - 20), FromDIP(34)));
        spectrum_sizer->Add(card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(5));
    }

    const int total_height   = std::max(FromDIP(40), int(m_adaptive_cycle_spectrum_colours.size()) * FromDIP(39));
    const int visible_height = std::min(total_height, FIX_SCROLL_HEIGTH);
    m_adaptive_spectrum_window->SetMinSize(wxSize(FromDIP(PANEL_WIDTH - 10), visible_height));
    m_adaptive_spectrum_window->SetMaxSize(wxSize(FromDIP(PANEL_WIDTH - 10), visible_height));
    m_adaptive_spectrum_window->SetVirtualSize(FromDIP(PANEL_WIDTH - 10), total_height);
    m_adaptive_spectrum_window->EnableScrolling(false, true);
    m_adaptive_spectrum_window->ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_DEFAULT);
    m_adaptive_spectrum_window->SetScrollRate(20, 20);
    m_adaptive_spectrum_window->Layout();
    m_adaptive_spectrum_window->FitInside();
    m_adaptive_spectrum_window->Thaw();
}

bool ObjColorPanel::report_image_map_progress(ObjImageMapProgressStage stage, size_t current, size_t total)
{
    if (!m_import_context.image_map_progress_fn) return true;
    const bool keep_going = m_import_context.image_map_progress_fn(stage, current, total);
    if (!keep_going) {
        if (auto* dialog = dynamic_cast<wxDialog*>(wxGetTopLevelParent(this));
            dialog != nullptr && dialog->IsModal())
            dialog->EndModal(wxID_CANCEL);
    }
    return keep_going;
}
