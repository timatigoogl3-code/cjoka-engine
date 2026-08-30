cmake_minimum_required(VERSION 3.20)

# ---- GLAD 2.0.8 gl:core=4.6 + loader (third_party) ----
add_library(glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include)
target_compile_options(glad PRIVATE -w)
set_target_properties(glad PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)

# ---- Engine Source List ----
set(ENGINE_SOURCES
    src/Core/Window.cpp
    src/Core/Input.cpp
    src/Core/Application.cpp
    src/Core/Profiler.cpp
    src/Scene/SceneManager.cpp
    src/Renderer/Shader.cpp
    src/Renderer/Mesh3D.cpp
    src/Renderer/Renderer.cpp
    src/Renderer/Texture.cpp
    src/Renderer/MeshLoader.cpp
    src/Renderer/Framebuffer.cpp
    src/Renderer/PostProcess.cpp
    src/Renderer/RenderPipeline.cpp
    src/Renderer/Batcher.cpp
    src/Renderer/Skybox.cpp
    src/Renderer/ShadowMap.cpp
    src/Renderer/CascadedShadowMap.cpp
    src/Renderer/ForwardPlus.cpp
    src/Renderer/MeshClusters.cpp
    src/Renderer/Decal.cpp
    src/Assets/AssetManager.cpp
    src/GUI/Font.cpp
    src/GUI/kGUI.cpp
    src/GUI/ImGuiLayer.cpp
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_demo.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
    third_party/imguizmo/ImGuizmo.cpp
)

if(CJOKA_ENABLE_AUDIO)
    list(APPEND ENGINE_SOURCES src/Audio/AudioEngine.cpp)
endif()

if(CJOKA_ENABLE_PHYSX)
    list(APPEND ENGINE_SOURCES src/Physics/Physics.cpp)
endif()

if(CJOKA_ENABLE_FBX)
    list(APPEND ENGINE_SOURCES src/Animation/FBXLoader.cpp third_party/ufbx/ufbx.c)
endif()

# ---- Engine Target ----
add_library(cjoka_engine STATIC ${ENGINE_SOURCES})

set_source_files_properties(
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_demo.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
    third_party/imguizmo/ImGuizmo.cpp
    PROPERTIES COMPILE_OPTIONS "-w"
)

if(CJOKA_ENABLE_AUDIO)
    set_source_files_properties(src/Audio/AudioEngine.cpp PROPERTIES COMPILE_OPTIONS "-w")
endif()
if(CJOKA_ENABLE_FBX)
    set_source_files_properties(third_party/ufbx/ufbx.c PROPERTIES COMPILE_OPTIONS "-w")
endif()

target_include_directories(cjoka_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/entt/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imguizmo
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(cjoka_engine PUBLIC
    glad
    OpenGL::GL
    glfw
    Threads::Threads
    m
    dl
)

# ---- PhysX 5.5 (prebuilt static) ----
if(CJOKA_ENABLE_PHYSX)
    set(PHYSX5_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/physx5)
    add_library(physx5 INTERFACE)
    target_include_directories(physx5 INTERFACE ${PHYSX5_DIR}/include)
    target_link_directories(physx5 INTERFACE ${PHYSX5_DIR}/lib)
    target_link_libraries(physx5 INTERFACE
        -Wl,--start-group
        PhysX_static_64
        PhysXCommon_static_64
        PhysXFoundation_static_64
        PhysXExtensions_static_64
        PhysXCooking_static_64
        PhysXCharacterKinematic_static_64
        PhysXPvdSDK_static_64
        -Wl,--end-group
    )
    target_link_libraries(cjoka_engine PUBLIC physx5)
    target_compile_definitions(cjoka_engine PUBLIC CJOKA_HAS_PHYSX=1)
else()
    target_compile_definitions(cjoka_engine PUBLIC CJOKA_HAS_PHYSX=0)
    message(STATUS "[cjoka] PhysX 5.5 DISABLED -> saving ~35MB binary footprint!")
endif()

if(CJOKA_ENABLE_AUDIO)
    target_compile_definitions(cjoka_engine PUBLIC CJOKA_HAS_AUDIO=1)
else()
    target_compile_definitions(cjoka_engine PUBLIC CJOKA_HAS_AUDIO=0)
    message(STATUS "[cjoka] Audio engine DISABLED -> stripped miniaudio!")
endif()

if(CJOKA_ENABLE_FBX)
    target_compile_definitions(cjoka_engine PUBLIC CJOKA_HAS_FBX=1)
else()
    target_compile_definitions(cjoka_engine PUBLIC CJOKA_HAS_FBX=0)
endif()

# glm header-only, find optional
find_package(glm QUIET)
if(glm_FOUND)
    target_link_libraries(cjoka_engine PUBLIC glm::glm)
endif()

set_target_properties(cjoka_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(cjoka_engine PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
