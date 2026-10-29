// ============================================================================
//  CustomLibDialog.cpp  --  元件库管理器实现
// ============================================================================
#include "CustomLibDialog.h"
#include "Renderer.h"
#include "Ids.h"

#include <wx/dcbuffer.h>
#include <wx/statline.h>
#include <wx/textdlg.h>

#include <algorithm>
#include <cmath>

namespace lc {

static const int DS_TOOL_BASE = wxID_HIGHEST + 3000;

// ============================================================================
//  引脚编辑对话框
// ============================================================================
PinEditDialog::PinEditDialog(wxWindow* parent, const CustomPinDef& in)
    : wxDialog(parent, wxID_ANY, wxT("编辑引脚"), wxDefaultPosition, wxSize(320, 300)),
      m_p(in)
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* g = new wxFlexGridSizer(2, 6, 8);
    g->AddGrowableCol(1, 1);

    g->Add(new wxStaticText(this, wxID_ANY, wxT("名称")), 0, wxALIGN_CENTER_VERTICAL);
    m_name = new wxTextCtrl(this, wxID_ANY, in.name);
    g->Add(m_name, 1, wxEXPAND);

    g->Add(new wxStaticText(this, wxID_ANY, wxT("方向")), 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString dirs;
    dirs.Add(wxT("输入"));
    dirs.Add(wxT("输出"));
    m_dir = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, dirs);
    m_dir->SetSelection(in.isInput ? 0 : 1);
    g->Add(m_dir, 1, wxEXPAND);

    g->Add(new wxStaticText(this, wxID_ANY, wxT("位宽")), 0, wxALIGN_CENTER_VERTICAL);
    m_bits = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                            wxDefaultSize, wxSP_ARROW_KEYS, 1, MAX_BITS, in.bits < 1 ? 1 : in.bits);
    g->Add(m_bits, 1, wxEXPAND);

    g->Add(new wxStaticText(this, wxID_ANY, wxT("X 坐标")), 0, wxALIGN_CENTER_VERTICAL);
    m_x = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                         wxDefaultSize, wxSP_ARROW_KEYS, -400, 400, in.pos.x);
    g->Add(m_x, 1, wxEXPAND);

    g->Add(new wxStaticText(this, wxID_ANY, wxT("Y 坐标")), 0, wxALIGN_CENTER_VERTICAL);
    m_y = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                         wxDefaultSize, wxSP_ARROW_KEYS, -400, 400, in.pos.y);
    g->Add(m_y, 1, wxEXPAND);

    root->Add(g, 1, wxEXPAND | wxALL, 14);

    wxBoxSizer* bs = new wxBoxSizer(wxHORIZONTAL);
    bs->AddStretchSpacer();
    bs->Add(new wxButton(this, wxID_OK, wxT("确定")), 0, wxRIGHT, 8);
    bs->Add(new wxButton(this, wxID_CANCEL, wxT("取消")), 0);
    root->Add(bs, 0, wxEXPAND | wxALL, 10);

    SetSizer(root);
    CentreOnParent();
}

// ============================================================================
//  设计器画布
// ============================================================================
DesignerCanvas::DesignerCanvas(wxWindow* parent, CustomLibDialog* dlg)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(420, 320), wxBORDER_SIMPLE),
      m_dlg(dlg)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(320, 260));
    Bind(wxEVT_PAINT, &DesignerCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, &DesignerCanvas::OnSize, this);
    Bind(wxEVT_LEFT_DOWN, &DesignerCanvas::OnLeftDown, this);
    Bind(wxEVT_LEFT_DCLICK, &DesignerCanvas::OnDClick, this);
    Bind(wxEVT_RIGHT_DOWN, &DesignerCanvas::OnRightDown, this);
    Bind(wxEVT_MOTION, &DesignerCanvas::OnMotion, this);
}

void DesignerCanvas::SetTool(int t)
{
    m_tool = t;
    CancelPending();
    SetCursor((t == TOOL_ERASE) ? wxCursor(wxCURSOR_NO_ENTRY)
                                : (t == TOOL_PIN ? wxCursor(wxCURSOR_CROSS)
                                                 : wxCursor(wxCURSOR_PENCIL)));
    Refresh();
}

void DesignerCanvas::CancelPending()
{
    m_poly.clear();
    m_hasRectA = false;
    m_hasCircleC = false;
    Refresh();
}

void DesignerCanvas::ComputeLayout()
{
    wxSize cs = GetClientSize();
    const CustomDef& d = m_dlg->Work();
    double dw = (d.w < 20) ? 20 : d.w;
    double dh = (d.h < 20) ? 20 : d.h;
    double s = std::min((cs.x - 40.0) / dw, (cs.y - 40.0) / dh);
    s *= m_zoomWanted / 2.0;          // 2.0 为基准
    if (s < 0.3) s = 0.3;
    if (s > 12.0) s = 12.0;
    m_scale = s;
    m_org = wxPoint((int)((cs.x - dw * s) / 2), (int)((cs.y - dh * s) / 2));
    m_laidOut = true;
}

wxPoint DesignerCanvas::ToLocal(const wxPoint& client) const
{
    DesignerCanvas* self = const_cast<DesignerCanvas*>(this);
    if (!self->m_laidOut) self->ComputeLayout();
    double x = (client.x - m_org.x) / m_scale;
    double y = (client.y - m_org.y) / m_scale;
    return wxPoint((int)std::floor(x + 0.5), (int)std::floor(y + 0.5));
}

wxPoint DesignerCanvas::Snap(const wxPoint& p) const
{
    int x = (int)(std::floor((double)p.x / GRID + 0.5)) * GRID;
    int y = (int)(std::floor((double)p.y / GRID + 0.5)) * GRID;
    return wxPoint(x, y);
}

void DesignerCanvas::OnSize(wxSizeEvent& e)
{
    m_laidOut = false;
    Refresh();
    e.Skip();
}

void DesignerCanvas::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(wxColour(250, 250, 253)));
    dc.Clear();

    CustomDef& d = m_dlg->Work();
    ComputeLayout();

    wxSize cs = GetClientSize();
    if (d.w < 20 || d.h < 20) return;

    dc.SetUserScale(m_scale, m_scale);
    dc.SetDeviceOrigin(m_org.x, m_org.y);

    // 栅格
    dc.SetPen(wxPen(wxColour(230, 234, 242), 0));
    for (int x = 0; x <= d.w; x += GRID) dc.DrawLine(x, 0, x, d.h);
    for (int y = 0; y <= d.h; y += GRID) dc.DrawLine(0, y, d.w, y);

    // 外形框
    dc.SetPen(wxPen(wxColour(198, 204, 216), 0));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(0, 0, d.w, d.h);

    // ---- 用正式渲染器画，保证"所见即所得" ----
    Document* pd = m_dlg->PreviewDoc();
    pd->customDefs.clear();
    pd->customDefs.push_back(d);
    Component cc;
    cc.type = CT_Custom;
    cc.cdef = 0;
    cc.pos  = wxPoint(0, 0);
    cc.facing = F_East;
    cc.bits = d.bits > 0 ? d.bits : 1;
    cc.label = wxEmptyString;
    Renderer::DrawComponent(dc, pd, cc, nullptr, false, false);

    // ---- 正在绘制的临时图形 ----
    dc.SetPen(wxPen(wxColour(60, 130, 220), 0));
    if (!m_poly.empty()) {
        std::vector<wxPoint> pts = m_poly;
        if (m_cur != pts.back()) pts.push_back(m_cur);
        if (pts.size() >= 2) dc.DrawLines((int)pts.size(), &pts[0]);
        dc.SetBrush(wxBrush(wxColour(60, 130, 220)));
        for (size_t i = 0; i < pts.size(); ++i) dc.DrawCircle(pts[i], 3);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
    }
    if (m_hasRectA) {
        int x0 = std::min(m_rectA.x, m_cur.x), y0 = std::min(m_rectA.y, m_cur.y);
        int x1 = std::max(m_rectA.x, m_cur.x), y1 = std::max(m_rectA.y, m_cur.y);
        dc.DrawRectangle(x0, y0, x1 - x0, y1 - y0);
    }
    if (m_hasCircleC) {
        int dx = m_cur.x - m_circleC.x, dy = m_cur.y - m_circleC.y;
        int r = (int)(std::sqrt((double)dx * dx + (double)dy * dy) + 0.5);
        dc.DrawCircle(m_circleC, r);
    }

    // 十字光标提示
    if (m_tool == TOOL_PIN) {
        dc.SetPen(wxPen(wxColour(220, 120, 40), 0));
        dc.DrawLine(m_cur.x - 6, m_cur.y, m_cur.x + 6, m_cur.y);
        dc.DrawLine(m_cur.x, m_cur.y - 6, m_cur.x, m_cur.y + 6);
    }
    (void)cs;
}

int DesignerCanvas::HitPrim(const wxPoint& lp) const
{
    const CustomDef& d = m_dlg->Work();
    int best = -1;
    double bestD = 1e9;
    for (size_t i = 0; i < d.prims.size(); ++i) {
        const CustomPrim& p = d.prims[i];
        for (size_t k = 0; k < p.pts.size(); ++k) {
            double dx = p.pts[k].x - lp.x;
            double dy = p.pts[k].y - lp.y;
            double dd = std::sqrt(dx * dx + dy * dy);
            if (dd < bestD) { bestD = dd; best = (int)i; }
        }
        // 矩形中心也算
        if (p.kind == CP_RECT && p.pts.size() >= 2) {
            double cx = (p.pts[0].x + p.pts[1].x) / 2.0 - lp.x;
            double cy = (p.pts[0].y + p.pts[1].y) / 2.0 - lp.y;
            double dd = std::sqrt(cx * cx + cy * cy) * 0.6;
            if (dd < bestD) { bestD = dd; best = (int)i; }
        }
    }
    if (bestD <= 18.0) return best;
    return -1;
}

int DesignerCanvas::HitPin(const wxPoint& lp) const
{
    const CustomDef& d = m_dlg->Work();
    for (size_t i = 0; i < d.pins.size(); ++i) {
        double dx = d.pins[i].pos.x - lp.x;
        double dy = d.pins[i].pos.y - lp.y;
        if (std::sqrt(dx * dx + dy * dy) <= 10.0) return (int)i;
    }
    return -1;
}

void DesignerCanvas::EndPoly()
{
    if (m_poly.size() < 2) { m_poly.clear(); Refresh(); return; }
    CustomPrim p;
    p.kind = CP_POLY;
    p.fill = false;
    p.pts = m_poly;
    m_dlg->Work().prims.push_back(p);
    m_poly.clear();
    m_dlg->WorkChanged();
}

void DesignerCanvas::OnMotion(wxMouseEvent& e)
{
    m_cur = Snap(ToLocal(e.GetPosition()));
    if (!m_poly.empty() || m_hasRectA || m_hasCircleC || m_tool == TOOL_PIN)
        Refresh(false);
    e.Skip();
}

void DesignerCanvas::OnLeftDown(wxMouseEvent& e)
{
    SetFocus();
    wxPoint raw = ToLocal(e.GetPosition());
    wxPoint lp  = Snap(raw);
    m_cur = lp;
    CustomDef& d = m_dlg->Work();

    switch (m_tool) {
    case TOOL_POLY:
        if (m_poly.empty()) m_poly.push_back(lp);
        else if (lp != m_poly.back()) m_poly.push_back(lp);
        Refresh(false);
        break;

    case TOOL_RECT:
        if (!m_hasRectA) { m_rectA = lp; m_hasRectA = true; }
        else {
            CustomPrim p;
            p.kind = CP_RECT;
            p.fill = false;
            p.pts.push_back(m_rectA);
            p.pts.push_back(lp);
            d.prims.push_back(p);
            m_hasRectA = false;
            m_dlg->WorkChanged();
        }
        Refresh(false);
        break;

    case TOOL_CIRCLE:
        if (!m_hasCircleC) { m_circleC = lp; m_hasCircleC = true; }
        else {
            CustomPrim p;
            p.kind = CP_CIRCLE;
            p.fill = false;
            p.pts.push_back(m_circleC);
            p.pts.push_back(lp);
            d.prims.push_back(p);
            m_hasCircleC = false;
            m_dlg->WorkChanged();
        }
        Refresh(false);
        break;

    case TOOL_TEXT: {
        wxString s = wxGetTextFromUser(wxT("图形中要显示的文字:"), wxT("添加文字"), wxT("文字"), this);
        if (s.empty()) break;
        CustomPrim p;
        p.kind = CP_TEXT;
        p.text = s;
        p.pts.push_back(wxPoint(lp.x, lp.y));
        d.prims.push_back(p);
        m_dlg->WorkChanged();
        break; }

    case TOOL_PIN: {
        CustomPinDef p;
        p.isInput = (lp.x <= 0 || lp.x < d.w / 2);
        p.bits = 1;
        p.pos = lp;
        int n = (int)d.pins.size() + 1;
        p.name = wxString::Format(wxT("%s%d"), p.isInput ? wxT("I") : wxT("O"), n);
        d.pins.push_back(p);
        m_dlg->NotifyPinsChanged();
        break; }

    case TOOL_ERASE: {
        int pi = HitPin(raw);
        if (pi >= 0) {
            d.pins.erase(d.pins.begin() + pi);
            m_dlg->NotifyPinsChanged();
            break;
        }
        int ri = HitPrim(raw);
        if (ri >= 0) {
            d.prims.erase(d.prims.begin() + ri);
            m_dlg->WorkChanged();
        }
        break; }

    default: break;
    }
}

void DesignerCanvas::OnDClick(wxMouseEvent& e)
{
    if (!m_poly.empty()) EndPoly();
    e.Skip();
}

void DesignerCanvas::OnRightDown(wxMouseEvent& e)
{
    if (!m_poly.empty()) { EndPoly(); return; }
    CancelPending();
    e.Skip();
}

// ============================================================================
//  元件库管理器
// ============================================================================
CustomLibDialog::CustomLibDialog(wxWindow* parent, Document* doc)
    : wxDialog(parent, wxID_ANY, wxT("元件库管理器 — 自定义元件"),
               wxDefaultPosition, wxSize(1000, 620),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX),
      m_doc(doc)
{
    m_defs = doc ? doc->customDefs : std::vector<CustomDef>();

    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* body = new wxBoxSizer(wxHORIZONTAL);

    // ---------------- 左：定义列表 ----------------
    wxBoxSizer* left = new wxBoxSizer(wxVERTICAL);
    wxStaticText* l1 = new wxStaticText(this, wxID_ANY, wxT("已定义的元件"));
    l1->SetFont(wxFont(wxFontInfo(9).Bold().FaceName(wxT("Microsoft YaHei"))));
    left->Add(l1, 0, wxBOTTOM, 4);
    m_list = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(190, -1));
    left->Add(m_list, 1, wxEXPAND);
    wxBoxSizer* lb = new wxBoxSizer(wxHORIZONTAL);
    wxButton* bn = new wxButton(this, wxID_ANY, wxT("新建"));
    wxButton* bd = new wxButton(this, wxID_ANY, wxT("复制"));
    wxButton* bx = new wxButton(this, wxID_ANY, wxT("删除"));
    bn->Bind(wxEVT_BUTTON, &CustomLibDialog::OnNew, this);
    bd->Bind(wxEVT_BUTTON, &CustomLibDialog::OnDup, this);
    bx->Bind(wxEVT_BUTTON, &CustomLibDialog::OnDel, this);
    lb->Add(bn, 1, wxRIGHT, 3);
    lb->Add(bd, 1, wxRIGHT, 3);
    lb->Add(bx, 1);
    left->Add(lb, 0, wxEXPAND | wxTOP, 4);
    body->Add(left, 0, wxEXPAND | wxALL, 8);

    // ---------------- 中：设计器 ----------------
    wxBoxSizer* mid = new wxBoxSizer(wxVERTICAL);

    wxPanel* toolRow = new wxPanel(this);
    wxBoxSizer* tr = new wxBoxSizer(wxHORIZONTAL);
    struct TD { int id; const wchar_t* text; };
    static const TD tdefs[6] = {
        { DesignerCanvas::TOOL_POLY,   wxT("折线") },
        { DesignerCanvas::TOOL_RECT,   wxT("矩形") },
        { DesignerCanvas::TOOL_CIRCLE, wxT("圆") },
        { DesignerCanvas::TOOL_TEXT,   wxT("文字") },
        { DesignerCanvas::TOOL_PIN,    wxT("引脚") },
        { DesignerCanvas::TOOL_ERASE,  wxT("橡皮") }
    };
    for (int i = 0; i < 6; ++i) {
        wxToggleButton* b = new wxToggleButton(toolRow, DS_TOOL_BASE + i, tdefs[i].text,
                                               wxDefaultPosition, wxSize(58, 28));
        b->SetValue(i == 0);
        int tool = tdefs[i].id;
        b->Bind(wxEVT_TOGGLEBUTTON, [this, tool](wxCommandEvent&) {
            m_canvas->SetTool(tool);
            for (size_t k = 0; k < m_toolBtns.size(); ++k)
                m_toolBtns[k]->SetValue((int)k == tool);
            m_canvas->Refresh();
        });
        tr->Add(b, 0, wxRIGHT, 3);
        m_toolBtns.push_back(b);
    }
    tr->AddStretchSpacer();
    wxButton* bclr = new wxButton(toolRow, wxID_ANY, wxT("清空图形"), wxDefaultPosition, wxSize(80, 28));
    bclr->Bind(wxEVT_BUTTON, &CustomLibDialog::OnClear, this);
    tr->Add(bclr, 0, wxRIGHT, 6);
    wxButton* budo = new wxButton(toolRow, wxID_ANY, wxT("撤销一笔"), wxDefaultPosition, wxSize(80, 28));
    budo->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (m_cur < 0 || m_work.prims.empty()) return;
        m_work.prims.pop_back();
        m_canvas->CancelPending();
        StoreWork();
        RefreshPreview();
    });
    tr->Add(budo, 0);
    toolRow->SetSizer(tr);
    mid->Add(toolRow, 0, wxEXPAND | wxBOTTOM, 6);

    m_canvas = new DesignerCanvas(this, this);
    mid->Add(m_canvas, 1, wxEXPAND);

    wxStaticText* hint = new wxStaticText(this, wxID_ANY,
        wxT("折线：左键逐点点击，右键或双击结束   |   矩形/圆：点两次   |   引脚：在边框上点击，右侧表格双击可改名称/方向"));
    hint->SetForegroundColour(wxColour(110, 110, 110));
    hint->SetFont(wxFont(wxFontInfo(8).FaceName(wxT("Microsoft YaHei"))));
    mid->Add(hint, 0, wxTOP, 4);

    body->Add(mid, 1, wxEXPAND | wxALL, 8);

    // ---------------- 右：属性 + 引脚 ----------------
    wxBoxSizer* right = new wxBoxSizer(wxVERTICAL);
    wxStaticText* r1 = new wxStaticText(this, wxID_ANY, wxT("元件属性"));
    r1->SetFont(wxFont(wxFontInfo(9).Bold().FaceName(wxT("Microsoft YaHei"))));
    right->Add(r1, 0, wxBOTTOM, 4);

    wxFlexGridSizer* g = new wxFlexGridSizer(2, 4, 6);
    g->AddGrowableCol(1, 1);
    g->Add(new wxStaticText(this, wxID_ANY, wxT("名称")), 0, wxALIGN_CENTER_VERTICAL);
    m_name = new wxTextCtrl(this, wxID_ANY);
    g->Add(m_name, 1, wxEXPAND);
    g->Add(new wxStaticText(this, wxID_ANY, wxT("说明")), 0, wxALIGN_CENTER_VERTICAL);
    m_desc = new wxTextCtrl(this, wxID_ANY);
    g->Add(m_desc, 1, wxEXPAND);
    g->Add(new wxStaticText(this, wxID_ANY, wxT("宽度")), 0, wxALIGN_CENTER_VERTICAL);
    m_w = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                         wxSP_ARROW_KEYS, 20, 400, 60);
    g->Add(m_w, 1, wxEXPAND);
    g->Add(new wxStaticText(this, wxID_ANY, wxT("高度")), 0, wxALIGN_CENTER_VERTICAL);
    m_h = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                         wxSP_ARROW_KEYS, 20, 400, 60);
    g->Add(m_h, 1, wxEXPAND);
    g->Add(new wxStaticText(this, wxID_ANY, wxT("位宽")), 0, wxALIGN_CENTER_VERTICAL);
    m_bits = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                            wxSP_ARROW_KEYS, 1, MAX_BITS, 1);
    g->Add(m_bits, 1, wxEXPAND);
    g->Add(new wxStaticText(this, wxID_ANY, wxT("逻辑")), 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString logics;
    for (int i = 0; i < CustomLogicCount(); ++i) logics.Add(CustomLogicName(i));
    m_logic = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, logics);
    g->Add(m_logic, 1, wxEXPAND);
    right->Add(g, 0, wxEXPAND | wxBOTTOM, 8);

    wxStaticText* r2 = new wxStaticText(this, wxID_ANY, wxT("引脚"));
    r2->SetFont(wxFont(wxFontInfo(9).Bold().FaceName(wxT("Microsoft YaHei"))));
    right->Add(r2, 0, wxBOTTOM, 4);

    m_pins = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(250, 180),
                            wxLC_REPORT | wxLC_SINGLE_SEL);
    m_pins->InsertColumn(0, wxT("名称"), wxLIST_FORMAT_LEFT, 64);
    m_pins->InsertColumn(1, wxT("方向"), wxLIST_FORMAT_LEFT, 46);
    m_pins->InsertColumn(2, wxT("位宽"), wxLIST_FORMAT_LEFT, 40);
    m_pins->InsertColumn(3, wxT("X"), wxLIST_FORMAT_LEFT, 40);
    m_pins->InsertColumn(4, wxT("Y"), wxLIST_FORMAT_LEFT, 40);
    right->Add(m_pins, 1, wxEXPAND);

    wxBoxSizer* pb = new wxBoxSizer(wxHORIZONTAL);
    wxButton* pa = new wxButton(this, wxID_ANY, wxT("添加引脚"));
    wxButton* pd = new wxButton(this, wxID_ANY, wxT("删除引脚"));
    pa->Bind(wxEVT_BUTTON, &CustomLibDialog::OnPinAdd, this);
    pd->Bind(wxEVT_BUTTON, &CustomLibDialog::OnPinDel, this);
    pb->Add(pa, 1, wxRIGHT, 4);
    pb->Add(pd, 1);
    right->Add(pb, 0, wxEXPAND | wxTOP, 4);
    m_pins->Bind(wxEVT_LIST_ITEM_ACTIVATED, &CustomLibDialog::OnPinActivated, this);

    body->Add(right, 0, wxEXPAND | wxALL, 8);

    root->Add(body, 1, wxEXPAND);
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

    wxBoxSizer* bs = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* note = new wxStaticText(this, wxID_ANY,
        wxT("确定后这些元件会出现在左侧“自定义”库里，并随工程保存"));
    note->SetForegroundColour(wxColour(110, 110, 110));
    bs->Add(note, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    wxButton* ok = new wxButton(this, wxID_OK, wxT("确定"), wxDefaultPosition, wxSize(90, 30));
    wxButton* ca = new wxButton(this, wxID_CANCEL, wxT("取消"), wxDefaultPosition, wxSize(90, 30));
    ok->Bind(wxEVT_BUTTON, &CustomLibDialog::OnOK, this);
    bs->Add(ok, 0, wxRIGHT, 6);
    bs->Add(ca, 0);
    root->Add(bs, 0, wxEXPAND | wxALL, 10);

    SetSizer(root);

    // 字段事件
    m_name->Bind(wxEVT_TEXT, &CustomLibDialog::OnField, this);
    m_desc->Bind(wxEVT_TEXT, &CustomLibDialog::OnField, this);
    m_w->Bind(wxEVT_SPINCTRL, &CustomLibDialog::OnField, this);
    m_h->Bind(wxEVT_SPINCTRL, &CustomLibDialog::OnField, this);
    m_w->Bind(wxEVT_TEXT, &CustomLibDialog::OnField, this);
    m_h->Bind(wxEVT_TEXT, &CustomLibDialog::OnField, this);
    m_bits->Bind(wxEVT_SPINCTRL, &CustomLibDialog::OnField, this);
    m_logic->Bind(wxEVT_CHOICE, &CustomLibDialog::OnLogic, this);
    m_list->Bind(wxEVT_LISTBOX, &CustomLibDialog::OnSelect, this);

    RefreshDefList();
    if (!m_defs.empty()) LoadDef(0);
    else NewDef();
    CentreOnParent();
}

// ---------------------------------------------------------------- 列表
void CustomLibDialog::RefreshDefList()
{
    m_syncing = true;
    m_list->Clear();
    for (size_t i = 0; i < m_defs.size(); ++i)
        m_list->Append(m_defs[i].name);
    if (m_cur >= 0 && m_cur < (int)m_defs.size()) m_list->SetSelection(m_cur);
    m_syncing = false;
}

void CustomLibDialog::StoreWork()
{
    if (m_cur < 0 || m_cur >= (int)m_defs.size()) return;
    if (m_name) {
        wxString n = m_name->GetValue();
        if (!n.empty()) m_work.name = n;
    }
    m_defs[m_cur] = m_work;
}

void CustomLibDialog::LoadDef(int idx)
{
    if (idx < 0 || idx >= (int)m_defs.size()) return;
    m_cur  = idx;
    m_work = m_defs[idx];

    m_syncing = true;
    m_name->SetValue(m_work.name);
    m_desc->SetValue(m_work.desc);
    m_w->SetValue(m_work.w);
    m_h->SetValue(m_work.h);
    m_bits->SetValue(m_work.bits > 0 ? m_work.bits : 1);
    int li = m_work.logic;
    if (li < 0 || li >= CustomLogicCount()) li = 0;
    m_logic->SetSelection(li);
    m_list->SetSelection(idx);
    m_syncing = false;

    m_canvas->CancelPending();
    RefreshPinList();
    RefreshPreview();
    m_canvas->SetTool(m_canvas->Tool());
}

void CustomLibDialog::WorkChanged()
{
    if (m_syncing) return;
    StoreWork();
    m_preview.customDefs.clear();
    m_preview.customDefs.push_back(m_work);
    m_canvas->Refresh(false);
}

void CustomLibDialog::RefreshPreview()
{
    m_preview.customDefs.clear();
    m_preview.customDefs.push_back(m_work);
    m_canvas->Refresh(false);
}

void CustomLibDialog::RefreshPinList()
{
    m_pins->DeleteAllItems();
    for (size_t i = 0; i < m_work.pins.size(); ++i) {
        const CustomPinDef& p = m_work.pins[i];
        long row = m_pins->InsertItem((long)i, p.name);
        m_pins->SetItem(row, 1, p.isInput ? wxT("输入") : wxT("输出"));
        m_pins->SetItem(row, 2, wxString::Format(wxT("%d"), p.bits));
        m_pins->SetItem(row, 3, wxString::Format(wxT("%d"), p.pos.x));
        m_pins->SetItem(row, 4, wxString::Format(wxT("%d"), p.pos.y));
    }
}

void CustomLibDialog::NotifyPinsChanged()
{
    StoreWork();
    RefreshPinList();
    RefreshPreview();
}

// ---------------------------------------------------------------- 事件
void CustomLibDialog::OnField(wxCommandEvent&)
{
    if (m_syncing || m_cur < 0) return;
    m_work.name = m_name->GetValue();
    m_work.desc = m_desc->GetValue();
    m_work.w = m_w->GetValue();
    m_work.h = m_h->GetValue();
    m_work.bits = m_bits->GetValue();
    if (m_work.w < 20) m_work.w = 20;
    if (m_work.h < 20) m_work.h = 20;
    RefreshDefList();
    m_canvas->Refresh(false);
    RefreshPreview();
}

void CustomLibDialog::OnLogic(wxCommandEvent&)
{
    if (m_syncing || m_cur < 0) return;
    m_work.logic = m_logic->GetSelection();
    RefreshPreview();
    StoreWork();
}

void CustomLibDialog::OnSelect(wxCommandEvent&)
{
    if (m_syncing) return;
    int sel = m_list->GetSelection();
    if (sel >= 0 && sel != m_cur) {
        StoreWork();
        LoadDef(sel);
    }
}

void CustomLibDialog::NewDef()
{
    CustomDef d;
    d.name = (m_doc && !m_defs.empty()) ? m_doc->UniqueCustomName(wxT("自定义元件"))
                                        : wxT("自定义元件");
    // 默认外形：一个方框 + 左右各一个引脚
    CustomPrim box;
    box.kind = CP_RECT;
    box.pts.push_back(wxPoint(0, 0));
    box.pts.push_back(wxPoint(d.w, d.h));
    d.prims.push_back(box);

    CustomPinDef pin;
    pin.pos = wxPoint(0, d.h / 2);
    pin.isInput = true;
    pin.bits = 1;
    pin.name = wxT("IN");
    d.pins.push_back(pin);

    pin.pos = wxPoint(d.w, d.h / 2);
    pin.isInput = false;
    pin.name = wxT("OUT");
    d.pins.push_back(pin);

    m_defs.push_back(d);
    m_cur = (int)m_defs.size() - 1;
    m_work = d;
    RefreshDefList();
    LoadDef(m_cur);
    RefreshPreview();
}

void CustomLibDialog::OnNew(wxCommandEvent&)
{
    if (m_cur >= 0) StoreWork();
    NewDef();
}

void CustomLibDialog::OnDup(wxCommandEvent&)
{
    if (m_cur < 0) return;
    StoreWork();
    CustomDef d = m_defs[m_cur];
    d.name = (m_doc ? m_doc->UniqueCustomName(m_defs[m_cur].name + wxT("副本"))
                    : (m_defs[m_cur].name + wxT("副本")));
    m_defs.push_back(d);
    m_cur = (int)m_defs.size() - 1;
    m_work = d;
    RefreshDefList();
    LoadDef(m_cur);
    RefreshPreview();
}

void CustomLibDialog::OnDel(wxCommandEvent&)
{
    if (m_cur < 0 || m_defs.empty()) return;
    m_defs.erase(m_defs.begin() + m_cur);
    m_cur = m_defs.empty() ? -1 : std::min(m_cur, (int)m_defs.size() - 1);
    RefreshDefList();
    if (m_cur >= 0) {
        LoadDef(m_cur);
    } else {
        m_work = CustomDef();
        m_name->ChangeValue(wxEmptyString);
        RefreshPinList();
        m_canvas->Refresh();
    }
}

void CustomLibDialog::OnClear(wxCommandEvent&)
{
    if (m_cur < 0) return;
    m_work.prims.clear();
    m_canvas->CancelPending();
    StoreWork();
    RefreshPreview();
}

void CustomLibDialog::OnPinAdd(wxCommandEvent&)
{
    if (m_cur < 0) return;
    CustomPinDef p;
    p.pos = wxPoint(0, m_work.h / 2);
    p.isInput = true;
    p.bits = 1;
    p.name = wxString::Format(wxT("P%d"), (int)m_work.pins.size() + 1);
    m_work.pins.push_back(p);
    NotifyPinsChanged();
    m_pins->EnsureVisible(m_pins->GetItemCount() - 1);
}

void CustomLibDialog::OnPinDel(wxCommandEvent&)
{
    long sel = m_pins->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel < 0 || sel >= (long)m_work.pins.size()) return;
    m_work.pins.erase(m_work.pins.begin() + sel);
    NotifyPinsChanged();
}

void CustomLibDialog::OnPinActivated(wxListEvent& e)
{
    long row = e.GetIndex();
    if (row < 0 || row >= (long)m_work.pins.size()) return;
    PinEditDialog dlg(this, m_work.pins[row]);
    if (dlg.ShowModal() == wxID_OK) {
        m_work.pins[row] = dlg.Result();
        NotifyPinsChanged();
    }
}

void CustomLibDialog::OnOK(wxCommandEvent&)
{
    StoreWork();
    if (m_doc) {
        m_doc->customDefs = m_defs;
        m_doc->Touch();
    }
    EndModal(wxID_OK);
}

} // namespace lc
