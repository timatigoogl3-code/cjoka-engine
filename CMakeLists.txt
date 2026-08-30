# cjoka — корневой проект
# engine/  -> libcjoka_engine.a (модульный движок, не пересобирается при правке game/)
# game/    -> exe cjoka (игровая логика, линкуется к движку)
cmake_minimum_required(VERSION 3.20)
project(cjoka LANGUAGES C CXX)

# ---------- Toolchain ----------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER clang)
endif()
if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER clang++)
endif()

message(STATUS "C:   ${CMAKE_C_COMPILER}")
message(STATUS "CXX: ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")

# ---------- Модульные подсистемы (Strip & Modularize) ----------
option(CJOKA_ENABLE_PHYSX "Enable NVIDIA PhysX 5.5 physics subsystem" ON)
option(CJOKA_ENABLE_AUDIO "Enable miniaudio 3D spatial sound engine" ON)
option(CJOKA_ENABLE_FBX "Enable ufbx FBX 3D models and skeletal animation" ON)

# ---------- Модули ----------
add_subdirectory(engine)
add_subdirectory(game)
