// ============================================================================
//  LogicModel.cpp  --  元件元数据 / 几何 / 属性 / 序列化
// ============================================================================
#include "LogicModel.h"

#include <wx/tokenzr.h>
#include <wx/textfile.h>
#include <wx/filename.h>
#include <wx/wfstream.h>

#include <algorithm>
#include <cmath>

namespace lc {

// ---------------------------------------------------------------- 小工具
int ClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

uint32_t MaskOf(int bits)
{
    if (bits <= 0)   return 0u;
    if (bits >= 32)  return 0xFFFFFFFFu;
    return (uint32_t)((1ull << bits) - 1ull);
}

wxString FormatValue(uint32_t v, int bits, bool hex)
{
    bits = ClampInt(bits, 1, 32);
    if (hex) {
        int digits = (bits + 3) / 4;
        wxString s = wxString::Format(wxT("%0*X"), digits, (unsigned)(v & MaskOf(bits)));
        return s;
    }
    return wxString::Format(wxT("%u"), (unsigned)(v & MaskOf(bits)));
}

// ---------------------------------------------------------------- 名称
struct TypeMeta { const wchar_t* name; const wchar_t* desc; int lib; };

static const TypeMeta g_meta[CT_COUNT] = {
    /* CT_InputPin   */ { wxT("输入引脚"),   wxT("电路输入端口，可用查探工具点击翻转电平"),       LIB_IO },
    /* CT_OutputPin  */ { wxT("输出引脚"),   wxT("电路输出端口，显示所在网络的值"),               LIB_IO },
    /* CT_Button     */ { wxT("按钮"),       wxT("按下时输出 1，松开输出 0"),                     LIB_IO },
    /* CT_LED        */ { wxT("LED"),        wxT("输入为 1 时点亮"),                             LIB_IO },
    /* CT_HexDisplay */ { wxT("数码管"),     wxT("4 位输入，十六进制七段显示"),                  LIB_IO },
    /* CT_NOT        */ { wxT("非门"),       wxT("输出 = 输入取反"),                             LIB_GATES },
    /* CT_AND        */ { wxT("与门"),       wxT("全部输入为 1 时输出 1"),                       LIB_GATES },
    /* CT_OR         */ { wxT("或门"),       wxT("任一输入为 1 时输出 1"),                       LIB_GATES },
    /* CT_NAND       */ { wxT("与非门"),     wxT("与门输出取反"),                                LIB_GATES },
    /* CT_NOR        */ { wxT("或非门"),     wxT("或门输出取反"),                                LIB_GATES },
    /* CT_XOR        */ { wxT("异或门"),     wxT("输入中 1 的个数为奇数时输出 1"),               LIB_GATES },
    /* CT_XNOR       */ { wxT("同或门"),     wxT("输入中 1 的个数为偶数时输出 1"),               LIB_GATES },
    /* CT_Buffer     */ { wxT("缓冲器"),     wxT("输出 = 输入"),                                 LIB_GATES },
    /* CT_Mux        */ { wxT("多路选择器"), wxT("按选择端从多路输入中选一路输出"),              LIB_PLEXERS },
    /* CT_Demux      */ { wxT("多路分配器"), wxT("把一路输入送到选择端指定的输出"),              LIB_PLEXERS },
    /* CT_Decoder    */ { wxT("译码器"),     wxT("选择端对应的输出为 1，其余为 0"),              LIB_PLEXERS },
    /* CT_Adder      */ { wxT("加法器"),     wxT("S = A + B + Cin，Cout 为进位"),                LIB_ARITH },
    /* CT_Subtractor */ { wxT("减法器"),     wxT("D = A - B，Bout 为借位"),                      LIB_ARITH },
    /* CT_Comparator */ { wxT("比较器"),     wxT("输出 A>B、A=B、A<B 三个结果"),                 LIB_ARITH },
    /* CT_Shifter    */ { wxT("移位器"),     wxT("把数据按给定位数左移或右移"),                  LIB_ARITH },
    /* CT_Multiplier */ { wxT("乘法器"),     wxT("输出 = A × B"),                                LIB_ARITH },
    /* CT_DFlipFlop  */ { wxT("D 触发器"),   wxT("时钟沿到来时锁存 D 到 Q"),                     LIB_MEMORY },
    /* CT_Register   */ { wxT("寄存器"),     wxT("时钟沿锁存，clr 为 1 时清零"),                 LIB_MEMORY },
    /* CT_Counter    */ { wxT("计数器"),     wxT("时钟沿加/减计数，clr 为 1 时清零"),            LIB_MEMORY },
    /* CT_RAM        */ { wxT("随机存储器"), wxT("读写存储器，we 为 1 时在时钟沿写入"),          LIB_MEMORY },
    /* CT_ROM        */ { wxT("只读存储器"), wxT("按地址读出预置数据"),                          LIB_MEMORY },
    /* CT_Clock      */ { wxT("时钟"),       wxT("每个仿真节拍翻转一次电平"),                    LIB_WIRING },
    /* CT_Constant   */ { wxT("常量"),       wxT("输出固定的常量值"),                            LIB_WIRING },
    /* CT_Tunnel     */ { wxT("隧道"),       wxT("同名隧道在电路内互相连通"),                    LIB_WIRING },
    /* CT_Probe      */ { wxT("探针"),       wxT("显示所在网络的值"),                            LIB_WIRING },
    /* CT_Splitter   */ { wxT("分线器"),     wxT("把总线拆成若干位段"),                          LIB_WIRING },
    /* CT_Text       */ { wxT("文本"),       wxT("电路中的文字注释，不参与仿真"),                LIB_WIRING },
    /* CT_SubCircuit */ { wxT("子电路"),     wxT("把另一张电路图当作元件使用"),                  LIB_PROJECT },
    /* CT_TriState   */ { wxT("三态缓冲器"), wxT("使能为 1 时输出=输入；使能为 0 时输出悬空(高阻 Z)"), LIB_GATES },
    /* CT_Custom     */ { wxT("自定义元件"), wxT("运行时定义的元件，可在“项目→元件库管理器”里新建/编辑"), LIB_CUSTOM },
};

// ---------------------------------------------------------------- 自定义元件
wxString CustomLogicName(int logic)
{
    switch (logic) {
    case CL_PASS:    return wxT("直通(输出=输入)");
    case CL_NOT:     return wxT("取反");
    case CL_AND:     return wxT("与");
    case CL_OR:      return wxT("或");
    case CL_XOR:     return wxT("异或");
    case CL_NAND:    return wxT("与非");
    case CL_NOR:     return wxT("或非");
    case CL_XNOR:    return wxT("同或");
    case CL_MUX:     return wxT("多路选择(最后一脚为选择端)");
    case CL_DECODER: return wxT("译码(输入0为选择端)");
    case CL_CONST:   return wxT("常量输出");
    case CL_NONE:    return wxT("纯图形(不参与逻辑)");
    }
    return wxT("直通(输出=输入)");
}

int CustomLogicCount() { return (int)CL_NONE + 1; }

wxString TypeName(int type)
{
    if (type < 0 || type >= CT_COUNT) return wxT("未知");
    return g_meta[type].name;
}

wxString TypeDesc(int type)
{
    if (type < 0 || type >= CT_COUNT) return wxEmptyString;
    return g_meta[type].desc;
}

int TypeLib(int type)
{
    if (type < 0 || type >= CT_COUNT) return LIB_WIRING;
    return g_meta[type].lib;
}

wxString LibName(int lib)
{
    switch (lib) {
    case LIB_IO:      return wxT("输入/输出");
    case LIB_GATES:   return wxT("门");
    case LIB_PLEXERS: return wxT("复用器");
    case LIB_ARITH:   return wxT("运算");
    case LIB_MEMORY:  return wxT("存储器");
    case LIB_WIRING:  return wxT("布线");
    case LIB_PROJECT: return wxT("项目");
    case LIB_CUSTOM:  return wxT("自定义");
    }
    return wxEmptyString;
}

std::vector<int> LibTools(int lib)
{
    std::vector<int> r;
    for (int t = 0; t < CT_COUNT; ++t) {
        // CT_SubCircuit 由「项目」库动态列出；CT_Custom 的动态库尚未接线，
        // 先不暴露，避免元件库里出现一个放不出来的按钮
        if (t == CT_SubCircuit || t == CT_Custom) continue;
        if (g_meta[t].lib == lib) r.push_back(t);
    }
    return r;
}

// ---------------------------------------------------------------- 几何
wxPoint LocalToWorld(const Component& c, const wxSize& sz, const wxPoint& p)
{
    const int w = sz.x, h = sz.y;
    const wxPoint ctr(c.pos.x + w / 2, c.pos.y + h / 2);
    switch (c.facing) {
    default:
    case F_East:  return wxPoint(c.pos.x + p.x,          c.pos.y + p.y);
    case F_West:  return wxPoint(ctr.x - (p.x - w / 2),  ctr.y + (p.y - h / 2));
    case F_North: return wxPoint(ctr.x + (p.y - h / 2),  ctr.y - (p.x - w / 2));
    case F_South: return wxPoint(ctr.x - (p.y - h / 2),  ctr.y + (p.x - w / 2));
    }
}

wxPoint WorldToLocal(const Component& c, const wxSize& sz, const wxPoint& p)
{
    const int w = sz.x, h = sz.y;
    const int dx = p.x - c.pos.x;
    const int dy = p.y - c.pos.y;
    switch (c.facing) {
    default:
    case F_East:  return wxPoint(dx, dy);
    case F_West:  return wxPoint(w - dx, h - dy);
    case F_North: return wxPoint(h / 2 + w / 2 - dy, dx - w / 2 + h / 2);
    case F_South: return wxPoint(dy - h / 2 + w / 2, w / 2 + h / 2 - dx);
    }
}

wxRect WorldBounds(const Component& c, const wxSize& sz)
{
    if (c.facing == F_North || c.facing == F_South) {
        wxPoint ctr(c.pos.x + sz.x / 2, c.pos.y + sz.y / 2);
        return wxRect(ctr.x - sz.y / 2, ctr.y - sz.x / 2, sz.y, sz.x);
    }
    return wxRect(c.pos.x, c.pos.y, sz.x, sz.y);
}

bool PointOnSegment(const wxPoint& p, const wxPoint& a, const wxPoint& b)
{
    // 仅支持正交/共线情形，足够描述本编辑器的导线
    if (a.x == b.x) {
        return p.x == a.x && p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y);
    }
    if (a.y == b.y) {
        return p.y == a.y && p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x);
    }
    // 斜线：判断叉积为 0 且落在外接矩形内
    long long cross = (long long)(b.x - a.x) * (p.y - a.y) - (long long)(b.y - a.y) * (p.x - a.x);
    if (cross != 0) return false;
    return p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x) &&
           p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y);
}

// ---------------------------------------------------------------- 电路
Component* Circuit::Find(int id)
{
    for (auto& c : comps)
        if (c.id == id) return &c;
    return nullptr;
}

void Circuit::RemoveSelected()
{
    for (int i = (int)comps.size() - 1; i >= 0; --i)
        if (comps[i].selected) comps.erase(comps.begin() + i);
    for (int i = (int)wires.size() - 1; i >= 0; --i)
        if (wires[i].selected) wires.erase(wires.begin() + i);
}

bool Circuit::HasSelection() const
{
    for (const auto& c : comps) if (c.selected) return true;
    for (const auto& w : wires) if (w.selected) return true;
    return false;
}

void Circuit::ClearSelection()
{
    for (auto& c : comps) c.selected = false;
    for (auto& w : wires) w.selected = false;
}

void Circuit::CollectBounds(wxRect& r) const
{
    bool first = true;
    for (const auto& w : wires)
        for (const auto& p : w.pts) {
            if (first) { r = wxRect(p, wxSize(0, 0)); first = false; }
            else {
                r.x = std::min(r.x, p.x); r.y = std::min(r.y, p.y);
                r.width  = std::max(r.GetRight(),  p.x) - r.x;
                r.height = std::max(r.GetBottom(), p.y) - r.y;
            }
        }
    for (const auto& c : comps) {
        wxRect b(c.pos.x, c.pos.y, 40, 40);
        if (first) { r = b; first = false; }
        else {
            r.x = std::min(r.x, b.x); r.y = std::min(r.y, b.y);
            r.width  = std::max(r.GetRight(),  b.GetRight())  - r.x;
            r.height = std::max(r.GetBottom(), b.GetBottom()) - r.y;
        }
    }
    if (first) r = wxRect(0, 0, 0, 0);
}

// ---------------------------------------------------------------- 文档
Document::Document()
{
    NewDocument();
}

void Document::NewDocument()
{
    circuits.clear();
    customDefs.clear();
    Circuit c;
    c.name = wxT("main");
    circuits.push_back(c);
    active  = 0;
    dirty   = false;
    version = 0;
    path    = wxEmptyString;
}

int Document::FindCircuit(const wxString& n) const
{
    for (size_t i = 0; i < circuits.size(); ++i)
        if (circuits[i].name == n) return (int)i;
    return -1;
}

int Document::AddCircuit(const wxString& n)
{
    wxString base = n;
    if (base.empty()) base = wxT("子电路");
    wxString name = base;
    int k = 1;
    while (FindCircuit(name) >= 0)
        name = wxString::Format(wxT("%s%d"), base, ++k);
    Circuit c;
    c.name = name;
    circuits.push_back(c);
    Touch();
    return (int)circuits.size() - 1;
}

bool Document::RemoveCircuit(int idx)
{
    if (idx <= 0 || idx >= (int)circuits.size()) return false;  // main 不可删
    circuits.erase(circuits.begin() + idx);
    // 修正其它元件的子电路下标
    for (auto& c : circuits)
        for (auto& comp : c.comps) {
            if (comp.type != CT_SubCircuit) continue;
            if (comp.sub == idx) comp.sub = -1;
            else if (comp.sub > idx) --comp.sub;
        }
    if (active >= (int)circuits.size()) active = (int)circuits.size() - 1;
    Touch();
    return true;
}

// ---------------------------------------------------------------- 自定义元件库
wxString Document::UniqueCustomName(const wxString& base) const
{
    wxString b = base.empty() ? wxString(wxT("自定义元件")) : base;
    wxString name = b;
    int k = 1;
    bool clash = true;
    while (clash) {
        clash = false;
        for (size_t i = 0; i < customDefs.size(); ++i)
            if (customDefs[i].name == name) { clash = true; break; }
        if (clash) name = wxString::Format(wxT("%s%d"), b, ++k);
    }
    return name;
}

int Document::AddCustomDef(const CustomDef& d)
{
    CustomDef nd = d;
    nd.name = UniqueCustomName(d.name);
    customDefs.push_back(nd);
    Touch();
    return (int)customDefs.size() - 1;
}

bool Document::RemoveCustomDef(int idx)
{
    if (idx < 0 || idx >= (int)customDefs.size()) return false;
    customDefs.erase(customDefs.begin() + idx);
    // 修正其它元件的引用
    for (size_t ci = 0; ci < circuits.size(); ++ci)
        for (size_t k = 0; k < circuits[ci].comps.size(); ++k) {
            Component& comp = circuits[ci].comps[k];
            if (comp.type != CT_Custom) continue;
            if (comp.cdef == idx) comp.cdef = -1;
            else if (comp.cdef > idx) --comp.cdef;
        }
    Touch();
    return true;
}

std::vector<int> Document::Ports(int circuitIdx) const
{
    std::vector<int> res;
    if (circuitIdx < 0 || circuitIdx >= (int)circuits.size()) return res;
    const Circuit& c = circuits[circuitIdx];
    std::vector<int> ins, outs;
    for (size_t i = 0; i < c.comps.size(); ++i) {
        if (c.comps[i].type == CT_InputPin)       ins.push_back((int)i);
        else if (c.comps[i].type == CT_OutputPin) outs.push_back((int)i);
    }
    auto cmp = [&](int a, int b) {
        const wxPoint& pa = c.comps[a].pos;
        const wxPoint& pb = c.comps[b].pos;
        if (pa.y != pb.y) return pa.y < pb.y;
        return pa.x < pb.x;
    };
    std::sort(ins.begin(), ins.end(), cmp);
    std::sort(outs.begin(), outs.end(), cmp);
    res = ins;
    res.insert(res.end(), outs.begin(), outs.end());
    return res;
}

int Document::NetCount(int circuitIdx) const
{
    return (int)Ports(circuitIdx).size();
}

wxSize Document::Size(const Component& c) const
{
    switch (c.type) {
    case CT_NOT: case CT_Buffer:
        return wxSize(50, 40);
    case CT_TriState:
        return wxSize(60, 60);
    case CT_Custom: {
        const CustomDef* d = FindCustom(c.cdef);
        if (d) return wxSize(d->w < 20 ? 20 : d->w, d->h < 20 ? 20 : d->h);
        return wxSize(60, 60);
    }
    case CT_AND: case CT_OR: case CT_NAND: case CT_NOR: case CT_XOR: case CT_XNOR: {
        int n = ClampInt(c.inputs, 2, 8);
        int h = std::max(40, 20 * n);
        return wxSize(std::max(60, h / 2 + 40), h);
    }
    case CT_Mux: case CT_Demux: {
        int n = 1 << ClampInt(c.selBits, 1, 3);
        return wxSize(60, std::max(60, 20 * n));
    }
    case CT_Decoder: {
        int n = 1 << ClampInt(c.selBits, 1, 3);
        return wxSize(60, std::max(40, 20 * n));
    }
    case CT_Adder: case CT_Subtractor: case CT_Shifter:
        return wxSize(60, 60);
    case CT_Comparator: case CT_Multiplier:
        return wxSize(60, 40);
    case CT_DFlipFlop:
        return wxSize(60, 60);
    case CT_Register: case CT_Counter:
        return wxSize(80, 80);
    case CT_RAM:
        return wxSize(100, 100);
    case CT_ROM:
        return wxSize(100, 60);
    case CT_HexDisplay:
        return wxSize(60, 80);
    case CT_LED:
        return wxSize(30, 20);
    case CT_Button:
        return wxSize(40, 40);
    case CT_InputPin: case CT_OutputPin:
        return wxSize(30, 20);
    case CT_Clock:
        return wxSize(40, 40);
    case CT_Constant:
        return wxSize(40, 20);
    case CT_Tunnel:
        return wxSize(40, 20);
    case CT_Probe:
        return wxSize(40, 20);
    case CT_Splitter: {
        int f = ClampInt(c.fanout, 2, 8);
        return wxSize(30, std::max(40, 20 * f));
    }
    case CT_Text: {
        int len = (int)c.label.length();
        if (len < 4) len = 4;
        return wxSize(len * 8 + 12, 20);
    }
    case CT_SubCircuit: {
        std::vector<int> p = Ports(c.sub);
        int nIn = 0, nOut = 0;
        if (c.sub >= 0 && c.sub < (int)circuits.size()) {
            for (int idx : p) {
                if (circuits[c.sub].comps[idx].type == CT_InputPin) ++nIn; else ++nOut;
            }
        }
        return wxSize(80, std::max(60, 20 * std::max(nIn, nOut)));
    }
    }
    return wxSize(40, 40);
}

std::vector<PinInfo> Document::Pins(const Component& c) const
{
    std::vector<PinInfo> v;
    auto in  = [&](int x, int y, int bits, const wxString& n) {
        PinInfo p; p.pos = wxPoint(x, y); p.isInput = true;  p.bits = bits; p.name = n; v.push_back(p);
    };
    auto out = [&](int x, int y, int bits, const wxString& n) {
        PinInfo p; p.pos = wxPoint(x, y); p.isInput = false; p.bits = bits; p.name = n; v.push_back(p);
    };
    const int nb = ClampInt(c.bits, 1, MAX_BITS);

    switch (c.type) {
    case CT_NOT: case CT_Buffer:
        in(0, 20, nb, wxT("I"));  out(50, 20, nb, wxT("O"));
        break;

    case CT_TriState:
        // 局部坐标(输出朝东): 数据输入在左边中点，使能端在底边中点，输出在三角尖端
        in(0, 30, nb, wxT("D"));
        in(30, 60, 1, wxT("EN"));
        out(60, 30, nb, wxT("O"));
        break;

    case CT_Custom: {
        const CustomDef* d = FindCustom(c.cdef);
        if (d) {
            for (size_t i = 0; i < d->pins.size(); ++i) {
                const CustomPinDef& p = d->pins[i];
                PinInfo pi;
                pi.pos     = p.pos;
                pi.isInput = p.isInput;
                pi.bits    = (p.bits > 0) ? p.bits : nb;
                pi.name    = p.name;
                v.push_back(pi);
            }
        }
        break; }

    case CT_AND: case CT_OR: case CT_NAND: case CT_NOR: case CT_XOR: case CT_XNOR: {
        int n = ClampInt(c.inputs, 2, 8);
        wxSize sz = Size(c);
        for (int i = 0; i < n; ++i)
            in(0, (i * 2 + 1) * sz.y / (2 * n), nb, wxString::Format(wxT("I%d"), i + 1));
        out(sz.x, sz.y / 2, nb, wxT("O"));
        break; }

    case CT_Mux: {
        int n = 1 << ClampInt(c.selBits, 1, 3);
        wxSize sz = Size(c);
        for (int i = 0; i < n; ++i)
            in(0, (i * 2 + 1) * sz.y / (2 * n), nb, wxString::Format(wxT("D%d"), i));
        in(sz.x / 2, sz.y, ClampInt(c.selBits, 1, 3), wxT("Sel"));
        out(60, (sz.y / 3 / GRID) * GRID, nb, wxT("O"));
        break; }

    case CT_Demux: {
        int n = 1 << ClampInt(c.selBits, 1, 3);
        wxSize sz = Size(c);
        in(0, (sz.y / 3 / GRID) * GRID, nb, wxT("I"));
        in(sz.x / 2, sz.y, ClampInt(c.selBits, 1, 3), wxT("Sel"));
        for (int i = 0; i < n; ++i)
            out(60, (i * 2 + 1) * sz.y / (2 * n), nb, wxString::Format(wxT("D%d"), i));
        break; }

    case CT_Decoder: {
        int n = 1 << ClampInt(c.selBits, 1, 3);
        wxSize sz = Size(c);
        in(0, sz.y / 2, ClampInt(c.selBits, 1, 3), wxT("Sel"));
        for (int i = 0; i < n; ++i)
            out(60, (i * 2 + 1) * sz.y / (2 * n), 1, wxString::Format(wxT("O%d"), i));
        break; }

    case CT_Adder: case CT_Subtractor:
        in(0, 20, nb, wxT("A"));
        in(0, 40, nb, wxT("B"));
        if (c.type == CT_Adder) { in(30, 60, 1, wxT("Cin")); out(60, 20, nb, wxT("S")); out(60, 40, 1, wxT("Cout")); }
        else                    { in(30, 60, 1, wxT("Bin")); out(60, 20, nb, wxT("D")); out(60, 40, 1, wxT("Bout")); }
        break;

    case CT_Comparator:
        in(0, 10, nb, wxT("A")); in(0, 30, nb, wxT("B"));
        out(60, 10, 1, wxT("A>B")); out(60, 20, 1, wxT("A=B")); out(60, 30, 1, wxT("A<B"));
        break;

    case CT_Shifter:
        in(0, 20, nb, wxT("D"));
        in(0, 40, 5, wxT("Sh"));
        out(60, 20, nb, wxT("O"));
        break;

    case CT_Multiplier:
        in(0, 10, nb, wxT("A")); in(0, 30, nb, wxT("B"));
        out(60, 20, std::min(32, nb * 2), wxT("O"));
        break;

    case CT_DFlipFlop:
        in(0, 20, nb, wxT("D"));
        in(0, 40, 1,  wxT(">"));
        out(60, 20, nb, wxT("Q"));
        out(60, 40, nb, wxT("Q'"));
        break;

    case CT_Register:
        in(0, 20, nb, wxT("D"));
        in(0, 50, 1,  wxT(">"));
        in(0, 70, 1,  wxT("clr"));
        out(80, 20, nb, wxT("Q"));
        break;

    case CT_Counter:
        in(0, 20, 1,  wxT(">"));
        in(0, 50, 1,  wxT("clr"));
        out(80, 20, nb, wxT("Q"));
        break;

    case CT_RAM:
        in(0, 20, ClampInt(c.addrBits, 1, 10), wxT("A"));
        in(0, 50, nb, wxT("D"));
        in(0, 80, 1,  wxT("we"));
        out(100, 20, nb, wxT("Q"));
        break;

    case CT_ROM:
        in(0, 30, ClampInt(c.addrBits, 1, 10), wxT("A"));
        out(100, 30, nb, wxT("Q"));
        break;

    case CT_HexDisplay:
        in(0, 40, 4, wxT("D"));
        break;

    case CT_LED:
        in(0, 10, 1, wxT("I"));
        break;

    case CT_Button:
        out(40, 20, 1, wxT("O"));
        break;

    case CT_InputPin:
        out(30, 10, nb, wxT("O"));
        break;

    case CT_OutputPin:
        in(0, 10, nb, wxT("I"));
        break;

    case CT_Clock:
        out(40, 20, 1, wxT("O"));
        break;

    case CT_Constant:
        out(40, 10, nb, wxT("O"));
        break;

    case CT_Tunnel:
        in(0, 10, nb, c.label.empty() ? wxT("隧道") : c.label);
        break;

    case CT_Probe:
        in(0, 10, nb, wxT("I"));
        break;

    case CT_Splitter: {
        int f = ClampInt(c.fanout, 2, 8);
        wxSize sz = Size(c);
        in(0, sz.y / 2, ClampInt(f * c.bitWidth, 1, 32), wxT("In"));
        for (int i = 0; i < f; ++i) {
            int hi = (f - 1 - i + 1) * c.bitWidth - 1;
            int lo = (f - 1 - i) * c.bitWidth;
            out(30, (i * 2 + 1) * sz.y / (2 * f), c.bitWidth,
                (lo == hi) ? wxString::Format(wxT("%d"), lo)
                           : wxString::Format(wxT("%d:%d"), hi, lo));
        }
        break; }

    case CT_Text:
    case CT_SubCircuit: {
        if (c.type == CT_SubCircuit) {
            wxSize sz = Size(c);
            std::vector<int> p = Ports(c.sub);
            int nIn = 0, nOut = 0;
            std::vector<int> ins, outs;
            for (int idx : p) {
                if (c.sub >= 0 && c.sub < (int)circuits.size() &&
                    circuits[c.sub].comps[idx].type == CT_InputPin) { ins.push_back(idx); ++nIn; }
                else { outs.push_back(idx); ++nOut; }
            }
            for (int i = 0; i < nIn; ++i) {
                const Component& pc = circuits[c.sub].comps[ins[i]];
                in(0, (i * 2 + 1) * sz.y / (2 * nIn), ClampInt(pc.bits, 1, 32),
                   pc.label.empty() ? wxString::Format(wxT("I%d"), i + 1) : pc.label);
            }
            for (int i = 0; i < nOut; ++i) {
                const Component& pc = circuits[c.sub].comps[outs[i]];
                out(sz.x, (i * 2 + 1) * sz.y / (2 * nOut), ClampInt(pc.bits, 1, 32),
                    pc.label.empty() ? wxString::Format(wxT("O%d"), i + 1) : pc.label);
            }
        }
        break; }
    }
    return v;
}

// ---------------------------------------------------------------- 属性
std::vector<AttrDef> GetAttrDefs(const Component& c)
{
    std::vector<AttrDef> v;
    auto add = [&](const wxString& k, const wxString& n, int kind, int lo, int hi,
                   const wxString& ch = wxEmptyString) {
        AttrDef d; d.key = k; d.name = n; d.kind = kind; d.minv = lo; d.maxv = hi; d.choices = ch;
        v.push_back(d);
    };

    add(wxT("label"), wxT("标签"), AK_STRING, 0, 0);

    switch (c.type) {
    case CT_InputPin: case CT_OutputPin:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        if (c.type == CT_InputPin) add(wxT("value"), wxT("当前值"), AK_HEX, 0, 0);
        break;
    case CT_Constant:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        add(wxT("value"), wxT("常量值"), AK_HEX, 0, 0);
        break;
    case CT_NOT: case CT_Buffer: case CT_TriState:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        break;
    case CT_Custom:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        break;
    case CT_AND: case CT_OR: case CT_NAND: case CT_NOR: case CT_XOR: case CT_XNOR:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        add(wxT("inputs"), wxT("输入端口数"), AK_INT, 2, 8);
        break;
    case CT_Mux: case CT_Demux:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        add(wxT("selBits"), wxT("选择位宽"), AK_INT, 1, 3);
        break;
    case CT_Decoder:
        add(wxT("selBits"), wxT("选择位宽"), AK_INT, 1, 3);
        break;
    case CT_Adder: case CT_Subtractor: case CT_Comparator: case CT_Multiplier:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        break;
    case CT_Shifter:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        add(wxT("flags"), wxT("移位方向"), AK_CHOICE, 0, 1, wxT("左移|右移"));
        break;
    case CT_DFlipFlop: case CT_Register:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        add(wxT("flags"), wxT("触发沿"), AK_CHOICE, 0, 1, wxT("上升沿|下降沿"));
        break;
    case CT_Counter:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        add(wxT("flags"), wxT("触发沿"), AK_CHOICE, 0, 1, wxT("上升沿|下降沿"));
        add(wxT("cflags"), wxT("计数方向"), AK_CHOICE, 0, 1, wxT("加计数|减计数"));
        break;
    case CT_RAM:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        add(wxT("addrBits"), wxT("地址位宽"), AK_INT, 1, 10);
        break;
    case CT_ROM:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        add(wxT("addrBits"), wxT("地址位宽"), AK_INT, 1, 10);
        add(wxT("text"), wxT("存储内容(十六进制)"), AK_HEX, 0, 0);
        break;
    case CT_Clock:
        add(wxT("value"), wxT("初始相位"), AK_CHOICE, 0, 1, wxT("低电平|高电平"));
        break;
    case CT_Tunnel:
        add(wxT("label"), wxT("隧道名"), AK_STRING, 0, 0);
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        break;
    case CT_Probe:
        add(wxT("bits"), wxT("数据位宽"), AK_INT, 1, MAX_BITS);
        break;
    case CT_Splitter:
        add(wxT("fanout"), wxT("输出路数"), AK_INT, 2, 8);
        add(wxT("bitWidth"), wxT("每路位宽"), AK_INT, 1, 8);
        add(wxT("bits"), wxT("输入位宽"), AK_READONLY, 0, 0);
        break;
    case CT_Text:
        v.clear();
        add(wxT("label"), wxT("文本内容"), AK_STRING, 0, 0);
        break;
    case CT_SubCircuit:
        add(wxT("sub"), wxT("目标电路"), AK_CHOICE, 0, 0, wxEmptyString);
        break;
    case CT_LED: case CT_Button: case CT_HexDisplay:
        break;
    default:
        break;
    }

    add(wxT("facing"), wxT("朝向"), AK_CHOICE, 0, 3, wxT("东|西|北|南"));
    add(wxT("posx"), wxT("X 坐标"), AK_READONLY, 0, 0);
    add(wxT("posy"), wxT("Y 坐标"), AK_READONLY, 0, 0);
    return v;
}

wxString GetAttrValue(const Component& c, const wxString& key)
{
    if (key == wxT("label"))    return c.label;
    if (key == wxT("text"))     return c.text;
    if (key == wxT("bits"))     return wxString::Format(wxT("%d"), c.bits);
    if (key == wxT("inputs"))   return wxString::Format(wxT("%d"), c.inputs);
    if (key == wxT("selBits"))  return wxString::Format(wxT("%d"), c.selBits);
    if (key == wxT("addrBits")) return wxString::Format(wxT("%d"), c.addrBits);
    if (key == wxT("fanout"))   return wxString::Format(wxT("%d"), c.fanout);
    if (key == wxT("bitWidth")) return wxString::Format(wxT("%d"), c.bitWidth);
    if (key == wxT("flags"))    return wxString::Format(wxT("%d"), c.flags & 1);
    if (key == wxT("cflags"))   return wxString::Format(wxT("%d"), (c.flags >> 1) & 1);
    if (key == wxT("value"))    return FormatValue(c.value, c.bits);
    if (key == wxT("sub"))      return wxString::Format(wxT("%d"), c.sub);
    if (key == wxT("facing"))   return wxString::Format(wxT("%d"), c.facing);
    if (key == wxT("posx"))     return wxString::Format(wxT("%d"), c.pos.x);
    if (key == wxT("posy"))     return wxString::Format(wxT("%d"), c.pos.y);
    return wxEmptyString;
}

static bool ParseUInt(const wxString& s, uint32_t* out)
{
    wxString t = s;
    t.Trim(true).Trim(false);
    if (t.empty()) return false;
    unsigned long v = 0;
    if (t.StartsWith(wxT("0x")) || t.StartsWith(wxT("0X")))
        return t.Mid(2).ToULong(&v, 16) && (*out = (uint32_t)v, true);
    if (t.StartsWith(wxT("0b")) || t.StartsWith(wxT("0B")))
        return t.Mid(2).ToULong(&v, 2) && (*out = (uint32_t)v, true);
    return t.ToULong(&v, 10) && (*out = (uint32_t)v, true);
}

bool SetAttrValue(Component& c, const wxString& key, const wxString& val, wxString* err)
{
    auto fail = [&](const wxString& m) { if (err) *err = m; return false; };
    auto toInt = [&](int* out) {
        long v = 0;
        if (!val.ToLong(&v)) return false;
        *out = (int)v;
        return true;
    };
    int iv = 0;

    if (key == wxT("label")) {
        c.label = val;
        c.label.Replace(wxT("\n"), wxT(" "));
        if (c.type == CT_SubCircuit) c.label = val;
        return true;
    }
    if (key == wxT("text")) { c.text = val; return true; }

    if (key == wxT("bits")) {
        if (c.type == CT_Splitter) return fail(wxT("分线器输入位宽由输出路数与每路位宽决定"));
        if (!toInt(&iv)) return fail(wxT("数据位宽必须是整数"));
        if (iv < 1 || iv > MAX_BITS) return fail(wxString::Format(wxT("数据位宽必须在 1~%d 之间"), MAX_BITS));
        c.bits = iv;
        c.value &= MaskOf(c.bits);
        return true;
    }
    if (key == wxT("inputs")) {
        if (!toInt(&iv)) return fail(wxT("输入端口数必须是整数"));
        if (iv < 2 || iv > 8) return fail(wxT("输入端口数必须在 2~8 之间"));
        c.inputs = iv;
        return true;
    }
    if (key == wxT("selBits")) {
        if (!toInt(&iv)) return fail(wxT("选择位宽必须是整数"));
        if (iv < 1 || iv > 3) return fail(wxT("选择位宽必须在 1~3 之间"));
        c.selBits = iv;
        return true;
    }
    if (key == wxT("addrBits")) {
        if (!toInt(&iv)) return fail(wxT("地址位宽必须是整数"));
        if (iv < 1 || iv > 10) return fail(wxT("地址位宽必须在 1~10 之间"));
        c.addrBits = iv;
        return true;
    }
    if (key == wxT("fanout")) {
        if (!toInt(&iv)) return fail(wxT("输出路数必须是整数"));
        if (iv < 2 || iv > 8) return fail(wxT("输出路数必须在 2~8 之间"));
        c.fanout = iv;
        return true;
    }
    if (key == wxT("bitWidth")) {
        if (!toInt(&iv)) return fail(wxT("每路位宽必须是整数"));
        if (iv < 1 || iv > 8) return fail(wxT("每路位宽必须在 1~8 之间"));
        c.bitWidth = iv;
        return true;
    }
    if (key == wxT("flags")) {
        if (!toInt(&iv)) return fail(wxT("取值非法"));
        c.flags = (c.flags & ~1) | (iv & 1);
        return true;
    }
    if (key == wxT("cflags")) {
        if (!toInt(&iv)) return fail(wxT("取值非法"));
        c.flags = (c.flags & ~2) | ((iv & 1) << 1);
        return true;
    }
    if (key == wxT("value")) {
        uint32_t v = 0;
        if (!ParseUInt(val, &v)) return fail(wxT("请输入十进制或 0x 开头的十六进制数"));
        c.value = v & MaskOf(c.type == CT_Clock ? 1 : c.bits);
        return true;
    }
    if (key == wxT("sub")) {
        if (!toInt(&iv)) return fail(wxT("子电路非法"));
        c.sub = iv;
        return true;
    }
    if (key == wxT("facing")) {
        if (!toInt(&iv)) return fail(wxT("朝向非法"));
        if (iv < 0 || iv > 3) return fail(wxT("朝向必须在 0~3 之间"));
        c.facing = iv;
        return true;
    }
    return fail(wxT("未知属性"));
}

// ---------------------------------------------------------------- 默认属性
void InitComponentDefaults(Component& c, int type, const Document* doc)
{
    c.type    = type;
    c.bits    = 1;
    c.inputs  = 2;
    c.selBits = 1;
    c.addrBits = 4;
    c.fanout  = 2;
    c.bitWidth = 1;
    c.flags   = 0;
    c.value   = 0;
    c.sub     = -1;
    c.cdef    = -1;
    c.facing  = F_East;

    switch (type) {
    case CT_Tunnel:     c.label = wxT("net");        break;
    case CT_InputPin:   c.label = wxT("in");         break;
    case CT_OutputPin:  c.label = wxT("out");        break;
    case CT_Text:       c.label = wxT("文本");        break;
    case CT_Constant:   c.label = wxEmptyString;     break;
    case CT_ROM:        c.text  = wxT("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F"); break;
    case CT_HexDisplay: c.bits  = 4;                 break;
    case CT_Shifter:    c.bits  = 8;                 break;
    case CT_RAM:        c.bits  = 8; c.addrBits = 4; break;
    case CT_Custom:
        if (doc && !doc->customDefs.empty()) {
            c.cdef = 0;
            c.bits = doc->customDefs[0].bits;
            c.label = doc->customDefs[0].name;
        }
        break;
    case CT_SubCircuit:
        if (doc && doc->circuits.size() > 1) {
            c.sub = 1;
            c.label = doc->circuits[1].name;
        } else {
            c.sub = -1;
            c.label = wxT("子电路");
        }
        break;
    default: break;
    }
}

// ---------------------------------------------------------------- 序列化
static wxString Escape(const wxString& s)
{
    wxString r;
    for (size_t i = 0; i < s.length(); ++i) {
        wxUniChar ch = s[i];
        if (ch == '\\')      r += wxT("\\\\");
        else if (ch == '"')  r += wxT("\\\"");
        else if (ch == '\n') r += wxT("\\n");
        else if (ch == '\r') r += wxT("\\r");
        else                 r += ch;
    }
    return r;
}

static bool ParseKV(const wxString& line, std::map<wxString, wxString>& kv)
{
    kv.clear();
    size_t i = 0;
    const size_t n = line.length();
    while (i < n) {
        while (i < n && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= n) break;
        size_t ks = i;
        while (i < n && line[i] != '=' && line[i] != ' ') ++i;
        wxString key = line.Mid(ks, i - ks);
        while (i < n && line[i] == ' ') ++i;
        if (i >= n || line[i] != '=') continue;
        ++i;
        wxString val;
        if (i < n && line[i] == '"') {
            ++i;
            while (i < n) {
                wxUniChar ch = line[i];
                if (ch == '\\' && i + 1 < n) {
                    wxUniChar nx = line[i + 1];
                    if (nx == 'n')      val += '\n';
                    else if (nx == 'r') val += '\r';
                    else                val += nx;
                    i += 2;
                    continue;
                }
                if (ch == '"') { ++i; break; }
                val += ch;
                ++i;
            }
        } else {
            size_t vs = i;
            while (i < n && line[i] != ' ') ++i;
            val = line.Mid(vs, i - vs);
        }
        if (!key.empty()) kv[key] = val;
    }
    return true;
}

static long KVLong(const std::map<wxString, wxString>& kv, const wxString& k, long def)
{
    auto it = kv.find(k);
    if (it == kv.end()) return def;
    long v = def;
    if (!it->second.ToLong(&v)) return def;
    return v;
}

wxString Document::Serialize() const
{
    wxString s;
    s += wxT("# 逻辑电路仿真器 工程文件 / Logic Circuit Simulator project file (UTF-8)\n");
    s += wxT("# 纯文本格式，可用版本管理工具比较；旧文件可直接打开\n");
    s += wxT("VERSION 1\n");

    // ---- 运行时自定义元件库 ----
    for (const auto& d : customDefs) {
        s += wxString::Format(wxT("CUSTOM name=\"%s\" desc=\"%s\" w=%d h=%d logic=%d bits=%d\n"),
                              Escape(d.name), Escape(d.desc), d.w, d.h, d.logic, d.bits);
        for (const auto& p : d.prims) {
            wxString pts;
            for (size_t i = 0; i < p.pts.size(); ++i) {
                if (i) pts += wxT(";");
                pts += wxString::Format(wxT("%d,%d"), p.pts[i].x, p.pts[i].y);
            }
            s += wxString::Format(wxT("  PRIM kind=%d fill=%d pts=\"%s\" text=\"%s\"\n"),
                                  p.kind, p.fill ? 1 : 0, pts, Escape(p.text));
        }
        for (const auto& p : d.pins) {
            s += wxString::Format(wxT("  PIN name=\"%s\" in=%d bits=%d x=%d y=%d\n"),
                                  Escape(p.name), p.isInput ? 1 : 0, p.bits,
                                  p.pos.x, p.pos.y);
        }
        s += wxT("ENDCUSTOM\n");
    }

    for (size_t ci = 0; ci < circuits.size(); ++ci) {
        const Circuit& c = circuits[ci];
        s += wxT("CIRCUIT name=\"") + Escape(c.name) + wxT("\"\n");
        for (const auto& w : c.wires) {
            wxString pts;
            for (size_t i = 0; i < w.pts.size(); ++i) {
                if (i) pts += wxT(";");
                pts += wxString::Format(wxT("%d,%d"), w.pts[i].x, w.pts[i].y);
            }
            s += wxT("  WIRE pts=\"") + pts + wxT("\"\n");
        }
        for (const auto& m : c.comps) {
            s += wxString::Format(
                wxT("  COMP id=%d type=%d x=%d y=%d face=%d bits=%d inputs=%d sel=%d addr=%d fan=%d bw=%d flags=%d val=%u sub=%d cdef=%d"),
                m.id, m.type, m.pos.x, m.pos.y, m.facing, m.bits, m.inputs, m.selBits,
                m.addrBits, m.fanout, m.bitWidth, m.flags, (unsigned)m.value, m.sub, m.cdef);
            s += wxT(" label=\"") + Escape(m.label) + wxT("\"");
            s += wxT(" text=\"")  + Escape(m.text)  + wxT("\"");
            s += wxT("\n");
        }
        s += wxT("ENDCIRCUIT\n");
    }
    return s;
}

bool Document::Deserialize(const wxString& text, wxString* err)
{
    std::vector<Circuit> newCircuits;
    std::vector<CustomDef> newCustoms;
    wxStringTokenizer tok(text, wxT("\n"), wxTOKEN_RET_EMPTY_ALL);
    Circuit* cur = nullptr;
    std::map<wxString, wxString> kv;
    int lineNo = 0;

    while (tok.HasMoreTokens()) {
        wxString line = tok.GetNextToken();
        ++lineNo;
        line.Trim(true);
        line.Trim(false);
        if (line.empty() || line.StartsWith(wxT("#"))) continue;

        if (line.StartsWith(wxT("VERSION"))) continue;

        // ---- 自定义元件定义块 ----
        if (line.StartsWith(wxT("CUSTOM"))) {
            ParseKV(line.Mid(6), kv);
            CustomDef d;
            d.name  = kv.count(wxT("name")) ? kv[wxT("name")] : wxT("自定义元件");
            d.desc  = kv.count(wxT("desc")) ? kv[wxT("desc")] : wxString();
            d.w     = (int)KVLong(kv, wxT("w"), 60);
            d.h     = (int)KVLong(kv, wxT("h"), 60);
            d.logic = (int)KVLong(kv, wxT("logic"), CL_PASS);
            d.bits  = (int)KVLong(kv, wxT("bits"), 1);
            if (d.w < 20) d.w = 20;
            if (d.h < 20) d.h = 20;
            newCustoms.push_back(d);
            continue;
        }
        if (line.StartsWith(wxT("ENDCUSTOM"))) continue;

        if (line.StartsWith(wxT("PRIM"))) {
            if (newCustoms.empty()) continue;
            ParseKV(line.Mid(4), kv);
            CustomPrim p;
            p.kind = (int)KVLong(kv, wxT("kind"), CP_POLY);
            p.fill = KVLong(kv, wxT("fill"), 0) != 0;
            p.text = kv.count(wxT("text")) ? kv[wxT("text")] : wxString();
            wxString pts = kv.count(wxT("pts")) ? kv[wxT("pts")] : wxString();
            wxStringTokenizer pt(pts, wxT(";"), wxTOKEN_STRTOK);
            while (pt.HasMoreTokens()) {
                wxString one = pt.GetNextToken();
                long x = 0, y = 0;
                if (one.BeforeFirst(',').ToLong(&x) && one.AfterFirst(',').ToLong(&y))
                    p.pts.push_back(wxPoint((int)x, (int)y));
            }
            newCustoms.back().prims.push_back(p);
            continue;
        }

        if (line.StartsWith(wxT("PIN"))) {
            if (newCustoms.empty()) continue;
            ParseKV(line.Mid(3), kv);
            CustomPinDef p;
            p.name    = kv.count(wxT("name")) ? kv[wxT("name")] : wxString();
            p.isInput = KVLong(kv, wxT("in"), 1) != 0;
            p.bits    = (int)KVLong(kv, wxT("bits"), 1);
            p.pos.x   = (int)KVLong(kv, wxT("x"), 0);
            p.pos.y   = (int)KVLong(kv, wxT("y"), 0);
            newCustoms.back().pins.push_back(p);
            continue;
        }

        if (line.StartsWith(wxT("CIRCUIT"))) {
            ParseKV(line.Mid(7), kv);
            Circuit c;
            c.name = kv.count(wxT("name")) ? kv[wxT("name")] : wxString::Format(wxT("电路%d"), (int)newCircuits.size());
            newCircuits.push_back(c);
            cur = &newCircuits.back();
            continue;
        }
        if (line.StartsWith(wxT("ENDCIRCUIT"))) { cur = nullptr; continue; }

        if (line.StartsWith(wxT("WIRE"))) {
            if (!cur) continue;
            ParseKV(line.Mid(4), kv);
            Wire w;
            wxString pts = kv.count(wxT("pts")) ? kv[wxT("pts")] : wxString();
            wxStringTokenizer pt(pts, wxT(";"), wxTOKEN_STRTOK);
            while (pt.HasMoreTokens()) {
                wxString one = pt.GetNextToken();
                long x = 0, y = 0;
                wxString xs = one.BeforeFirst(',');
                wxString ys = one.AfterFirst(',');
                if (xs.ToLong(&x) && ys.ToLong(&y))
                    w.pts.push_back(wxPoint((int)x, (int)y));
            }
            if (w.pts.size() >= 2) cur->wires.push_back(w);
            continue;
        }

        if (line.StartsWith(wxT("COMP"))) {
            if (!cur) continue;
            ParseKV(line.Mid(4), kv);
            Component m;
            m.id      = (int)KVLong(kv, wxT("id"), 0);
            m.type    = (int)KVLong(kv, wxT("type"), CT_AND);
            m.pos.x   = (int)KVLong(kv, wxT("x"), 0);
            m.pos.y   = (int)KVLong(kv, wxT("y"), 0);
            m.facing  = (int)KVLong(kv, wxT("face"), 0);
            m.bits    = (int)KVLong(kv, wxT("bits"), 1);
            m.inputs  = (int)KVLong(kv, wxT("inputs"), 2);
            m.selBits = (int)KVLong(kv, wxT("sel"), 1);
            m.addrBits= (int)KVLong(kv, wxT("addr"), 4);
            m.fanout  = (int)KVLong(kv, wxT("fan"), 2);
            m.bitWidth= (int)KVLong(kv, wxT("bw"), 1);
            m.flags   = (int)KVLong(kv, wxT("flags"), 0);
            m.value   = (uint32_t)KVLong(kv, wxT("val"), 0);
            m.sub     = (int)KVLong(kv, wxT("sub"), -1);
            m.cdef    = (int)KVLong(kv, wxT("cdef"), -1);
            m.label   = kv.count(wxT("label")) ? kv[wxT("label")] : wxString();
            m.text    = kv.count(wxT("text"))  ? kv[wxT("text")]  : wxString();
            if (m.type < 0 || m.type >= CT_COUNT) m.type = CT_AND;
            m.pos.x = (m.pos.x / GRID) * GRID;
            m.pos.y = (m.pos.y / GRID) * GRID;
            if (m.id <= 0) m.id = cur->nextId++;
            else if (m.id >= cur->nextId) cur->nextId = m.id + 1;
            cur->comps.push_back(m);
            continue;
        }
    }

    if (newCircuits.empty()) {
        if (err) *err = wxT("文件中没有找到任何电路定义。");
        return false;
    }

    circuits = newCircuits;
    customDefs = newCustoms;
    active   = 0;
    dirty    = false;
    Touch();
    dirty = false;
    return true;
}

bool Document::SaveTo(const wxString& p, wxString* err)
{
    wxFileOutputStream out(p);
    if (!out.IsOk()) {
        if (err) *err = wxT("无法写入文件: ") + p;
        return false;
    }
    const wxScopedCharBuffer utf8 = Serialize().utf8_str();
    out.Write(utf8.data(), utf8.length());
    out.Close();
    path  = p;
    dirty = false;
    return true;
}

bool Document::LoadFrom(const wxString& p, wxString* err)
{
    if (!wxFileExists(p)) {
        if (err) *err = wxT("文件不存在: ") + p;
        return false;
    }
    wxFileInputStream in(p);
    if (!in.IsOk()) {
        if (err) *err = wxT("无法读取文件: ") + p;
        return false;
    }
    wxFileOffset len = in.GetLength();
    std::string buf;
    buf.resize((size_t)(len > 0 ? len : 0));
    if (len > 0) in.Read(&buf[0], (size_t)len);
    wxString text = wxString::FromUTF8(buf.c_str());
    if (!Deserialize(text, err)) return false;
    path  = p;
    dirty = false;
    return true;
}

} // namespace lc
