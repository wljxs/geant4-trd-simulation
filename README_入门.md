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
确认无报错和输出路径正确，再增大 `/run/beamOn` 后面的数字。

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
| `/trd/output/directory` | ROOT 输出目录 | 如 `output/test01` |
| `/random/setSeeds` | 可选的固定随机种子 | 如 `12345 67890` |
| `/run/initialize` | 按以上参数初始化几何和物理 | 不带参数 |
| `/run/beamOn` | 开始模拟指定事例数 | 如 `/run/beamOn 10000` |

`totalLength / (foilThickness + gapThickness)` 若不是整数，程序只建立完整周期，
所以实际总长会略小于设定值。实际周期数和长度会打印在终端中。

所有 `/trd/...` 参数必须写在 `/run/initialize` 之前。初始化完成后，用
`/run/beamOn N` 模拟 N 个入射粒子。这与标准 Geant4 宏的使用方式一致。

## 怎样读 `trd.cc`

程序的执行顺序是：

1. `main()` 创建 `G4RunManager`。
2. 创建几何、物理、粒子源和各级 Action；各类同时注册自己负责的宏命令。
3. `main()` 通过 `/control/execute` 执行指定的 `.mac`。
4. 宏参数直接写入 `DetectorConstruction`、`PrimaryGeneratorAction` 或
   `RunAction`。
5. 宏中的 `/run/initialize` 建立几何和物理表。
6. 宏中的 `/run/beamOn N` 开始 N 个事例的循环。
7. `RunAction`/`OutputManager` 把结果写入 ROOT 文件。

若只想理解“怎样用宏启动 Geant4”，先读简化后的 `main()`，然后对照
`example.mac` 查看命令分别在哪个类注册。具体电离与渡越辐射模型继续读
`PhysicsList.cc`，几何材料读 `DetectorConstruction.cc`，数据记录读各个
`*Action.cc`。

## 其他源文件分别做什么

| 文件 | 初学时要抓住的重点 |
|---|---|
| `DetectorConstruction.hh/.cc` | 注册辐射体/气体命令，建立 World、辐射体和21层气体 |
| `PhysicsList.hh/.cc` | 注册标准电磁过程、衰变和 XTR 过程 |
| `PrimaryGeneratorAction.hh/.cc` | 注册束流命令，每个 event 发射一个沿 +z 的粒子 |
| `SteppingAction.hh/.cc` | 每一步检查所在气体层并累计能量沉积 |
| `StackingAction.hh/.cc` | 识别新产生的 TR 光子并标记其后代 |
| `EventAction.hh/.cc` | 保存一个 event 的临时累计结果 |
| `RunAction.hh/.cc` | 注册输出命令，在 run 开始/结束时打开和关闭 ROOT 文件 |
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
./build/trd --ui
```

交互窗口启动在 PreInit 状态。先输入需要的 `/trd/...` 参数，再输入
`/run/initialize` 和 `/run/beamOn 10`。定量批处理仍建议使用
`./build/trd macros/你的文件.mac`，便于保存完整参数。
