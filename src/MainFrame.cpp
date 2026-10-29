// ============================================================================
//  MainFrame.cpp  --  主窗口实现
// ============================================================================
#include "MainFrame.h"
#include "CanvasPanel.h"
#include "SidePanel.h"
#include "ToolBoxPanel.h"
#include "Renderer.h"
#include "Dialogs.h"
#include "Ids.h"

#include <wx/dcmemory.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/statline.h>
#include <wx/textdlg.h>
#include <wx/aboutdlg.h>

#include <algorithm>
#include <cmath>

namespace lc {

// ------------------------------------------------------------------ 工具条图标
static wxBitmap MakeBarIcon(int kind, int S = 16)
{
    wxBitmap bmp(S, S, 24);
    wxMemoryDC dc(bmp);
    dc.SetBackground(wxBrush(wxColour(245, 246, 248)));
    dc.Clear();
    dc.SetPen(wxPen(wxColour(50, 60, 75), 1));
    dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
    dc.SetTextForeground(wxColour(50, 60, 75));

    switch (kind) {
    case ID_New: {
        dc.DrawRectangle(3, 2, 9, 12);
        dc.SetBrush(wxBrush(wxColour(210, 220, 235)));
        wxPoint p[3] = { wxPoint(12, 2), wxPoint(12, 6), wxPoint(8, 2) };
        dc.DrawPolygon(3, p);
        break; }
    case ID_Open: {
        dc.SetBrush(wxBrush(wxColour(240, 200, 90)));
        dc.DrawRectangle(2, 4, 12, 9);
        dc.SetBrush(wxBrush(wxColour(250, 220, 130)));
        wxPoint p[4] = { wxPoint(2, 4), wxPoint(6, 2), wxPoint(9, 2), wxPoint(9, 4) };
        dc.DrawPolygon(4, p);
        break; }
    case ID_Save: {
        dc.SetBrush(wxBrush(wxColour(120, 150, 220)));
        dc.DrawRectangle(2, 2, 12, 12);
        dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
        dc.DrawRectangle(5, 3, 6, 5);
        dc.DrawRectangle(4, 9, 8, 5);
        break; }
    case ID_ExportPng: {
        dc.DrawRectangle(2, 2, 12, 12);
        dc.SetBrush(wxBrush(wxColour(90, 180, 110)));
        dc.DrawCircle(6, 6, 2);
        wxPoint p[3] = { wxPoint(3, 13), wxPoint(8, 8), wxPoint(13, 13) };
        dc.DrawPolygon(3, p);
        break; }
    case ID_Undo: case ID_Redo: {
        bool re = (kind == ID_Redo);
        dc.SetPen(wxPen(wxColour(60, 110, 180), 2));
        dc.DrawEllipticArc(3, 4, 10, 9, re ? 200 : 340, re ? 340 : 200);
        dc.SetBrush(wxBrush(wxColour(60, 110, 180)));
        wxPoint p[3];
        if (re) { p[0] = wxPoint(13, 6); p[1] = wxPoint(13, 11); p[2] = wxPoint(9, 8); }
        else    { p[0] = wxPoint(3, 6);  p[1] = wxPoint(3, 11);  p[2] = wxPoint(7, 8); }
        dc.DrawPolygon(3, p);
        break; }
    case ID_Cut: {
        dc.SetPen(wxPen(wxColour(90, 90, 90), 2));
        dc.DrawLine(4, 2, 12, 11);
        dc.DrawLine(12, 2, 4, 11);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wxColour(190, 70, 70), 1));
        dc.DrawCircle(4, 12, 2);
        dc.DrawCircle(12, 12, 2);
        break; }
    case ID_Copy: {
        dc.SetBrush(wxBrush(wxColour(235, 240, 250)));
        dc.DrawRectangle(2, 2, 8, 9);
        dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
        dc.DrawRectangle(5, 5, 9, 9);
        break; }
    case ID_Paste: {
        dc.SetBrush(wxBrush(wxColour(200, 180, 130)));
        dc.DrawRectangle(3, 3, 10, 11);
        dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
        dc.DrawRectangle(5, 5, 6, 7);
        break; }
    case ID_Delete: {
        dc.SetPen(wxPen(wxColour(190, 60, 60), 2));
        dc.DrawLine(4, 4, 12, 12);
        dc.DrawLine(12, 4, 4, 12);
        break; }
    case ID_ZoomIn: case ID_ZoomOut: {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wxColour(60, 70, 90), 2));
        dc.DrawCircle(7, 7, 5);
        dc.DrawLine(11, 11, 14, 14);
        dc.SetPen(wxPen(wxColour(60, 70, 90), 2));
        dc.DrawLine(4, 7, 10, 7);
        if (kind == ID_ZoomIn) dc.DrawLine(7, 4, 7, 10);
        break; }
    case ID_ZoomFit: {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wxColour(60, 70, 90), 1));
        dc.DrawRectangle(2, 3, 12, 10);
        dc.SetBrush(wxBrush(wxColour(90, 150, 220)));
        dc.DrawRectangle(5, 6, 6, 4);
        break; }
    case ID_SimEnable: {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxColour(40, 170, 60)));
        wxPoint p[3] = { wxPoint(4, 2), wxPoint(4, 14), wxPoint(14, 8) };
        dc.DrawPolygon(3, p);
        break; }
    case ID_TickOnce: {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxColour(60, 120, 200)));
        wxPoint p[3] = { wxPoint(3, 3), wxPoint(3, 13), wxPoint(11, 8) };
        dc.DrawPolygon(3, p);
        dc.SetBrush(wxBrush(wxColour(60, 120, 200)));
        dc.DrawRectangle(12, 3, 2, 10);
        break; }
    case ID_ResetSim: {
        dc.SetPen(wxPen(wxColour(200, 110, 40), 2));
        dc.DrawEllipticArc(3, 3, 10, 10, 60, 330);
        dc.SetBrush(wxBrush(wxColour(200, 110, 40)));
        wxPoint p[3] = { wxPoint(11, 1), wxPoint(14, 5), wxPoint(9, 5) };
        dc.DrawPolygon(3, p);
        break; }
    case ID_ToolEdit: {
        dc.SetPen(wxPen(wxColour(40, 40, 40), 1));
        dc.SetBrush(wxBrush(wxColour(70, 110, 180)));
        wxPoint p[7] = { wxPoint(4, 2), wxPoint(4, 12), wxPoint(7, 9), wxPoint(9, 14),
                         wxPoint(11, 13), wxPoint(9, 8), wxPoint(12, 7) };
        dc.DrawPolygon(7, p);
        break; }
    default:
        dc.DrawRectangle(3, 3, 10, 10);
        break;
    }
    return bmp;
}

static wxBitmapBundle BB(int kind) { return wxBitmapBundle::FromBitmap(MakeBarIcon(kind)); }

// ------------------------------------------------------------------ 构造
MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, wxT("逻辑电路仿真器"),
              wxDefaultPosition, wxSize(1280, 820))
{
    SetMinSize(wxSize(900, 600));

    // 图标
    wxIconBundle icons;
    for (int s : { 16, 32, 48 }) {
        wxIcon ic;
        ic.CopyFromBitmap(Renderer::MakeLogoBitmap(s));
        icons.AddIcon(ic);
    }
    SetIcons(icons);

    m_sim.Attach(&m_doc);
    m_timer.SetOwner(this);          // 定时器必须指定事件处理者，否则节拍不会触发

    BuildMenu();
    BuildToolBar();
    BuildLayout();

    CreateStatusBar(4);
    int widths[4] = { -3, 130, 110, 260 };
    SetStatusWidths(4, widths);
    SetStatus(wxT("就绪 — 从左侧选择元件，在画布上点击放置"), 0);
    SetStatus(wxT(""), 1);
    SetStatus(wxT("缩放 100%"), 2);
    UpdateSimUI();

    Bind(wxEVT_TIMER, &MainFrame::OnTimer, this);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
    Bind(wxEVT_ACTIVATE, &MainFrame::OnActivate, this);

    Bind(wxEVT_MENU, &MainFrame::OnFile,    this, ID_New, ID_ExportPng);
    Bind(wxEVT_MENU, &MainFrame::OnEdit,    this, ID_Undo, ID_Duplicate);
    Bind(wxEVT_MENU, &MainFrame::OnProject, this, ID_AddCircuit, ID_TruthTable);
    Bind(wxEVT_MENU, &MainFrame::OnSim,     this, ID_SimEnable, ID_ResetSim);
    Bind(wxEVT_MENU, &MainFrame::OnFreq,    this, ID_FreqBase, ID_FreqEnd);
    Bind(wxEVT_MENU, &MainFrame::OnView,    this, ID_ZoomIn, ID_ShowWireValue);
    Bind(wxEVT_MENU, &MainFrame::OnHelp,    this, ID_About, ID_Tips);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);

    m_canvas->SetContext(&m_doc, &m_sim);
    m_canvas->SetTool(TOOL_EDIT, -1);

    RefreshAll();
    m_canvas->SetFocus();
}

MainFrame::~MainFrame()
{
    m_timer.Stop();
}

// ------------------------------------------------------------------ 菜单
void MainFrame::BuildMenu()
{
    wxMenuBar* mb = new wxMenuBar();

    wxMenu* file = new wxMenu();
    file->Append(ID_New,       wxT("新建(&N)\tCtrl+N"));
    file->Append(ID_Open,      wxT("打开(&O)...\tCtrl+O"));
    file->Append(ID_Save,      wxT("保存(&S)\tCtrl+S"));
    file->Append(ID_SaveAs,    wxT("另存为(&A)...\tCtrl+Shift+S"));
    file->AppendSeparator();
    file->Append(ID_ExportPng, wxT("导出为 PNG 图片(&E)..."));
    file->AppendSeparator();
    file->Append(wxID_EXIT,    wxT("退出(&X)\tAlt+F4"));
    mb->Append(file, wxT("文件(&F)"));

    wxMenu* edit = new wxMenu();
    edit->Append(ID_Undo,      wxT("撤销(&U)\tCtrl+Z"));
    edit->Append(ID_Redo,      wxT("重做(&R)\tCtrl+Y"));
    edit->AppendSeparator();
    edit->Append(ID_Cut,       wxT("剪切(&T)\tCtrl+X"));
    edit->Append(ID_Copy,      wxT("复制(&C)\tCtrl+C"));
    edit->Append(ID_Paste,     wxT("粘贴(&P)\tCtrl+V"));
    edit->Append(ID_Duplicate, wxT("创建副本\tCtrl+D"));
    edit->AppendSeparator();
    edit->Append(ID_Delete,    wxT("删除(&D)\tDelete"));
    edit->Append(ID_SelectAll, wxT("全选(&A)\tCtrl+A"));
    edit->Append(ID_Deselect,  wxT("取消选择\tEsc"));
    edit->AppendSeparator();
    edit->Append(ID_RotateCW,  wxT("顺时针旋转\tR"));
    edit->Append(ID_RotateCCW, wxT("逆时针旋转"));
    edit->AppendSeparator();
    edit->Append(ID_EditAttrs, wxT("属性(&P)"));
    mb->Append(edit, wxT("编辑(&E)"));

    wxMenu* proj = new wxMenu();
    proj->Append(ID_AddCircuit,    wxT("添加电路(&A)..."));
    proj->Append(ID_RenameCircuit, wxT("重命名当前电路(&R)..."));
    proj->Append(ID_RemoveCircuit, wxT("删除当前电路(&D)"));
    proj->AppendSeparator();
    proj->Append(ID_MoveUp,        wxT("上移(&U)"));
    proj->Append(ID_MoveDown,      wxT("下移(&W)"));
    proj->AppendSeparator();
    proj->Append(ID_TruthTable,    wxT("组合逻辑真值表(&T)..."));
    mb->Append(proj, wxT("项目(&P)"));

    wxMenu* sim = new wxMenu();
    sim->AppendCheckItem(ID_SimEnable, wxT("启用仿真(&S)\tCtrl+E"));
    sim->Append(ID_TickOnce, wxT("单步一个节拍(&T)\tCtrl+T"));
    sim->Append(ID_ResetSim, wxT("复位仿真(&R)\tCtrl+R"));
    sim->AppendSeparator();
    wxMenu* freq = new wxMenu();
    static const int hzs[7] = { 1, 2, 4, 8, 16, 32, 64 };
    for (int i = 0; i < 7; ++i)
        freq->AppendRadioItem(ID_FreqBase + i, wxString::Format(wxT("%d Hz"), hzs[i]));
    sim->AppendSubMenu(freq, wxT("时钟频率(&F)"));
    mb->Append(sim, wxT("仿真(&S)"));

    wxMenu* view = new wxMenu();
    view->Append(ID_ZoomIn,    wxT("放大(&I)\tCtrl+="));
    view->Append(ID_ZoomOut,   wxT("缩小(&O)\tCtrl+-"));
    view->Append(ID_ZoomFit,   wxT("适应窗口(&F)\tCtrl+0"));
    view->Append(ID_Zoom100,   wxT("实际大小(&A)\tCtrl+1"));
    view->AppendSeparator();
    view->AppendCheckItem(ID_ShowGrid, wxT("显示网格(&G)\tCtrl+G"));
    view->Check(ID_ShowGrid, true);
    mb->Append(view, wxT("视图(&V)"));

    wxMenu* help = new wxMenu();
    help->Append(ID_Help,  wxT("使用说明 / 快捷键(&H)\tF1"));
    help->Append(ID_About, wxT("关于逻辑电路仿真器(&A)..."));
    mb->Append(help, wxT("帮助(&H)"));

    SetMenuBar(mb);
}

void MainFrame::BuildToolBar()
{
    wxToolBar* tb = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER);
    tb->SetToolBitmapSize(wxSize(16, 16));

    tb->AddTool(ID_New,       wxT("新建"), BB(ID_New), wxT("新建电路 (Ctrl+N)"));
    tb->AddTool(ID_Open,      wxT("打开"), BB(ID_Open), wxT("打开工程 (Ctrl+O)"));
    tb->AddTool(ID_Save,      wxT("保存"), BB(ID_Save), wxT("保存工程 (Ctrl+S)"));
    tb->AddTool(ID_ExportPng, wxT("导图"), BB(ID_ExportPng), wxT("导出为 PNG 图片"));
    tb->AddSeparator();
    tb->AddTool(ID_Undo,      wxT("撤销"), BB(ID_Undo), wxT("撤销 (Ctrl+Z)"));
    tb->AddTool(ID_Redo,      wxT("重做"), BB(ID_Redo), wxT("重做 (Ctrl+Y)"));
    tb->AddSeparator();
    tb->AddTool(ID_Cut,       wxT("剪切"), BB(ID_Cut), wxT("剪切 (Ctrl+X)"));
    tb->AddTool(ID_Copy,      wxT("复制"), BB(ID_Copy), wxT("复制 (Ctrl+C)"));
    tb->AddTool(ID_Paste,     wxT("粘贴"), BB(ID_Paste), wxT("粘贴 (Ctrl+V)"));
    tb->AddTool(ID_Delete,    wxT("删除"), BB(ID_Delete), wxT("删除选中 (Del)"));
    tb->AddSeparator();
    tb->AddTool(ID_ZoomOut,   wxT("缩小"), BB(ID_ZoomOut), wxT("缩小 (Ctrl+-)"));
    tb->AddTool(ID_ZoomIn,    wxT("放大"), BB(ID_ZoomIn), wxT("放大 (Ctrl+=)"));
    tb->AddTool(ID_ZoomFit,   wxT("适应"), BB(ID_ZoomFit), wxT("适应窗口 (Ctrl+0)"));
    tb->AddSeparator();
    tb->AddTool(ID_SimEnable, wxT("仿真"), BB(ID_SimEnable), wxT("启用/暂停仿真 (Ctrl+E)"));
    tb->AddTool(ID_TickOnce,  wxT("单步"), BB(ID_TickOnce), wxT("单步一个时钟节拍 (Ctrl+T)"));
    tb->AddTool(ID_ResetSim,  wxT("复位"), BB(ID_ResetSim), wxT("复位仿真状态 (Ctrl+R)"));
    tb->Realize();
}

void MainFrame::BuildLayout()
{
    // 先建分割窗口，再以分割窗口为父创建各面板
    m_splitOuter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxSP_LIVE_UPDATE | wxSP_3DSASH);
    m_splitOuter->SetMinimumPaneSize(170);

    m_splitInner = new wxSplitterWindow(m_splitOuter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxSP_LIVE_UPDATE | wxSP_3DSASH);
    m_splitInner->SetMinimumPaneSize(160);

    m_side   = new SidePanel(m_splitInner, this);
    m_canvas = new CanvasPanel(m_splitInner, this);
    m_splitInner->SplitVertically(m_side, m_canvas, 250);

    m_tools = new ToolBoxPanel(m_splitOuter, this);
    m_splitOuter->SplitVertically(m_tools, m_splitInner, 200);

    wxBoxSizer* s = new wxBoxSizer(wxVERTICAL);
    s->Add(m_splitOuter, 1, wxEXPAND);
    SetSizer(s);
    Layout();
}

// ------------------------------------------------------------------ 状态
void MainFrame::SetStatus(const wxString& s, int field)
{
    if (GetStatusBar() && field >= 0 && field < 4)
        SetStatusText(s, field);
}

void MainFrame::UpdateTitle()
{
    // 标题栏只显示软件名，不显示文件名 / 电路名 / 未保存标记
    SetTitle(wxT("逻辑电路仿真器"));
}

void MainFrame::UpdateUndoUI()
{
    if (wxMenuBar* mb = GetMenuBar()) {
        mb->Enable(ID_Undo, !m_undo.empty());
        mb->Enable(ID_Redo, !m_redo.empty());
    }
    if (wxToolBar* tb = GetToolBar()) {
        tb->EnableTool(ID_Undo, !m_undo.empty());
        tb->EnableTool(ID_Redo, !m_redo.empty());
    }
}

void MainFrame::UpdateSimUI()
{
    if (wxMenuBar* mb = GetMenuBar())
        mb->Check(ID_SimEnable, m_simOn);
    wxString state;
    if (!m_simOn)                   state = wxT("已暂停");
    else if (m_sim.IsOscillating()) state = wxT("运行中 — 检测到振荡!");
    else                            state = wxString::Format(wxT("运行中 (%d Hz)"), m_freqHz);
    SetStatus(wxString::Format(wxT("仿真: %s   节拍 %ld"), state, m_ticks), 3);
}

void MainFrame::RefreshCanvas()
{
    if (m_canvas) m_canvas->Refresh(false);
}

void MainFrame::RefreshAttrs()
{
    if (m_side) m_side->RefreshAttrs();
}

void MainFrame::RefreshAll()
{
    if (m_canvas) m_canvas->RefreshAll();
    if (m_side)   m_side->RefreshExplorer();
    RefreshAttrs();
    UpdateTitle();
    UpdateUndoUI();
    UpdateSimUI();
}

// ------------------------------------------------------------------ 撤销
void MainFrame::PushUndo(const wxString& name)
{
    if (m_loading) return;
    Snap s;
    s.text   = m_doc.Serialize();
    s.active = m_doc.active;
    s.name   = name;
    m_undo.push_back(s);
    if (m_undo.size() > 120) m_undo.erase(m_undo.begin());
    m_redo.clear();
    UpdateUndoUI();
}

void MainFrame::DropLastUndo()
{
    if (!m_undo.empty()) m_undo.pop_back();
    UpdateUndoUI();
}

void MainFrame::Undo()
{
    if (m_undo.empty()) {
        SetStatus(wxT("没有可撤销的操作"), 0);
        return;
    }
    Snap cur;
    cur.text   = m_doc.Serialize();
    cur.active = m_doc.active;
    cur.name   = wxT("重做");
    Snap s = m_undo.back();
    m_undo.pop_back();
    m_redo.push_back(cur);

    m_loading = true;
    wxString err;
    if (!m_doc.Deserialize(s.text, &err)) {
        m_loading = false;
        wxMessageBox(err, wxT("撤销失败"), wxOK | wxICON_ERROR, this);
        return;
    }
    m_doc.active = ClampInt(s.active, 0, (int)m_doc.circuits.size() - 1);
    m_doc.dirty  = true;
    m_sim.SetRoot(m_doc.active);
    m_sim.Invalidate();
    m_sim.Settle();
    m_loading = false;

    SetStatus(wxString::Format(wxT("已撤销: %s"), s.name), 0);
    RefreshAll();
}

void MainFrame::Redo()
{
    if (m_redo.empty()) {
        SetStatus(wxT("没有可重做的操作"), 0);
        return;
    }
    Snap cur;
    cur.text   = m_doc.Serialize();
    cur.active = m_doc.active;
    cur.name   = wxT("撤销");
    Snap s = m_redo.back();
    m_redo.pop_back();
    m_undo.push_back(cur);

    m_loading = true;
    wxString err;
    if (!m_doc.Deserialize(s.text, &err)) {
        m_loading = false;
        wxMessageBox(err, wxT("重做失败"), wxOK | wxICON_ERROR, this);
        return;
    }
    m_doc.active = ClampInt(s.active, 0, (int)m_doc.circuits.size() - 1);
    m_doc.dirty  = true;
    m_sim.SetRoot(m_doc.active);
    m_sim.Invalidate();
    m_sim.Settle();
    m_loading = false;

    SetStatus(wxT("已重做"), 0);
    RefreshAll();
}

void MainFrame::AfterEdit()
{
    m_doc.Touch();
    m_sim.Invalidate();
    m_sim.Settle();
    RefreshAll();
}

// ------------------------------------------------------------------ 文件
bool MainFrame::ConfirmSave()
{
    if (!m_doc.dirty) return true;
    int r = wxMessageBox(wxT("当前工程尚未保存，是否保存？"), wxT("逻辑电路仿真器"),
                         wxYES_NO | wxCANCEL | wxICON_QUESTION, this);
    if (r == wxCANCEL) return false;
    if (r == wxYES)    return SaveFile(false);
    return true;
}

void MainFrame::NewFile()
{
    if (!ConfirmSave()) return;
    m_doc.NewDocument();
    m_sim.SetRoot(0);
    m_sim.Reset();
    m_undo.clear();
    m_redo.clear();
    m_canvas->SetActiveCircuit(0);
    RefreshAll();
    SetStatus(wxT("已新建工程"), 0);
}

void MainFrame::OpenFile(const wxString& path)
{
    wxString p = path;
    if (p.empty()) {
        if (!ConfirmSave()) return;
        wxFileDialog dlg(this, wxT("打开电路工程"), wxEmptyString, wxEmptyString,
                         wxT("逻辑电路工程 (*.mlc)|*.mlc|文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*"),
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK) return;
        p = dlg.GetPath();
    }
    wxString err;
    if (!m_doc.LoadFrom(p, &err)) {
        wxMessageBox(err, wxT("打开失败"), wxOK | wxICON_ERROR, this);
        return;
    }
    m_undo.clear();
    m_redo.clear();
    m_sim.SetRoot(m_doc.active);
    m_sim.Reset();
    m_canvas->SetActiveCircuit(m_doc.active);
    m_canvas->ZoomFit();
    RefreshAll();
    SetStatus(wxT("已打开: ") + p, 0);
}

bool MainFrame::SaveFile(bool saveAs)
{
    wxString p = m_doc.path;
    if (saveAs || p.empty()) {
        wxFileDialog dlg(this, wxT("保存电路工程"), wxEmptyString,
                         p.empty() ? wxString(wxT("circuit.mlc")) : wxFileName(p).GetFullName(),
                         wxT("逻辑电路工程 (*.mlc)|*.mlc|所有文件 (*.*)|*.*"),
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK) return false;
        p = dlg.GetPath();
    }
    wxString err;
    if (!m_doc.SaveTo(p, &err)) {
        wxMessageBox(err, wxT("保存失败"), wxOK | wxICON_ERROR, this);
        return false;
    }
    RefreshAll();
    SetStatus(wxT("已保存: ") + p, 0);
    return true;
}

void MainFrame::ExportPng()
{
    Circuit& c = m_doc.Active();
    if (c.comps.empty() && c.wires.empty()) {
        wxMessageBox(wxT("当前电路是空的，无法导出。"), wxT("导出图片"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    wxFileDialog dlg(this, wxT("导出为 PNG"), wxEmptyString,
                     wxFileName(m_doc.path).GetName() + wxT(".png"),
                     wxT("PNG 图片 (*.png)|*.png"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    wxBitmap bmp = Renderer::RenderCircuit(&m_doc, &m_sim, m_doc.active, 30);
    wxImage img = bmp.ConvertToImage();
    if (!img.SaveFile(dlg.GetPath(), wxBITMAP_TYPE_PNG)) {
        wxMessageBox(wxT("写入图片失败。"), wxT("导出图片"), wxOK | wxICON_ERROR, this);
        return;
    }
    SetStatus(wxT("已导出图片: ") + dlg.GetPath(), 0);
}

// ------------------------------------------------------------------ 编辑
void MainFrame::RotateSelection(bool cw)
{
    Circuit& c = m_doc.Active();
    bool any = false;
    for (size_t i = 0; i < c.comps.size(); ++i)
        if (c.comps[i].selected) { any = true; break; }
    if (!any) return;
    PushUndo(wxT("旋转"));
    for (size_t i = 0; i < c.comps.size(); ++i) {
        if (!c.comps[i].selected) continue;
        c.comps[i].facing = (c.comps[i].facing + (cw ? 1 : 3)) % 4;
    }
    AfterEdit();
}

void MainFrame::EditAttributesOf(Component* c)
{
    if (!c) {
        SetStatus(wxT("请先选中一个元件"), 0);
        return;
    }
    Circuit& cir = m_doc.Active();
    cir.ClearSelection();
    c->selected = true;
    m_canvas->Refresh(false);
    RefreshAttrs();
    if (m_side) {
        SetStatus(wxString::Format(wxT("在左下角属性表中双击“值”列即可修改 %s 的属性"), TypeName(c->type)), 0);
    }
}

void MainFrame::DuplicateSelection()
{
    if (!m_canvas || m_canvas->SelectedCount() == 0) return;
    m_canvas->CopySelection();
    m_canvas->PasteClipboard();
}

// ------------------------------------------------------------------ 电路页
void MainFrame::SetActiveCircuit(int idx)
{
    if (idx < 0 || idx >= (int)m_doc.circuits.size()) return;
    if (idx == m_doc.active) return;
    m_doc.active = idx;
    m_sim.SetRoot(idx);
    m_sim.Invalidate();
    m_sim.Settle();
    m_canvas->SetActiveCircuit(idx);
    m_canvas->ZoomFit();
    RefreshAll();
    SetStatus(wxString::Format(wxT("已切换到电路: %s"), m_doc.Active().name), 0);
}

void MainFrame::AddCircuit()
{
    wxTextEntryDialog dlg(this, wxT("新电路名称:"), wxT("添加电路"), wxT("sub"));
    if (dlg.ShowModal() != wxID_OK) return;
    wxString name = dlg.GetValue();
    if (name.empty()) return;
    PushUndo(wxT("添加电路"));
    int idx = m_doc.AddCircuit(name);
    m_tools->RebuildProjectLib();
    RefreshAll();
    SetStatus(wxString::Format(wxT("已添加电路: %s"), m_doc.circuits[idx].name), 0);
}

void MainFrame::RenameCircuit()
{
    int idx = m_doc.active;
    wxTextEntryDialog dlg(this, wxT("新的电路名称:"), wxT("重命名电路"), m_doc.circuits[idx].name);
    if (dlg.ShowModal() != wxID_OK) return;
    wxString name = dlg.GetValue();
    if (name.empty() || name == m_doc.circuits[idx].name) return;
    PushUndo(wxT("重命名电路"));
    m_doc.circuits[idx].name = name;
    m_doc.Touch();
    m_tools->RebuildProjectLib();
    RefreshAll();
}

void MainFrame::RemoveCircuit()
{
    int idx = m_doc.active;
    if (idx == 0) {
        wxMessageBox(wxT("主电路 main 不能删除。"), wxT("删除电路"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (wxMessageBox(wxString::Format(wxT("确定删除电路 “%s” 吗？"), m_doc.circuits[idx].name),
                     wxT("删除电路"), wxYES_NO | wxICON_QUESTION, this) != wxYES)
        return;
    PushUndo(wxT("删除电路"));
    m_doc.RemoveCircuit(idx);
    m_sim.SetRoot(m_doc.active);
    m_sim.Invalidate();
    m_sim.Settle();
    m_tools->RebuildProjectLib();
    RefreshAll();
}

void MainFrame::MoveCircuit(int delta)
{
    int idx = m_doc.active;
    int ni = idx + delta;
    if (idx == 0 || ni <= 0 || ni >= (int)m_doc.circuits.size()) return;
    PushUndo(wxT("移动电路"));
    std::swap(m_doc.circuits[idx], m_doc.circuits[ni]);
    // 交换后要修正所有子电路引用
    for (size_t ci = 0; ci < m_doc.circuits.size(); ++ci) {
        for (size_t k = 0; k < m_doc.circuits[ci].comps.size(); ++k) {
            Component& c = m_doc.circuits[ci].comps[k];
            if (c.type != CT_SubCircuit) continue;
            if (c.sub == idx) c.sub = ni;
            else if (c.sub == ni) c.sub = idx;
        }
    }
    m_doc.active = ni;
    m_doc.Touch();
    m_sim.SetRoot(ni);
    m_sim.Invalidate();
    m_sim.Settle();
    m_tools->RebuildProjectLib();
    RefreshAll();
}

void MainFrame::ShowTruthTable()
{
    TruthTableDialog dlg(this, this);
    dlg.ShowModal();
}

// ------------------------------------------------------------------ 工具
void MainFrame::SetTool(int tool, int placeType)
{
    m_tool = tool;
    if (placeType >= 0) m_placeType = placeType;
    if (m_canvas) {
        m_canvas->SetTool(m_tool, m_placeType);
        m_canvas->SetFocus();      // 让键盘快捷键始终作用于画布
    }
    if (m_tools)  m_tools->SyncTool(m_tool, m_placeType);

    if (tool == TOOL_PLACE) {
        int t = m_placeType;
        wxString nm = (t == CT_SubCircuit && m_placeSub >= 0 && m_placeSub < (int)m_doc.circuits.size())
                          ? m_doc.circuits[m_placeSub].name
                          : TypeName(t);
        SetStatus(wxT("放置元件: ") + nm + wxT("  —  在画布上点击放置，Esc 取消"), 0);
    } else {
        const wchar_t* names[5] = { wxT("选择/编辑"), wxT("查探"), wxT("布线"), wxT("文本"), wxT("删除") };
        SetStatus(wxString::Format(wxT("当前工具: %s"), names[tool < 5 ? tool : 0]), 0);
    }
}

void MainFrame::SelectLibrary(int lib)
{
    if (m_tools) m_tools->SetLibrary(lib);
    SetStatus(wxString::Format(wxT("元件库: %s"), LibName(lib)), 0);
}

// ------------------------------------------------------------------ 仿真
void MainFrame::SetSimEnabled(bool on)
{
    m_simOn = on;
    if (on) {
        ApplyFreq();
        m_sim.Settle();
    } else {
        m_timer.Stop();
    }
    UpdateSimUI();
    RefreshCanvas();
}

void MainFrame::ApplyFreq()
{
    if (m_simOn) {
        int ms = 1000 / std::max(1, m_freqHz);
        m_timer.Start(std::max(1, ms));
    }
}

void MainFrame::SetFreq(int hz)
{
    m_freqHz = ClampInt(hz, 1, 64);
    ApplyFreq();
    UpdateSimUI();
}

void MainFrame::TickOnce()
{
    ++m_ticks;
    m_sim.Tick();
    RefreshCanvas();
    UpdateSimUI();
    SetStatus(wxT("已单步一个节拍"), 0);
}

void MainFrame::ResetSim()
{
    m_ticks = 0;
    m_sim.Reset();
    RefreshCanvas();
    UpdateSimUI();
    SetStatus(wxT("仿真状态已复位"), 0);
}

void MainFrame::OnTimer(wxTimerEvent&)
{
    ++m_ticks;
    m_sim.Tick();
    RefreshCanvas();
    UpdateSimUI();
}

// ------------------------------------------------------------------ 事件
void MainFrame::OnActivate(wxActivateEvent& e)
{
    e.Skip();
}

void MainFrame::OnClose(wxCloseEvent& e)
{
    if (e.CanVeto() && !ConfirmSave()) {
        e.Veto();
        return;
    }
    m_timer.Stop();
    Destroy();
}

void MainFrame::OnFile(wxCommandEvent& e)
{
    switch (e.GetId()) {
    case ID_New:       NewFile(); break;
    case ID_Open:      OpenFile(); break;
    case ID_Save:      SaveFile(false); break;
    case ID_SaveAs:    SaveFile(true); break;
    case ID_ExportPng: ExportPng(); break;
    default: break;
    }
}

void MainFrame::OnEdit(wxCommandEvent& e)
{
    switch (e.GetId()) {
    case ID_Undo:      Undo(); break;
    case ID_Redo:      Redo(); break;
    case ID_Cut:       m_canvas->CutSelection(); break;
    case ID_Copy:      m_canvas->CopySelection(); break;
    case ID_Paste:     m_canvas->PasteClipboard(); break;
    case ID_Delete:    m_canvas->DeleteSelection(); break;
    case ID_SelectAll: m_canvas->SelectAll(); break;
    case ID_Deselect:  m_canvas->ClearSelection(); break;
    case ID_RotateCW:  RotateSelection(true); break;
    case ID_RotateCCW: RotateSelection(false); break;
    case ID_Duplicate: DuplicateSelection(); break;
    case ID_EditAttrs: EditAttributesOf(m_canvas->FirstSelected()); break;
    default: break;
    }
}

void MainFrame::OnProject(wxCommandEvent& e)
{
    switch (e.GetId()) {
    case ID_AddCircuit:    AddCircuit(); break;
    case ID_RenameCircuit: RenameCircuit(); break;
    case ID_RemoveCircuit: RemoveCircuit(); break;
    case ID_MoveUp:        MoveCircuit(-1); break;
    case ID_MoveDown:      MoveCircuit(1); break;
    case ID_TruthTable:    ShowTruthTable(); break;
    case ID_OpenCircuit:   break;
    default: break;
    }
}

void MainFrame::OnSim(wxCommandEvent& e)
{
    switch (e.GetId()) {
    case ID_SimEnable: SetSimEnabled(!m_simOn); break;
    case ID_TickOnce:  TickOnce(); break;
    case ID_ResetSim:  ResetSim(); break;
    default: break;
    }
}

void MainFrame::OnFreq(wxCommandEvent& e)
{
    static const int hzs[7] = { 1, 2, 4, 8, 16, 32, 64 };
    int i = e.GetId() - ID_FreqBase;
    if (i >= 0 && i < 7) {
        SetFreq(hzs[i]);
        SetStatus(wxString::Format(wxT("时钟频率: %d Hz"), hzs[i]), 0);
    }
}

void MainFrame::OnView(wxCommandEvent& e)
{
    switch (e.GetId()) {
    case ID_ZoomIn:   m_canvas->ZoomStep(true); break;
    case ID_ZoomOut:  m_canvas->ZoomStep(false); break;
    case ID_ZoomFit:  m_canvas->ZoomFit(); break;
    case ID_Zoom100:  { wxSize cs = m_canvas->GetClientSize();
                        m_canvas->ZoomTo(1.0, wxPoint(cs.x / 2, cs.y / 2)); } break;
    case ID_ShowGrid:
        m_canvas->SetShowGrid(e.IsChecked());
        break;
    default: break;
    }
}

void MainFrame::OnHelp(wxCommandEvent& e)
{
    switch (e.GetId()) {
    case ID_About: { AboutDialog dlg(this); dlg.ShowModal(); break; }
    case ID_Help:  { HelpDialog dlg(this);  dlg.ShowModal(); break; }
    default: break;
    }
}

} // namespace lc
