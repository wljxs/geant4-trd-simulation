#ifndef TRDConstants_h
#define TRDConstants_h 1

#include "G4SystemOfUnits.hh"

namespace TRD {
// 前五项可由 CMake 的 -DTRD_...=... 在编译时覆盖。
// .mac 模式下，辐射体层数会根据总长度、薄膜和间隙厚度重新计算。
#ifndef TRD_REGIONS
#define TRD_REGIONS 21
#endif
#ifndef TRD_RADIATOR_LAYERS
#define TRD_RADIATOR_LAYERS 487
#endif
#ifndef TRD_FOIL_THICKNESS_UM
#define TRD_FOIL_THICKNESS_UM 2.15
#endif
#ifndef TRD_RADIATOR_GAP_UM
#define TRD_RADIATOR_GAP_UM 80.0
#endif
#ifndef TRD_RADIATOR_CUT_UM
#define TRD_RADIATOR_CUT_UM 1.0
#endif
constexpr int kRegions = TRD_REGIONS;  // 气体读出层数
constexpr int kRadiatorLayers = TRD_RADIATOR_LAYERS;
constexpr double kFoilThickness = TRD_FOIL_THICKNESS_UM * um;
constexpr double kRadiatorGap = TRD_RADIATOR_GAP_UM * um;
// production cut 是产生次级粒子的“射程阈值”，不是直接的能量阈值。
constexpr double kRadiatorProductionCut = TRD_RADIATOR_CUT_UM * um;
constexpr double kGasLength = 40.0 * mm;
constexpr double kRegionLength = kGasLength / kRegions;
constexpr double kGasProductionCut = 1.0 * um;
}

#endif
