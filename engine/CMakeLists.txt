cmake_minimum_required(VERSION 3.20)

# ---- GLAD 2.0.8 gl:core=4.6 + loader (third_party) ----
add_library(glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include)
target_compile_options(glad PRIVATE -w)
set_target_properties(glad PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)

# ---- Engine ----
add_library(cjoka_engine STATIC
    src/Core/Window.cpp
    src/Core/Application.cpp
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
    src/Assets/AssetManager.cpp
    src/GUI/Font.cpp
    src/GUI/kGUI.cpp
)

target_include_directories(cjoka_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

target_link_libraries(cjoka_engine PUBLIC
    glad
    OpenGL::GL
    glfw
    dl
)

# ECS — header-only, но зависимости (glm)
# glm header-only, find optional
find_package(glm QUIET)
if(glm_FOUND)
    target_link_libraries(cjoka_engine PUBLIC glm::glm)
endif()

set_target_properties(cjoka_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(cjoka_engine PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
