// ============================================================================
//  ToolBoxPanel.cpp  --  左侧元件库 / 工具面板
// ============================================================================
#include "ToolBoxPanel.h"
#include "MainFrame.h"
#include "Renderer.h"
#include "Ids.h"

#include <wx/dcmemory.h>
#include <wx/statline.h>

#include <algorithm>

namespace lc {

// ---------------------------------------------------------------- 小图标
static wxBitmap MakeSmallIcon(int kind, int size = 22)
{
    wxBitmap bmp(size, size, 24);
    wxMemoryDC dc(bmp);
    dc.SetBackground(wxBrush(wxColour(250, 250, 250)));
    dc.Clear();
    dc.SetPen(wxPen(wxColour(40, 40, 40), 2));
    dc.SetBrush(wxBrush(wxColour(70, 110, 180)));

    switch (kind) {
    case TOOL_EDIT: {
        wxPoint p[7] = { wxPoint(6, 3), wxPoint(6, 17), wxPoint(10, 13), wxPoint(13, 19), wxPoint(15, 18), wxPoint(12, 12), wxPoint(17, 11) };
        dc.DrawPolygon(7, p);
        break; }
    case TOOL_POKE: {
        dc.SetBrush(wxBrush(wxColour(230, 180, 60)));
        wxPoint p[8] = { wxPoint(8, 19), wxPoint(8, 10), wxPoint(10, 10), wxPoint(10, 4),
                         wxPoint(12, 4), wxPoint(12, 10), wxPoint(16, 11), wxPoint(16, 19) };
        dc.DrawPolygon(8, p);
        break; }
    case TOOL_WIRE: {
        dc.SetPen(wxPen(wxColour(0, 150, 0), 2));
        wxPoint p[4] = { wxPoint(3, 17), wxPoint(3, 9), wxPoint(13, 9), wxPoint(13, 4) };
        dc.DrawLines(4, p);
        dc.SetPen(wxPen(wxColour(40, 40, 40), 2));
        dc.DrawCircle(3, 17, 2);
        dc.DrawCircle(13, 4, 2);
        break; }
    case TOOL_TEXT: {
        dc.SetFont(wxFont(wxFontInfo(12).Bold()));
        dc.DrawText(wxT("A"), 6, 4);
        break; }
    case TOOL_ERASE: {
        dc.SetBrush(wxBrush(wxColour(230, 120, 120)));
        dc.DrawRectangle(5, 10, 13, 8);
        dc.SetBrush(wxBrush(wxColour(200, 200, 200)));
        dc.DrawRectangle(5, 6, 13, 5);
        break; }
    default:
        dc.DrawRectangle(4, 4, size - 8, size - 8);
        break;
    }
    return bmp;
}

// ---------------------------------------------------------------- 构造
ToolBoxPanel::ToolBoxPanel(wxWindow* parent, MainFrame* frame)
    : wxPanel(parent, wxID_ANY), m_frame(frame)
{
    SetBackgroundColour(wxColour(245, 246, 248));

    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

    // --- 固定工具行
    m_topHost = new wxPanel(this);
    m_topHost->SetBackgroundColour(wxColour(245, 246, 248));
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
    BuildToolRow(m_topHost);
    for (size_t i = 0; i < m_fixedBtns.size(); ++i) {
        topSizer->Add(m_fixedBtns[i], 0, wxALL, 2);
    }
    m_topHost->SetSizer(topSizer);

    root->Add(m_topHost, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 4);
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 4);

    // --- 库列表
    m_libHost = new wxPanel(this);
    m_libHost->SetBackgroundColour(wxColour(245, 246, 248));
    m_libSizer = new wxBoxSizer(wxVERTICAL);
    m_libHost->SetSizer(m_libSizer);
    BuildLibRow(m_libHost);

    root->Add(m_libHost, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);

    wxStaticText* lbl = new wxStaticText(this, wxID_ANY, wxT("元件"));
    lbl->SetForegroundColour(wxColour(90, 90, 90));
    root->Add(lbl, 0, wxLEFT | wxTOP, 8);

    // --- 工具网格
    m_toolHost = new wxPanel(this);
    m_toolHost->SetBackgroundColour(wxColour(255, 255, 255));
    m_toolSizer = new wxGridSizer(3, 4, 4);
    m_toolHost->SetSizer(m_toolSizer);
    root->Add(m_toolHost, 0, wxEXPAND | wxALL, 6);
    root->AddStretchSpacer(1);

    SetSizer(root);
    BuildToolGrid();
    SyncTool(m_tool, m_placeType);
}

void ToolBoxPanel::BuildToolRow(wxWindow* parent)
{
    struct Def { int tool; const wchar_t* tip; };
    static const Def defs[5] = {
        { TOOL_EDIT,  wxT("选择 / 编辑 (默认)\n拖动移动元件，拖动空白处框选") },
        { TOOL_POKE,  wxT("查探 (手)\n点击输入引脚翻转电平，按住按钮使其为 1") },
        { TOOL_WIRE,  wxT("布线\n逐点点击画导线，双击 / 右键 / Esc 结束") },
        { TOOL_TEXT,  wxT("文本\n在电路上放置文字注释") },
        { TOOL_ERASE, wxT("删除\n点击元件或导线直接删除") },
    };
    for (int i = 0; i < 5; ++i) {
        wxBitmap bmp = MakeSmallIcon(defs[i].tool);
        wxBitmapBundle bb = wxBitmapBundle::FromBitmap(bmp);
        wxToggleButton* b = new wxToggleButton(parent, wxID_ANY, wxEmptyString,
                                               wxDefaultPosition, wxSize(34, 30),
                                               wxBU_EXACTFIT);
        b->SetBitmap(bb);
        b->SetToolTip(defs[i].tip);
        b->SetValue(defs[i].tool == m_tool);
        int tool = defs[i].tool;
        b->Bind(wxEVT_TOGGLEBUTTON, [this, tool](wxCommandEvent&) { OnToolClick(tool, -1); });
        m_fixedBtns.push_back(b);
    }
}

void ToolBoxPanel::BuildLibRow(wxWindow* parent)
{
    m_libSizer->Clear(true);
    m_libBtns.clear();
    for (int lib = 0; lib < LIB_COUNT; ++lib) {
        if (lib == LIB_CUSTOM) continue;   // 动态元件库尚未接线，先不显示
        int n = (int)LibTools(lib).size();
        wxString txt = LibName(lib);
        if (lib == LIB_PROJECT) {
            n = (int)(m_frame ? m_frame->Doc()->circuits.size() : 1) - 1;
            if (n < 0) n = 0;
        }
        txt += wxString::Format(wxT("  (%d)"), n);
        wxToggleButton* b = new wxToggleButton(parent, wxID_ANY, txt, wxDefaultPosition,
                                               wxSize(150, 26));
        b->SetValue(lib == m_lib);
        b->SetFont(wxFont(wxFontInfo(9).FaceName(wxT("Microsoft YaHei"))));
        int L = lib;
        b->Bind(wxEVT_TOGGLEBUTTON, [this, L](wxCommandEvent&) { OnLibClick(L); });
        m_libSizer->Add(b, 0, wxEXPAND | wxTOP | wxBOTTOM, 1);
        m_libBtns.push_back(b);
    }
    Layout();
}

void ToolBoxPanel::BuildToolGrid()
{
    m_toolSizer->Clear(true);
    m_placeBtns.clear();
    m_placeTypes.clear();

    std::vector<int> types = LibTools(m_lib);
    if (m_lib == LIB_PROJECT) {
        types.clear();
        if (m_frame) {
            Document* d = m_frame->Doc();
            for (size_t i = 1; i < d->circuits.size(); ++i)
                types.push_back(CT_SubCircuit);   // 占位，具体下标另外记录
        }
    }

    if (types.empty()) {
        wxStaticText* t = new wxStaticText(m_toolHost, wxID_ANY,
                                           wxT("（此库暂无元件）\n可通过“项目”菜单添加子电路"));
        t->SetForegroundColour(wxColour(130, 130, 130));
        m_toolSizer->Add(t, 0, wxALL, 6);
        m_toolHost->Layout();
        return;
    }

    for (size_t i = 0; i < types.size(); ++i) {
        Component proto;
        if (m_lib == LIB_PROJECT) {
            InitComponentDefaults(proto, CT_SubCircuit, m_frame ? m_frame->Doc() : nullptr);
            proto.sub = (int)i + 1;
            proto.label = m_frame ? m_frame->Doc()->circuits[i + 1].name : wxString(wxT("子电路"));
        } else {
            InitComponentDefaults(proto, types[i], m_frame ? m_frame->Doc() : nullptr);
        }
        wxBitmap bmp = Renderer::MakeToolBitmap(m_frame ? m_frame->Doc() : nullptr, proto,
                                                wxSize(52, 34), wxColour(255, 255, 255));
        wxToggleButton* b = new wxToggleButton(m_toolHost, wxID_ANY, wxEmptyString,
                                               wxDefaultPosition, wxSize(56, 38));
        b->SetBitmap(wxBitmapBundle::FromBitmap(bmp));
        b->SetToolTip(m_lib == LIB_PROJECT ? proto.label : TypeName(types[i]) + wxT("\n") + TypeDesc(types[i]));
        int placeType = types[i];
        if (m_lib == LIB_PROJECT) {
            b->Bind(wxEVT_TOGGLEBUTTON, [this, i](wxCommandEvent&) {
                OnToolClick(TOOL_PLACE, CT_SubCircuit);
                if (m_frame) m_frame->SetPlaceSubCircuit((int)i + 1);
            });
        } else {
            b->Bind(wxEVT_TOGGLEBUTTON, [this, placeType](wxCommandEvent&) {
                OnToolClick(TOOL_PLACE, placeType);
            });
        }
        m_toolSizer->Add(b, 0, wxALL, 1);
        m_placeBtns.push_back(b);
        m_placeTypes.push_back(types[i]);
    }
    m_toolHost->Layout();
    Layout();
}

void ToolBoxPanel::RebuildProjectLib()
{
    if (m_libHost) BuildLibRow(m_libHost);
    if (m_lib == LIB_PROJECT) BuildToolGrid();
}

void ToolBoxPanel::SetLibrary(int lib)
{
    if (lib < 0 || lib >= LIB_COUNT) return;
    OnLibClick(lib);
}

void ToolBoxPanel::OnLibClick(int lib)
{
    m_lib = lib;
    for (size_t i = 0; i < m_libBtns.size(); ++i)
        m_libBtns[i]->SetValue((int)i == lib);
    BuildToolGrid();
}

void ToolBoxPanel::OnToolClick(int tool, int placeType)
{
    if (m_syncing) return;
    m_tool = tool;
    if (placeType >= 0) m_placeType = placeType;
    SyncTool(m_tool, m_placeType);
    if (m_frame) m_frame->SetTool(tool, placeType);
}

void ToolBoxPanel::SyncTool(int tool, int placeType)
{
    m_syncing = true;
    m_tool = tool;
    if (placeType >= 0) m_placeType = placeType;

    for (size_t i = 0; i < m_fixedBtns.size(); ++i) {
        int t = TOOL_EDIT + (int)i;
        m_fixedBtns[i]->SetValue(t == tool);
    }
    for (size_t i = 0; i < m_placeBtns.size(); ++i) {
        bool sel = (tool == TOOL_PLACE && m_placeTypes[i] == m_placeType);
        m_placeBtns[i]->SetValue(sel);
    }
    m_syncing = false;
}

} // namespace lc
