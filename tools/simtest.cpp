// ============================================================================
//  simtest.cpp  --  仿真内核无界面自测程序
//  编译: msbuild tools\SimTest.vcxproj /p:Configuration=Debug /p:Platform=x64
// ============================================================================
#include "../src/LogicModel.h"
#include "../src/Simulator.h"

#include <wx/init.h>
#include <wx/string.h>

#include <cstdio>

using namespace lc;

static int g_fail = 0;

static void Check(const char* what, long got, long want)
{
    bool ok = (got == want);
    if (!ok) ++g_fail;
    std::printf("  [%s] %-46s got=%-6ld want=%ld\n", ok ? "OK" : "FAIL", what, got, want);
}

static int Comp(Document& d, int circ, int type, int x, int y)
{
    Component c;
    InitComponentDefaults(c, type, &d);
    c.pos = wxPoint(x, y);
    c.id = d.circuits[circ].nextId++;
    d.circuits[circ].comps.push_back(c);
    return c.id;
}

static Component* Find(Document& d, int circ, int id)
{
    return d.circuits[circ].Find(id);
}

/// 按引脚名取值(避免记错引脚序号)
static PinValue Val(Document& d, Simulator& sim, int circ, int id, const char* name)
{
    Component* c = Find(d, circ, id);
    if (!c) return PinValue();
    std::vector<PinInfo> pins = d.Pins(*c);
    wxString want = wxString::FromUTF8(name);
    for (size_t i = 0; i < pins.size(); ++i)
        if (pins[i].name == want) return sim.PinOf(*c, (int)i);
    return PinValue();
}

static void AddWire(Document& d, int circ, const char* pts)
{
    Wire w;
    wxString s = wxString::FromUTF8(pts);
    wxArrayString parts = wxSplit(s, ';');
    for (size_t i = 0; i < parts.GetCount(); ++i) {
        long x = 0, y = 0;
        wxString a = parts[i].BeforeFirst(',');
        wxString b = parts[i].AfterFirst(',');
        a.ToLong(&x);
        b.ToLong(&y);
        w.pts.push_back(wxPoint((int)x, (int)y));
    }
    d.circuits[circ].wires.push_back(w);
}

// ------------------------------------------------------------------ 测试
static void TestGates()
{
    std::printf("\n== 组合逻辑: 与门/或门/非门 ==\n");
    Document d;
    int A = Comp(d, 0, CT_InputPin, 100, 100);
    int B = Comp(d, 0, CT_InputPin, 100, 200);
    int G = Comp(d, 0, CT_AND,    200, 100);   // 输入 (200,110) (200,130) 输出 (260,120)
    int N = Comp(d, 0, CT_NOT,    200, 200);   // 输入 (200,220) 输出 (250,220)
    int O = Comp(d, 0, CT_OR,     320, 100);   // 输入 (320,110) (320,130) 输出 (380,120)
    int P = Comp(d, 0, CT_OutputPin, 460, 110);
    AddWire(d, 0, "130,110;160,110;160,110;200,110");
    AddWire(d, 0, "130,210;150,210;150,130;200,130");
    AddWire(d, 0, "260,120;280,120;280,110;320,110");
    AddWire(d, 0, "130,210;140,210;140,220;200,220");
    AddWire(d, 0, "250,220;280,220;280,130;320,130");
    AddWire(d, 0, "380,120;460,120");

    Simulator sim;
    sim.Attach(&d);
    // 电路功能: Out = AND(A,B) OR NOT(B)
    //   A B    AND  NOT(B)  Out
    //   0 0     0     1      1
    //   1 0     0     1      1
    //   0 1     0     0      0
    //   1 1     1     0      1
    struct Case { uint32_t a, b; uint32_t wantOut; };
    Case cs[4] = { {0,0,1}, {1,0,1}, {0,1,0}, {1,1,1} };
    for (int i = 0; i < 4; ++i) {
        Find(d, 0, A)->value = cs[i].a;
        Find(d, 0, B)->value = cs[i].b;
        d.Touch();
        sim.Settle();
        PinValue v = Val(d, sim, 0, P, "I");
        char buf[64];
        std::snprintf(buf, sizeof(buf), "A=%u B=%u -> Out", cs[i].a, cs[i].b);
        Check(buf, (long)v.bits, (long)cs[i].wantOut);
    }
}

static void TestAdder()
{
    std::printf("\n== 一位全加器 ==\n");
    Document d;
    int A  = Comp(d, 0, CT_InputPin, 100, 100);
    int B  = Comp(d, 0, CT_InputPin, 100, 200);
    int C  = Comp(d, 0, CT_InputPin, 100, 300);
    Comp(d, 0, CT_XOR, 200, 120);   // X1: (200,130)(200,150) -> (260,140)
    Comp(d, 0, CT_XOR, 340, 220);   // X2: (340,230)(340,250) -> (400,240)
    Comp(d, 0, CT_AND, 200, 260);   // A1: (200,270)(200,290) -> (260,280)
    Comp(d, 0, CT_AND, 340, 340);   // A2: (340,350)(340,370) -> (400,360)
    Comp(d, 0, CT_OR,  480, 300);   // O1: (480,310)(480,330) -> (540,320)
    int S = Comp(d, 0, CT_OutputPin, 480, 220);
    int O = Comp(d, 0, CT_OutputPin, 620, 310);
    AddWire(d, 0, "130,110;160,110;160,130;200,130");
    AddWire(d, 0, "130,210;160,210;160,150;200,150");
    AddWire(d, 0, "130,210;140,210;140,290;200,290");
    AddWire(d, 0, "130,110;150,110;150,270;200,270");
    AddWire(d, 0, "130,310;180,310;180,250;340,250");
    AddWire(d, 0, "260,140;300,140;300,230;340,230");
    AddWire(d, 0, "260,140;280,140;280,350;340,350");
    AddWire(d, 0, "130,310;140,310;140,370;340,370");
    AddWire(d, 0, "260,280;440,280;440,310;480,310");
    AddWire(d, 0, "400,360;440,360;440,330;480,330");
    AddWire(d, 0, "400,240;440,240;440,230;480,230");
    AddWire(d, 0, "540,320;620,320");

    Simulator sim;
    sim.Attach(&d);
    for (int a = 0; a < 2; ++a)
        for (int b = 0; b < 2; ++b)
            for (int c = 0; c < 2; ++c) {
                Find(d, 0, A)->value = a;
                Find(d, 0, B)->value = b;
                Find(d, 0, C)->value = c;
                d.Touch();
                sim.Settle();
                int sum  = (int)Val(d, sim, 0, S, "I").bits;
                int cout = (int)Val(d, sim, 0, O, "I").bits;
                int want = a + b + c;
                char buf[96];
                std::snprintf(buf, sizeof(buf), "A=%d B=%d Cin=%d -> S,Cout", a, b, c);
                Check(buf, sum * 10 + cout, (want & 1) * 10 + (want >> 1));
            }
}

static void TestClockCounter()
{
    std::printf("\n== 时钟 + 计数器 ==\n");
    Document d;
    Comp(d, 0, CT_Clock,   100, 100);   // 输出 (140,120)
    int CNT = Comp(d, 0, CT_Counter, 200, 100);  // clk(200,120) clr(200,150) Q(280,120)
    Find(d, 0, CNT)->bits = 4;
    AddWire(d, 0, "140,120;200,120");

    Simulator sim;
    sim.Attach(&d);
    // 时钟初始为低: 第 1 个节拍就是第一个上升沿 -> 计数 1; 之后每 2 个节拍加 1
    for (int t = 1; t <= 10; ++t) {
        sim.Tick();
        int v = (int)Val(d, sim, 0, CNT, "Q").bits;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "第 %d 个节拍后计数器", t);
        Check(buf, v, (t + 1) / 2);
    }
}

static void TestMultiBit()
{
    std::printf("\n== 多位宽 + 分线器 + 隧道 ==\n");
    Document d;
    int A = Comp(d, 0, CT_InputPin, 100, 100);
    Find(d, 0, A)->bits = 8;
    Find(d, 0, A)->value = 0xB4;      // 1011 0100
    int SP = Comp(d, 0, CT_Splitter, 200, 100);
    Find(d, 0, SP)->fanout = 2;
    Find(d, 0, SP)->bitWidth = 4;     // in(200,120)  out(230,110) out(230,130)
    int P0 = Comp(d, 0, CT_OutputPin, 320, 100);
    int P1 = Comp(d, 0, CT_OutputPin, 320, 120);
    Find(d, 0, P0)->bits = 4;
    Find(d, 0, P1)->bits = 4;
    AddWire(d, 0, "130,110;160,110;160,120;200,120");   // A -> splitter 输入
    AddWire(d, 0, "230,110;320,110");
    AddWire(d, 0, "230,130;320,130");

    Simulator sim;
    sim.Attach(&d);
    sim.Settle();
    Check("高 4 位 (11)", (long)Val(d, sim, 0, P0, "I").bits, 0xB);
    Check("低 4 位 (4)",  (long)Val(d, sim, 0, P1, "I").bits, 0x4);

    // 隧道连通性
    Document t;
    int T1 = Comp(t, 0, CT_InputPin, 100, 100);
    int TA = Comp(t, 0, CT_Tunnel,   200, 120);
    int TB = Comp(t, 0, CT_Tunnel,   400, 300);
    int TP = Comp(t, 0, CT_OutputPin, 500, 290);
    Find(t, 0, TA)->label = wxT("bus");
    Find(t, 0, TB)->label = wxT("bus");
    AddWire(t, 0, "130,110;180,110;180,130;200,130");
    AddWire(t, 0, "400,310;460,310;460,300;500,300");
    Simulator sim2;
    sim2.Attach(&t);
    Find(t, 0, T1)->value = 1;
    t.Touch();
    sim2.Settle();
    Check("隧道同名连通", (long)Val(t, sim2, 0, TP, "I").bits, 1);
}

static void TestSequentialAndRam()
{
    std::printf("\n== D 触发器 / 寄存器 / RAM ==\n");
    Document d;
    Comp(d, 0, CT_Clock, 100, 100);              // out (140,120)
    int D = Comp(d, 0, CT_InputPin, 100, 300);   // out (130,310)
    int FF = Comp(d, 0, CT_DFlipFlop, 200, 100); // D(200,120) clk(200,140) Q(260,120) Q'(260,140)
    Comp(d, 0, CT_OutputPin, 340, 110);
    AddWire(d, 0, "140,120;170,120;170,140;200,140");     // 时钟 -> clk
    AddWire(d, 0, "130,310;180,310;180,120;200,120");     // 数据 -> D
    AddWire(d, 0, "260,120;340,120");                      // Q -> 输出引脚

    Simulator sim;
    sim.Attach(&d);
    Find(d, 0, D)->value = 1;
    d.Touch();
    sim.Settle();
    Check("时钟为低时 Q 保持 0", (long)Val(d, sim, 0, FF, "Q").bits, 0);
    sim.Tick();    // 时钟 0->1 上升沿，锁存 D=1
    Check("上升沿后 Q = 1", (long)Val(d, sim, 0, FF, "Q").bits, 1);
    Find(d, 0, D)->value = 0;
    d.Touch();
    sim.Settle();
    Check("D 变 0 但未到时钟沿 Q 仍为 1", (long)Val(d, sim, 0, FF, "Q").bits, 1);
    sim.Tick();    // 1->0 下降沿
    sim.Tick();    // 0->1 上升沿
    Check("下一个上升沿后 Q = 0", (long)Val(d, sim, 0, FF, "Q").bits, 0);
}

static void TestSubCircuit()
{
    std::printf("\n== 子电路层次 ==\n");
    Document d;
    d.AddCircuit(wxT("inv"));           // 电路 1
    // 电路 1: 输入引脚 in -> 非门 -> 输出引脚 out
    int i1 = Comp(d, 1, CT_InputPin, 100, 100);
    Comp(d, 1, CT_NOT, 200, 100);
    int o1 = Comp(d, 1, CT_OutputPin, 350, 110);
    AddWire(d, 1, "130,110;160,110;160,120;200,120");
    AddWire(d, 1, "250,120;350,120");

    // 电路 0: 输入引脚 -> 子电路 -> 输出引脚
    int A = Comp(d, 0, CT_InputPin, 100, 100);
    Component sub;
    InitComponentDefaults(sub, CT_SubCircuit, &d);
    sub.sub = 1;
    sub.pos = wxPoint(300, 100);        // 端口 h=60: in(300,130) out(380,130)
    sub.id = d.circuits[0].nextId++;
    d.circuits[0].comps.push_back(sub);
    int P = Comp(d, 0, CT_OutputPin, 500, 120);
    AddWire(d, 0, "130,110;160,110;160,130;300,130");
    AddWire(d, 0, "380,130;500,130");

    Simulator sim;
    sim.Attach(&d);
    Find(d, 1, i1);
    Find(d, 0, A)->value = 0;
    d.Touch();
    sim.Settle();
    Check("子电路 反相器(0) = 1", (long)Val(d, sim, 0, P, "I").bits, 1);
    Find(d, 0, A)->value = 1;
    d.Touch();
    sim.Settle();
    Check("子电路 反相器(1) = 0", (long)Val(d, sim, 0, P, "I").bits, 0);
    (void)o1;
}

static void TestSerialization()
{
    std::printf("\n== 存盘 / 读盘 ==\n");
    Document d;
    int A = Comp(d, 0, CT_InputPin, 100, 100);
    Find(d, 0, A)->label = wxT("输入甲");
    Comp(d, 0, CT_AND, 200, 100);
    AddWire(d, 0, "130,110;160,110;160,110;200,110");
    wxString text = d.Serialize();

    Document d2;
    wxString err;
    bool ok = d2.Deserialize(text, &err);
    Check("反序列化成功", ok ? 1 : 0, 1);
    Check("元件数量", (long)d2.circuits[0].comps.size(), 2);
    Check("导线数量", (long)d2.circuits[0].wires.size(), 1);
    Check("中文标签保留",
          d2.circuits[0].comps[0].label == wxT("输入甲") ? 1 : 0, 1);
    Check("坐标保留", (long)d2.circuits[0].comps[1].pos.x, 200);
}

int main(int argc, char** argv)
{
    wxInitializer init(argc, argv);
    if (!init.IsOk()) {
        std::printf("wxWidgets 初始化失败\n");
        return 2;
    }
    std::printf("MiniLogisim 仿真内核自测\n=========================\n");
    TestGates();
    TestAdder();
    TestClockCounter();
    TestMultiBit();
    TestSequentialAndRam();
    TestSubCircuit();
    TestSerialization();
    std::printf("\n=========================\n%s (失败 %d 项)\n",
                g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
