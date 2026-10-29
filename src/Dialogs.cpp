// ============================================================================
//  Dialogs.cpp  --  对话框实现
// ============================================================================
#include "Dialogs.h"
#include "MainFrame.h"
#include "Simulator.h"

#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/numdlg.h>
#include <wx/textdlg.h>

#include <algorithm>

namespace lc {

// ---------------------------------------------------------------- 属性编辑
bool EditAttrValue(wxWindow* parent, const AttrDef& d, const wxString& cur,
                   Document* doc, wxString* out)
{
    switch (d.kind) {
    case AK_STRING: {
        long style = (d.key == wxT("text")) ? (wxTE_MULTILINE | wxTE_PROCESS_ENTER) : 0;
        wxTextEntryDialog dlg(parent, wxT("请输入") + d.name + wxT(":"), wxT("编辑属性"),
                              cur, style);
        if (d.key == wxT("text")) dlg.SetSize(wxSize(460, 260));
        if (dlg.ShowModal() == wxID_OK) { *out = dlg.GetValue(); return true; }
        return false; }

    case AK_INT: {
        long v = 0;
        cur.ToLong(&v);
        wxNumberEntryDialog dlg(parent, wxEmptyString, d.name + wxT(":"), wxT("编辑属性"),
                                v, d.minv, d.maxv);
        if (dlg.ShowModal() == wxID_OK) {
            *out = wxString::Format(wxT("%ld"), dlg.GetValue());
            return true;
        }
        return false; }

    case AK_HEX: {
        wxTextEntryDialog dlg(parent, wxT("请输入") + d.name + wxT(" (十进制或 0x 十六进制):"),
                              wxT("编辑属性"), cur);
        if (dlg.ShowModal() == wxID_OK) { *out = dlg.GetValue(); return true; }
        return false; }

    case AK_CHOICE: {
        wxArrayString ch;
        if (d.key == wxT("sub")) {
            if (!doc) return false;
            for (size_t i = 0; i < doc->circuits.size(); ++i)
                ch.Add(doc->circuits[i].name);
        } else {
            wxString rest = d.choices;
            while (!rest.empty()) {
                wxString one = rest.BeforeFirst('|');
                rest = rest.AfterFirst('|');
                ch.Add(one);
                if (rest == one) break;
            }
        }
        if (ch.IsEmpty()) return false;
        long curv = 0;
        cur.ToLong(&curv);
        wxSingleChoiceDialog dlg(parent, wxT("请选择") + d.name + wxT(":"), wxT("编辑属性"), ch);
        if (curv >= 0 && curv < (long)ch.GetCount()) dlg.SetSelection((int)curv);
        if (dlg.ShowModal() == wxID_OK) {
            *out = wxString::Format(wxT("%d"), dlg.GetSelection());
            return true;
        }
        return false; }

    default:
        return false;
    }
}

// ---------------------------------------------------------------- 关于
AboutDialog::AboutDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, wxT("关于逻辑电路仿真器"), wxDefaultPosition, wxSize(480, 340))
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

    wxStaticText* title = new wxStaticText(this, wxID_ANY, wxT("逻辑电路仿真器"));
    title->SetFont(wxFont(wxFontInfo(14).Bold().FaceName(wxT("Microsoft YaHei"))));
    root->Add(title, 0, wxALL, 12);

    wxStaticText* body = new wxStaticText(this, wxID_ANY,
        wxT("一个用 C++ / wxWidgets 编写的数字逻辑电路设计与仿真工具，\n")
        wxT("界面与操作方式参考 Logisim 2.7.1。\n\n")
        wxT("· 门电路 / 复用器 / 运算器 / 存储器 / 输入输出 共 30 余种元件\n")
        wxT("· 布线工具、隧道、分线器、探针、数码管显示\n")
        wxT("· 实时逻辑仿真：组合逻辑迭代收敛 + 时钟节拍驱动时序元件\n")
        wxT("· 支持多张电路图与子电路层次化设计\n")
        wxT("· 撤销 / 重做、复制粘贴、属性编辑、真值表分析\n\n")
        wxT("构建环境: Visual Studio 2022 (v143, x64) + wxWidgets 3.2.2.1\n")
        wxT("界面框架: wxWidgets 3.2.2.1 (MSW, Unicode, 静态库)"));
    body->SetFont(wxFont(wxFontInfo(9).FaceName(wxT("Microsoft YaHei"))));
    root->Add(body, 1, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    wxButton* ok = new wxButton(this, wxID_OK, wxT("确定"));
    root->Add(ok, 0, wxALIGN_CENTER | wxALL, 10);
    SetSizer(root);
    CentreOnParent();
}

// ---------------------------------------------------------------- 帮助
HelpDialog::HelpDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, wxT("使用说明 / 快捷键"), wxDefaultPosition, wxSize(560, 460))
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    wxTextCtrl* txt = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                     wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    txt->SetFont(wxFont(wxFontInfo(9).FaceName(wxT("Microsoft YaHei"))));
    txt->SetValue(
        wxT("【基本流程】\n")
        wxT("1. 在左侧“元件”面板选择一个库，再选一个元件，然后在画布上点击放置。\n")
        wxT("2. 用“布线”工具把引脚连起来（点击落点，双击 / 右键 / Esc 结束一条导线）。\n")
        wxT("3. 用“查探”工具点击输入引脚翻转电平，或选中时钟后按 Ctrl+T 单步仿真。\n\n")
        wxT("【工具】\n")
        wxT("  选择/编辑：单击选中，拖动移动，空白处拖动框选，双击元件编辑属性\n")
        wxT("  查探(手)：点击输入引脚翻转电平；Shift+点击多位引脚按位循环；按住按钮=1\n")
        wxT("  布线：逐点点击，自动吸附栅格与引脚；双击 / 右键 / Esc 结束\n")
        wxT("  文本：放置文字注释\n")
        wxT("  删除：点击元件或导线直接删除\n\n")
        wxT("【快捷键】\n")
        wxT("  Ctrl+N/O/S        新建 / 打开 / 保存\n")
        wxT("  Ctrl+Z / Ctrl+Y   撤销 / 重做\n")
        wxT("  Ctrl+X/C/V        剪切 / 复制 / 粘贴\n")
        wxT("  Delete            删除选中\n")
        wxT("  Ctrl+A            全选\n")
        wxT("  方向键            移动选中 10 像素 (Ctrl+方向键 = 1 像素)\n")
        wxT("  R                 顺时针旋转选中元件\n")
        wxT("  Ctrl+= / Ctrl+-   放大 / 缩小，Ctrl+0 适应窗口，Ctrl+1 实际大小\n")
        wxT("  Ctrl+E            启用 / 暂停仿真\n")
        wxT("  Ctrl+T            单步一个时钟节拍\n")
        wxT("  Ctrl+R            复位仿真\n")
        wxT("  鼠标中键拖动      平移画布；滚轮滚动，Ctrl+滚轮缩放\n\n")
        wxT("【颜色约定】\n")
        wxT("  亮绿 = 逻辑 1    暗绿 = 逻辑 0\n")
        wxT("  蓝色 = 悬空(未连接)   红色 = 冲突 / 错误 / 振荡\n\n")
        wxT("【说明】\n")
        wxT("  仿真以当前打开的电路图为根，其中的子电路元件会被展开参与仿真。\n")
        wxT("  打开子电路图页时，该图作为独立根进行仿真，便于单独调试。\n"));
    root->Add(txt, 1, wxEXPAND | wxALL, 8);
    wxButton* ok = new wxButton(this, wxID_OK, wxT("关闭"));
    root->Add(ok, 0, wxALIGN_CENTER | wxALL, 8);
    SetSizer(root);
    CentreOnParent();
}

// ---------------------------------------------------------------- 真值表
static wxString BitString(uint32_t v, int bits)
{
    wxString s;
    for (int i = bits - 1; i >= 0; --i)
        s += ((v >> i) & 1u) ? wxT("1") : wxT("0");
    return s;
}

TruthTableDialog::TruthTableDialog(wxWindow* parent, MainFrame* frame)
    : wxDialog(parent, wxID_ANY, wxT("组合逻辑真值表"), wxDefaultPosition, wxSize(680, 520),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    wxListCtrl* list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxLC_REPORT | wxLC_SINGLE_SEL);
    root->Add(list, 1, wxEXPAND | wxALL, 8);
    wxBoxSizer* bs = new wxBoxSizer(wxHORIZONTAL);
    bs->AddStretchSpacer();
    bs->Add(new wxButton(this, wxID_OK, wxT("关闭")), 0);
    root->Add(bs, 0, wxEXPAND | wxALL, 8);
    SetSizer(root);

    Document* doc = frame->Doc();
    Simulator* sim = frame->Sim();
    Circuit& c = doc->Active();

    std::vector<int> ins, outs;
    for (size_t i = 0; i < c.comps.size(); ++i) {
        if (c.comps[i].type == CT_InputPin)       ins.push_back((int)i);
        else if (c.comps[i].type == CT_OutputPin) outs.push_back((int)i);
    }
    auto cmp = [&](int a, int b) {
        if (c.comps[a].pos.y != c.comps[b].pos.y) return c.comps[a].pos.y < c.comps[b].pos.y;
        return c.comps[a].pos.x < c.comps[b].pos.x;
    };
    std::sort(ins.begin(), ins.end(), cmp);
    std::sort(outs.begin(), outs.end(), cmp);

    if (ins.empty() || ins.size() > 8) {
        list->InsertColumn(0, wxT("提示"), wxLIST_FORMAT_LEFT, 600);
        list->InsertItem(0, ins.empty() ? wxT("当前电路没有输入引脚，无法生成真值表。")
                                        : wxT("输入引脚超过 8 个，组合过多，未生成真值表。"));
        CentreOnParent();
        return;
    }

    int col = 0;
    for (size_t i = 0; i < ins.size(); ++i) {
        wxString nm = c.comps[ins[i]].label;
        if (nm.empty()) nm = wxString::Format(wxT("in%d"), (int)i + 1);
        list->InsertColumn(col++, nm, wxLIST_FORMAT_CENTER, 70);
    }
    for (size_t i = 0; i < outs.size(); ++i) {
        wxString nm = c.comps[outs[i]].label;
        if (nm.empty()) nm = wxString::Format(wxT("out%d"), (int)i + 1);
        list->InsertColumn(col++, nm, wxLIST_FORMAT_CENTER, 70);
    }

    std::vector<uint32_t> save;
    for (size_t i = 0; i < ins.size(); ++i) save.push_back(c.comps[ins[i]].value);

    const int n = (int)ins.size();
    const uint32_t total = 1u << n;
    for (uint32_t k = 0; k < total; ++k) {
        for (int i = 0; i < n; ++i) {
            c.comps[ins[i]].value = ((k >> (n - 1 - i)) & 1u) ? 1u : 0u;
        }
        doc->Touch();
        sim->Settle();
        long row = list->InsertItem((int)k, BitString(k, n));
        for (size_t o = 0; o < outs.size(); ++o) {
            PinValue v = sim->PinOf(c.comps[outs[o]], 0);
            wxString s;
            if (v.state == 0) s = wxT("Z");
            else if (v.state == 2) s = wxT("E");
            else s = wxString::Format(wxT("%u"), (unsigned)(v.bits & 1u));
            list->SetItem(row, (int)o + n, s);
        }
    }

    for (size_t i = 0; i < ins.size(); ++i) c.comps[ins[i]].value = save[i];
    doc->Touch();
    sim->Settle();
    frame->RefreshAll();

    CentreOnParent();
}

} // namespace lc
