// ============================================================================
//  Renderer.cpp  --  元件矢量绘图实现
// ============================================================================
#include "Renderer.h"
#include "Simulator.h"

#include <wx/dcmemory.h>
#include <wx/pen.h>
#include <wx/brush.h>
#include <wx/font.h>

#include <algorithm>
#include <cmath>

namespace lc {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------- 颜色
wxColour Renderer::ColWireOn()    { return wxColour(0, 190, 0); }
wxColour Renderer::ColWireOff()   { return wxColour(0, 100, 0); }
wxColour Renderer::ColWireFloat() { return wxColour(70, 130, 200); }
wxColour Renderer::ColWireErr()   { return wxColour(220, 0, 0); }
wxColour Renderer::ColOutline()   { return wxColour(20, 20, 20); }
wxColour Renderer::ColBody()      { return wxColour(255, 255, 255); }
wxColour Renderer::ColSelect()    { return wxColour(80, 140, 220); }
wxColour Renderer::ColGrid()      { return wxColour(232, 236, 242); }
wxColour Renderer::ColGridBold()  { return wxColour(214, 221, 232); }
wxColour Renderer::ColCanvas()    { return wxColour(255, 255, 255); }

wxColour Renderer::StateColour(int state, bool bit1)
{
    if (state == 2) return ColWireErr();
    if (state == 0) return ColWireFloat();
    return bit1 ? ColWireOn() : ColWireOff();
}

wxColour Renderer::StateColour(const PinValue& v)
{
    return StateColour(v.state, v.Bit(0));
}

// ---------------------------------------------------------------- 几何小工具
namespace {

typedef std::vector<wxPoint> Poly;

void AddQuad(Poly& out, const wxPoint& a, const wxPoint& c, const wxPoint& b, int n = 14)
{
    for (int i = 1; i <= n; ++i) {
        double t = (double)i / n;
        double mt = 1.0 - t;
        double x = mt * mt * a.x + 2 * mt * t * b.x + t * t * c.x;
        double y = mt * mt * a.y + 2 * mt * t * b.y + t * t * c.y;
        out.push_back(wxPoint((int)std::floor(x + 0.5), (int)std::floor(y + 0.5)));
    }
}

void AddArc(Poly& out, int cx, int cy, int r, double a0, double a1, int n = 16)
{
    for (int i = 0; i <= n; ++i) {
        double a = a0 + (a1 - a0) * i / n;
        out.push_back(wxPoint((int)std::floor(cx + r * std::cos(a) + 0.5),
                              (int)std::floor(cy + r * std::sin(a) + 0.5)));
    }
}

void AddRect(Poly& out, int x, int y, int w, int h)
{
    out.push_back(wxPoint(x, y));
    out.push_back(wxPoint(x + w, y));
    out.push_back(wxPoint(x + w, y + h));
    out.push_back(wxPoint(x, y + h));
}

/// 把局部坐标点转换成世界坐标
inline wxPoint W(const Component& c, const wxSize& sz, int x, int y)
{
    return LocalToWorld(c, sz, wxPoint(x, y));
}

void DrawPolyWorld(wxDC& dc, const Component& c, const wxSize& sz, const Poly& p,
                   bool fill, bool close = true)
{
    if (p.size() < 2) return;
    Poly w;
    w.reserve(p.size());
    for (size_t i = 0; i < p.size(); ++i)
        w.push_back(LocalToWorld(c, sz, p[i]));
    if (fill) dc.DrawPolygon((int)w.size(), &w[0]);
    else      dc.DrawLines((int)w.size(), &w[0]);
    (void)close;
}

void LineW(wxDC& dc, const Component& c, const wxSize& sz, int x1, int y1, int x2, int y2)
{
    dc.DrawLine(W(c, sz, x1, y1), W(c, sz, x2, y2));
}

void CircleW(wxDC& dc, const Component& c, const wxSize& sz, int x, int y, int r)
{
    wxPoint p = W(c, sz, x, y);
    dc.DrawCircle(p, r);
}

void TextW(wxDC& dc, const Component& c, const wxSize& sz, int x, int y, const wxString& s,
           bool center = true)
{
    wxPoint p = W(c, sz, x, y);
    if (center) {
        wxSize e = dc.GetTextExtent(s);
        dc.DrawText(s, p.x - e.x / 2, p.y - e.y / 2);
    } else {
        dc.DrawText(s, p);
    }
}

// 逻辑门的形状高度/宽度
int GateH(int inputs) { return std::max(40, 20 * std::max(2, inputs)); }
int GateW(int h)      { return std::max(60, h / 2 + 40); }

wxString FmtBits(int b) { return wxString::Format(wxT("%d 位"), b); }

bool IsNegated(int t)
{
    return t == CT_NAND || t == CT_NOR || t == CT_XNOR || t == CT_NOT;
}
bool IsOrShape(int t)
{
    return t == CT_OR || t == CT_NOR || t == CT_XOR || t == CT_XNOR;
}

} // namespace

// ---------------------------------------------------------------- 元件
void Renderer::DrawComponent(wxDC& dc, Document* doc, const Component& c,
                             const Simulator* sim, bool selected, bool iconMode)
{
    wxSize sz(40, 40);
    std::vector<PinInfo> pins;
    if (doc) {
        sz = doc->Size(c);
        pins = doc->Pins(c);
    }

    // ---- 选中高亮
    if (selected && !iconMode) {
        wxRect b = WorldBounds(c, sz);
        wxColour old = dc.GetPen().GetColour();
        dc.SetPen(wxPen(ColSelect(), 2));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(b.x - 4, b.y - 4, b.width + 8, b.height + 8);
        dc.SetPen(wxPen(old));
    }

    const int w = sz.x, h = sz.y;
    dc.SetPen(wxPen(ColOutline(), 2));
    dc.SetBrush(wxBrush(ColBody()));
    dc.SetTextForeground(ColOutline());

    int smallPt = 7;
    wxFont fSmall(wxFontInfo(smallPt).FaceName(wxT("Microsoft YaHei")));
    wxFont fTiny(wxFontInfo(6).FaceName(wxT("Microsoft YaHei")));
    wxFont fBold(wxFontInfo(8).FaceName(wxT("Microsoft YaHei")).Bold());
    dc.SetFont(fSmall);

    // 逻辑值取用
    auto pinVal = [&](int k) -> PinValue {
        if (!sim) return PinValue();
        return sim->PinOf(c, k);
    };
    auto inVal  = [&](int k) -> PinValue { return pinVal(k); };
    auto outVal = [&](int k) -> PinValue { return pinVal((int)pins.size() - 1 - k); };

    switch (c.type) {

    // ================================================ 三态/门电路
    case CT_NOT: case CT_Buffer: case CT_AND: case CT_OR:
    case CT_NAND: case CT_NOR: case CT_XOR: case CT_XNOR: {
        const bool neg  = IsNegated(c.type);
        const bool orSh = IsOrShape(c.type);
        const int  tipX = neg ? w - 10 : w;

        Poly body;
        if (orSh) {
            body.push_back(wxPoint(0, 0));
            AddQuad(body, wxPoint(0, 0), wxPoint(tipX, h / 2), wxPoint((int)(tipX * 0.62), 0));
            AddQuad(body, wxPoint(tipX, h / 2), wxPoint(0, h), wxPoint((int)(tipX * 0.62), h));
            AddQuad(body, wxPoint(0, h), wxPoint(0, 0), wxPoint(16, h / 2));
            if (c.type == CT_XOR || c.type == CT_XNOR) {
                Poly back;
                back.push_back(wxPoint(0, 0));
                AddQuad(back, wxPoint(0, 0), wxPoint(0, h), wxPoint(10, h / 2));
                DrawPolyWorld(dc, c, sz, back, false);
            }
        } else if (c.type == CT_NOT || c.type == CT_Buffer) {
            body.push_back(wxPoint(0, 0));
            body.push_back(wxPoint(tipX, h / 2));
            body.push_back(wxPoint(0, h));
        } else {
            const int bx = tipX - h / 2;
            body.push_back(wxPoint(0, 0));
            body.push_back(wxPoint(bx, 0));
            AddArc(body, bx, h / 2, h / 2, -M_PI / 2, M_PI / 2);
            body.push_back(wxPoint(bx, h));
            body.push_back(wxPoint(0, h));
        }
        dc.SetBrush(wxBrush(ColBody()));
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        if (neg) {
            dc.SetBrush(wxBrush(ColBody()));
            CircleW(dc, c, sz, tipX + 5, h / 2, 5);
        }
        break; }

    // ================================================ 三态缓冲器(新增元件范例)
    case CT_TriState: {
        // 1) 三角形本体: 局部坐标 (0,0)-(60,30)-(0,60)
        Poly body;
        body.push_back(wxPoint(0, 0));
        body.push_back(wxPoint(w, h / 2));
        body.push_back(wxPoint(0, h));
        dc.SetBrush(wxBrush(ColBody()));
        DrawPolyWorld(dc, c, sz, body, true);    // 填充
        DrawPolyWorld(dc, c, sz, body, false);   // 描边

        // 2) 使能端引线: 从底边引脚 (30,60) 画到斜边上的 (30,45)
        LineW(dc, c, sz, w / 2, h, w / 2, h / 2 + h / 4);
        dc.SetFont(fTiny);
        TextW(dc, c, sz, w / 2 + 14, h - 8, wxT("EN"));

        // 3) 使能无效时在体内标一个 Z，表示输出高阻
        PinValue en = inVal(1);              // 引脚 1 = EN
        if (!(en.state == 1 && en.Bit(0))) {
            dc.SetTextForeground(wxColour(150, 150, 150));
            dc.SetFont(fBold);
            TextW(dc, c, sz, w / 2 - 6, h / 2, wxT("Z"));
            dc.SetTextForeground(ColOutline());
        }
        break; }

    // ================================================ 运行时自定义元件
    case CT_Custom: {
        const CustomDef* d = doc ? doc->FindCustom(c.cdef) : nullptr;

        // 外形底色
        Poly box;
        AddRect(box, 0, 0, w, h);
        dc.SetBrush(wxBrush(wxColour(252, 252, 248)));
        DrawPolyWorld(dc, c, sz, box, true);
        dc.SetPen(wxPen(ColOutline(), 2));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        DrawPolyWorld(dc, c, sz, box, false);

        if (!d) {
            dc.SetFont(fTiny);
            dc.SetTextForeground(wxColour(190, 60, 60));
            TextW(dc, c, sz, w / 2, h / 2, wxT("定义已删除"));
            dc.SetTextForeground(ColOutline());
            break;
        }

        // 图元列表
        for (size_t i = 0; i < d->prims.size(); ++i) {
            const CustomPrim& p = d->prims[i];
            dc.SetPen(wxPen(ColOutline(), 2));
            dc.SetBrush(p.fill ? wxBrush(wxColour(226, 234, 246)) : *wxTRANSPARENT_BRUSH);
            switch (p.kind) {
            case CP_POLY:
                if (p.pts.size() >= 2) DrawPolyWorld(dc, c, sz, p.pts, p.fill);
                break;
            case CP_RECT:
                if (p.pts.size() >= 2) {
                    int x0 = std::min(p.pts[0].x, p.pts[1].x);
                    int y0 = std::min(p.pts[0].y, p.pts[1].y);
                    int x1 = std::max(p.pts[0].x, p.pts[1].x);
                    int y1 = std::max(p.pts[0].y, p.pts[1].y);
                    Poly r;
                    AddRect(r, x0, y0, x1 - x0, y1 - y0);
                    DrawPolyWorld(dc, c, sz, r, p.fill);
                }
                break;
            case CP_CIRCLE:
                if (p.pts.size() >= 2) {
                    int dx = p.pts[1].x - p.pts[0].x;
                    int dy = p.pts[1].y - p.pts[0].y;
                    int rr = (int)(std::sqrt((double)dx * dx + (double)dy * dy) + 0.5);
                    if (rr < 1) rr = 1;
                    wxPoint ctr = LocalToWorld(c, sz, p.pts[0]);
                    dc.DrawCircle(ctr, rr);
                }
                break;
            case CP_TEXT:
                if (!p.text.empty() && !p.pts.empty()) {
                    dc.SetFont(fTiny);
                    dc.SetTextForeground(wxColour(40, 40, 40));
                    TextW(dc, c, sz, p.pts[0].x, p.pts[0].y, p.text);
                    dc.SetTextForeground(ColOutline());
                }
                break;
            default: break;
            }
        }

        // 引脚：圆点 + 名称
        for (size_t k = 0; k < pins.size(); ++k) {
            const wxPoint& lp = pins[k].pos;
            wxPoint wp = LocalToWorld(c, sz, lp);
            wxColour col(60, 60, 60);
            if (sim) col = StateColour(sim->PinOf(c, (int)k));
            dc.SetPen(wxPen(col, 1));
            dc.SetBrush(wxBrush(col));
            dc.DrawCircle(wp, 3);
            if (!pins[k].name.empty()) {
                dc.SetFont(fTiny);
                dc.SetTextForeground(wxColour(70, 70, 70));
                int tx = lp.x, ty = lp.y;
                if (lp.x <= 1)            tx = lp.x + 6;
                else if (lp.x >= w - 1)   tx = lp.x - 6 - (int)pins[k].name.length() * 7;
                else                      ty = lp.y - 9;
                TextW(dc, c, sz, tx, ty, pins[k].name, false);
                dc.SetTextForeground(ColOutline());
            }
        }
        dc.SetPen(wxPen(ColOutline(), 2));
        break; }

    // ================================================ 复用器
    case CT_Mux: {
        int outY = (h / 3 / GRID) * GRID;
        Poly body;
        body.push_back(wxPoint(0, 0));
        body.push_back(wxPoint(w, outY - 10));
        body.push_back(wxPoint(w, outY + 10));
        body.push_back(wxPoint(0, h));
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        LineW(dc, c, sz, w / 2, h, w / 2, outY);
        dc.SetFont(fSmall);
        TextW(dc, c, sz, w / 2 + 4, h / 2, wxT("MUX"));
        break; }

    case CT_Demux: {
        int inY = (h / 3 / GRID) * GRID;
        Poly body;
        body.push_back(wxPoint(0, inY - 10));
        body.push_back(wxPoint(w, 0));
        body.push_back(wxPoint(w, h));
        body.push_back(wxPoint(0, inY + 10));
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        LineW(dc, c, sz, w / 2, h, w / 2, inY);
        dc.SetFont(fTiny);
        TextW(dc, c, sz, w / 2, h / 2, wxT("DMX"));
        break; }

    case CT_Decoder: {
        Poly body;
        body.push_back(wxPoint(0, h / 2 - 14));
        body.push_back(wxPoint(w, 0));
        body.push_back(wxPoint(w, h));
        body.push_back(wxPoint(0, h / 2 + 14));
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        dc.SetFont(fTiny);
        TextW(dc, c, sz, w / 2, h / 2, wxT("DEC"));
        break; }

    // ================================================ 运算
    case CT_Adder: case CT_Subtractor: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        LineW(dc, c, sz, w / 2, h, w / 2, h - 12);
        dc.SetFont(fBold);
        TextW(dc, c, sz, w / 2, h / 2, c.type == CT_Adder ? wxT("+") : wxT("-"));
        dc.SetFont(fTiny);
        TextW(dc, c, sz, w / 2, h / 2 + 14,
              c.type == CT_Adder ? wxT("加法器") : wxT("减法器"));
        break; }

    case CT_Comparator: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        dc.SetFont(fBold);
        TextW(dc, c, sz, w / 2, h / 2, wxT("CMP"));
        break; }

    case CT_Shifter: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        dc.SetFont(fBold);
        TextW(dc, c, sz, w / 2, h / 2 - 4, (c.flags & 1) ? wxT(">>") : wxT("<<"));
        dc.SetFont(fTiny);
        TextW(dc, c, sz, w / 2, h / 2 + 12, wxT("移位器"));
        break; }

    case CT_Multiplier: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        dc.SetFont(fBold);
        TextW(dc, c, sz, w / 2, h / 2, wxT("x"));
        break; }

    // ================================================ 时序
    case CT_DFlipFlop: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        // 时钟三角
        Poly tri;
        tri.push_back(wxPoint(0, 34));
        tri.push_back(wxPoint(10, 40));
        tri.push_back(wxPoint(0, 46));
        DrawPolyWorld(dc, c, sz, tri, false);
        dc.SetFont(fTiny);
        TextW(dc, c, sz, w / 2, 12, wxT("D"));
        TextW(dc, c, sz, w / 2, h - 12, wxT("Q"));
        break; }

    case CT_Register: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        Poly tri;
        tri.push_back(wxPoint(0, 44));
        tri.push_back(wxPoint(10, 50));
        tri.push_back(wxPoint(0, 56));
        DrawPolyWorld(dc, c, sz, tri, false);
        dc.SetFont(fTiny);
        TextW(dc, c, sz, w / 2, 20, FmtBits(c.bits));
        TextW(dc, c, sz, w / 2, 44, wxT("REG"));
        TextW(dc, c, sz, w / 2, 70, wxT("clr"));
        break; }

    case CT_Counter: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        Poly tri;
        tri.push_back(wxPoint(0, 14));
        tri.push_back(wxPoint(10, 20));
        tri.push_back(wxPoint(0, 26));
        DrawPolyWorld(dc, c, sz, tri, false);
        dc.SetFont(fTiny);
        TextW(dc, c, sz, w / 2, 20, FmtBits(c.bits));
        TextW(dc, c, sz, w / 2, 45, wxT("CNT"));
        TextW(dc, c, sz, w / 2, 70, wxT("clr"));
        break; }

    case CT_RAM: case CT_ROM: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        dc.SetFont(fTiny);
        if (c.type == CT_RAM) {
            TextW(dc, c, sz, w / 2, 20, wxT("RAM"));
            TextW(dc, c, sz, 16, 50, wxT("D"));
            TextW(dc, c, sz, 14, 80, wxT("we"));
            TextW(dc, c, sz, w / 2, h / 2 + 12,
                  wxString::Format(wxT("%d x %d"), 1 << ClampInt(c.addrBits, 1, 10), ClampInt(c.bits, 1, 32)));
        } else {
            TextW(dc, c, sz, w / 2, 30, wxT("ROM"));
            TextW(dc, c, sz, w / 2, h / 2 + 10,
                  wxString::Format(wxT("%d x %d"), 1 << ClampInt(c.addrBits, 1, 10), ClampInt(c.bits, 1, 32)));
        }
        break; }

    // ================================================ 输入/输出
    case CT_InputPin: {
        PinValue v = outVal(0);
        wxColour col = sim ? StateColour(v) : wxColour(0, 100, 0);
        dc.SetBrush(wxBrush(col));
        Poly sq;
        AddRect(sq, 0, 3, 16, 14);
        DrawPolyWorld(dc, c, sz, sq, true);
        dc.SetBrush(wxBrush(col));
        Poly tri;
        tri.push_back(wxPoint(16, 3));
        tri.push_back(wxPoint(29, 10));
        tri.push_back(wxPoint(16, 17));
        DrawPolyWorld(dc, c, sz, tri, true);
        dc.SetTextForeground(wxColour(255, 255, 255));
        if (c.bits == 1) {
            dc.SetFont(fTiny);
            TextW(dc, c, sz, 8, 10, sim ? (v.Bit(0) ? wxT("1") : wxT("0")) : wxT("1"));
        }
        dc.SetTextForeground(ColOutline());
        if (!c.label.empty()) {
            dc.SetFont(fTiny);
            TextW(dc, c, sz, 8, -8, c.label);
        }
        break; }

    case CT_OutputPin: {
        PinValue v = inVal(0);
        wxColour col = sim ? StateColour(v) : wxColour(0, 100, 0);
        dc.SetBrush(wxBrush(col));
        Poly tri;
        tri.push_back(wxPoint(0, 10));
        tri.push_back(wxPoint(13, 3));
        tri.push_back(wxPoint(13, 17));
        DrawPolyWorld(dc, c, sz, tri, true);
        Poly sq;
        AddRect(sq, 14, 3, 16, 14);
        dc.SetBrush(wxBrush(col));
        DrawPolyWorld(dc, c, sz, sq, true);
        dc.SetTextForeground(wxColour(255, 255, 255));
        if (c.bits == 1) {
            dc.SetFont(fTiny);
            TextW(dc, c, sz, 22, 10, sim ? (v.Bit(0) ? wxT("1") : wxT("0")) : wxT("0"));
        }
        dc.SetTextForeground(ColOutline());
        if (!c.label.empty()) {
            dc.SetFont(fTiny);
            TextW(dc, c, sz, 22, -8, c.label);
        }
        break; }

    case CT_LED: {
        PinValue v = inVal(0);
        bool on = sim && v.state == 1 && v.Bit(0);
        bool err = sim && v.state == 2;
        dc.SetBrush(wxBrush(wxColour(120, 120, 120)));
        Poly tri;
        tri.push_back(wxPoint(0, 4));
        tri.push_back(wxPoint(0, 16));
        tri.push_back(wxPoint(11, 10));
        DrawPolyWorld(dc, c, sz, tri, true);
        wxColour body = err ? ColWireErr()
                            : (on ? wxColour(255, 60, 60)
                                  : (sim ? wxColour(90, 90, 90) : wxColour(140, 140, 140)));
        dc.SetBrush(wxBrush(body));
        dc.SetPen(wxPen(ColOutline(), 2));
        CircleW(dc, c, sz, 20, 10, 8);
        if (on) {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(wxPen(wxColour(255, 150, 150), 2));
            CircleW(dc, c, sz, 20, 10, 11);
        }
        break; }

    case CT_Button: {
        bool on = sim && c.value != 0;
        wxColour face = on ? wxColour(120, 200, 120) : wxColour(228, 228, 228);
        dc.SetBrush(wxBrush(face));
        Poly body;
        AddRect(body, 2, 2, w - 4, h - 4);
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        dc.SetBrush(wxBrush(on ? wxColour(60, 160, 60) : wxColour(180, 180, 180)));
        CircleW(dc, c, sz, w / 2, h / 2, on ? 8 : 10);
        break; }

    case CT_HexDisplay: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        dc.SetBrush(wxBrush(wxColour(30, 30, 30)));
        DrawPolyWorld(dc, c, sz, body, true);
        int dx0 = 8, dy0 = 8, dx1 = w - 8, dy1 = h - 8;
        int mid = (dy0 + dy1) / 2;
        PinValue v = inVal(0);
        int digit = (sim && v.state == 1) ? (int)(v.bits & 0xF) : -1;
        static const unsigned char SEG[16] = {
            0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
            0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71
        };
        unsigned char m = (digit >= 0) ? SEG[digit] : 0;
        struct SegDef { int x1, y1, x2, y2; };
        SegDef segs[7] = {
            { dx0 + 5, dy0 + 4,  dx1 - 5, dy0 + 4  },   // a
            { dx1 - 3, dy0 + 7,  dx1 - 3, mid - 4  },   // b
            { dx1 - 3, mid + 4,  dx1 - 3, dy1 - 7  },   // c
            { dx0 + 5, dy1 - 4,  dx1 - 5, dy1 - 4  },   // d
            { dx0 + 3, mid + 4,  dx0 + 3, dy1 - 7  },   // e
            { dx0 + 3, dy0 + 7,  dx0 + 3, mid - 4  },   // f
            { dx0 + 5, mid,      dx1 - 5, mid      }    // g
        };
        dc.SetPen(wxPen(wxColour(70, 70, 70), 5, wxPENSTYLE_SOLID));
        for (int i = 0; i < 7; ++i)
            LineW(dc, c, sz, segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2);
        dc.SetPen(wxPen(wxColour(255, 70, 40), 5, wxPENSTYLE_SOLID));
        for (int i = 0; i < 7; ++i)
            if (m & (1 << i))
                LineW(dc, c, sz, segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2);
        if (digit < 0 && sim) {
            dc.SetPen(wxPen(ColWireErr(), 2));
            Poly z;
            AddRect(z, 4, 4, w - 8, h - 8);
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            DrawPolyWorld(dc, c, sz, z, false);
        }
        dc.SetPen(wxPen(ColOutline(), 2));
        break; }

    // ================================================ 布线
    case CT_Clock: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        dc.SetBrush(wxBrush(ColBody()));
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        Poly wave;
        wave.push_back(wxPoint(6, 28));
        wave.push_back(wxPoint(6, 14));
        wave.push_back(wxPoint(20, 14));
        wave.push_back(wxPoint(20, 28));
        wave.push_back(wxPoint(34, 28));
        wave.push_back(wxPoint(34, 14));
        PinValue v = outVal(0);
        dc.SetPen(wxPen(sim ? StateColour(v) : ColOutline(), 2));
        DrawPolyWorld(dc, c, sz, wave, false);
        dc.SetPen(wxPen(ColOutline(), 2));
        break; }

    case CT_Constant: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        dc.SetBrush(wxBrush(ColBody()));
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        LineW(dc, c, sz, 0, 4, w, 4);
        dc.SetFont(fTiny);
        TextW(dc, c, sz, w / 2, h / 2 + 2, FormatValue(c.value, c.bits));
        break; }

    case CT_Tunnel: {
        dc.SetBrush(wxBrush(ColBody()));
        Poly body;
        body.push_back(wxPoint(0, 10));
        body.push_back(wxPoint(10, 0));
        body.push_back(wxPoint(w, 0));
        body.push_back(wxPoint(w, 20));
        body.push_back(wxPoint(10, 20));
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        dc.SetFont(fTiny);
        TextW(dc, c, sz, w / 2 + 4, 10, c.label.empty() ? wxT("?") : c.label);
        break; }

    case CT_Probe: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        PinValue v = inVal(0);
        wxColour col = sim ? StateColour(v) : wxColour(200, 200, 200);
        dc.SetBrush(wxBrush(col));
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        dc.SetFont(fTiny);
        dc.SetTextForeground(sim && v.state == 1 ? wxColour(255, 255, 255) : wxColour(20, 20, 20));
        wxString txt = sim ? FormatValue(v.bits, c.bits)
                           : wxString::Format(wxT("%d 位"), ClampInt(c.bits, 1, 32));
        TextW(dc, c, sz, w / 2, h / 2, txt);
        dc.SetTextForeground(ColOutline());
        break; }

    case CT_Splitter: {
        int f = ClampInt(c.fanout, 2, 8);
        Poly body;
        body.push_back(wxPoint(10, 0));
        body.push_back(wxPoint(30, 0));
        body.push_back(wxPoint(30, h));
        body.push_back(wxPoint(10, h));
        dc.SetBrush(wxBrush(ColBody()));
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        LineW(dc, c, sz, 0, h / 2, 10, h / 2);
        LineW(dc, c, sz, 30, h / 2, 30, h / 2);
        LineW(dc, c, sz, 18, h / 2, 30, h / 2);
        for (int i = 0; i < f; ++i) {
            int y = (i * 2 + 1) * h / (2 * f);
            LineW(dc, c, sz, 18, h / 2, 30, y);
        }
        break; }

    case CT_Text: {
        dc.SetFont(wxFont(wxFontInfo(10).FaceName(wxT("Microsoft YaHei"))));
        TextW(dc, c, sz, 6, h / 2, c.label, false);
        break; }

    // ================================================ 子电路
    case CT_SubCircuit: {
        Poly body;
        AddRect(body, 0, 0, w, h);
        dc.SetBrush(wxBrush(wxColour(250, 250, 240)));
        DrawPolyWorld(dc, c, sz, body, true);
        DrawPolyWorld(dc, c, sz, body, false);
        Poly inner;
        AddRect(inner, 3, 3, w - 6, h - 6);
        dc.SetPen(wxPen(wxColour(120, 120, 120), 1));
        DrawPolyWorld(dc, c, sz, inner, false);
        dc.SetPen(wxPen(ColOutline(), 2));
        dc.SetFont(fTiny);
        wxString nm = c.label;
        if (nm.empty() && doc && c.sub >= 0 && c.sub < (int)doc->circuits.size())
            nm = doc->circuits[c.sub].name;
        TextW(dc, c, sz, w / 2, h / 2, nm.empty() ? wxT("子电路") : nm);
        break; }

    default:
        break;
    }

    // ---- 引脚小圆点(显示逻辑值)
    if (!iconMode && sim && c.type != CT_Custom) {
        for (size_t k = 0; k < pins.size(); ++k) {
            wxPoint p = LocalToWorld(c, sz, pins[k].pos);
            PinValue v = sim->PinOf(c, (int)k);
            wxColour col = StateColour(v);
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(col));
            dc.DrawCircle(p, 3);
        }
        dc.SetPen(wxPen(ColOutline(), 2));
    }

    // ---- 元件标签(画在元件上方)
    if (!iconMode && !c.label.empty() &&
        c.type != CT_InputPin && c.type != CT_OutputPin &&
        c.type != CT_Tunnel && c.type != CT_Text && c.type != CT_SubCircuit &&
        c.type != CT_Probe && c.type != CT_Constant) {
        wxRect b = WorldBounds(c, sz);
        dc.SetFont(fSmall);
        dc.SetTextForeground(wxColour(70, 70, 70));
        wxSize e = dc.GetTextExtent(c.label);
        wxPoint anchor = LocalToWorld(c, sz, wxPoint(w / 2, -6));
        dc.DrawText(c.label, anchor.x - e.x / 2, anchor.y - e.y);
        dc.SetTextForeground(ColOutline());
    }

    (void)smallPt;
}

// ---------------------------------------------------------------- 导线
void Renderer::DrawWire(wxDC& dc, const Wire& w, const Simulator* sim, bool selected,
                        int zoomPercent)
{
    if (w.pts.size() < 2) return;

    int penW = (w.bits > 1) ? 4 : 2;
    wxColour col = ColWireOff();
    if (sim) {
        PinValue v = sim->ValueAtPoint(w.pts[0]);
        col = StateColour(v);
    }

    if (selected) {
        dc.SetPen(wxPen(ColSelect(), penW + 6));
        dc.DrawLines((int)w.pts.size(), &w.pts[0]);
    }
    dc.SetPen(wxPen(col, penW));
    dc.DrawLines((int)w.pts.size(), &w.pts[0]);

    // 多位导线画一条斜杠并标注位宽
    if (w.bits > 1 && zoomPercent >= 75) {
        size_t best = 0;
        int bestLen = -1;
        for (size_t i = 0; i + 1 < w.pts.size(); ++i) {
            int len = std::abs(w.pts[i + 1].x - w.pts[i].x) + std::abs(w.pts[i + 1].y - w.pts[i].y);
            if (len > bestLen) { bestLen = len; best = i; }
        }
        if (bestLen >= 20) {
            wxPoint a = w.pts[best], b = w.pts[best + 1];
            wxPoint m((a.x + b.x) / 2, (a.y + b.y) / 2);
            dc.SetPen(wxPen(col, 2));
            dc.DrawLine(m.x - 5, m.y + 5, m.x + 5, m.y - 5);
            wxFont f(wxFontInfo(7).FaceName(wxT("Microsoft YaHei")));
            dc.SetFont(f);
            dc.SetTextForeground(col);
            dc.DrawText(wxString::Format(wxT("%d"), w.bits), m.x + 7, m.y - 16);
            dc.SetTextForeground(ColOutline());
        }
    }
}

// ---------------------------------------------------------------- 图标
wxBitmap Renderer::MakeToolBitmap(Document* doc, const Component& proto, const wxSize& sz,
                                  const wxColour& bg)
{
    wxBitmap bmp(sz.x, sz.y, 24);
    {
        wxMemoryDC dc(bmp);
        dc.SetBackground(wxBrush(bg));
        dc.Clear();

        Component c = proto;
        if (!doc) {
            // 没有文档时用默认尺寸画(不会走到这里，仅防御)
            dc.SetPen(wxPen(ColOutline(), 1));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(2, 2, sz.x - 4, sz.y - 4);
            return bmp;
        }
        wxSize cs = doc->Size(c);
        if (cs.x < 1) cs.x = 1;
        if (cs.y < 1) cs.y = 1;
        double s = std::min((sz.x - 6.0) / cs.x, (sz.y - 6.0) / cs.y);
        if (s > 1.0) s = 1.0;
        if (s < 0.2) s = 0.2;

        dc.SetUserScale(s, s);
        dc.SetDeviceOrigin((int)((sz.x - cs.x * s) / 2), (int)((sz.y - cs.y * s) / 2));
        DrawComponent(dc, doc, c, nullptr, false, true);
    }
    return bmp;
}

wxBitmap Renderer::RenderCircuit(Document* doc, const Simulator* sim, int circuitIdx, int margin)
{
    if (!doc || circuitIdx < 0 || circuitIdx >= (int)doc->circuits.size())
        return wxBitmap(16, 16, 24);
    const Circuit& c = doc->circuits[circuitIdx];

    bool first = true;
    int minX = 0, minY = 0, maxX = 400, maxY = 300;
    for (size_t i = 0; i < c.comps.size(); ++i) {
        wxSize sz = doc->Size(c.comps[i]);
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

    int w = maxX - minX + margin * 2;
    int h = maxY - minY + margin * 2;
    if (w < 16) w = 16;
    if (h < 16) h = 16;
    if (w > 16000) w = 16000;
    if (h > 16000) h = 16000;

    wxBitmap bmp(w, h, 24);
    wxMemoryDC dc(bmp);
    dc.SetBackground(wxBrush(ColCanvas()));
    dc.Clear();
    dc.SetDeviceOrigin(-minX + margin, -minY + margin);
    for (size_t i = 0; i < c.wires.size(); ++i)
        DrawWire(dc, c.wires[i], sim, false, 100);
    for (size_t i = 0; i < c.comps.size(); ++i)
        DrawComponent(dc, doc, c.comps[i], sim, false, false);
    dc.SelectObject(wxNullBitmap);
    return bmp;
}

wxBitmap Renderer::MakeLogoBitmap(int size)
{
    wxBitmap bmp(size, size, 24);
    {
        wxMemoryDC dc(bmp);
        dc.SetBackground(wxBrush(wxColour(255, 255, 255)));
        dc.Clear();
        double s = size / 32.0;
        dc.SetUserScale(s, s);

        dc.SetPen(wxPen(wxColour(20, 20, 20), 2));
        dc.SetBrush(wxBrush(wxColour(0, 170, 0)));
        wxPoint tri[3] = { wxPoint(4, 4), wxPoint(4, 28), wxPoint(22, 16) };
        dc.DrawPolygon(3, tri);
        dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
        dc.DrawCircle(26, 16, 4);
        dc.SetBrush(wxBrush(wxColour(0, 120, 200)));
        dc.DrawCircle(26, 16, 2);
    }
    return bmp;
}

} // namespace lc
