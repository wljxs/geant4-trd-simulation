# 版本功能记录

本文件记录正式版本包含的主要功能。模拟生成的 ROOT 文件、日志、图片和
分析指标不属于源码版本，均由 `.gitignore` 排除。

## v1.3（2026-08-18）

对应提交：[`71daed9`](https://github.com/wljxs/geant4-trd-simulation/commit/71daed9f3b5f4770aaf7e0438a8150da4c3ccafa)

- 在辐射体出口或指定的下游虚拟面统计直接 TR 光子的通量。
- 新增 `/trd/scoring/exitFlux` 和 `/trd/scoring/exitDistance` 宏命令；虚拟面没有材料，不影响光子输运。
- ROOT 输出保留产生处的 `tr_photon_*`，并新增出口处的 `tr_exit_photon_*` 数量、能量和方向数据。
- `trOnly` 模式改为在 TR 光子穿过出口统计面后终止输运，兼容无气体几何。
- 源码默认 XTR 模型由 `gammaM` 改为 `gammaR`；宏命令和旧环境变量仍可覆盖。
- 新增 `simulation2`，包含动量、箔厚和间隙共 12 组 `gammaR` Fig.3 扫描宏。
- 新增 CERN ROOT 分析宏，使用 `TF1` 绘制理论曲线并与 Geant4 出口能谱比较。
- `gammaR` 的 12 组验证模拟均完成；1--32 keV 积分的 Geant4/理论平均比值为 0.992。

## v1.1（2026-08-18）

对应提交：[`b9827db`](https://github.com/wljxs/geant4-trd-simulation/commit/b9827db7596e74ff88f72dcef866b08da77ecccb)

- 新增 `/trd/radiator/model`，可在宏文件中选择 `gammaM`、`gammaR` 或 `transpR`。
- 新增 `/trd/mode/trOnly`，可仅模拟和统计 TR 光子而不建立下游气体探测器。
- `.mac` 命令成为模型和模式的主要配置方式，同时保留旧环境变量兼容入口。
- 新增 `simulation` 下的 12 组 Fig.3 宏，分别扫描入射动量、箔片厚度和空气间隙。
- Fig.3 扫描脚本改为在生成的宏中显式写入 `gammaM` 和 `trOnly`，提高结果可复现性。
- TR-only 模式可在记录生成谱后停止 TR 光子输运，减少无探测器扫描的运行时间。

## v1（2026-08-17）

对应提交：[`fd2620b`](https://github.com/wljxs/geant4-trd-simulation/commit/fd2620b602b63ab5496993862ad49b75af987c1a)

- 气体电离统一使用 Geant4 标准电磁物理模型。
- 移除气体 Region 中的 PAI 和 PAIPhot 电离覆盖，不再依赖 `TRD_IONISATION_MODEL` 或 `TRD_PAI_MODEL`。
- 电子、μ 子和 π 子均使用 `G4EmStandardPhysics` 提供的标准电离过程及能损涨落。
- 保留辐射体 XTR 过程、衰变过程以及气体 Region 的独立 production cut。
- Rohacell 3 GeV/c、4 cm 模拟脚本同步取消 PAIPhot 环境变量设置，并更新相关说明。
