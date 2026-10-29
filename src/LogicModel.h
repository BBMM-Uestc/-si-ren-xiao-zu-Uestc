// ============================================================================
//  LogicModel.h  --  MiniLogisim 电路数据模型
//  元件 / 引脚 / 连线 / 电路 / 文档
//  目标环境: Visual Studio 2022 + wxWidgets 3.2.2.1 (x64, Unicode, 静态库)
// ============================================================================
#pragma once

#include <wx/wx.h>
#include <wx/string.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace lc {

// ---------------------------------------------------------------- 基本常量
const int GRID     = 10;   // 栅格步长(像素)
const int MAX_BITS = 32;   // 支持的最大数据位宽

// ---------------------------------------------------------------- 元件类型
enum CompType
{
    // 输入/输出
    CT_InputPin = 0, CT_OutputPin, CT_Button, CT_LED, CT_HexDisplay,
    // 门电路
    CT_NOT, CT_AND, CT_OR, CT_NAND, CT_NOR, CT_XOR, CT_XNOR, CT_Buffer,
    // 复用器
    CT_Mux, CT_Demux, CT_Decoder,
    // 运算器
    CT_Adder, CT_Subtractor, CT_Comparator, CT_Shifter, CT_Multiplier,
    // 存储器
    CT_DFlipFlop, CT_Register, CT_Counter, CT_RAM, CT_ROM,
    // 布线
    CT_Clock, CT_Constant, CT_Tunnel, CT_Probe, CT_Splitter, CT_Text,
    // 层次电路
    CT_SubCircuit,
    // ===== 新增元件类型一律追加在这里(CT_COUNT 之前) =====
    // 这样已有类型编号不变，旧工程文件仍然可以正确打开
    CT_TriState,                 // 三态缓冲器(示例：高阻输出)
    CT_Custom,                   // 运行时自定义元件(由工程内的 CustomDef 决定外形与逻辑)
    CT_COUNT
};

// ---------------------------------------------------------------- 元件库
enum LibId
{
    LIB_IO = 0,      // 输入/输出
    LIB_GATES,       // 门
    LIB_PLEXERS,     // 复用器
    LIB_ARITH,       // 运算
    LIB_MEMORY,      // 存储器
    LIB_WIRING,      // 布线
    LIB_PROJECT,     // 项目(自定义子电路)
    LIB_CUSTOM,      // 自定义(运行时元件，可在元件库管理器里随时新增)
    LIB_COUNT
};

// ---------------------------------------------------------------- 朝向
enum Facing { F_East = 0, F_West, F_North, F_South };

// ---------------------------------------------------------------- 工具
enum ToolId
{
    TOOL_EDIT = 0,   // 选择/编辑
    TOOL_POKE,       // 查探(手)
    TOOL_WIRE,       // 布线
    TOOL_TEXT,       // 文本
    TOOL_ERASE,      // 删除
    TOOL_PLACE       // 放置元件(配合 m_placeType)
};

// ---------------------------------------------------------------- 引脚
struct PinInfo
{
    wxPoint  pos;        // 局部坐标(标准朝向: 输出朝东)
    bool     isInput;
    int      bits;
    wxString name;
};

// ---------------------------------------------------------------- 连线
struct Wire
{
    std::vector<wxPoint> pts;
    int  bits     = 1;
    bool selected = false;
};

// ---------------------------------------------------------------- 元件
struct Component
{
    int      id     = 0;
    int      type   = CT_AND;
    wxPoint  pos;                    // 局部原点在电路中的位置
    int      facing = F_East;
    int      bits   = 1;             // 数据位宽
    int      inputs = 2;             // 输入端口数(门电路)
    int      selBits = 1;            // 选择位宽(复用器/译码器)
    int      addrBits = 4;           // 地址位宽(RAM/ROM)
    int      fanout  = 2;            // 分线器输出路数
    int      bitWidth = 1;           // 分线器每路位宽
    int      flags   = 0;            // 杂项选项: 移位方向/触发沿/计数方向
    uint32_t value  = 0;             // 常量值 / 输入引脚当前值 / ROM 初值
    wxString label;                  // 标签(隧道名 / 引脚名 / 元件名)
    wxString text;                   // 文本元件内容 / ROM 数据
    int      sub    = -1;            // 子电路下标
    int      cdef   = -1;            // 自定义元件定义下标(CT_Custom 用)
    bool     selected = false;

    bool IsSequential() const
    {
        return type == CT_DFlipFlop || type == CT_Register || type == CT_Counter;
    }
    bool IsSink() const
    {
        return type == CT_OutputPin || type == CT_LED || type == CT_HexDisplay ||
               type == CT_Probe;
    }
};

// ---------------------------------------------------------------- 逻辑值
// state: 0 = 悬空(未驱动)  1 = 正常  2 = 冲突/错误
struct PinValue
{
    uint32_t bits  = 0;
    int      state = 0;

    bool IsError() const { return state == 2; }
    bool IsFloat() const { return state == 0; }
    bool Bit(int i) const { return ((bits >> i) & 1u) != 0; }
};

// ---------------------------------------------------------------- 元件运行时状态
struct CompState
{
    uint32_t value = 0;      // 寄存器/计数器当前值, 时钟相位
    uint32_t next  = 0;      // 下一个时钟沿要锁存的值
    bool     clkPrev = false;
    bool     clkNow  = false;
    bool     init    = false;
    uint32_t memAddr = 0;    // RAM 采样
    uint32_t memData = 0;
    bool     memWE   = false;
    std::vector<uint32_t> mem;
};

// ---------------------------------------------------------------- 自定义元件
// 运行时定义的元件：图形(图元列表) + 引脚 + 逻辑模板，全部保存在 .mlc 里
enum CustomLogic
{
    CL_PASS = 0,   // 输出0 = 输入0           (直通/缓冲)
    CL_NOT,        // 输出0 = ~输入0
    CL_AND,        // 输出0 = 所有输入相与
    CL_OR,
    CL_XOR,
    CL_NAND,
    CL_NOR,
    CL_XNOR,
    CL_MUX,        // 前 n-1 个输入是数据，最后一个输入是选择端
    CL_DECODER,    // 输入0 是选择端，输出 i = (i == sel)
    CL_CONST,      // 输出0 = 元件的常量值
    CL_NONE        // 纯装饰(不参与逻辑)
};
wxString CustomLogicName(int logic);
int      CustomLogicCount();

enum CustomPrimKind { CP_POLY = 0, CP_RECT, CP_CIRCLE, CP_TEXT };

struct CustomPrim
{
    int  kind = CP_POLY;
    bool fill = false;
    std::vector<wxPoint> pts;    // 折线顶点 / 矩形两角 / 圆心+圆周点 / 文本位置
    wxString text;               // CP_TEXT 用
};

struct CustomPinDef
{
    wxPoint pos;
    bool    isInput = true;
    int     bits = 1;
    wxString name;
};

struct CustomDef
{
    wxString name = wxT("自定义元件");
    wxString desc = wxT("运行时定义的元件");
    int w = 60, h = 60;          // 外形尺寸(局部坐标)
    int logic = CL_PASS;         // 逻辑模板
    int bits  = 1;               // 默认数据位宽
    std::vector<CustomPinDef> pins;
    std::vector<CustomPrim>   prims;
};

// ---------------------------------------------------------------- 属性定义
enum AttrKind { AK_INT = 0, AK_STRING, AK_CHOICE, AK_HEX, AK_READONLY };

struct AttrDef
{
    wxString key;       // 内部键
    wxString name;      // 显示名
    int      kind = AK_INT;
    wxString choices;   // "东|西|北|南"
    int      minv = 0;
    int      maxv = 100000;
    wxString tip;
};

// ---------------------------------------------------------------- 电路
class Circuit
{
public:
    wxString name;
    std::vector<Component> comps;
    std::vector<Wire>      wires;
    int nextId = 1;

    Component* Find(int id);
    void RemoveSelected();
    bool HasSelection() const;
    void ClearSelection();

    /// 覆盖指定点(用于拖动、框选、连线吸附)
    void CollectBounds(wxRect& r) const;
};

// ---------------------------------------------------------------- 文档
class Document
{
public:
    std::vector<Circuit> circuits;
    std::vector<CustomDef> customDefs;   // 运行时自定义元件库(随工程保存)
    int      active  = 0;
    wxString path;
    bool     dirty   = false;
    long     version = 0;      // 结构版本号(供仿真缓存失效)

    Document();

    void     NewDocument();
    Circuit& Active() { return circuits[active]; }
    const Circuit& Active() const { return circuits[active]; }

    void Touch() { ++version; dirty = true; }

    int  FindCircuit(const wxString& n) const;
    int  AddCircuit(const wxString& n);
    bool RemoveCircuit(int idx);

    // ---- 自定义元件库 ----
    const CustomDef* FindCustom(int idx) const
    {
        if (idx < 0 || idx >= (int)customDefs.size()) return nullptr;
        return &customDefs[idx];
    }
    int  AddCustomDef(const CustomDef& d);
    bool RemoveCustomDef(int idx);
    wxString UniqueCustomName(const wxString& base) const;

    /// 该电路的输入/输出端口(输入引脚在前，输出引脚在后)，按位置排序
    std::vector<int> Ports(int circuitIdx) const;

    /// 元件几何(需要文档上下文: 子电路端口数)
    wxSize              Size(const Component& c) const;
    std::vector<PinInfo> Pins(const Component& c) const;
    int                 NetCount(int circuitIdx) const;   // 端口数量

    // 序列化
    wxString Serialize() const;
    bool     Deserialize(const wxString& text, wxString* err);
    bool     SaveTo(const wxString& p, wxString* err);
    bool     LoadFrom(const wxString& p, wxString* err);
};

// ---------------------------------------------------------------- 几何工具
wxPoint LocalToWorld(const Component& c, const wxSize& sz, const wxPoint& p);
wxPoint WorldToLocal(const Component& c, const wxSize& sz, const wxPoint& p);
wxRect  WorldBounds(const Component& c, const wxSize& sz);
bool    PointOnSegment(const wxPoint& p, const wxPoint& a, const wxPoint& b);

// ---------------------------------------------------------------- 名称/分类
wxString TypeName(int type);
wxString TypeDesc(int type);
int      TypeLib(int type);
wxString LibName(int lib);
std::vector<int> LibTools(int lib);            // 空表示动态(项目库)

// ---------------------------------------------------------------- 属性读写
std::vector<AttrDef> GetAttrDefs(const Component& c);
wxString             GetAttrValue(const Component& c, const wxString& key);
bool                 SetAttrValue(Component& c, const wxString& key,
                                 const wxString& val, wxString* err);

/// 新建元件时的默认属性
void InitComponentDefaults(Component& c, int type, const Document* doc);

// ---------------------------------------------------------------- 数值工具
int      ClampInt(int v, int lo, int hi);
uint32_t MaskOf(int bits);
wxString FormatValue(uint32_t v, int bits, bool hex = true);

} // namespace lc
