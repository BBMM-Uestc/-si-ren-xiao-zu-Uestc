// ============================================================================
//  ToolBoxPanel.h  --  左侧元件库 / 工具面板
// ============================================================================
#pragma once

#include "LogicModel.h"

#include <wx/wx.h>
#include <wx/tglbtn.h>

#include <vector>

namespace lc {

class MainFrame;

class ToolBoxPanel : public wxPanel
{
public:
    ToolBoxPanel(wxWindow* parent, MainFrame* frame);

    /// 与主窗口当前工具保持同步(不触发回调)
    void SyncTool(int tool, int placeType);
    void RebuildProjectLib();
    void SetLibrary(int lib);

    int  Library() const { return m_lib; }

private:
    void BuildToolRow(wxWindow* parent);
    void BuildLibRow(wxWindow* parent);
    void BuildToolGrid();
    void OnLibClick(int lib);
    void OnToolClick(int tool, int placeType);

    MainFrame* m_frame = nullptr;
    int m_lib       = LIB_IO;
    int m_tool      = TOOL_EDIT;
    int m_placeType = CT_AND;
    bool m_syncing  = false;

    wxPanel*      m_topHost    = nullptr;
    wxPanel*      m_libHost    = nullptr;
    wxBoxSizer*   m_libSizer   = nullptr;
    wxPanel*      m_toolHost   = nullptr;
    wxGridSizer*  m_toolSizer  = nullptr;

    std::vector<wxToggleButton*> m_fixedBtns;
    std::vector<wxToggleButton*> m_libBtns;
    std::vector<wxToggleButton*> m_placeBtns;
    std::vector<int>             m_placeTypes;
};

} // namespace lc
