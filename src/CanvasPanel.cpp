// ============================================================================
//  CanvasPanel.cpp  --  电路绘图/编辑画布实现
// ============================================================================
#include "CanvasPanel.h"
#include "MainFrame.h"
#include "Renderer.h"
#include "Simulator.h"
#include "Ids.h"

#include <wx/dcbuffer.h>
#include <wx/menu.h>
#include <wx/textdlg.h>

#include <algorithm>
#include <cmath>

namespace lc {

static int SnapV(int v)
{
    double q = (double)v / GRID;
    return (int)(q >= 0 ? std::floor(q + 0.5) : std::ceil(q - 0.5)) * GRID;
}

// ---------------------------------------------------------------- 构造
CanvasPanel::CanvasPanel(wxWindow* parent, MainFrame* frame)
    : wxScrolledWindow(parent, ID_Canvas, wxDefaultPosition, wxDefaultSize,
                       wxHSCROLL | wxVSCROLL | wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE),
      m_frame(frame)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(1, 1);
    SetVirtualSize(1200, 900);
    SetBackgroundColour(Renderer::ColCanvas());

    Bind(wxEVT_PAINT,            &CanvasPanel::OnPaint, this);
    Bind(wxEVT_SIZE,             &CanvasPanel::OnSize, this);
    Bind(wxEVT_LEFT_DOWN,        &CanvasPanel::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP,          &CanvasPanel::OnLeftUp, this);
    Bind(wxEVT_LEFT_DCLICK,      &CanvasPanel::OnLeftDClick, this);
    Bind(wxEVT_MOTION,           &CanvasPanel::OnMotion, this);
    Bind(wxEVT_RIGHT_DOWN,       &CanvasPanel::OnRightDown, this);
    Bind(wxEVT_MIDDLE_DOWN,      &CanvasPanel::OnMiddleDown, this);
    Bind(wxEVT_MIDDLE_UP,        &CanvasPanel::OnMiddleUp, this);
    Bind(wxEVT_MOUSEWHEEL,       &CanvasPanel::OnMouseWheel, this);
    Bind(wxEVT_KEY_DOWN,         &CanvasPanel::OnKeyDown, this);
    Bind(wxEVT_LEAVE_WINDOW,     &CanvasPanel::OnLeave, this);
    Bind(wxEVT_ERASE_BACKGROUND, &CanvasPanel::OnEraseBackground, this);
    Bind(wxEVT_MENU,             &CanvasPanel::OnContextMenu, this, ID_EditAttrs, ID_EditAttrs);
}

void CanvasPanel::SetContext(Document* doc, Simulator* sim)
{
    m_doc = doc;
    m_sim = sim;
    RefreshAll();
}

void CanvasPanel::SetActiveCircuit(int idx)
{
    if (!m_doc) return;
    if (idx < 0 || idx >= (int)m_doc->circuits.size()) return;
    m_doc->active = idx;
    m_haveOrigin = false;      // 换电路图 → 重建坐标基准
    m_placeValid = false;
    CancelWire();
    RefreshAll();
}

void CanvasPanel::SetTool(int tool, int placeType)
{
    if (tool >= 0) m_tool = tool;
    if (placeType >= 0) m_placeType = placeType;
    m_placeValid = false;
    CancelWire();
    if (m_tool == TOOL_WIRE)        SetCursor(wxCursor(wxCURSOR_PENCIL));
    else if (m_tool == TOOL_POKE)   SetCursor(wxCursor(wxCURSOR_HAND));
    else if (m_tool == TOOL_ERASE)  SetCursor(wxCursor(wxCURSOR_NO_ENTRY));
    else if (m_tool == TOOL_PLACE)  SetCursor(wxCursor(wxCURSOR_CROSS));
    else if (m_tool == TOOL_TEXT)   SetCursor(wxCursor(wxCURSOR_IBEAM));
    else                            SetCursor(wxCursor(wxCURSOR_ARROW));
    Refresh(false);
}

// ---------------------------------------------------------------- 坐标
wxPoint CanvasPanel::ToWorld(const wxPoint& client) const
{
    CanvasPanel* self = const_cast<CanvasPanel*>(this);
    int sx = 0, sy = 0, pxu = 1, pyu = 1;
    self->GetViewStart(&sx, &sy);
    self->GetScrollPixelsPerUnit(&pxu, &pyu);
    double vx = client.x + sx * pxu;
    double vy = client.y + sy * pyu;
    return wxPoint((int)std::floor(vx / m_zoom) + m_origin.x,
                   (int)std::floor(vy / m_zoom) + m_origin.y);
}

wxPoint CanvasPanel::ToClientPt(const wxPoint& world) const
{
    CanvasPanel* self = const_cast<CanvasPanel*>(this);
    int sx = 0, sy = 0, pxu = 1, pyu = 1;
    self->GetViewStart(&sx, &sy);
    self->GetScrollPixelsPerUnit(&pxu, &pyu);
    return wxPoint((int)((world.x - m_origin.x) * m_zoom) - sx * pxu,
                   (int)((world.y - m_origin.y) * m_zoom) - sy * pyu);
}

wxPoint CanvasPanel::SnapPoint(const wxPoint& p) const
{
    return wxPoint(SnapV(p.x), SnapV(p.y));
}

// 光标世界坐标 → 元件左上角落点。
// 规则：先把光标吸附到栅格，再让元件以该点为中心；最后把左上角也对齐到栅格，
// 这样元件的引脚(局部坐标都是 10 的整数倍)必定落在栅格上，布线吸附才正常。
// 预览和真正放置都调用这一个函数，所以"虚影在哪、落点就在哪"。
wxPoint CanvasPanel::ComputePlacePos(const wxPoint& world, wxSize* szOut) const
{
    Component proto;
    if (m_doc) InitComponentDefaults(proto, m_placeType, m_doc);
    if (m_placeType == CT_SubCircuit && m_frame) proto.sub = m_frame->PlaceSubCircuit();
    wxSize sz = m_doc ? m_doc->Size(proto) : wxSize(40, 40);
    if (szOut) *szOut = sz;
    wxPoint sp = SnapPoint(world);
    return wxPoint(SnapV(sp.x - sz.x / 2), SnapV(sp.y - sz.y / 2));
}

wxPoint CanvasPanel::SnapNearPin(const wxPoint& p, bool* hitPin) const
{
    if (hitPin) *hitPin = false;
    if (!m_doc) return SnapPoint(p);
    const Circuit& c = m_doc->Active();
    int tol = (int)(8 / m_zoom) + 3;
    for (size_t i = 0; i < c.comps.size(); ++i) {
        const Component& m = c.comps[i];
        wxSize sz = m_doc->Size(m);
        std::vector<PinInfo> pins = m_doc->Pins(m);
        for (size_t k = 0; k < pins.size(); ++k) {
            wxPoint wp = LocalToWorld(m, sz, pins[k].pos);
            if (std::abs(wp.x - p.x) <= tol && std::abs(wp.y - p.y) <= tol) {
                if (hitPin) *hitPin = true;
                return wp;
            }
        }
    }
    return SnapPoint(p);
}

// ---------------------------------------------------------------- 虚拟大小
void CanvasPanel::RecalcVirtual()
{
    if (!m_doc) return;
    const Circuit& c = m_doc->Active();
    bool first = true;
    int minX = 0, minY = 0, maxX = 800, maxY = 600;

    for (size_t i = 0; i < c.comps.size(); ++i) {
        wxSize sz = m_doc->Size(c.comps[i]);
        wxRect b = WorldBounds(c.comps[i], sz);
        if (first) { minX = b.x; minY = b.y; maxX = b.GetRight(); maxY = b.GetBottom(); first = false; }
        else {
            minX = std::min(minX, b.x); minY = std::min(minY, b.y);
            maxX = std::max(maxX, b.GetRight()); maxY = std::max(maxY, b.GetBottom());
        }
    }
    for (size_t i = 0; i < c.wires.size(); ++i)
        for (size_t k = 0; k < c.wires[i].pts.size(); ++k) {
            const wxPoint& p = c.wires[i].pts[k];
            if (first) { minX = p.x; minY = p.y; maxX = p.x; maxY = p.y; first = false; }
            else {
                minX = std::min(minX, p.x); minY = std::min(minY, p.y);
                maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y);
            }
        }

    // ---- 坐标原点只“向外扩”，不“向内收” ----
    // 如果原点跟着内容包围盒一起移动，那么每放下一个元件，屏幕上已有的内容都会整体平移，
    // 看起来就像“点这里、元件却出现在别处”。所以原点一旦确定就只允许向左/向上扩展。
    wxPoint want(minX - 300, minY - 300);
    wxPoint oldOrigin = m_origin;
    bool had = m_haveOrigin;
    if (!had) {
        m_origin = want;
        m_haveOrigin = true;
    } else {
        m_origin.x = std::min(m_origin.x, want.x);
        m_origin.y = std::min(m_origin.y, want.y);
    }

    int vw = (int)((std::max(maxX + 300, m_origin.x + 400) - m_origin.x) * m_zoom);
    int vh = (int)((std::max(maxY + 300, m_origin.y + 400) - m_origin.y) * m_zoom);
    vw = std::max(vw, 200);
    vh = std::max(vh, 200);

    if (had && m_origin != oldOrigin) {
        // 原点左/上扩展了：补偿滚动量，让屏幕上的内容保持不动
        int sx = 0, sy = 0, pxu = 1, pyu = 1;
        GetViewStart(&sx, &sy);
        GetScrollPixelsPerUnit(&pxu, &pyu);
        wxPoint worldTL((int)std::floor((sx * pxu) / m_zoom) + oldOrigin.x,
                        (int)std::floor((sy * pyu) / m_zoom) + oldOrigin.y);
        SetVirtualSize(vw, vh);
        int nx = (int)((worldTL.x - m_origin.x) * m_zoom);
        int ny = (int)((worldTL.y - m_origin.y) * m_zoom);
        Scroll(std::max(0, nx), std::max(0, ny));
    } else {
        SetVirtualSize(vw, vh);
    }
}

void CanvasPanel::RefreshAll()
{
    RecalcVirtual();
    Refresh(false);
}

void CanvasPanel::CenterOn(const wxPoint& world)
{
    wxSize cs = GetClientSize();
    int vx = (int)((world.x - m_origin.x) * m_zoom);
    int vy = (int)((world.y - m_origin.y) * m_zoom);
    Scroll(std::max(0, vx - cs.x / 2), std::max(0, vy - cs.y / 2));
}

// ---------------------------------------------------------------- 缩放
void CanvasPanel::ZoomTo(double z, const wxPoint& anchor)
{
    z = std::max(0.25, std::min(4.0, z));
    if (std::fabs(z - m_zoom) < 1e-6) return;

    wxPoint before = ToWorld(anchor);
    m_zoom = z;
    RecalcVirtual();
    wxPoint after = ToWorld(anchor);
    int sx = 0, sy = 0;
    GetViewStart(&sx, &sy);
    Scroll(std::max(0, sx + (int)((after.x - before.x) * m_zoom)),
           std::max(0, sy + (int)((after.y - before.y) * m_zoom)));
    Refresh(false);
    if (m_frame) m_frame->SetStatus(wxString::Format(wxT("缩放 %d%%"), (int)(m_zoom * 100)), 2);
}

void CanvasPanel::ZoomStep(bool in)
{
    static const double steps[] = { 0.25, 0.33, 0.5, 0.67, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0 };
    const int n = (int)(sizeof(steps) / sizeof(steps[0]));
    int cur = 5;
    for (int i = 0; i < n; ++i)
        if (std::fabs(steps[i] - m_zoom) < 0.02) { cur = i; break; }
    cur += in ? 1 : -1;
    cur = std::max(0, std::min(n - 1, cur));
    wxSize cs = GetClientSize();
    ZoomTo(steps[cur], wxPoint(cs.x / 2, cs.y / 2));
}

void CanvasPanel::ZoomFit()
{
    if (!m_doc) return;
    const Circuit& c = m_doc->Active();
    if (c.comps.empty() && c.wires.empty()) { ZoomTo(1.0, wxPoint(0, 0)); return; }
    bool first = true;
    int minX = 0, minY = 0, maxX = 100, maxY = 100;
    for (size_t i = 0; i < c.comps.size(); ++i) {
        wxSize sz = m_doc->Size(c.comps[i]);
        wxRect b = WorldBounds(c.comps[i], sz);
        if (first) { minX = b.x; minY = b.y; maxX = b.GetRight(); maxY = b.GetBottom(); first = false; }
        else {
            minX = std::min(minX, b.x); minY = std::min(minY, b.y);
            maxX = std::max(maxX, b.GetRight()); maxY = std::max(maxY, b.GetBottom());
        }
    }
    for (size_t i = 0; i < c.wires.size(); ++i)
        for (size_t k = 0; k < c.wires[i].pts.size(); ++k) {
            const wxPoint& p = c.wires[i].pts[k];
            if (first) { minX = p.x; minY = p.y; maxX = p.x; maxY = p.y; first = false; }
            else {
                minX = std::min(minX, p.x); minY = std::min(minY, p.y);
                maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y);
            }
        }
    wxSize cs = GetClientSize();
    double zx = (double)(cs.x - 60) / std::max(1, maxX - minX + 60);
    double zy = (double)(cs.y - 60) / std::max(1, maxY - minY + 60);
    double z = std::min(zx, zy);
    z = std::max(0.25, std::min(2.0, z));
    m_zoom = z;
    RecalcVirtual();
    CenterOn(wxPoint((minX + maxX) / 2, (minY + maxY) / 2));
    Refresh(false);
    if (m_frame) m_frame->SetStatus(wxString::Format(wxT("缩放 %d%%"), (int)(m_zoom * 100)), 2);
}

// ---------------------------------------------------------------- 命中
int CanvasPanel::HitComponent(const wxPoint& world, Component** out) const
{
    if (out) *out = nullptr;
    if (!m_doc) return -2;
    Circuit& c = m_doc->Active();
    int tol = (int)(6 / m_zoom) + 3;
    for (int i = (int)c.comps.size() - 1; i >= 0; --i) {
        Component& m = c.comps[i];
        wxSize sz = m_doc->Size(m);
        std::vector<PinInfo> pins = m_doc->Pins(m);
        for (size_t k = 0; k < pins.size(); ++k) {
            wxPoint wp = LocalToWorld(m, sz, pins[k].pos);
            if (std::abs(wp.x - world.x) <= tol && std::abs(wp.y - world.y) <= tol) {
                if (out) *out = &m;
                return (int)k;
            }
        }
        wxRect b = WorldBounds(m, sz);
        if (b.Contains(world)) {
            if (out) *out = &m;
            return -1;
        }
    }
    return -2;
}

int CanvasPanel::HitWire(const wxPoint& world, int tol, int* outIdx) const
{
    if (outIdx) *outIdx = -1;
    if (!m_doc) return -1;
    Circuit& c = m_doc->Active();
    if (tol < 2) tol = 2;
    for (int i = (int)c.wires.size() - 1; i >= 0; --i) {
        const Wire& w = c.wires[i];
        for (size_t k = 0; k + 1 < w.pts.size(); ++k) {
            const wxPoint& a = w.pts[k];
            const wxPoint& b = w.pts[k + 1];
            int d;
            if (a.x == b.x)      d = std::abs(world.x - a.x) + (world.y < std::min(a.y, b.y) ? std::min(a.y, b.y) - world.y : (world.y > std::max(a.y, b.y) ? world.y - std::max(a.y, b.y) : 0));
            else if (a.y == b.y) d = std::abs(world.y - a.y) + (world.x < std::min(a.x, b.x) ? std::min(a.x, b.x) - world.x : (world.x > std::max(a.x, b.x) ? world.x - std::max(a.x, b.x) : 0));
            else                 d = 1000;
            if (d <= tol) {
                if (outIdx) *outIdx = i;
                return i;
            }
        }
    }
    return -1;
}

int CanvasPanel::SelectedCount() const
{
    if (!m_doc) return 0;
    const Circuit& c = m_doc->Active();
    int n = 0;
    for (size_t i = 0; i < c.comps.size(); ++i) if (c.comps[i].selected) ++n;
    for (size_t i = 0; i < c.wires.size(); ++i) if (c.wires[i].selected) ++n;
    return n;
}

Component* CanvasPanel::FirstSelected()
{
    if (!m_doc) return nullptr;
    Circuit& c = m_doc->Active();
    for (size_t i = 0; i < c.comps.size(); ++i)
        if (c.comps[i].selected) return &c.comps[i];
    return nullptr;
}

void CanvasPanel::SelectAll()
{
    if (!m_doc) return;
    Circuit& c = m_doc->Active();
    for (size_t i = 0; i < c.comps.size(); ++i) c.comps[i].selected = true;
    for (size_t i = 0; i < c.wires.size(); ++i) c.wires[i].selected = true;
    Refresh(false);
    if (m_frame) m_frame->RefreshAttrs();
}

void CanvasPanel::ClearSelection()
{
    if (!m_doc) return;
    m_doc->Active().ClearSelection();
    Refresh(false);
    if (m_frame) m_frame->RefreshAttrs();
}

void CanvasPanel::SelectAt(const wxPoint& world, bool add)
{
    Circuit& c = m_doc->Active();
    if (!add) c.ClearSelection();
    Component* hitC = nullptr;
    int pin = HitComponent(world, &hitC);
    if (hitC) {
        hitC->selected = add ? !hitC->selected : true;
    } else {
        int wi = -1;
        if (HitWire(world, 6, &wi) >= 0 && wi >= 0)
            c.wires[wi].selected = add ? !c.wires[wi].selected : true;
    }
}

void CanvasPanel::MoveSelection(int dx, int dy)
{
    if (!m_doc) return;
    Circuit& c = m_doc->Active();
    for (size_t i = 0; i < c.comps.size(); ++i)
        if (c.comps[i].selected) {
            c.comps[i].pos.x += dx;
            c.comps[i].pos.y += dy;
        }
    for (size_t i = 0; i < c.wires.size(); ++i)
        if (c.wires[i].selected)
            for (size_t k = 0; k < c.wires[i].pts.size(); ++k) {
                c.wires[i].pts[k].x += dx;
                c.wires[i].pts[k].y += dy;
            }
}

void CanvasPanel::DeleteSelection()
{
    if (!m_doc || SelectedCount() == 0) return;
    if (m_frame) m_frame->PushUndo(wxT("删除"));
    m_doc->Active().RemoveSelected();
    if (m_frame) m_frame->AfterEdit();
}

// ---------------------------------------------------------------- 剪贴板
void CanvasPanel::CopySelection()
{
    if (!m_doc) return;
    Circuit& c = m_doc->Active();
    m_clipComps.clear();
    m_clipWires.clear();
    for (size_t i = 0; i < c.comps.size(); ++i)
        if (c.comps[i].selected) m_clipComps.push_back(c.comps[i]);
    for (size_t i = 0; i < c.wires.size(); ++i)
        if (c.wires[i].selected) m_clipWires.push_back(c.wires[i]);
    if (m_frame)
        m_frame->SetStatus(wxString::Format(wxT("已复制 %d 个元件、%d 条导线"),
                                            (int)m_clipComps.size(), (int)m_clipWires.size()), 0);
}

void CanvasPanel::CutSelection()
{
    CopySelection();
    DeleteSelection();
}

void CanvasPanel::PasteClipboard()
{
    if (!m_doc) return;
    if (m_clipComps.empty() && m_clipWires.empty()) {
        if (m_frame) m_frame->SetStatus(wxT("剪贴板为空"), 0);
        return;
    }
    if (m_frame) m_frame->PushUndo(wxT("粘贴"));
    Circuit& c = m_doc->Active();
    c.ClearSelection();
    const int off = 20;
    for (size_t i = 0; i < m_clipComps.size(); ++i) {
        Component m = m_clipComps[i];
        m.pos.x += off;
        m.pos.y += off;
        m.selected = true;
        m.id = c.nextId++;
        c.comps.push_back(m);
    }
    for (size_t i = 0; i < m_clipWires.size(); ++i) {
        Wire w = m_clipWires[i];
        for (size_t k = 0; k < w.pts.size(); ++k) {
            w.pts[k].x += off;
            w.pts[k].y += off;
        }
        w.selected = true;
        c.wires.push_back(w);
    }
    if (m_frame) m_frame->AfterEdit();
}

// ---------------------------------------------------------------- 画布绘制
void CanvasPanel::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(Renderer::ColCanvas()));
    dc.Clear();

    if (!m_doc) return;

    int sx = 0, sy = 0, pxu = 1, pyu = 1;
    GetViewStart(&sx, &sy);
    GetScrollPixelsPerUnit(&pxu, &pyu);
    dc.SetUserScale(m_zoom, m_zoom);
    dc.SetDeviceOrigin(-sx * pxu - (int)(m_origin.x * m_zoom),
                       -sy * pyu - (int)(m_origin.y * m_zoom));

    wxSize cs = GetClientSize();
    wxPoint tl = ToWorld(wxPoint(0, 0));
    wxPoint br = ToWorld(wxPoint(cs.x, cs.y));

    // ---- 栅格
    if (m_showGrid && m_zoom >= 0.4) {
        int x0 = (tl.x / GRID) * GRID - GRID;
        int y0 = (tl.y / GRID) * GRID - GRID;
        int nx = (br.x - x0) / GRID + 2;
        int ny = (br.y - y0) / GRID + 2;
        if (nx < 800 && ny < 800) {
            dc.SetPen(wxPen(Renderer::ColGrid(), 0));
            for (int i = 0; i < nx; ++i) {
                int x = x0 + i * GRID;
                if ((x / GRID) % 5 == 0) continue;
                dc.DrawLine(x, tl.y, x, br.y);
            }
            for (int j = 0; j < ny; ++j) {
                int y = y0 + j * GRID;
                if ((y / GRID) % 5 == 0) continue;
                dc.DrawLine(tl.x, y, br.x, y);
            }
            dc.SetPen(wxPen(Renderer::ColGridBold(), 0));
            for (int i = 0; i < nx; ++i) {
                int x = x0 + i * GRID;
                if ((x / GRID) % 5 != 0) continue;
                dc.DrawLine(x, tl.y, x, br.y);
            }
            for (int j = 0; j < ny; ++j) {
                int y = y0 + j * GRID;
                if ((y / GRID) % 5 != 0) continue;
                dc.DrawLine(tl.x, y, br.x, y);
            }
        }
    }

    Circuit& c = m_doc->Active();

    // ---- 导线
    for (size_t i = 0; i < c.wires.size(); ++i)
        Renderer::DrawWire(dc, c.wires[i], m_sim, c.wires[i].selected, (int)(m_zoom * 100));

    // ---- 元件
    std::vector<PinInfo> tmp;
    for (size_t i = 0; i < c.comps.size(); ++i)
        Renderer::DrawComponent(dc, m_doc, c.comps[i], m_sim, c.comps[i].selected, false);

    // ---- 正在绘制的导线
    if (m_wiring && !m_wirePts.empty()) {
        std::vector<wxPoint> pts = m_wirePts;
        wxPoint m = SnapNearPin(m_wireCur, nullptr);
        if (m != pts.back()) {
            wxPoint L = pts.back();
            if (m.x != L.x && m.y != L.y) {
                if (std::abs(m.x - L.x) >= std::abs(m.y - L.y)) pts.push_back(wxPoint(m.x, L.y));
                else                                            pts.push_back(wxPoint(L.x, m.y));
            }
            pts.push_back(m);
        }
        dc.SetPen(wxPen(Renderer::ColWireOff(), 2));
        dc.DrawLines((int)pts.size(), &pts[0]);
        dc.SetPen(wxPen(Renderer::ColSelect(), 1));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        for (size_t i = 0; i < pts.size(); ++i)
            dc.DrawRectangle(pts[i].x - 3, pts[i].y - 3, 6, 6);
    }

    // ---- 橡皮筋
    if (m_rubber) {
        int x1 = std::min(m_rubberA.x, m_rubberB.x), x2 = std::max(m_rubberA.x, m_rubberB.x);
        int y1 = std::min(m_rubberA.y, m_rubberB.y), y2 = std::max(m_rubberA.y, m_rubberB.y);
        dc.SetPen(wxPen(Renderer::ColSelect(), 0, wxPENSTYLE_SHORT_DASH));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(x1, y1, x2 - x1, y2 - y1);
    }

    // ---- 放置预览（用 m_placePos，和真正放置同一套计算）
    if (m_tool == TOOL_PLACE && m_placeValid) {
        Component proto;
        InitComponentDefaults(proto, m_placeType, m_doc);
        if (m_placeType == CT_SubCircuit && m_frame) proto.sub = m_frame->PlaceSubCircuit();
        proto.pos = m_placePos;
        wxSize psz = m_doc->Size(proto);
        // 先把元件本体的样子画出来(不带仿真值)，所见即所得
        Renderer::DrawComponent(dc, m_doc, proto, nullptr, false, false);
        // 再套一个虚线框
        dc.SetPen(wxPen(Renderer::ColSelect(), 0, wxPENSTYLE_DOT));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        wxRect b = WorldBounds(proto, psz);
        dc.DrawRectangle(b.x - 2, b.y - 2, b.width + 4, b.height + 4);
    }
}

// ---------------------------------------------------------------- 事件
void CanvasPanel::OnSize(wxSizeEvent& e)
{
    RecalcVirtual();
    Refresh(false);
    e.Skip();
}

void CanvasPanel::OnLeave(wxMouseEvent& e)
{
    if (m_frame) m_frame->SetStatus(wxEmptyString, 1);
    e.Skip();
}

void CanvasPanel::OnMouseWheel(wxMouseEvent& e)
{
    if (e.ControlDown() || e.GetWheelRotation() == 0) {
        ZoomStep(e.GetWheelRotation() > 0);
    } else {
        int sx = 0, sy = 0;
        GetViewStart(&sx, &sy);
        if (e.ShiftDown()) Scroll(std::max(0, sx - e.GetWheelRotation() / 40), sy);
        else               Scroll(sx, std::max(0, sy - e.GetWheelRotation() / 40));
    }
}

void CanvasPanel::OnMiddleDown(wxMouseEvent& e)
{
    m_panning = true;
    m_panStart = e.GetPosition();
    int sx = 0, sy = 0;
    GetViewStart(&sx, &sy);
    m_panOriginStart = wxPoint(sx, sy);
    SetCursor(wxCursor(wxCURSOR_SIZING));
}

void CanvasPanel::OnMiddleUp(wxMouseEvent&)
{
    m_panning = false;
    SetTool(m_tool, -1);
}

void CanvasPanel::OnMotion(wxMouseEvent& e)
{
    if (!m_doc) return;
    wxPoint w = ToWorld(e.GetPosition());

    if (m_panning) {
        wxPoint d = e.GetPosition() - m_panStart;
        Scroll(std::max(0, m_panOriginStart.x - d.x), std::max(0, m_panOriginStart.y - d.y));
        return;
    }

    // 状态栏坐标
    if (m_frame)
        m_frame->SetStatus(wxString::Format(wxT("X: %d   Y: %d"), w.x, w.y), 1);

    if (m_dragging) {
        wxPoint d = w - m_dragLast;
        wxPoint s(SnapV(d.x), SnapV(d.y));
        if (s.x != 0 || s.y != 0) {
            if (!m_dragUndo && m_frame) { m_frame->PushUndo(wxT("移动")); m_dragUndo = true; }
            MoveSelection(s.x, s.y);
            m_dragLast += s;
            RecalcVirtual();
            Refresh(false);
        }
        return;
    }
    if (m_rubber) {
        m_rubberB = w;
        Refresh(false);
        return;
    }
    if (m_wiring) {
        m_wireCur = w;
        Refresh(false);
        return;
    }
    if (m_tool == TOOL_PLACE) {
        // 预览位置与真正放置共用 ComputePlacePos，二者不可能再跑偏
        wxPoint pp = ComputePlacePos(w);
        if (!m_placeValid || pp != m_placePos) {
            m_placePos = pp;
            m_placeValid = true;
            Refresh(false);          // ← 关键：让虚影跟着光标走
        }
    } else if (m_placeValid) {
        m_placeValid = false;
        Refresh(false);
    }
}

void CanvasPanel::OnLeftDown(wxMouseEvent& e)
{
    SetFocus();
    if (!m_doc) return;
    wxPoint w = ToWorld(e.GetPosition());
    Circuit& c = m_doc->Active();

    switch (m_tool) {
    case TOOL_EDIT: {
        Component* hitC = nullptr;
        int pin = HitComponent(w, &hitC);
        int wi = -1;
        int wh = HitWire(w, 6, &wi);
        bool hitSomething = (hitC != nullptr) || (wh >= 0);
        bool alreadySel = false;
        if (hitC) alreadySel = hitC->selected;
        else if (wh >= 0 && wi >= 0) alreadySel = c.wires[wi].selected;

        if (hitSomething) {
            if (e.ShiftDown()) {
                if (hitC) hitC->selected = !hitC->selected;
                else if (wi >= 0) c.wires[wi].selected = !c.wires[wi].selected;
            } else if (!alreadySel) {
                c.ClearSelection();
                if (hitC) hitC->selected = true;
                else if (wi >= 0) c.wires[wi].selected = true;
            }
            m_dragging = true;
            m_dragUndo = false;
            m_dragLast = w;
        } else {
            if (!e.ShiftDown()) c.ClearSelection();
            m_rubber = true;
            m_rubberA = m_rubberB = w;
        }
        Refresh(false);
        if (m_frame) m_frame->RefreshAttrs();
        (void)pin;
        break; }

    case TOOL_POKE: {
        Circuit& cc = m_doc->Active();
        Component* hitC = nullptr;
        int pin = HitComponent(w, &hitC);

        // 只点中“引脚连接点”才算数太苛刻了：点在输入引脚/按钮的符号本身上也应该有效。
        // 另外给 20px 的宽容度，找最近的输入引脚/按钮。
        if (!hitC || (hitC->type != CT_InputPin && hitC->type != CT_Button)) {
            Component* nearComp = nullptr;      // 注意: 不能叫 near，它是 Windows 的遗留宏
            double best = 1e9;
            for (size_t i = 0; i < cc.comps.size(); ++i) {
                Component& m2 = cc.comps[i];
                if (m2.type != CT_InputPin && m2.type != CT_Button) continue;
                wxSize s2 = m_doc->Size(m2);
                wxRect b2 = WorldBounds(m2, s2);
                int dx = std::max(0, std::max(b2.x - w.x, w.x - b2.GetRight()));
                int dy = std::max(0, std::max(b2.y - w.y, w.y - b2.GetBottom()));
                double d = std::sqrt((double)dx * dx + (double)dy * dy);
                if (d < best) { best = d; nearComp = &m2; }
            }
            if (nearComp && best <= 20.0) { hitC = nearComp; pin = 0; }
        }

        if (hitC && hitC->type == CT_InputPin) {
            if (m_frame) m_frame->PushUndo(wxT("改变输入"));
            int b = ClampInt(hitC->bits, 1, MAX_BITS);
            if (e.ShiftDown() && b > 1) {
                hitC->value = ((hitC->value << 1) | (hitC->value >> (b - 1))) & MaskOf(b);
            } else {
                hitC->value ^= 1u;
            }
            if (m_frame) {
                m_frame->SetStatus(wxString::Format(wxT("输入 “%s” = %s"),
                    hitC->label.empty() ? wxString(wxT("引脚")) : hitC->label,
                    FormatValue(hitC->value, b)), 0);
                m_frame->AfterEditNoUndo();
            }
        } else if (hitC && hitC->type == CT_Button) {
            if (m_frame) m_frame->PushUndo(wxT("按下按钮"));
            hitC->value = 1;
            m_pokeBtn = hitC;
            if (m_frame) {
                m_frame->SetStatus(wxT("按钮按下（松开恢复 0）"), 0);
                m_frame->AfterEditNoUndo();
            }
        } else if (hitC && (hitC->type == CT_OutputPin || hitC->type == CT_LED ||
                            hitC->type == CT_Probe || hitC->type == CT_HexDisplay)) {
            // 显示类元件：查探时把当前值显示在状态栏，避免“点了没反应”的错觉
            PinValue v = m_sim ? m_sim->PinOf(*hitC, 0) : PinValue();
            if (m_frame) {
                wxString txt = (v.state == 0) ? wxString(wxT("悬空"))
                             : (v.state == 2) ? wxString(wxT("冲突/错误"))
                             : FormatValue(v.bits, ClampInt(hitC->bits, 1, MAX_BITS));
                m_frame->SetStatus(wxT("该处电平: ") + txt, 0);
            }
        } else if (m_frame) {
            m_frame->SetStatus(wxT("查探：点击输入引脚或按钮可翻转电平（Shift+点击多位引脚按位循环）"), 0);
        }
        Refresh(false);
        (void)pin;
        break; }

    case TOOL_WIRE: {
        bool atPin = false;
        wxPoint p = SnapNearPin(w, &atPin);
        if (!m_wiring) {
            m_wiring = true;
            m_wirePts.clear();
            m_wirePts.push_back(p);
        } else {
            wxPoint L = m_wirePts.back();
            if (p != L) {
                if (p.x != L.x && p.y != L.y) {
                    if (std::abs(p.x - L.x) >= std::abs(p.y - L.y)) m_wirePts.push_back(wxPoint(p.x, L.y));
                    else                                            m_wirePts.push_back(wxPoint(L.x, p.y));
                }
                m_wirePts.push_back(p);
            }
        }
        m_wireCur = w;
        Refresh(false);
        break; }

    case TOOL_PLACE: {
        if (m_placeType == CT_SubCircuit && m_doc->circuits.size() < 2) {
            if (m_frame) m_frame->SetStatus(wxT("还没有子电路，请先在“项目”菜单里添加"), 0);
            break;
        }
        if (m_frame) m_frame->PushUndo(wxT("放置元件"));
        Component m;
        InitComponentDefaults(m, m_placeType, m_doc);
        // 工具箱里选的是“第几个”子电路，必须在这里落到元件上
        if (m_placeType == CT_SubCircuit && m_frame) m.sub = m_frame->PlaceSubCircuit();
        wxSize sz = m_doc->Size(m);
        // 与预览共用同一个函数，保证虚影位置 = 实际落点
        m.pos = ComputePlacePos(w);
        m.id = c.nextId++;
        c.comps.push_back(m);
        if (m_frame) m_frame->AfterEditNoUndo();
        break; }

    case TOOL_ERASE: {
        Component* hitC = nullptr;
        int pin = HitComponent(w, &hitC);
        int wi = -1;
        int wh = HitWire(w, 6, &wi);
        if (hitC || wh >= 0) {
            if (m_frame) m_frame->PushUndo(wxT("删除"));
            if (hitC) {
                for (size_t i = 0; i < c.comps.size(); ++i)
                    if (&c.comps[i] == hitC) { c.comps.erase(c.comps.begin() + i); break; }
            } else if (wi >= 0) {
                c.wires.erase(c.wires.begin() + wi);
            }
            if (m_frame) m_frame->AfterEditNoUndo();
        }
        (void)pin;
        break; }

    case TOOL_TEXT: {
        wxString s = wxGetTextFromUser(wxT("请输入文本内容:"), wxT("添加文本"), wxT("注释"), this);
        if (s.empty()) break;
        if (m_frame) m_frame->PushUndo(wxT("添加文本"));
        Component m;
        InitComponentDefaults(m, CT_Text, m_doc);
        m.label = s;
        m.pos = wxPoint(SnapV(w.x), SnapV(w.y));
        m.id = c.nextId++;
        c.comps.push_back(m);
        if (m_frame) m_frame->AfterEditNoUndo();
        break; }

    default: break;
    }
}

void CanvasPanel::OnLeftUp(wxMouseEvent& e)
{
    if (!m_doc) return;
    wxPoint w = ToWorld(e.GetPosition());

    if (m_pokeBtn) {
        Component* b = m_pokeBtn;
        m_pokeBtn = nullptr;
        if (b) {
            b->value = 0;
            if (m_frame) m_frame->AfterEditNoUndo();
        }
    }

    if (m_dragging) {
        m_dragging = false;
        if (m_dragUndo && m_frame) m_frame->AfterEditNoUndo();
    }

    if (m_rubber) {
        m_rubber = false;
        int x1 = std::min(m_rubberA.x, m_rubberB.x), x2 = std::max(m_rubberA.x, m_rubberB.x);
        int y1 = std::min(m_rubberA.y, m_rubberB.y), y2 = std::max(m_rubberA.y, m_rubberB.y);
        if (x2 - x1 > 3 || y2 - y1 > 3) {
            Circuit& c = m_doc->Active();
            for (size_t i = 0; i < c.comps.size(); ++i) {
                wxSize sz = m_doc->Size(c.comps[i]);
                wxRect b = WorldBounds(c.comps[i], sz);
                if (b.x >= x1 && b.y >= y1 && b.GetRight() <= x2 && b.GetBottom() <= y2)
                    c.comps[i].selected = true;
            }
            for (size_t i = 0; i < c.wires.size(); ++i) {
                bool all = !c.wires[i].pts.empty();
                for (size_t k = 0; k < c.wires[i].pts.size(); ++k) {
                    const wxPoint& p = c.wires[i].pts[k];
                    if (!(p.x >= x1 && p.x <= x2 && p.y >= y1 && p.y <= y2)) { all = false; break; }
                }
                if (all) c.wires[i].selected = true;
            }
            if (m_frame) m_frame->RefreshAttrs();
        }
        Refresh(false);
    }

    // 双击完成连线由 OnLeftDClick 处理；这里处理"点空白处结束"
    if (m_wiring && m_wirePts.size() >= 2) {
        // 点在最后一个点上两次视为结束
        if (w == m_wirePts.back()) FinishWire();
    }
}

void CanvasPanel::OnLeftDClick(wxMouseEvent& e)
{
    if (!m_doc) return;
    wxPoint w = ToWorld(e.GetPosition());
    if (m_wiring) { FinishWire(); return; }

    Component* hitC = nullptr;
    int pin = HitComponent(w, &hitC);
    if (hitC && hitC->type == CT_SubCircuit && hitC->sub >= 0) {
        if (m_frame) m_frame->SetActiveCircuit(hitC->sub);
        return;
    }
    if (hitC && m_tool == TOOL_EDIT) {
        if (m_frame) m_frame->EditAttributesOf(hitC);
    }
    (void)pin;
}

void CanvasPanel::FinishWire()
{
    if (!m_wiring) return;
    m_wiring = false;
    std::vector<wxPoint> pts = m_wirePts;
    m_wirePts.clear();
    if (pts.size() < 2 || !m_doc) { Refresh(false); return; }

    if (m_frame) m_frame->PushUndo(wxT("绘制导线"));
    Circuit& c = m_doc->Active();
    Wire w;
    w.pts = pts;
    w.bits = 1;
    c.wires.push_back(w);
    if (m_frame) m_frame->AfterEditNoUndo();
}

void CanvasPanel::CancelWire()
{
    if (m_wiring) {
        m_wiring = false;
        m_wirePts.clear();
        Refresh(false);
    }
    if (m_rubber) {
        m_rubber = false;
        Refresh(false);
    }
}

void CanvasPanel::OnRightDown(wxMouseEvent& e)
{
    SetFocus();
    if (!m_doc) return;
    if (m_wiring) { FinishWire(); return; }

    wxPoint w = ToWorld(e.GetPosition());
    Circuit& c = m_doc->Active();
    Component* hitC = nullptr;
    int pin = HitComponent(w, &hitC);
    int wi = -1;
    int wh = HitWire(w, 6, &wi);
    if (hitC && !hitC->selected) {
        c.ClearSelection();
        hitC->selected = true;
        Refresh(false);
        if (m_frame) m_frame->RefreshAttrs();
    } else if (!hitC && wh >= 0 && wi >= 0 && !c.wires[wi].selected) {
        c.ClearSelection();
        c.wires[wi].selected = true;
        Refresh(false);
        if (m_frame) m_frame->RefreshAttrs();
    } else if (!hitC && wh < 0) {
        c.ClearSelection();
        Refresh(false);
        if (m_frame) m_frame->RefreshAttrs();
    }

    wxMenu menu;
    bool has = SelectedCount() > 0;
    if (has) {
        menu.Append(ID_EditAttrs, wxT("属性(&P)..."));
        menu.Append(ID_RotateCW,  wxT("顺时针旋转\tR"));
        menu.Append(ID_RotateCCW, wxT("逆时针旋转"));
        menu.AppendSeparator();
        menu.Append(ID_Cut,       wxT("剪切\tCtrl+X"));
        menu.Append(ID_Copy,      wxT("复制\tCtrl+C"));
        menu.Append(ID_Duplicate, wxT("创建副本"));
        menu.Append(ID_Delete,    wxT("删除\tDel"));
        menu.AppendSeparator();
    }
    menu.Append(ID_Paste,     wxT("粘贴\tCtrl+V"));
    menu.Append(ID_SelectAll, wxT("全选\tCtrl+A"));
    menu.Append(ID_Deselect,  wxT("取消选择"));
    menu.AppendSeparator();
    menu.Append(ID_ZoomIn,    wxT("放大\tCtrl+="));
    menu.Append(ID_ZoomOut,   wxT("缩小\tCtrl+-"));
    menu.Append(ID_ZoomFit,   wxT("适应窗口\tCtrl+0"));
    menu.Append(ID_Zoom100,   wxT("实际大小\tCtrl+1"));
    menu.AppendSeparator();
    menu.Append(ID_ShowGrid,  wxT("显示网格"), wxEmptyString, wxITEM_CHECK);
    menu.Check(ID_ShowGrid, m_showGrid);
    PopupMenu(&menu);
    (void)pin;
}

void CanvasPanel::OnContextMenu(wxCommandEvent& e)
{
    if (e.GetId() == ID_EditAttrs) {
        Component* c = FirstSelected();
        if (c && m_frame) m_frame->EditAttributesOf(c);
    }
    e.Skip();
}

// ---------------------------------------------------------------- 键盘
void CanvasPanel::OnKeyDown(wxKeyEvent& e)
{
    if (!m_doc) { e.Skip(); return; }
    bool ctrl = e.ControlDown();
    switch (e.GetKeyCode()) {
    case WXK_ESCAPE:
        CancelWire();
        ClearSelection();
        return;
    case WXK_DELETE:
        DeleteSelection();
        return;
    case WXK_LEFT:
        if (m_frame) m_frame->PushUndo(wxT("移动"));
        MoveSelection(ctrl ? -1 : -GRID, 0);
        if (m_frame) m_frame->AfterEditNoUndo();
        return;
    case WXK_RIGHT:
        if (m_frame) m_frame->PushUndo(wxT("移动"));
        MoveSelection(ctrl ? 1 : GRID, 0);
        if (m_frame) m_frame->AfterEditNoUndo();
        return;
    case WXK_UP:
        if (m_frame) m_frame->PushUndo(wxT("移动"));
        MoveSelection(0, ctrl ? -1 : -GRID);
        if (m_frame) m_frame->AfterEditNoUndo();
        return;
    case WXK_DOWN:
        if (m_frame) m_frame->PushUndo(wxT("移动"));
        MoveSelection(0, ctrl ? 1 : GRID);
        if (m_frame) m_frame->AfterEditNoUndo();
        return;
    case 'R': case 'r':
        if (m_frame) m_frame->RotateSelection(true);
        return;
    default: break;
    }
    e.Skip();
}

void CanvasPanel::ApplyAttrChange(const wxString&, const wxString&)
{
    if (m_frame) m_frame->AfterEditNoUndo();
}

} // namespace lc
