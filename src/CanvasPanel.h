// ============================================================================
//  CanvasPanel.h  --  电路绘图/编辑画布
// ============================================================================
#pragma once

#include "LogicModel.h"

#include <wx/wx.h>
#include <wx/scrolwin.h>

#include <vector>

namespace lc {

class MainFrame;
class Simulator;

class CanvasPanel : public wxScrolledWindow
{
public:
    CanvasPanel(wxWindow* parent, MainFrame* frame);

    void SetContext(Document* doc, Simulator* sim);
    void SetActiveCircuit(int idx);
    void SetTool(int tool, int placeType);
    void SetShowGrid(bool on) { m_showGrid = on; Refresh(); }
    bool ShowGrid() const { return m_showGrid; }

    void RefreshAll();               // 重算范围并刷新
    void CenterOn(const wxPoint& world);

    double Zoom() const { return m_zoom; }
    void   ZoomTo(double z, const wxPoint& clientAnchor);
    void   ZoomFit();
    void   ZoomStep(bool in);

    void DeleteSelection();
    void SelectAll();
    void ClearSelection();
    void MoveSelection(int dx, int dy);

    void CopySelection();
    void CutSelection();
    void PasteClipboard();

    /// 属性面板改完属性后调用
    void ApplyAttrChange(const wxString& key, const wxString& value);

    wxPoint ToWorld(const wxPoint& client) const;
    wxPoint ToClientPt(const wxPoint& world) const;

    int  SelectedCount() const;
    Component* FirstSelected();

private:
    void OnPaint(wxPaintEvent& e);
    void OnSize(wxSizeEvent& e);
    void OnLeftDown(wxMouseEvent& e);
    void OnLeftUp(wxMouseEvent& e);
    void OnLeftDClick(wxMouseEvent& e);
    void OnMotion(wxMouseEvent& e);
    void OnRightDown(wxMouseEvent& e);
    void OnMiddleDown(wxMouseEvent& e);
    void OnMiddleUp(wxMouseEvent& e);
    void OnMouseWheel(wxMouseEvent& e);
    void OnKeyDown(wxKeyEvent& e);
    void OnLeave(wxMouseEvent& e);
    void OnContextMenu(wxCommandEvent& e);
    void OnEraseBackground(wxEraseEvent& e) {}

    void RecalcVirtual();
    wxPoint SnapPoint(const wxPoint& p) const;
    /// 光标世界坐标 → 元件落点(预览与真正放置共用，保证两者完全一致)
    wxPoint ComputePlacePos(const wxPoint& world, wxSize* szOut = nullptr) const;
    wxPoint SnapNearPin(const wxPoint& p, bool* hitPin) const;

    /// 返回 >=0: 命中的引脚号; -1: 命中元件本体; -2: 未命中
    int  HitComponent(const wxPoint& world, Component** out) const;
    int  HitWire(const wxPoint& world, int tol, int* outIdx = nullptr) const;
    void SelectAt(const wxPoint& world, bool add);

    void FinishWire();
    void CancelWire();

    MainFrame* m_frame = nullptr;
    Document*  m_doc   = nullptr;
    Simulator* m_sim   = nullptr;

    double m_zoom     = 1.0;
    bool   m_showGrid = true;
    wxPoint m_origin;                 // 虚拟坐标 (0,0) 对应的世界坐标
    bool   m_haveOrigin = false;      // 原点是否已确定(只向外扩，不向内收)

    // 放置预览：与真正放置用同一套坐标计算，保证虚影 = 落点
    wxPoint m_placePos;
    bool    m_placeValid = false;

    int m_tool      = TOOL_EDIT;
    int m_placeType = CT_AND;

    // 交互状态
    bool    m_panning   = false;
    wxPoint m_panStart, m_panOriginStart;
    bool    m_dragging  = false;
    bool    m_dragUndo  = false;
    wxPoint m_dragLast;
    bool    m_rubber    = false;
    wxPoint m_rubberA, m_rubberB;
    bool    m_wiring    = false;
    std::vector<wxPoint> m_wirePts;
    wxPoint m_wireCur;
    Component* m_pokeBtn = nullptr;
    wxPoint m_btnDownAt;

    std::vector<Component> m_clipComps;
    std::vector<Wire>      m_clipWires;
};

} // namespace lc
