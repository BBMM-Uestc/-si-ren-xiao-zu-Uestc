// ============================================================================
//  SidePanel.cpp  --  资源管理器与属性表
// ============================================================================
#include "SidePanel.h"
#include "MainFrame.h"
#include "CanvasPanel.h"
#include "Simulator.h"
#include "Dialogs.h"
#include "Ids.h"

#include <wx/menu.h>
#include <wx/statline.h>

#include <algorithm>

namespace lc {

namespace {
class TreeData : public wxTreeItemData
{
public:
    TreeData(int k, int i) : kind(k), idx(i) {}
    int kind;   // 0=根 1=电路 2=库
    int idx;
};
} // namespace

// ---------------------------------------------------------------- 构造
SidePanel::SidePanel(wxWindow* parent, MainFrame* frame)
    : wxPanel(parent, wxID_ANY), m_frame(frame)
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

    wxStaticText* l1 = new wxStaticText(this, wxID_ANY, wxT(" 资源管理器"));
    l1->SetFont(wxFont(wxFontInfo(9).Bold().FaceName(wxT("Microsoft YaHei"))));
    l1->SetBackgroundColour(wxColour(228, 232, 238));
    root->Add(l1, 0, wxEXPAND);

    m_tree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 180),
                            wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE | wxTR_HIDE_ROOT);
    m_tree->SetFont(wxFont(wxFontInfo(9).FaceName(wxT("Microsoft YaHei"))));
    root->Add(m_tree, 1, wxEXPAND);
    m_tree->Bind(wxEVT_TREE_ITEM_ACTIVATED, &SidePanel::OnTreeActivated, this);
    m_tree->Bind(wxEVT_TREE_SEL_CHANGED,   &SidePanel::OnTreeSelChanged, this);
    m_tree->Bind(wxEVT_TREE_ITEM_RIGHT_CLICK, &SidePanel::OnTreeMenu, this);

    root->Add(new wxStaticLine(this), 0, wxEXPAND);

    m_head = new wxStaticText(this, wxID_ANY, wxT(" 属性"));
    m_head->SetFont(wxFont(wxFontInfo(9).Bold().FaceName(wxT("Microsoft YaHei"))));
    m_head->SetBackgroundColour(wxColour(228, 232, 238));
    root->Add(m_head, 0, wxEXPAND);

    m_attrs = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 240),
                             wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES);
    m_attrs->SetFont(wxFont(wxFontInfo(9).FaceName(wxT("Microsoft YaHei"))));
    m_attrs->InsertColumn(0, wxT("属性"), wxLIST_FORMAT_LEFT, 96);
    m_attrs->InsertColumn(1, wxT("值"),   wxLIST_FORMAT_LEFT, 128);
    root->Add(m_attrs, 1, wxEXPAND);
    m_attrs->Bind(wxEVT_LIST_ITEM_ACTIVATED, &SidePanel::OnAttrActivated, this);
    m_attrs->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &SidePanel::OnAttrMenu, this);

    SetSizer(root);
    SetMinSize(wxSize(200, 200));
}

// ---------------------------------------------------------------- 资源管理器
void SidePanel::RefreshExplorer()
{
    if (!m_frame) return;
    Document* doc = m_frame->Doc();
    m_updating = true;
    wxTreeItemId root = m_tree->GetRootItem();
    if (!root.IsOk()) root = m_tree->AddRoot(wxT("root"));
    m_tree->DeleteChildren(root);

    wxTreeItemId proj = m_tree->AppendItem(root, wxT("项目"));
    m_tree->SetItemData(proj, new TreeData(0, -1));
    for (size_t i = 0; i < doc->circuits.size(); ++i) {
        wxString nm = doc->circuits[i].name;
        if ((int)i == doc->active) nm = wxT("▶ ") + nm;
        wxTreeItemId it = m_tree->AppendItem(proj, nm);
        m_tree->SetItemData(it, new TreeData(1, (int)i));
        if ((int)i == doc->active) {
            m_tree->SetItemBold(it, true);
            m_tree->SelectItem(it);
        }
    }
    m_tree->Expand(proj);

    wxTreeItemId libs = m_tree->AppendItem(root, wxT("库"));
    m_tree->SetItemData(libs, new TreeData(0, -1));
    for (int l = 0; l < LIB_COUNT; ++l) {
        if (l == LIB_CUSTOM) continue;   // 动态元件库尚未接线，先不显示
        int n = (int)LibTools(l).size();
        if (l == LIB_PROJECT) n = (int)doc->circuits.size() - 1;
        wxTreeItemId it = m_tree->AppendItem(libs, wxString::Format(wxT("%s (%d)"),
                                                                    LibName(l), n));
        m_tree->SetItemData(it, new TreeData(2, l));
    }
    m_tree->Expand(libs);
    m_updating = false;
}

void SidePanel::SelectCircuitInTree(int)
{
    RefreshExplorer();
}

void SidePanel::OnTreeSelChanged(wxTreeEvent& e)
{
    if (m_updating) return;
    wxTreeItemData* d = m_tree->GetItemData(e.GetItem());
    TreeData* td = dynamic_cast<TreeData*>(d);
    if (td && td->kind == 2 && m_frame)
        m_frame->SelectLibrary(td->idx);
}

void SidePanel::OnTreeActivated(wxTreeEvent& e)
{
    wxTreeItemData* d = m_tree->GetItemData(e.GetItem());
    TreeData* td = dynamic_cast<TreeData*>(d);
    if (!td) return;
    if (td->kind == 1 && m_frame)
        m_frame->SetActiveCircuit(td->idx);
    else if (td->kind == 2 && m_frame)
        m_frame->SelectLibrary(td->idx);
}

void SidePanel::OnTreeMenu(wxTreeEvent& e)
{
    wxTreeItemData* d = m_tree->GetItemData(e.GetItem());
    TreeData* td = dynamic_cast<TreeData*>(d);
    if (!td) return;
    if (td->kind == 1) {
        if (m_frame) m_frame->SetActiveCircuit(td->idx);
        wxMenu menu;
        menu.Append(ID_OpenCircuit,   wxT("打开该电路"));
        menu.Append(ID_RenameCircuit, wxT("重命名..."));
        menu.Append(ID_AddCircuit,    wxT("添加电路..."));
        menu.Append(ID_MoveUp,        wxT("上移"));
        menu.Append(ID_MoveDown,      wxT("下移"));
        menu.AppendSeparator();
        menu.Append(ID_RemoveCircuit, wxT("删除该电路"));
        PopupMenu(&menu);
    } else if (td->kind == 2) {
        wxMenu menu;
        menu.Append(ID_OpenCircuit, wxT("切换到该元件库"));
        PopupMenu(&menu);
        if (m_frame) m_frame->SelectLibrary(td->idx);
    } else {
        wxMenu menu;
        menu.Append(ID_AddCircuit, wxT("添加电路..."));
        PopupMenu(&menu);
    }
    e.Skip();
}

// ---------------------------------------------------------------- 属性
void SidePanel::RefreshAttrs()
{
    if (!m_frame) return;
    m_updating = true;
    long top = m_attrs->GetTopItem();
    m_attrs->DeleteAllItems();
    m_defs.clear();
    m_curCompId = -1;
    m_curWire = -1;
    m_wireMode = false;

    Document* doc = m_frame->Doc();
    Circuit& c = doc->Active();
    CanvasPanel* cv = m_frame->Canvas();

    Component* comp = nullptr;
    for (size_t i = 0; i < c.comps.size(); ++i)
        if (c.comps[i].selected) { comp = &c.comps[i]; break; }

    if (comp) {
        m_curCompId = comp->id;
        m_head->SetLabel(wxString::Format(wxT(" 属性 — %s #%d"), TypeName(comp->type), comp->id));
        m_defs = GetAttrDefs(*comp);
        for (size_t i = 0; i < m_defs.size(); ++i) {
            long row = m_attrs->InsertItem((long)i, m_defs[i].name);
            m_attrs->SetItem(row, 1, GetAttrValue(*comp, m_defs[i].key));
            if (m_defs[i].kind == AK_READONLY)
                m_attrs->SetItemTextColour(row, wxColour(110, 110, 110));
        }
    } else {
        int wi = -1;
        for (size_t i = 0; i < c.wires.size(); ++i)
            if (c.wires[i].selected) { wi = (int)i; break; }
        if (wi >= 0) {
            m_wireMode = true;
            m_curWire = wi;
            m_head->SetLabel(wxT(" 属性 — 导线"));
            AttrDef d1; d1.key = wxT("wbits"); d1.name = wxT("位宽"); d1.kind = AK_INT; d1.minv = 1; d1.maxv = MAX_BITS;
            AttrDef d2; d2.key = wxT("wcount"); d2.name = wxT("折点数"); d2.kind = AK_READONLY;
            AttrDef d3; d3.key = wxT("wx"); d3.name = wxT("起点 X"); d3.kind = AK_READONLY;
            AttrDef d4; d4.key = wxT("wy"); d4.name = wxT("起点 Y"); d4.kind = AK_READONLY;
            m_defs.push_back(d1); m_defs.push_back(d2); m_defs.push_back(d3); m_defs.push_back(d4);
            Wire& w = c.wires[wi];
            m_attrs->InsertItem(0, d1.name);
            m_attrs->SetItem(0, 1, wxString::Format(wxT("%d"), w.bits));
            m_attrs->InsertItem(1, d2.name);
            m_attrs->SetItem(1, 1, wxString::Format(wxT("%d"), (int)w.pts.size()));
            m_attrs->InsertItem(2, d3.name);
            m_attrs->SetItem(2, 1, wxString::Format(wxT("%d"), w.pts.empty() ? 0 : w.pts[0].x));
            m_attrs->InsertItem(3, d4.name);
            m_attrs->SetItem(3, 1, wxString::Format(wxT("%d"), w.pts.empty() ? 0 : w.pts[0].y));
        } else {
            m_head->SetLabel(wxT(" 属性 (未选中对象)"));
        }
    }
    if (top > 0) m_attrs->EnsureVisible(top);
    m_updating = false;
    (void)cv;
}

void SidePanel::ApplyValue(int row, const wxString& newVal)
{
    if (!m_frame || row < 0 || row >= (int)m_defs.size()) return;
    const AttrDef& d = m_defs[row];
    if (d.kind == AK_READONLY) return;

    Document* doc = m_frame->Doc();
    Circuit& c = doc->Active();

    if (m_wireMode) {
        if (m_curWire < 0 || m_curWire >= (int)c.wires.size()) return;
        long v = 0;
        if (!newVal.ToLong(&v)) return;
        if (d.key == wxT("wbits")) {
            m_frame->PushUndo(wxT("修改导线位宽"));
            c.wires[m_curWire].bits = ClampInt((int)v, 1, MAX_BITS);
            m_frame->AfterEditNoUndo();
        }
        return;
    }

    if (m_curCompId < 0) return;
    Component* comp = c.Find(m_curCompId);
    if (!comp) return;

    // 子电路属性：确认不会造成直接自包含
    m_frame->PushUndo(wxT("修改属性"));
    wxString err;
    if (!SetAttrValue(*comp, d.key, newVal, &err)) {
        wxMessageBox(err.empty() ? wxT("属性值无效") : err, wxT("属性错误"),
                     wxOK | wxICON_WARNING, m_frame);
        m_frame->DropLastUndo();
        return;
    }
    if (d.key == wxT("sub")) {
        long v = 0;
        newVal.ToLong(&v);
        comp->sub = (int)v;
        if (comp->sub >= 0 && comp->sub < (int)doc->circuits.size() && comp->label.empty())
            comp->label = doc->circuits[comp->sub].name;
    }
    m_frame->AfterEditNoUndo();
    RefreshAttrs();
}

void SidePanel::OnAttrActivated(wxListEvent& e)
{
    if (m_updating) return;
    long row = e.GetIndex();
    if (row < 0 || row >= (int)m_defs.size()) return;
    const AttrDef& d = m_defs[row];
    if (d.kind == AK_READONLY) return;

    wxString cur = m_attrs->GetItemText(row, 1);
    Document* doc = m_frame->Doc();
    if (d.key == wxT("sub")) {
        // 当前值就是电路序号
        Component* comp = doc->Active().Find(m_curCompId);
        if (comp) cur = wxString::Format(wxT("%d"), comp->sub);
    }
    if (d.key == wxT("text")) {
        Component* comp = doc->Active().Find(m_curCompId);
        if (comp) cur = comp->text;
    }
    wxString out;
    if (EditAttrValue(this, d, cur, doc, &out))
        ApplyValue((int)row, out);
}

void SidePanel::OnAttrMenu(wxListEvent& e)
{
    long row = e.GetIndex();
    if (row < 0 || row >= (int)m_defs.size()) return;
    if (m_defs[row].kind == AK_READONLY) return;
    wxMenu menu;
    menu.Append(ID_EditAttrs, wxT("编辑..."));
    PopupMenu(&menu);
    e.Skip();
}

} // namespace lc
