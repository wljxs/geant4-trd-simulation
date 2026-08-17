cmake_minimum_required(VERSION 3.16)
project(TRDSimulation VERSION 1.0 LANGUAGES CXX)

# 生成 compile_commands.json，方便编辑器跳转和代码补全。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 这些是“编译期默认值”。常规模拟更推荐在 .mac 中修改可用参数。
set(TRD_REGIONS 21 CACHE STRING "Number of gas readout layers")
set(TRD_RADIATOR_LAYERS 487 CACHE STRING "Number of Mylar/air radiator periods")
set(TRD_FOIL_THICKNESS_UM 2.15 CACHE STRING "Mylar foil thickness in micrometres")
set(TRD_RADIATOR_GAP_UM 80.0 CACHE STRING "Air gap thickness in micrometres")
set(TRD_RADIATOR_CUT_UM 1.0 CACHE STRING "Radiator production cut in micrometres")
# REQUIRED 表示缺少 Geant4 或 ROOT 时，配置阶段立即给出错误。
find_package(Geant4 REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Core Hist Tree RIO)
include(${Geant4_USE_FILE})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 一个可执行文件由入口、几何、物理、粒子源和各级 Action 共同组成。
add_executable(trd
  trd.cc
  src/SimulationConfig.cc
  src/DetectorConstruction.cc
  src/PhysicsList.cc
  src/PrimaryGeneratorAction.cc
  src/OutputManager.cc
  src/RunAction.cc
  src/EventAction.cc
  src/StackingAction.cc
  src/SteppingAction.cc)

# 把 CMake 缓存变量变成 C++ 预处理宏，供 TRDConstants.hh 使用。
target_include_directories(trd PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(trd PRIVATE
  TRD_REGIONS=${TRD_REGIONS}
  TRD_RADIATOR_LAYERS=${TRD_RADIATOR_LAYERS}
  TRD_FOIL_THICKNESS_UM=${TRD_FOIL_THICKNESS_UM}
  TRD_RADIATOR_GAP_UM=${TRD_RADIATOR_GAP_UM}
  TRD_RADIATOR_CUT_UM=${TRD_RADIATOR_CUT_UM})
# Geant4 负责输运，ROOT 负责 TTree/直方图和 .root 文件。
target_link_libraries(trd PRIVATE ${Geant4_LIBRARIES} ROOT::Core ROOT::Hist ROOT::Tree ROOT::RIO)
