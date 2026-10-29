# 四人小组分工与 Git 规范

> 项目：逻辑电路仿真器（C++17 + wxWidgets 3.2.2.1 + VS2022）
> 规模：21 个源文件 / 6688 行 + 292 行自测 + 5 份文档

---

## 一、四块划分（按"变化的理由"分，不是按行数硬切）

| 板块 | 负责文件 | 行数 | 职责一句话 | 技术难点 |
|---|---|---|---|---|
| **A 数据模型与工程文件** | `src/LogicModel.h/.cpp` | 1349 | 定义"电路长什么样"：35 种元件的尺寸/引脚/属性、`.mlc` 存盘读盘 | 引脚顺序即仿真语义；向前兼容 |
| **B 仿真内核与质量** | `src/Simulator.h/.cpp`<br>`tools/simtest.cpp`、`tools/SimTest.vcxproj` | 1051 | 把电路算出来：网络归并、迭代求值、时钟节拍、36 项自测 | 算法最难：并查集、收敛、边沿触发 |
| **C 渲染与画布交互** | `src/Renderer.h/.cpp`<br>`src/CanvasPanel.h/.cpp` | 1896 | 画出来 + 点得动：矢量绘制、坐标变换、布线/框选/命中测试 | 坐标互逆、交互状态机 |
| **D 界面框架与面板** | `src/MainFrame.h/.cpp`、`App.h/.cpp`<br>`ToolBoxPanel.*`、`SidePanel.*`<br>`Dialogs.*`、`CustomLibDialog.*`、`Ids.h` | 2684 | 组装：菜单/工具条/状态栏、元件库面板、属性表、撤销栈、文件操作 | 文件多但模式重复 |

**注意**：行数 ≠ 工作量。D 的文件多但大多是 wx 控件装配（熟了就快）；B 行数最少但**算法最难**，调试时间最长。

### 每个人负责的核心函数（评审时看这些）

| 板块 | 必须讲清楚的函数 |
|---|---|
| A | `Document::Size()` / `Pins()` / `Serialize()` / `Deserialize()` / `PointOnSegment()` |
| B | `Simulator::Rebuild()` / `Evaluate()` / `Settle()` / `Tick()` |
| C | `Renderer::DrawComponent()` / `CanvasPanel::OnPaint()` / `ToWorld()` / `ComputePlacePos()` |
| D | `MainFrame::PushUndo()` / `SetTool()` / `MainFrame::OnTimer()`、`SidePanel::RefreshAttrs()` |

---

## 二、三处"公共契约"（改之前必须打招呼）

模块之间靠这三个文件衔接，**它们不是任何一个人的私产**：

| 契约 | 谁能改 | 规则 |
|---|---|---|
| `LogicModel.h` 的结构定义（`Component` / `Wire` / `Circuit` / `Document` / `PinInfo`） | 任何人，但要走 PR | 加字段必须同步：A 的引脚/序列化 + B 的求值 + C 的绘制；**只能追加、不能改既有字段含义** |
| `LogicModel.h` 的 `enum CompType` | 任何人 | **只能追加在 `CT_COUNT` 之前**，否则旧 `.mlc` 全部失效 |
| `Ids.h` | C、D | 新增命令 ID 追加即可，不要重排 |

> **好消息**：`.vcxproj` 用的是 `src\*.cpp` 通配，所以**新增源文件不用改工程文件**——这消除了一大类冲突。

---

## 三、Git 规范

### 3.1 仓库结构

整个工程目录就是仓库根，一个仓库四个板块（不拆成 4 个 repo，因为四块互相依赖极强）。

```
logic-sim/                  ← repo 根
├── MiniLogisim.sln
├── MiniLogisim.vcxproj
├── build.bat
├── .gitignore              ← 已提供
├── src/                    ← 21 个源文件
├── tools/                  ← 自测程序
├── examples/               ← 示例工程（.mlc）
├── docs/                   ← 文档与截图
└── res/app.rc
```

`bin/`、`obj/`、`.vs/`、`*.user`、`*.pdb` **不进仓库**（已在 `.gitignore` 里）。

### 3.2 分支模型（简化版 Git Flow）

```
main            ← 永远可编译、可运行、可演示，受保护
 ├── feat/a-model      (A)
 ├── feat/b-sim        (B)
 ├── feat/c-canvas     (C)
 └── feat/d-ui         (D)
```

规则：

1. `main` 上**不允许直接 push**，只能通过 PR（Pull Request）合并
2. 个人分支名：`feat/<板块>-<一句话>`，例如 `feat/b-edge-trigger`
3. **小步提交**：一次提交只做一件事，别攒一整天
4. **每天至少与 main 同步一次**：`git pull --rebase origin main`（避免长期分叉）
5. 合并前：本地 `F7` 编译通过 + `bin\Debug\SimTest.exe` 全过
6. PR 至少 **1 人 review**；涉及 `LogicModel.h` 的结构改动**要 2 人**

**分支已建好**（都从 `main` 拉出，clone 后直接可切）：

| 分支 | 板块 |
|---|---|
| `feat/a-model` | A 数据模型与工程文件 |
| `feat/b-sim` | B 仿真内核与质量 |
| `feat/c-canvas` | C 渲染与画布交互 |
| `feat/d-ui` | D 界面框架与面板 |

`git switch feat/a-model` 会自动建立本地分支并跟踪 `origin/feat/a-model`。

`.github/CODEOWNERS` 里已放好映射模板：把 `@A-user` 等换成各自的 GitHub 用户名、去掉行首 `#`，
之后 PR 改动某个板块的文件时 GitHub 会自动指派对应 reviewer。

### 3.3 提交信息格式
```
<类型>(<板块>): <做了什么>

例：
feat(B): 计数器支持下降沿触发
fix(C): 修正放置虚影不跟随光标
docs(A): 补充 .mlc 文件格式说明
refactor(D): 属性表改为按 id 定位元件
test(B): 增加分线器位段用例
```

类型只用这六个：`feat / fix / docs / refactor / test / chore`。

### 3.4 版本标签

每次能完整演示的状态打一个 tag：

```bash
git tag -a v0.1 -m "首次可运行版本：全加器+计数器+子电路"
git push origin v0.1
```

---

## 四、首次上手命令（每人一次）

```bash
# 1. 克隆（把 <url> 换成你们仓库地址）
git clone <url> logic-sim
cd logic-sim

# 2. 配置身份（提交记录里显示的名字）
git config user.name  "你的名字"
git config user.email "you@example.com"

# 3. 切到自己的板块分支（四个分支仓库里已经建好，clone 下来直接切）
git switch feat/a-model        # A 数据模型
#  git switch feat/b-sim        B 仿真内核
#  git switch feat/c-canvas     C 渲染与画布
#  git switch feat/d-ui         D 界面框架

# 4. 日常循环
git add -A
git commit -m "feat(A): 增加三态缓冲器引脚布局"
git pull --rebase origin main
git push -u origin feat/a-model
# 然后在网页上开 PR → 别人 review → 合并到 main
```

> 用 VS2022 自带的 Git 也可以：`Git → 新建分支 / 提交 / 拉取`，命令等价。

---

## 五、协作注意事项（都是真实会踩的）

| 注意 | 原因 |
|---|---|
| **不要在仓库里提交 `bin/`、`obj/`** | exe 13MB + pdb 34MB，仓库会爆炸 |
| **不要各自新建 VS 工程再往里拷文件** | 会造成两份 `.vcxproj`，工程配置（wxWidgets 路径）各不一样，别人拉下来编不过 |
| **改 `LogicModel.h` 先在群里说** | 它是四块的交汇点，改动会波及其他三人 |
| **每人提交前必须本地编译通过** | "编译不过但先 push 备份"会毁掉别人的 rebase |
| **提交前关掉正在运行的程序** | 否则 `LNK1104: 无法打开 exe` |
| **不要提交 `.mlc` 里带个人测试数据的文件** | 示例文件只保留 `examples/` 下的两个 |
| **文档改动也走 PR** | 避免 main 上的文档和代码不一致 |
| **大文件（截图）控制在几百 KB** | 现在 `docs/` 总共约 160KB，可以接受 |

---

## 六、建议的推进节奏（4 周）

| 周 | 目标 | 谁 |
|---|---|---|
| 第 1 周 | 各自读通自己那块 + 补注释 + 每人至少修 1 个 bug | 全员 |
| 第 2 周 | 每人加 1 个功能：A 加 2 种元件；B 加传播延迟或新测试；C 加视口裁剪或网格对齐优化；D 加属性表内联编辑 | 各自 |
| 第 3 周 | 集成：接上"运行时自定义元件库"（A+B+C+D 各出一部分） | 全员 |
| 第 4 周 | 打磨 + 写文档 + 录演示 | 全员 |

> 第 3 周的"运行时自定义元件库"是**最适合四人协作的任务**：
> A 负责 `CustomDef` 数据与序列化、B 负责逻辑模板求值、C 负责按图元绘制符号、D 负责元件库管理器对话框与工具箱入口。
> 代码骨架已经有了（`CustomDef` 结构、`CT_Custom` 分支、`CustomLibDialog`），缺的就是"接线"。
