// ============================================================================
//  Simulator.cpp  --  逻辑仿真内核实现
// ============================================================================
#include "Simulator.h"

#include <wx/tokenzr.h>

#include <algorithm>
#include <cstdio>

namespace lc {

static const int MAX_PASS = 120;   // 组合逻辑收敛迭代上限
static const int MAX_DEPTH = 8;    // 子电路层次上限

// ---------------------------------------------------------------- 小工具
static PinValue OkV(uint32_t v)   { PinValue p; p.bits = v; p.state = 1; return p; }
static PinValue ErrV()            { PinValue p; p.bits = 0; p.state = 2; return p; }
static PinValue FloatV()          { PinValue p; p.bits = 0; p.state = 0; return p; }

static bool AnyBad(const std::vector<PinValue>& v)
{
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].state != 1) return true;
    return false;
}

static std::vector<uint32_t> ParseHexList(const wxString& text, int count)
{
    std::vector<uint32_t> m((size_t)std::max(0, count), 0u);
    wxString t = text;
    t.Replace(wxT(","), wxT(" "));
    t.Replace(wxT(";"), wxT(" "));
    t.Replace(wxT("\t"), wxT(" "));
    wxStringTokenizer tk(t, wxT(" "), wxTOKEN_STRTOK);
    int i = 0;
    while (tk.HasMoreTokens() && i < count) {
        wxString one = tk.GetNextToken();
        one.Trim(true).Trim(false);
        if (one.empty()) continue;
        unsigned long v = 0;
        if (one.StartsWith(wxT("0x")) || one.StartsWith(wxT("0X")))
            one.Mid(2).ToULong(&v, 16);
        else if (!one.ToULong(&v, 16))
            v = 0;
        m[(size_t)i++] = (uint32_t)v;
    }
    return m;
}

// ---------------------------------------------------------------- 构造
Simulator::Simulator()
{
    m_parent.clear();
}

void Simulator::Attach(Document* doc)
{
    m_doc = doc;
    m_root = doc ? doc->active : 0;
    Reset();
}

void Simulator::SetRoot(int circuitIdx)
{
    if (m_root == circuitIdx) return;
    m_root = circuitIdx;
    Invalidate();
}

void Simulator::Invalidate()
{
    m_dirty = true;
}

void Simulator::Reset()
{
    m_states.clear();
    m_dirty = true;
    Settle();
}

// ---------------------------------------------------------------- key
std::string Simulator::CompKey(const std::string& prefix, int id)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "#%d", id);
    return prefix + buf;
}

std::string Simulator::PointKey(const std::string& prefix, const wxPoint& p)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "|%d,%d", p.x, p.y);
    return prefix + buf;
}

// ---------------------------------------------------------------- 并查集
int Simulator::Find(int x)
{
    while (m_parent[x] != x) {
        m_parent[x] = m_parent[m_parent[x]];
        x = m_parent[x];
    }
    return x;
}

void Simulator::Union(int a, int b)
{
    int ra = Find(a), rb = Find(b);
    if (ra != rb) m_parent[rb] = ra;
}

int Simulator::ScopeId(const std::string& prefix)
{
    std::map<std::string, int>::iterator it = m_scopeIds.find(prefix);
    if (it != m_scopeIds.end()) return it->second;
    int id = (int)m_scopeIds.size();
    m_scopeIds[prefix] = id;
    return id;
}

int Simulator::PointIndex(const std::string& prefix, const wxPoint& p, int scope)
{
    std::string key = PointKey(prefix, p);
    std::map<std::string, int>::iterator it = m_ptIdx.find(key);
    if (it != m_ptIdx.end()) return it->second;
    int idx = (int)m_ptPos.size();
    m_ptPos.push_back(p);
    m_ptScope.push_back(scope);
    m_ptNet.push_back(-1);
    m_parent.push_back(idx);
    m_ptIdx[key] = idx;
    if (prefix.empty()) m_netOfPoint[key] = idx;   // 稍后替换成网络号
    return idx;
}

void Simulator::AddSegToGrid(int segIdx)
{
    const Seg& s = m_segs[segIdx];
    int x0 = std::min(s.pa.x, s.pb.x) / GRID;
    int x1 = std::max(s.pa.x, s.pb.x) / GRID;
    int y0 = std::min(s.pa.y, s.pb.y) / GRID;
    int y1 = std::max(s.pa.y, s.pb.y) / GRID;
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y) {
            long long key = ((long long)s.scope << 42) |
                            ((long long)(x + 100000) << 21) |
                            (long long)(y + 100000);
            m_segGrid[key].push_back(segIdx);
        }
}

// ---------------------------------------------------------------- 展开
void Simulator::Flatten(int circIdx, const std::string& prefix, int scope, int depth,
                        std::set<int>& visiting)
{
    if (!m_doc) return;
    if (circIdx < 0 || circIdx >= (int)m_doc->circuits.size()) return;
    if (depth > MAX_DEPTH) return;
    if (visiting.count(circIdx)) return;      // 防止自包含递归
    visiting.insert(circIdx);

    const Circuit& circ = m_doc->circuits[circIdx];

    // ---- 导线：连续顶点两两相连，并登记到网格索引
    for (size_t wi = 0; wi < circ.wires.size(); ++wi) {
        const Wire& w = circ.wires[wi];
        int prev = -1;
        for (size_t i = 0; i < w.pts.size(); ++i) {
            int idx = PointIndex(prefix, w.pts[i], scope);
            if (prev >= 0 && prev != idx) {
                Seg s;
                s.a = prev;
                s.b = idx;
                s.scope = scope;
                s.pa = m_ptPos[prev];
                s.pb = m_ptPos[idx];
                int si = (int)m_segs.size();
                m_segs.push_back(s);
                AddSegToGrid(si);
                Union(prev, idx);
            }
            prev = idx;
        }
    }

    // ---- 元件
    for (size_t ci = 0; ci < circ.comps.size(); ++ci) {
        const Component& c = circ.comps[ci];
        wxSize sz = m_doc->Size(c);
        std::vector<PinInfo> pins = m_doc->Pins(c);

        FlatComp fc;
        fc.def     = &c;
        fc.circuit = circIdx;
        fc.key     = CompKey(prefix, c.id);
        fc.isSub   = (c.type == CT_SubCircuit);
        fc.pinPoint.resize(pins.size(), -1);
        fc.pinNet.resize(pins.size(), -1);

        for (size_t k = 0; k < pins.size(); ++k) {
            wxPoint wp = LocalToWorld(c, sz, pins[k].pos);
            fc.pinPoint[k] = PointIndex(prefix, wp, scope);
            if (pins[k].isInput) fc.inIdx.push_back((int)k);
            else                 fc.outIdx.push_back((int)k);
        }

        if (c.type == CT_SubCircuit) {
            if (c.sub >= 0 && c.sub < (int)m_doc->circuits.size() && !visiting.count(c.sub)) {
                std::string subPrefix = prefix + CompKey("", c.id) + "/";
                int subScope = ScopeId(subPrefix);
                std::vector<int> ports = m_doc->Ports(c.sub);
                const Circuit& sub = m_doc->circuits[c.sub];
                for (size_t k = 0; k < ports.size() && k < fc.pinPoint.size(); ++k) {
                    const Component& pc = sub.comps[ports[k]];
                    std::vector<PinInfo> pp = m_doc->Pins(pc);
                    if (pp.empty()) continue;
                    wxSize psz = m_doc->Size(pc);
                    wxPoint wp = LocalToWorld(pc, psz, pp[0].pos);
                    int idx = PointIndex(subPrefix, wp, subScope);
                    Union(fc.pinPoint[k], idx);
                }
                Flatten(c.sub, subPrefix, subScope, depth + 1, visiting);
            }
        } else if (c.type == CT_Tunnel) {
            // 同名隧道互通(同一实例作用域内)
            std::string tkey = prefix + "||" + std::string(c.label.utf8_str());
            if (!fc.pinPoint.empty())
                m_tunnels[tkey].push_back(fc.pinPoint[0]);
        }

        m_flat.push_back(fc);
    }

    visiting.erase(circIdx);
}

void Simulator::Rebuild()
{
    m_ptPos.clear();
    m_ptScope.clear();
    m_ptNet.clear();
    m_ptIdx.clear();
    m_segs.clear();
    m_segGrid.clear();
    m_scopeIds.clear();
    m_tunnels.clear();
    m_flat.clear();
    m_netOfPoint.clear();
    m_parent.clear();

    if (!m_doc || m_doc->circuits.empty()) {
        m_numNets = 0;
        m_netVal.clear();
        m_result.nets.clear();
        m_result.pins.clear();
        return;
    }
    if (m_root < 0 || m_root >= (int)m_doc->circuits.size()) m_root = 0;

    std::set<int> visiting;
    int rootScope = ScopeId("");
    Flatten(m_root, "", rootScope, 0, visiting);

    // ---- 交叉/搭接点：任何落在某线段上的点与该线段归并
    for (size_t i = 0; i < m_ptPos.size(); ++i) {
        const wxPoint& p = m_ptPos[i];
        long long key = ((long long)m_ptScope[i] << 42) |
                        ((long long)(p.x / GRID + 100000) << 21) |
                        (long long)(p.y / GRID + 100000);
        std::map<long long, std::vector<int> >::iterator it = m_segGrid.find(key);
        if (it == m_segGrid.end()) continue;
        const std::vector<int>& cand = it->second;
        for (size_t k = 0; k < cand.size(); ++k) {
            const Seg& s = m_segs[cand[k]];
            if (s.a == (int)i || s.b == (int)i) continue;
            if (PointOnSegment(p, s.pa, s.pb)) Union((int)i, s.a);
        }
    }

    // ---- 隧道归并
    for (std::map<std::string, std::vector<int> >::iterator it = m_tunnels.begin();
         it != m_tunnels.end(); ++it) {
        const std::vector<int>& v = it->second;
        for (size_t i = 1; i < v.size(); ++i) Union(v[0], v[i]);
    }

    // ---- 分配网络号
    std::map<int, int> rootNet;
    m_numNets = 0;
    for (size_t i = 0; i < m_ptPos.size(); ++i) {
        int r = Find((int)i);
        std::map<int, int>::iterator it = rootNet.find(r);
        int net;
        if (it == rootNet.end()) {
            net = m_numNets++;
            rootNet[r] = net;
        } else {
            net = it->second;
        }
        m_ptNet[i] = net;
    }

    // ---- 回填到元件引脚 & 根作用域点表
    for (size_t i = 0; i < m_flat.size(); ++i) {
        FlatComp& fc = m_flat[i];
        for (size_t k = 0; k < fc.pinPoint.size(); ++k)
            fc.pinNet[k] = m_ptNet[fc.pinPoint[k]];
    }
    for (std::map<std::string, int>::iterator it = m_netOfPoint.begin();
         it != m_netOfPoint.end(); ++it)
        it->second = m_ptNet[it->second];

    m_netVal.assign(m_numNets, FloatV());
    m_drives.assign(m_numNets, std::vector<PinValue>());
    m_result.nets.assign(m_numNets, FloatV());
    m_result.pins.clear();
}

// ---------------------------------------------------------------- 状态
CompState& Simulator::StateOf(FlatComp& fc, bool create)
{
    std::map<std::string, CompState>::iterator it = m_states.find(fc.key);
    if (it != m_states.end()) return it->second;
    static CompState dummy;
    if (!create) return dummy;

    CompState st;
    const Component& c = *fc.def;
    if (c.type == CT_Clock) st.value = (uint32_t)(c.value & 1);
    if (c.type == CT_RAM) {
        int n = 1 << ClampInt(c.addrBits, 1, 10);
        st.mem.assign((size_t)n, 0u);
    } else if (c.type == CT_ROM) {
        int n = 1 << ClampInt(c.addrBits, 1, 10);
        st.mem = ParseHexList(c.text, n);
    }
    st.init = true;
    std::pair<std::map<std::string, CompState>::iterator, bool> r =
        m_states.insert(std::make_pair(fc.key, st));
    return r.first->second;
}

// ---------------------------------------------------------------- 求值
void Simulator::Evaluate(FlatComp& fc)
{
    const Component& c = *fc.def;
    const int nb = ClampInt(c.bits, 1, MAX_BITS);
    const uint32_t mask = MaskOf(nb);

    struct Helper
    {
        Simulator* sim;
        FlatComp*  fc;
        PinValue In(int k) const
        {
            if (k < 0 || k >= (int)fc->inIdx.size()) return FloatV();
            return sim->m_netVal[fc->pinNet[fc->inIdx[k]]];
        }
        void Out(int k, const PinValue& v)
        {
            if (k < 0 || k >= (int)fc->outIdx.size()) return;
            sim->m_drives[fc->pinNet[fc->outIdx[k]]].push_back(v);
        }
        int InCount() const  { return (int)fc->inIdx.size(); }
    } h;
    h.sim = this;
    h.fc  = &fc;

    switch (c.type) {
    // ------------------------------------------------ 信号源
    case CT_InputPin:
        h.Out(0, OkV(c.value & MaskOf(nb)));
        break;
    case CT_Button:
        h.Out(0, OkV(c.value ? 1u : 0u));
        break;
    case CT_Constant:
        h.Out(0, OkV(c.value & mask));
        break;
    case CT_Clock: {
        CompState& st = StateOf(fc, true);
        h.Out(0, OkV(st.value & 1u));
        break; }

    // ------------------------------------------------ 门电路
    case CT_NOT: case CT_Buffer:
    case CT_AND: case CT_OR: case CT_NAND: case CT_NOR: case CT_XOR: case CT_XNOR: {
        int n = (c.type == CT_NOT || c.type == CT_Buffer) ? 1 : ClampInt(c.inputs, 2, 8);
        std::vector<PinValue> ins;
        for (int i = 0; i < n; ++i) ins.push_back(h.In(i));
        if (AnyBad(ins)) { h.Out(0, ErrV()); break; }

        uint32_t r = ins[0].bits & mask;
        for (int i = 1; i < n; ++i) {
            uint32_t x = ins[i].bits & mask;
            switch (c.type) {
            case CT_AND: case CT_NAND: r &= x; break;
            case CT_OR:  case CT_NOR:  r |= x; break;
            case CT_XOR: case CT_XNOR: r ^= x; break;
            default: break;
            }
        }
        switch (c.type) {
        case CT_NOT:  r = ~ins[0].bits & mask; break;
        case CT_NAND: r = ~r & mask; break;
        case CT_NOR:  r = ~r & mask; break;
        case CT_XNOR: r = ~r & mask; break;
        default: break;
        }
        h.Out(0, OkV(r & mask));
        break; }

    // ------------------------------------------------ 三态缓冲器(新增元件范例)
    case CT_TriState: {
        PinValue d  = h.In(0);       // 数据
        PinValue en = h.In(1);       // 使能
        if (d.state != 1)  { h.Out(0, ErrV()); break; }   // 数据悬空/冲突 -> 输出错误
        if (en.state == 0) { h.Out(0, ErrV()); break; }   // 使能悬空 -> 输出错误
        if (en.Bit(0)) h.Out(0, OkV(d.bits & mask));      // 使能有效 -> 正常驱动
        // 使能=0 时“不产生任何驱动”，该网络若无其它驱动即为高阻(悬空)，这正是三态门的语义
        break; }

    // ------------------------------------------------ 运行时自定义元件
    case CT_Custom: {
        const CustomDef* d = m_doc ? m_doc->FindCustom(c.cdef) : nullptr;
        int nIn  = (int)fc.inIdx.size();
        int nOut = (int)fc.outIdx.size();
        if (!d || d->logic == CL_NONE) break;      // 纯图形，不驱动任何输出

        // 取所有输入
        std::vector<PinValue> ins(nIn);
        for (int i = 0; i < nIn; ++i) ins[i] = h.In(i);

        auto allBad = [&](const std::vector<PinValue>& v, int upto) {
            for (int i = 0; i < upto && i < (int)v.size(); ++i)
                if (v[i].state != 1) return true;
            return false;
        };
        auto errAll = [&]() { for (int i = 0; i < nOut; ++i) h.Out(i, ErrV()); };

        switch (d->logic) {
        case CL_CONST:
            if (nOut > 0) h.Out(0, OkV(c.value & mask));
            break;

        case CL_PASS:
            if (nIn < 1 || nOut < 1) break;
            if (allBad(ins, 1)) { errAll(); break; }
            h.Out(0, OkV(ins[0].bits & mask));
            break;

        case CL_NOT:
            if (nIn < 1 || nOut < 1) break;
            if (allBad(ins, 1)) { errAll(); break; }
            h.Out(0, OkV(~ins[0].bits & mask));
            break;

        case CL_AND: case CL_OR: case CL_XOR:
        case CL_NAND: case CL_NOR: case CL_XNOR: {
            if (nIn < 1 || nOut < 1) break;
            if (allBad(ins, nIn)) { errAll(); break; }
            uint32_t r = ins[0].bits & mask;
            for (int i = 1; i < nIn; ++i) {
                uint32_t x = ins[i].bits & mask;
                if (d->logic == CL_AND || d->logic == CL_NAND)      r &= x;
                else if (d->logic == CL_OR || d->logic == CL_NOR)   r |= x;
                else                                                r ^= x;
            }
            if (d->logic == CL_NAND || d->logic == CL_NOR || d->logic == CL_XNOR)
                r = ~r & mask;
            h.Out(0, OkV(r & mask));
            break; }

        case CL_MUX: {
            int nData = nIn - 1;
            if (nData < 1 || nOut < 1) break;
            if (allBad(ins, nIn)) { errAll(); break; }
            int sel = (int)(ins[nIn - 1].bits & 0x1Fu);
            if (sel >= nData) sel = nData - 1;
            h.Out(0, OkV(ins[sel].bits & mask));
            break; }

        case CL_DECODER: {
            if (nIn < 1 || nOut < 1) break;
            if (allBad(ins, 1)) { errAll(); break; }
            int sel = (int)(ins[0].bits & 0x1Fu);
            for (int i = 0; i < nOut; ++i) h.Out(i, OkV(i == sel ? 1u : 0u));
            break; }

        default: break;
        }
        break; }

    // ------------------------------------------------ 复用器
    case CT_Mux: {        int n = 1 << ClampInt(c.selBits, 1, 3);
        PinValue sel = h.In(n);
        if (sel.state != 1) { h.Out(0, ErrV()); break; }
        int idx = (int)(sel.bits & 0x7u);
        if (idx >= n) { h.Out(0, ErrV()); break; }
        h.Out(0, h.In(idx));
        break; }

    case CT_Demux: {
        int n = 1 << ClampInt(c.selBits, 1, 3);
        PinValue data = h.In(0);
        PinValue sel  = h.In(1);
        if (sel.state != 1) { for (int i = 0; i < n; ++i) h.Out(i, ErrV()); break; }
        int idx = (int)(sel.bits & 0x7u);
        if (idx < n) h.Out(idx, data);
        break; }

    case CT_Decoder: {
        int n = 1 << ClampInt(c.selBits, 1, 3);
        PinValue sel = h.In(0);
        if (sel.state != 1) { for (int i = 0; i < n; ++i) h.Out(i, ErrV()); break; }
        int idx = (int)(sel.bits & 0x7u);
        for (int i = 0; i < n; ++i) h.Out(i, OkV(i == idx ? 1u : 0u));
        break; }

    // ------------------------------------------------ 运算
    case CT_Adder: {
        PinValue a = h.In(0), b = h.In(1), ci = h.In(2);
        if (a.state != 1 || b.state != 1 || ci.state != 1) { h.Out(0, ErrV()); h.Out(1, ErrV()); break; }
        uint64_t s = (uint64_t)(a.bits & mask) + (uint64_t)(b.bits & mask) + (uint64_t)(ci.bits & 1u);
        h.Out(0, OkV((uint32_t)s & mask));
        h.Out(1, OkV((uint32_t)((s >> nb) & 1u)));
        break; }

    case CT_Subtractor: {
        PinValue a = h.In(0), b = h.In(1), bi = h.In(2);
        if (a.state != 1 || b.state != 1 || bi.state != 1) { h.Out(0, ErrV()); h.Out(1, ErrV()); break; }
        long long d = (long long)(a.bits & mask) - (long long)(b.bits & mask) - (long long)(bi.bits & 1u);
        h.Out(0, OkV((uint32_t)(d & (long long)mask)));
        h.Out(1, OkV(d < 0 ? 1u : 0u));
        break; }

    case CT_Comparator: {
        PinValue a = h.In(0), b = h.In(1);
        if (a.state != 1 || b.state != 1) { h.Out(0, ErrV()); h.Out(1, ErrV()); h.Out(2, ErrV()); break; }
        uint32_t x = a.bits & mask, y = b.bits & mask;
        h.Out(0, OkV(x > y ? 1u : 0u));
        h.Out(1, OkV(x == y ? 1u : 0u));
        h.Out(2, OkV(x < y ? 1u : 0u));
        break; }

    case CT_Shifter: {
        PinValue d = h.In(0), s = h.In(1);
        if (d.state != 1 || s.state != 1) { h.Out(0, ErrV()); break; }
        int sh = (int)(s.bits & 31u);
        uint32_t r;
        if (c.flags & 1) r = (sh >= 32) ? 0u : (uint32_t)((d.bits & mask) >> sh);
        else             r = (sh >= 32) ? 0u : (uint32_t)((d.bits & mask) << sh);
        h.Out(0, OkV(r & mask));
        break; }

    case CT_Multiplier: {
        PinValue a = h.In(0), b = h.In(1);
        int ow = std::min(32, nb * 2);
        if (a.state != 1 || b.state != 1) { h.Out(0, ErrV()); break; }
        uint64_t p = (uint64_t)(a.bits & mask) * (uint64_t)(b.bits & mask);
        h.Out(0, OkV((uint32_t)p & MaskOf(ow)));
        break; }

    // ------------------------------------------------ 时序
    case CT_DFlipFlop: case CT_Register: case CT_Counter: {
        CompState& st = StateOf(fc, true);
        if (c.type == CT_DFlipFlop) {
            st.clkNow = (h.In(1).state == 1) && h.In(1).Bit(0);
            PinValue d = h.In(0);
            if (d.state == 1) st.next = d.bits & mask;
        } else if (c.type == CT_Register) {
            st.clkNow = (h.In(1).state == 1) && h.In(1).Bit(0);
            bool clr = (h.In(2).state == 1) && h.In(2).Bit(0);
            PinValue d = h.In(0);
            if (clr) st.next = 0;
            else if (d.state == 1) st.next = d.bits & mask;
        } else {
            st.clkNow = (h.In(0).state == 1) && h.In(0).Bit(0);
            bool clr = (h.In(1).state == 1) && h.In(1).Bit(0);
            if (clr) st.next = 0;
            else {
                uint32_t cur = st.value & mask;
                if (c.flags & 2) st.next = (cur == 0) ? mask : ((cur - 1) & mask);
                else             st.next = (cur + 1) & mask;
            }
        }
        h.Out(0, OkV(st.value & mask));
        if (c.type == CT_DFlipFlop) h.Out(1, OkV(~st.value & mask));
        break; }

    case CT_RAM: {
        CompState& st = StateOf(fc, true);
        int n = 1 << ClampInt(c.addrBits, 1, 10);
        if ((int)st.mem.size() != n) st.mem.resize((size_t)n, 0u);
        PinValue a = h.In(0), d = h.In(1), we = h.In(2);
        if (a.state != 1) { h.Out(0, ErrV()); break; }
        uint32_t addr = a.bits & MaskOf(ClampInt(c.addrBits, 1, 10));
        if (addr >= (uint32_t)n) addr = 0;
        st.memAddr = addr;
        st.memData = (d.state == 1) ? (d.bits & mask) : 0u;
        st.memWE   = (we.state == 1) && we.Bit(0);
        h.Out(0, OkV(st.mem[addr] & mask));
        break; }

    case CT_ROM: {
        CompState& st = StateOf(fc, true);
        int n = 1 << ClampInt(c.addrBits, 1, 10);
        if ((int)st.mem.size() != n) st.mem = ParseHexList(c.text, n);
        PinValue a = h.In(0);
        if (a.state != 1) { h.Out(0, ErrV()); break; }
        uint32_t addr = a.bits & MaskOf(ClampInt(c.addrBits, 1, 10));
        if (addr >= (uint32_t)n) addr = 0;
        h.Out(0, OkV(st.mem[addr] & mask));
        break; }

    // ------------------------------------------------ 布线
    case CT_Splitter: {
        int f = ClampInt(c.fanout, 2, 8);
        int bw = ClampInt(c.bitWidth, 1, 8);
        PinValue in = h.In(0);
        if (in.state != 1) { for (int i = 0; i < f; ++i) h.Out(i, ErrV()); break; }
        for (int i = 0; i < f; ++i) {
            int shift = (f - 1 - i) * bw;
            uint32_t v = (shift >= 32) ? 0u : ((in.bits >> shift) & MaskOf(bw));
            h.Out(i, OkV(v));
        }
        break; }

    // ------------------------------------------------ 无输出
    case CT_Tunnel:
    case CT_Text:
    case CT_SubCircuit:
    case CT_OutputPin:
    case CT_LED:
    case CT_HexDisplay:
    case CT_Probe:
    default:
        break;
    }
}

// ---------------------------------------------------------------- 收敛
void Simulator::Settle()
{
    if (!m_doc) return;
    if (m_dirty || m_cachedVersion != m_doc->version) {
        Rebuild();
        m_dirty = false;
        m_cachedVersion = m_doc->version;
    }

    m_result.oscillating = false;
    if (m_numNets == 0) return;

    for (size_t i = 0; i < m_drives.size(); ++i) m_drives[i].clear();
    for (size_t i = 0; i < m_netVal.size(); ++i) m_netVal[i] = FloatV();

    for (int pass = 0; pass < MAX_PASS; ++pass) {
        for (size_t i = 0; i < m_drives.size(); ++i) m_drives[i].clear();

        for (size_t i = 0; i < m_flat.size(); ++i)
            Evaluate(m_flat[i]);

        bool changed = false;
        for (int n = 0; n < m_numNets; ++n) {
            const std::vector<PinValue>& dr = m_drives[n];
            PinValue nv = FloatV();
            uint32_t acc = 0;
            int cnt = 0;
            bool err = false;
            for (size_t k = 0; k < dr.size(); ++k) {
                if (dr[k].state == 0) continue;          // 悬空驱动忽略
                if (dr[k].state == 2) { err = true; continue; }
                if (cnt == 0) acc = dr[k].bits;
                else if (acc != dr[k].bits) err = true;
                ++cnt;
            }
            if (err)        nv = ErrV();
            else if (cnt)   nv = OkV(acc);
            else            nv = FloatV();

            if (nv.bits != m_netVal[n].bits || nv.state != m_netVal[n].state) {
                m_netVal[n] = nv;
                changed = true;
            }
        }
        if (!changed) break;
        if (pass == MAX_PASS - 1) m_result.oscillating = true;
    }

    // ---- 记录结果
    m_result.nets = m_netVal;
    m_result.pins.clear();
    for (size_t i = 0; i < m_flat.size(); ++i) {
        FlatComp& fc = m_flat[i];
        std::vector<PinValue> vals(fc.pinPoint.size());
        for (size_t k = 0; k < fc.pinPoint.size(); ++k) {
            int net = fc.pinNet[k];
            vals[k] = (net >= 0 && net < (int)m_netVal.size()) ? m_netVal[net] : FloatV();
        }
        m_result.pins[fc.key] = vals;
    }
}

// ---------------------------------------------------------------- 节拍
void Simulator::Tick()
{
    if (!m_doc) return;

    // 1) 时钟翻转
    for (size_t i = 0; i < m_flat.size(); ++i) {
        if (m_flat[i].def->type != CT_Clock) continue;
        CompState& st = StateOf(m_flat[i], true);
        st.value ^= 1u;
    }

    // 2) 传播组合逻辑，并记录时序元件的时钟沿/下一状态
    Settle();

    // 3) 时钟沿锁存
    for (size_t i = 0; i < m_flat.size(); ++i) {
        FlatComp& fc = m_flat[i];
        if (!fc.def->IsSequential()) continue;
        CompState& st = StateOf(fc, true);
        bool rising;
        if (fc.def->flags & 1) rising = (!st.clkNow && st.clkPrev);
        else                   rising = (st.clkNow && !st.clkPrev);
        if (rising) st.value = st.next;
        st.clkPrev = st.clkNow;
    }

    // 4) 存储器写入
    for (size_t i = 0; i < m_flat.size(); ++i) {
        FlatComp& fc = m_flat[i];
        if (fc.def->type != CT_RAM) continue;
        CompState& st = StateOf(fc, true);
        if (st.memWE && st.memAddr < st.mem.size())
            st.mem[st.memAddr] = st.memData;
    }

    // 5) 再传播一次，让新状态显示出来
    Settle();
}

// ---------------------------------------------------------------- 查询
PinValue Simulator::PinOf(const Component& c, int k) const
{
    std::map<std::string, std::vector<PinValue> >::const_iterator it =
        m_result.pins.find(CompKey("", c.id));
    if (it == m_result.pins.end()) return FloatV();
    if (k < 0 || k >= (int)it->second.size()) return FloatV();
    return it->second[k];
}

PinValue Simulator::ValueAtPoint(const wxPoint& p) const
{
    std::map<std::string, int>::const_iterator it = m_netOfPoint.find(PointKey("", p));
    if (it == m_netOfPoint.end()) return FloatV();
    int n = it->second;
    if (n < 0 || n >= (int)m_result.nets.size()) return FloatV();
    return m_result.nets[n];
}

bool Simulator::HasNetAtPoint(const wxPoint& p) const
{
    return m_netOfPoint.find(PointKey("", p)) != m_netOfPoint.end();
}

PinValue Simulator::NetValue(int net) const
{
    if (net < 0 || net >= (int)m_result.nets.size()) return FloatV();
    return m_result.nets[net];
}

} // namespace lc
