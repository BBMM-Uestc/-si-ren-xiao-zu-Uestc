// ============================================================================
//  Ids.h  --  命令 ID 集中定义
// ============================================================================
#pragma once

#include <wx/defs.h>

namespace lc {

enum CmdId
{
    ID_New = wxID_HIGHEST + 1,
    ID_Open, ID_Save, ID_SaveAs, ID_ExportPng, ID_Quit, ID_RecentBase = ID_Quit + 40,

    ID_Undo, ID_Redo, ID_Cut, ID_Copy, ID_Paste, ID_Delete, ID_SelectAll, ID_Deselect,
    ID_RotateCW, ID_RotateCCW, ID_FlipH, ID_EditAttrs, ID_Duplicate,

    ID_AddCircuit, ID_RenameCircuit, ID_RemoveCircuit, ID_MoveUp, ID_MoveDown,
    ID_OpenCircuit, ID_TruthTable,

    ID_SimEnable, ID_TickOnce, ID_ResetSim, ID_FreqBase, ID_FreqEnd = ID_FreqBase + 12,

    ID_ZoomIn, ID_ZoomOut, ID_ZoomFit, ID_Zoom100, ID_ShowGrid, ID_ShowWireValue,

    ID_ToolEdit, ID_ToolPoke, ID_ToolWire, ID_ToolText, ID_ToolErase,
    ID_LibBase, ID_LibEnd = ID_LibBase + 16,
    ID_ToolBase, ID_ToolEnd = ID_ToolBase + 64,

    ID_About, ID_Help, ID_Tips,

    ID_Canvas = wxID_HIGHEST + 900
};

} // namespace lc
