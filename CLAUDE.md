# CLAUDE.md — Kotocord AI 协作约定

## Git 操作

- **所有 git 命令由用户本人执行。** AI 只提供命令和预期反馈，不执行 `git add`、`git commit`、`git push`、`git rebase` 等任何 git 操作。
- AI 可以提供建议性的 git 命令，描述命令的作用和预期结果，但绝不在终端中实际运行它们。
- **不允许将 Claude 或任何 AI agent 列为 git 仓库的协作者（contributor/co-author）。** 所有 commit 的作者和贡献者只能是真实人类。

## 项目信息

- **项目名称**：Kotocord — VTuber 中间件（输入→字幕/情绪/TTS/虚拟形象驱动的节点化管线；2026-09-09 定位重写，原"直播字幕渲染工具"）
- **语言**：C++17
- **UI 框架**：Qt 6 (Widgets + Multimedia + OpenGL[QtNodes])
- **构建系统**：CMake 3.16+ + MSVC 2022
- **包管理**: FetchContent（whisper.cpp v1.7.5 / QtNodes pin-commit，均静态）+ 手动 third_party（Vosk）；Qt6 由 MaintenanceTool 安装，环境变量定位
- **测试框架**：Qt Test（11 个测试目标，tests/CMakeLists.txt 注册）

## 构建

```powershell
# 本机 (Qt 6.9.3) — 用 CMakeUserPresets.json (已 gitignore, 内含绝对路径)
cmake --preset msvc-debug     # configure: FetchContent 自动拉取 whisper.cpp
cmake --build --preset debug

# 其他机器 — 复制 CMakePresets.json.example → CMakePresets.json
#   Qt6 经 Qt6_DIR 环境变量或 CMAKE_PREFIX_PATH 定位
#   whisper.cpp 经 FetchContent 自动拉取 (需网络)
#   Vosk 需手动放置 third_party/vosk/ 预编译包

# Qt DLL 部署 (Qt MaintenanceTool 安装模式)
windeployqt bin/Debug/Kotocord.exe

# 运行测试
ctest --test-dir build/msvc-debug -C Debug
```

> Qt6 由 MaintenanceTool 管理，经 `CMAKE_PREFIX_PATH`（本机 preset）或 `Qt6_DIR` 环境变量（其他机器）定位。
> whisper.cpp 经 FetchContent 静态链接（v1.7.5），无需 DLL 部署。
> 此前 `USE_VCPKG_QT`/`USE_VCPKG` 双分支已于 2026-08-12 移除。

## 资源迁移（二进制依赖）

Vosk DLL/导入库与本地模型不进 git，通过打包/拆解脚本迁移：

```powershell
# 打包 (本机 → dist/kotocord-resources-<日期>.zip)
.\pack-resources.ps1

# 拆解安放 (新机器, 自动解压到仓库根 + 兜底生成导入库 + check-deps 验证)
.\unpack-resources.ps1 dist\kotocord-resources-<日期>.zip
```

| 脚本 | 用途 |
|------|------|
| `check-deps.ps1` | 资源就位检查 |
| `generate-vosk-import-lib.ps1` | 从 libvosk.dll 生成 MSVC 导入库 vosk.lib |
| `pack-resources.ps1` | 打包未跟踪二进制资源 |
| `unpack-resources.ps1` | 拆解资源包 + 验证 |

## 代码风格

- C++17 标准，使用现代 C++ 特性（智能指针、移动语义、模板）
- 遵循 C++ Core Guidelines（RAII、接口隔离、编译期类型检查）
- Qt 信号槽用于模块间通信
- 抽象接口（纯虚类）用于模块解耦
- 工厂模式用于运行时组件选择和依赖注入

## 模块架构

图即配置（2026-09 节点编辑器重构后）：main.cpp 从 `resources/pipelines/*.json` 读图 → `GraphCompiler` 校验装配 → `Pipeline` 持有实例。编辑器（QtNodes）编辑幻影节点，Apply 停机重建。

```
main.cpp → GraphCompiler → Pipeline (图装配)
                │
   ┌────────────┴────────────────────────────┐
   │ 源: VoiceInputNode(mic+Vosk/Whisper)     │ ← MainWindow (旁路: 启停/切换/API Key)
   │     ReminderScheduler(定时提醒)          │
   │ 处理: OrchestratorNode (复合)            │
   │   └ AppController(队列/视觉锁)            │
   │      + Mock/DeepSeek LLM + Kaomoji       │
   │ 汇/处理: SubtitleRenderer(TextLayout    │
   │   Engine 纯排版 + 呈现) / PythonEdgeTTS  │
   │   → TTSPlayer                            │
   └ SystemResourceMonitor (旁路, 不进图)      │

核心抽象 src/core/graph/: GraphModel(纯数据+JSON) /
NodeRegistry(元信息+连接器闭包) / GraphCompiler(校验+装配) /
Pipeline(生命周期, 先断连再逆序销毁) / NodeCatalog(真实节点目录)
UI 侧 src/ui/: NodeEditorView + GraphQtBridge(双向转换)
```

## 已知测试陷阱

- Qt Test 中 `QSignalSpy` 对自定义类型（如 `SubtitleFrame`）不稳定，改用 lambda 计数器
- 测试间状态泄漏：需在 `init()` 中调用 `resetState()`
- 需要 `QT_ASSUME_STDERR_HAS_CONSOLE=1` 以在 Windows 控制台输出测试日志
