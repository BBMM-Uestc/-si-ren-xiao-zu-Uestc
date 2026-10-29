// ============================================================================
//  Simulator.h  --  逻辑仿真内核
//  * 把当前电路(含子电路层次)展开成扁平元件表
//  * 用并查集把导线/引脚/隧道归并成"网络"
//  * 迭代求值直到收敛(可检测组合环振荡)
//  * 时钟节拍驱动时序元件与存储器
// ============================================================================
#pragma once

#include "LogicModel.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace lc {

struct SimResult
{
    std::vector<PinValue> nets;
    std::map<std::string, std::vector<PinValue> > pins;   // 元件 key -> 各引脚值
    bool oscillating = false;
};

class Simulator
{
public:
    Simulator();

    void Attach(Document* doc);
    void SetRoot(int circuitIdx);      // 以哪张电路图作为仿真根
    void Invalidate();                 // 结构改变，下次仿真重建网络
    void Reset();                      // 复位全部时序状态
    void Settle();                     // 组合逻辑收敛(不推进时钟)
    void Tick();                       // 一个仿真节拍

    bool IsOscillating() const { return m_result.oscillating; }
    int  Root() const { return m_root; }

    // ---- 查询(供绘制使用，均在根作用域下) ----
    PinValue PinOf(const Component& c, int k) const;
    PinValue ValueAtPoint(const wxPoint& p) const;
    PinValue NetValue(int net) const;
    bool     HasNetAtPoint(const wxPoint& p) const;

    const SimResult& Result() const { return m_result; }
    std::map<std::string, CompState>& States() { return m_states; }

    static std::string CompKey(const std::string& prefix, int id);
    static std::string PointKey(const std::string& prefix, const wxPoint& p);

private:
    struct FlatComp
    {
        const Component* def = nullptr;
        int              circuit = 0;
        std::string      key;
        std::vector<int> pinPoint;
        std::vector<int> pinNet;
        std::vector<int> inIdx;    // pinPoint 中的输入脚下标
        std::vector<int> outIdx;   // pinPoint 中的输出脚下标
        bool             isSub = false;
    };

    struct Seg
    {
        int     a = 0, b = 0;
        int     scope = 0;
        wxPoint pa, pb;
    };

    // 并查集
    std::vector<int> m_parent;
    int  Find(int x);
    void Union(int a, int b);

    int  ScopeId(const std::string& prefix);
    int  PointIndex(const std::string& prefix, const wxPoint& p, int scope);
    void AddSegToGrid(int segIdx);

    void Rebuild();
    void Flatten(int circIdx, const std::string& prefix, int scope, int depth,
                 std::set<int>& visiting);
    void Evaluate(FlatComp& fc);
    CompState& StateOf(FlatComp& fc, bool create);

    Document* m_doc = nullptr;
    int       m_root = 0;
    bool      m_dirty = true;
    long      m_cachedVersion = -1;

    std::vector<wxPoint>      m_ptPos;
    std::vector<int>          m_ptScope;
    std::vector<int>          m_ptNet;
    std::map<std::string, int> m_ptIdx;
    std::vector<Seg>          m_segs;
    std::map<long long, std::vector<int> > m_segGrid;
    std::map<std::string, int> m_scopeIds;
    std::map<std::string, std::vector<int> > m_tunnels;
    std::vector<FlatComp>     m_flat;

    int m_numNets = 0;
    std::vector<std::vector<PinValue> > m_drives;
    std::vector<PinValue> m_netVal;
    std::map<std::string, int> m_netOfPoint;   // 仅根作用域
    std::map<std::string, CompState> m_states;
    SimResult m_result;
};

} // namespace lc
