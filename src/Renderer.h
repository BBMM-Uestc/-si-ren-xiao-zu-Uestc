// ============================================================================
//  Renderer.h  --  元件矢量绘图(同时用于画布与工具图标)
// ============================================================================
#pragma once

#include "LogicModel.h"

#include <wx/wx.h>
#include <wx/dc.h>
#include <wx/bitmap.h>

namespace lc {

class Simulator;

class Renderer
{
public:
    // ---- 颜色 ----
    static wxColour ColWireOn();
    static wxColour ColWireOff();
    static wxColour ColWireFloat();
    static wxColour ColWireErr();
    static wxColour ColOutline();
    static wxColour ColBody();
    static wxColour ColSelect();
    static wxColour ColGrid();
    static wxColour ColGridBold();
    static wxColour ColCanvas();

    static wxColour StateColour(const PinValue& v);
    static wxColour StateColour(int state, bool bit1);

    /// 画一个元件。sim 可为 nullptr(不显示逻辑值)
    static void DrawComponent(wxDC& dc, Document* doc, const Component& c,
                              const Simulator* sim, bool selected, bool iconMode = false);

    /// 画一条导线
    static void DrawWire(wxDC& dc, const Wire& w, const Simulator* sim, bool selected,
                         int zoomPercent);

    /// 生成工具条图标
    static wxBitmap MakeToolBitmap(Document* doc, const Component& proto, const wxSize& sz,
                                   const wxColour& bg);
    static wxBitmap MakeLogoBitmap(int size);

    /// 把一张电路图渲染成位图(可带仿真值)
    static wxBitmap RenderCircuit(Document* doc, const Simulator* sim, int circuitIdx,
                                  int margin = 30);
};

} // namespace lc
