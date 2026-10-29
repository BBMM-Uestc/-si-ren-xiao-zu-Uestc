#pragma once

#include <wx/wx.h>
#include <wx/app.h>
#include <wx/cmdline.h>

namespace lc {

class MainFrame;

/// 应用程序对象
class App : public wxApp
{
public:
    bool OnInit() override;
    int  OnExit() override;

    void OnInitCmdLine(wxCmdLineParser& parser) override;
    bool OnCmdLineParsed(wxCmdLineParser& parser) override;

    /// 便捷：取得唯一的主窗口
    static MainFrame* Frame();

private:
    MainFrame* m_frame = nullptr;
    wxString   m_fileToOpen;
    wxString   m_exportPng;      // --export=<file.png>  无界面导出
    long       m_exportTicks = 0; // --ticks=<n>        导出前先仿真的节拍数
    long       m_exportCircuit = 0;
};

} // namespace lc
