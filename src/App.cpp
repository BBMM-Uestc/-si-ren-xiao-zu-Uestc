#include "App.h"
#include "MainFrame.h"
#include "Renderer.h"
#include "Simulator.h"

#include <wx/cmdline.h>
#include <wx/debug.h>
#include <wx/image.h>
#include <wx/log.h>

#include <cstdio>

wxIMPLEMENT_APP(lc::App);

namespace lc {

// 调试用：设置环境变量 MINILOGISIM_ASSERTLOG=<文件> 后，
// 断言与日志会写入该文件而不是弹出对话框（便于自动化测试）。
static void MiniAssertHandler(const wxString& file, int line, const wxString& func,
                              const wxString& cond, const wxString& msg)
{
    wxString fn = wxGetenv(wxT("MINILOGISIM_ASSERTLOG"));
    if (fn.empty()) return;
    FILE* f = wxFopen(fn, wxT("a"));
    if (!f) return;
    wxString s = wxString::Format(
        wxT("ASSERT %s:%d\n  func: %s\n  cond: %s\n  msg : %s\n\n"),
        file, line, func, cond, msg);
    const wxScopedCharBuffer utf8 = s.utf8_str();
    fputs(utf8.data(), f);
    fclose(f);
}

bool App::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    SetAppName(wxT("逻辑电路仿真器"));
    SetVendorName(wxT("逻辑电路仿真器"));
    wxInitAllImageHandlers();

    const wxString assertLog = wxGetenv(wxT("MINILOGISIM_ASSERTLOG"));
    if (!assertLog.empty()) {
        wxSetAssertHandler(&MiniAssertHandler);
        FILE* lf = wxFopen(assertLog, wxT("w"));
        if (lf) wxLog::SetActiveTarget(new wxLogStderr(lf));
    }

    // 无界面模式: --export=xxx.png [--ticks=n] [--circuit=i] 电路.mlc
    if (!m_exportPng.empty()) {
        if (m_fileToOpen.empty()) {
            std::fprintf(stderr, "--export 需要同时给出电路文件\n");
            return false;
        }
        Document doc;
        wxString err;
        if (!doc.LoadFrom(m_fileToOpen, &err)) {
            std::fprintf(stderr, "打开失败: %s\n", (const char*)err.utf8_str());
            return false;
        }
        Simulator sim;
        sim.Attach(&doc);
        doc.active = ClampInt((int)m_exportCircuit, 0, (int)doc.circuits.size() - 1);
        sim.SetRoot(doc.active);
        sim.Invalidate();
        sim.Settle();
        for (long i = 0; i < m_exportTicks; ++i)
            sim.Tick();
        wxBitmap bmp = Renderer::RenderCircuit(&doc, &sim, doc.active, 30);
        wxImage img = bmp.ConvertToImage();
        if (!img.SaveFile(m_exportPng, wxBITMAP_TYPE_PNG)) {
            std::fprintf(stderr, "写入 PNG 失败: %s\n", (const char*)m_exportPng.utf8_str());
            return false;
        }
        std::fprintf(stdout, "已导出 %s (%d x %d), 仿真 %ld 个节拍\n",
                     (const char*)m_exportPng.utf8_str(), img.GetWidth(), img.GetHeight(),
                     m_exportTicks);
        return false;   // 不进入图形界面
    }

    m_frame = new MainFrame();
    m_frame->Show(true);
    SetTopWindow(m_frame);

    // 命令行里带文件名则直接打开
    if (!m_fileToOpen.empty() && wxFileExists(m_fileToOpen))
        m_frame->OpenFile(m_fileToOpen);

    return true;
}

void App::OnInitCmdLine(wxCmdLineParser& parser)
{
    wxApp::OnInitCmdLine(parser);
    parser.AddOption(wxT("e"), wxT("export"), wxT("把电路导出为 PNG 图片后退出(无需界面)"),
                     wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
    parser.AddOption(wxT("t"), wxT("ticks"), wxT("导出前先仿真的时钟节拍数"),
                     wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL);
    parser.AddOption(wxT("c"), wxT("circuit"), wxT("要导出的电路序号(从 0 开始)"),
                     wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL);
    parser.AddParam(wxT("电路文件"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
}

bool App::OnCmdLineParsed(wxCmdLineParser& parser)
{
    if (!wxApp::OnCmdLineParsed(parser))
        return false;
    if (parser.GetParamCount() > 0)
        m_fileToOpen = parser.GetParam(0);
    parser.Found(wxT("export"), &m_exportPng);
    parser.Found(wxT("ticks"), &m_exportTicks);
    parser.Found(wxT("circuit"), &m_exportCircuit);
    return true;
}

int App::OnExit()
{
    return wxApp::OnExit();
}

MainFrame* App::Frame()
{
    return static_cast<MainFrame*>(wxTheApp->GetTopWindow());
}

} // namespace lc
