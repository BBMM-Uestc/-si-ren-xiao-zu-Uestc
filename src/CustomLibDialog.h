// ============================================================================
//  CustomLibDialog.h  --  元件库管理器(运行时自定义元件)
//  * 左侧：已定义元件列表 + 新建/复制/删除
//  * 中间：设计器画布(折线/矩形/圆/文字/引脚)，用正式渲染器实时预览
//  * 右侧：名称/尺寸/逻辑模板 + 引脚表
//  确定后写回 Document::customDefs，左侧工具箱“自定义”库立即出现
// ============================================================================
#pragma once

#include "LogicModel.h"

#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/tglbtn.h>

#include <vector>

namespace lc {

class CustomLibDialog;

/// 单根引脚编辑
class PinEditDialog : public wxDialog
{
public:
    PinEditDialog(wxWindow* parent, const CustomPinDef& in);
    CustomPinDef Result() const { return m_p; }
private:
    CustomPinDef m_p;
    wxTextCtrl* m_name = nullptr;
    wxChoice*   m_dir  = nullptr;
    wxSpinCtrl* m_bits = nullptr;
    wxSpinCtrl* m_x = nullptr;
    wxSpinCtrl* m_y = nullptr;
};

/// 设计器画布
class DesignerCanvas : public wxPanel
{
public:
    static const int TOOL_POLY   = 0;
    static const int TOOL_RECT   = 1;
    static const int TOOL_CIRCLE = 2;
    static const int TOOL_TEXT   = 3;
    static const int TOOL_PIN    = 4;
    static const int TOOL_ERASE  = 5;

    DesignerCanvas(wxWindow* parent, CustomLibDialog* dlg);

    void SetTool(int t);
    int  Tool() const { return m_tool; }
    void SetZoom(double z) { m_zoomWanted = z; Refresh(); }
    double Zoom() const { return m_zoomWanted; }
    void CancelPending();

private:
    void   OnPaint(wxPaintEvent& e);
    void   OnSize(wxSizeEvent& e);
    void   OnLeftDown(wxMouseEvent& e);
    void   OnRightDown(wxMouseEvent& e);
    void   OnDClick(wxMouseEvent& e);
    void   OnMotion(wxMouseEvent& e);
    void   ComputeLayout();
    wxPoint ToLocal(const wxPoint& client) const;
    wxPoint Snap(const wxPoint& p) const;
    void   EndPoly();
    int    HitPrim(const wxPoint& lp) const;
    int    HitPin(const wxPoint& lp) const;

    CustomLibDialog* m_dlg = nullptr;
    int      m_tool = TOOL_POLY;
    double   m_zoomWanted = 2.0;
    double   m_scale = 2.0;
    wxPoint  m_org;
    bool     m_laidOut = false;

    std::vector<wxPoint> m_poly;
    wxPoint  m_cur;
    bool     m_hasRectA = false;
    wxPoint  m_rectA;
    bool     m_hasCircleC = false;
    wxPoint  m_circleC;
};

/// 元件库管理器
class CustomLibDialog : public wxDialog
{
public:
    CustomLibDialog(wxWindow* parent, Document* doc);

    CustomDef&       Work()       { return m_work; }
    const CustomDef& Work() const { return m_work; }
    Document*        PreviewDoc() { return &m_preview; }
    bool             HasWork() const { return m_cur >= 0; }

    void WorkChanged();      // 画布或字段变化后调用
    void RefreshPreview();
    void NotifyPinsChanged(); // 引脚增删后刷新引脚表

private:
    void NewDef();
    void RefreshDefList();
    void LoadDef(int idx);
    void StoreWork();
    void RefreshPinList();
    void OnNew(wxCommandEvent& e);
    void OnDup(wxCommandEvent& e);
    void OnDel(wxCommandEvent& e);
    void OnSelect(wxCommandEvent& e);
    void OnField(wxCommandEvent& e);
    void OnLogic(wxCommandEvent& e);
    void OnPinAdd(wxCommandEvent& e);
    void OnPinDel(wxCommandEvent& e);
    void OnPinActivated(wxListEvent& e);
    void OnClear(wxCommandEvent& e);
    void OnOK(wxCommandEvent& e);

    Document* m_doc = nullptr;
    Document  m_preview;             // 只用于渲染预览
    std::vector<CustomDef> m_defs;   // 工作副本
    CustomDef m_work;
    int       m_cur = -1;
    bool      m_syncing = false;

    wxListBox*      m_list = nullptr;
    wxTextCtrl*     m_name = nullptr;
    wxTextCtrl*     m_desc = nullptr;
    wxSpinCtrl*     m_w = nullptr;
    wxSpinCtrl*     m_h = nullptr;
    wxSpinCtrl*     m_bits = nullptr;
    wxChoice*       m_logic = nullptr;
    DesignerCanvas* m_canvas = nullptr;
    wxListCtrl*     m_pins = nullptr;
    std::vector<wxToggleButton*> m_toolBtns;
};

} // namespace lc
