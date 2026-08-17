# TRD Geant4 模拟入门

## 先运行一次

在已经加载 Geant4 和 ROOT 环境的终端中执行：

```bash
cd /home/wljxs/projects/TR/geant4simulation
cmake -S . -B build
cmake --build build -j
./build/trd macros/example.mac
```

示例只模拟 10 个入射电子，结果写到：

```text
output/tutorial/trd_electron.root
```

建议复制 `macros/example.mac` 后只修改副本。正式模拟前先用 10 个事例检查，
确认无报错和输出路径正确，再增大 `/trd/run/events`。

## `.mac` 参数

| 命令 | 含义 | 可用值/单位示例 |
|---|---|---|
| `/trd/beam/particle` | 入射粒子 | `e-`、`pi-`、`mu-` |
| `/trd/beam/momentum` | 入射动量 | `3 GeV` |
| `/trd/radiator/enabled` | 是否放置辐射体 | `true` 或 `false` |
| `/trd/radiator/material` | 薄膜材料 | Geant4 材料名，如 `G4_MYLAR` |
| `/trd/radiator/foilThickness` | 单层薄膜厚度 | `2.15 um` |
| `/trd/radiator/gapThickness` | 单层空气间隙 | `80 um` |
| `/trd/radiator/totalLength` | 辐射体目标总长 | `4 cm` |
| `/trd/detector/gas` | 探测器气体 | `XeNeIsobutane`、`XeCO2_85_15`、`XeCO2_95_5` |
| `/trd/run/events` | 入射粒子（事例）数 | 正整数，如 `10000` |
| `/trd/output/directory` | ROOT 输出目录 | 如 `output/test01` |

`totalLength / (foilThickness + gapThickness)` 若不是整数，程序只建立完整周期，
所以实际总长会略小于设定值。实际周期数和长度会打印在终端中。

这个项目会在读完宏以后自动初始化并调用 `BeamOn`，因此宏里不要添加
`/run/initialize` 或 `/run/beamOn`。

## 怎样读 `trd.cc`

程序的执行顺序是：

1. `main()` 识别 `.mac` 文件并调用 `RunMacro()`。
2. `SimulationConfig` 注册 `/trd/...` 命令，宏把参数写入 `config`。
3. `RunSimulation()` 创建 `G4RunManager`。
4. 注册 `DetectorConstruction`（几何）和 `PhysicsList`（物理过程）。
5. 注册粒子源及 Run、Event、Stacking、Stepping 动作。
6. `Initialize()` 建立几何和物理表，`BeamOn(events)` 开始事例循环。
7. `RunAction`/`OutputManager` 把结果写入 ROOT 文件。

若只想理解“怎样用宏启动 Geant4”，先读 `main()`、`RunMacro()` 和
`RunSimulation()`；具体电离与渡越辐射模型再继续读 `PhysicsList.cc`，几何材料
读 `DetectorConstruction.cc`，数据记录读各个 `*Action.cc`。

## 其他源文件分别做什么

| 文件 | 初学时要抓住的重点 |
|---|---|
| `SimulationConfig.hh/.cc` | 把 `/trd/...` 宏命令绑定到 C++ 参数 |
| `DetectorConstruction.hh/.cc` | 建立 World、等效辐射体和 21 层气体几何 |
| `PhysicsList.hh/.cc` | 注册标准电磁过程、可选 PAI 电离模型和 XTR 过程 |
| `PrimaryGeneratorAction.hh/.cc` | 每个 event 发射一个沿 +z 的初级粒子 |
| `SteppingAction.hh/.cc` | 每一步检查所在气体层并累计能量沉积 |
| `StackingAction.hh/.cc` | 识别新产生的 TR 光子并标记其后代 |
| `EventAction.hh/.cc` | 保存一个 event 的临时累计结果 |
| `RunAction.hh/.cc` | 在 event 数据与 ROOT 输出管理器之间转发 |
| `OutputManager.hh/.cc` | 创建和填充 ROOT TTree、直方图，最后写盘 |
| `TRDConstants.hh` | 保存编译期默认层数、厚度和 production cut |

数据流可以简记为：

```text
Geant4 产生新轨迹 ──> StackingAction ──> 标记 TR 来源
粒子完成一个 step ──> SteppingAction ──> EventAction 累加
event 结束          ──> RunAction ──> OutputManager ──> ROOT 文件
```

## ROOT 中新增的电离和二维分布

每个 event 的 `events` 树包含：

- `ionization_energy_keV[21]`：每层非 TR（电离背景）沉积能。
- `ionization_total_energy_keV`：21 层电离背景沉积能之和。
- `tr_energy_keV[21]` 和 `tr_total_energy_keV`：TR 光子及其后代的沉积。
- `energy_keV[21]` 和 `total_energy_keV`：所有来源的总沉积。

这里采用
`ionization_energy = total_energy - tr_energy`。因此它严格表示“非 TR 来源
的沉积能”，在本模拟中主要是入射粒子及其电离次级粒子的贡献。

ROOT 文件还直接保存两张二维直方图：

- `region_tr_energy`：横轴为气体层号，纵轴为该层 TR 沉积能。
- `region_ionization_energy`：横轴为气体层号，纵轴为该层电离背景沉积能。

所有 `region_*` 二维能谱的纵轴范围均为 0--20 keV，共 200 个 bin，
即每个 bin 为 0.1 keV。超过 20 keV 的值进入 ROOT overflow bin；
TTree 中的原始数值不会被截断。

## 可视化（可选）

```bash
./build/trd --ui e-
```

窗口打开后可输入 `/run/beamOn 10`。这个兼容入口不读取 `.mac` 中的
`/trd/...` 参数；定量批处理仍建议使用 `./build/trd macros/你的文件.mac`。
