// ============================================================================
//  SidePanel.h  --  左侧“资源管理器 + 属性表”面板
// ============================================================================
#pragma once

#include "LogicModel.h"

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>

#include <vector>

namespace lc {

class MainFrame;

class SidePanel : public wxPanel
{
public:
    SidePanel(wxWindow* parent, MainFrame* frame);

    void RefreshExplorer();
    void RefreshAttrs();
    void SelectCircuitInTree(int idx);

private:
    void OnTreeActivated(wxTreeEvent& e);
    void OnTreeSelChanged(wxTreeEvent& e);
    void OnTreeMenu(wxTreeEvent& e);
    void OnAttrActivated(wxListEvent& e);
    void OnAttrMenu(wxListEvent& e);
    void ApplyValue(int row, const wxString& newVal);

    MainFrame*  m_frame = nullptr;
    wxTreeCtrl* m_tree  = nullptr;
    wxListCtrl* m_attrs = nullptr;
    wxStaticText* m_head = nullptr;

    std::vector<AttrDef> m_defs;
    int  m_curCompId = -1;
    int  m_curWire   = -1;
    bool m_wireMode  = false;
    bool m_updating  = false;
};

} // namespace lc
