// ============================================================================
//  Dialogs.h  --  对话框集合
// ============================================================================
#pragma once

#include "LogicModel.h"

#include <wx/wx.h>
#include <wx/dialog.h>

namespace lc {

class MainFrame;

/// 编辑单个属性；返回 true 表示 out 中有新值
bool EditAttrValue(wxWindow* parent, const AttrDef& d, const wxString& cur,
                   Document* doc, wxString* out);

/// 关于对话框
class AboutDialog : public wxDialog
{
public:
    explicit AboutDialog(wxWindow* parent);
};

/// 组合逻辑真值表
class TruthTableDialog : public wxDialog
{
public:
    TruthTableDialog(wxWindow* parent, MainFrame* frame);
};

/// 快捷键说明
class HelpDialog : public wxDialog
{
public:
    explicit HelpDialog(wxWindow* parent);
};

} // namespace lc
