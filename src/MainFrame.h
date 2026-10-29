// ============================================================================
//  MainFrame.h  --  主窗口
// ============================================================================
#pragma once

#include "LogicModel.h"
#include "Simulator.h"

#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/timer.h>

#include <vector>

namespace lc {

class CanvasPanel;
class SidePanel;
class ToolBoxPanel;

class MainFrame : public wxFrame
{
public:
    MainFrame();
    ~MainFrame() override;

    // ---- 访问
    Document*     Doc()   { return &m_doc; }
    Simulator*    Sim()   { return &m_sim; }
    CanvasPanel*  Canvas(){ return m_canvas; }
    SidePanel*    Side()  { return m_side; }
    ToolBoxPanel* Tools() { return m_tools; }

    // ---- 文件
    void NewFile();
    void OpenFile(const wxString& path = wxEmptyString);
    bool SaveFile(bool saveAs = false);
    void ExportPng();
    bool ConfirmSave();
    void UpdateTitle();

    // ---- 编辑
    void PushUndo(const wxString& name);
    void DropLastUndo();
    void Undo();
    void Redo();
    void AfterEdit();
    void AfterEditNoUndo() { AfterEdit(); }
    void RefreshAll();
    void RefreshCanvas();
    void RefreshAttrs();
    void RotateSelection(bool cw);
    void EditAttributesOf(Component* c);
    void DuplicateSelection();

    // ---- 电路页
    void SetActiveCircuit(int idx);
    void AddCircuit();
    void RenameCircuit();
    void RemoveCircuit();
    void MoveCircuit(int delta);
    void ShowTruthTable();

    // ---- 工具
    void SetTool(int tool, int placeType = -1);
    void SetPlaceSubCircuit(int sub) { m_placeSub = sub; }
    int  PlaceSubCircuit() const { return m_placeSub; }   // 放置子电路时取用
    void SelectLibrary(int lib);
    int  CurTool() const { return m_tool; }
    int  PlaceType() const { return m_placeType; }

    // ---- 仿真
    void SetSimEnabled(bool on);
    bool SimEnabled() const { return m_simOn; }
    void TickOnce();
    void ResetSim();
    void SetFreq(int hz);
    int  Freq() const { return m_freqHz; }

    void SetStatus(const wxString& s, int field = 0);

private:
    void BuildMenu();
    void BuildToolBar();
    void BuildLayout();
    void UpdateUndoUI();
    void UpdateSimUI();
    void ApplyFreq();

    void OnTimer(wxTimerEvent& e);

    void OnFile(wxCommandEvent& e);
    void OnEdit(wxCommandEvent& e);
    void OnProject(wxCommandEvent& e);
    void OnSim(wxCommandEvent& e);
    void OnView(wxCommandEvent& e);
    void OnHelp(wxCommandEvent& e);
    void OnFreq(wxCommandEvent& e);
    void OnClose(wxCloseEvent& e);
    void OnActivate(wxActivateEvent& e);

    Document  m_doc;
    Simulator m_sim;

    CanvasPanel*  m_canvas = nullptr;
    SidePanel*    m_side   = nullptr;
    ToolBoxPanel* m_tools  = nullptr;
    wxSplitterWindow* m_splitOuter = nullptr;
    wxSplitterWindow* m_splitInner = nullptr;

    wxTimer m_timer;
    bool m_simOn   = false;
    int  m_freqHz  = 4;
    long m_ticks   = 0;
    int  m_tool    = TOOL_EDIT;
    int  m_placeType = CT_AND;
    int  m_placeSub  = -1;

    struct Snap { wxString text; int active = 0; wxString name; };
    std::vector<Snap> m_undo;
    std::vector<Snap> m_redo;
    bool m_loading = false;
};

} // namespace lc
